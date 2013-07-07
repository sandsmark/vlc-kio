/*****************************************************************************
 * kioplugin.cpp: An access plugin using KDE KIO.
 *****************************************************************************
 * Copyright (C) 2013 Martin Sandsmark
 *
 * Authors: Martin Sandsmark <martin.sandsmark@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "kioplugin.h"

// Generic includes
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// VLC includes
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

// Qt includes
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>

// KDE includes
#include <KIO/Job>
#include <KProtocolManager>
#include <QtGui/QApplication>

/************************ VLC stuffs *************************/

// Forward declarations
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int Control(access_t *, int i_query, va_list args);
static block_t *Block(access_t *);
static int Seek(access_t *obj, uint64_t pos);

// Module descriptor
vlc_module_begin()
    set_shortname(N_("KIO"))
    set_description(N_("KIO access module"))
    set_capability("access", 600)
    set_callbacks(Open, Close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_shortcut("sftp")
vlc_module_end ()

static int Open(vlc_object_t *obj)
{
    access_t *access = (access_t*)obj;

    // Construct a proper URL
    QUrl url(QString::fromLocal8Bit(access->psz_access) + QLatin1String("://") + QString::fromLocal8Bit(access->psz_location));

    // Check if we can open it
    if (!KProtocolManager::supportsOpening(url)) {
        qWarning() << Q_FUNC_INFO << "Unable to open URL:" << url;
        return VLC_EGENERIC;
    }

    access_InitFields(access);
    access->pf_block = Block;
    access->pf_control = Control;
    access->pf_seek = Seek;
    access->pf_read = 0;
    KioPlugin *kio = new KioPlugin;
    access->p_sys = reinterpret_cast<access_sys_t*>(kio);

    kio->m_launched.lock();
    QMetaObject::invokeMethod(kio, "openUrl", Q_ARG(const QUrl&, url));
    kio->m_launched.lock(); // Wait until file is opened

    return VLC_SUCCESS;
}

/**
 * Stops the interface. 
 */
static void Close(vlc_object_t *obj)
{
    access_t *intf = (access_t *)obj;
    KioPlugin *kio = reinterpret_cast<KioPlugin*>(intf->p_sys);

    // Delete everything
    kio->deleteLater();
}

static int Seek(access_t *obj, uint64_t pos)
{
    KioPlugin *kio = reinterpret_cast<KioPlugin*>(obj->p_sys);

    // Invoke this via the meta object to make sure it is in the right thread (KIO is not threadsafe)
    QMetaObject::invokeMethod(kio, "seek", Q_ARG(quint64, pos));
    obj->info.b_eof = false;
    obj->info.i_pos = pos;
    return VLC_SUCCESS;
}

static int Control(access_t*, int query, va_list arguments)
{
    bool *b;
    int64_t *i;

    switch(query) {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_PAUSE:
            b = (bool*)va_arg(arguments, bool*);
            *b = true;
            break;
        case ACCESS_CAN_CONTROL_PACE:
        case ACCESS_CAN_FASTSEEK:
            b = (bool*)va_arg(arguments, bool*);
            *b = false;
            break;
        case ACCESS_GET_PTS_DELAY:
            i = (int64_t*)va_arg(arguments, int64_t *);
            *i = 300000;
            break;
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_GET_META:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_SIGNAL:
            return VLC_EGENERIC;
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
            break;
        default:
            qWarning() << Q_FUNC_INFO << "unimplemented query:" << query;
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

// 65536 is the largest size KIO can get apparently, 65536/8=8192 is stolen from the sftp access plugin
#define BLOCK_SIZE 65536/8

static block_t *Block(access_t *obj)
{
    KioPlugin *kio = reinterpret_cast<KioPlugin*>(obj->p_sys);
    if (kio->m_waitingForData)
        return NULL;

    const QByteArray &buffer = kio->m_data;

    if (kio->m_eof) {
        obj->info.b_eof = true;
    } else if (buffer.size() < BLOCK_SIZE) {
        // If we aren't at the end of the file, fetch more
        kio->m_waitingForData = true;

        // Invoke this via the meta object to make sure it is in the right thread (KIO is not threadsafe)
        QMetaObject::invokeMethod(kio, "read", Q_ARG(quint64, BLOCK_SIZE)); // 65536 seems to be the max we can get from kio
        QMutex dataMutex;
        kio->m_waitForData.wait(&dataMutex);
    }

    QMutexLocker locker(&kio->m_mutex);
    if (buffer.size() == 0)
        return NULL;

    block_t *block = block_Alloc(buffer.size());
    memcpy(block->p_buffer, buffer.constData(), buffer.size() - 1);
    obj->info.i_size = kio->m_job->size();
    obj->info.i_pos = kio->m_pos;

//    qDebug() << 100 * kio->m_pos / kio->m_job->size();


    kio->m_pos += kio->m_data.size();
    kio->m_data.clear();
    return block;
}

/************************ KIO stuffs *************************/

KioPlugin::KioPlugin(): QObject()
{
    // KIO only supports being called from the main Qt thread
    moveToThread(qApp->thread());
}

KioPlugin::~KioPlugin()
{
    m_job->kill();
}


void KioPlugin::openUrl(const QUrl& url)
{
    m_eof = false;
    m_waitingForData = false;
    m_pos = 0;
    m_job = KIO::open(url, QIODevice::ReadOnly);
    QObject::connect(m_job, SIGNAL(result(KJob*)), this, SLOT(handleResult(KJob*)));
    QObject::connect(m_job, SIGNAL(data(KIO::Job*, const QByteArray&)), this, SLOT(handleData(KIO::Job*, const QByteArray&)));
    QObject::connect(m_job, SIGNAL(position(KIO::Job*, KIO::filesize_t)), this, SLOT(handlePosition(KIO::Job*, KIO::filesize_t)));
    QObject::connect(m_job, SIGNAL(open(KIO::Job*)), this, SLOT(handleOpen(KIO::Job*)));

    m_job->addMetaData("UserAgent", QLatin1String("VLC KIO Plugin"));
}

void KioPlugin::handleData(KIO::Job* job, const QByteArray& data)
{
    Q_UNUSED(job);
    QMutexLocker locker(&m_mutex);
    m_data.append(data);
    m_waitingForData = false;
    m_waitForData.wakeAll();
}

void KioPlugin::handlePosition(KIO::Job* job, KIO::filesize_t pos)
{
    Q_UNUSED(job);
    QMutexLocker locker(&m_mutex);
    m_pos = pos;
}

void KioPlugin::handleResult(KJob* job)
{
    if (job->error())
        qWarning() << Q_FUNC_INFO << job->errorString();

    m_eof = true;
    m_launched.unlock();
}

void KioPlugin::handleOpen(KIO::Job* job)
{
    Q_UNUSED(job);
    m_launched.unlock();
}
void KioPlugin::seek(quint64 position)
{
    QMutexLocker locker(&m_mutex);

    // Discard the current buffer
    m_data.clear();

    m_job->seek(position);
}

void KioPlugin::read(quint64 amount)
{
    m_job->read(amount);
}