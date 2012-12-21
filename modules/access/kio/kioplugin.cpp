/*****************************************************************************
 * kioplugin.cpp: An access plugin using KDE KIO.
 *****************************************************************************
 * Copyright (C) 2012 Martin Sandsmark
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
#include <vlc_interface.h>
#include <vlc_access.h>

// Qt includes
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>

// KDE includes
#include <kio/job.h>
#include <kio/filejob.h>
#include <kprotocolmanager.h>
#include <QtGui/QApplication>

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

// Internal state for an instance of the module
struct access_sys_t
{
    KIO::FileJob *job;
    KioPlugin plugin;
};

static int Open(vlc_object_t *obj)
{
    access_t *access = (access_t*)obj;
    access_InitFields(access);
    access->pf_block = Block;
    access->pf_control = Control;
    access->pf_seek = Seek;
    access->pf_read = 0;
    access_sys_t *d = access->p_sys = new access_sys_t;
    QMutexLocker locker(&d->plugin.m_mutex);
    d->plugin.moveToThread(&d->plugin.m_thread);
    
    
    // Construct a proper URL
    qDebug() << access->psz_location << access->psz_access;
    QUrl url(QString::fromLocal8Bit(access->psz_access) + QLatin1String("://") + QString::fromLocal8Bit(access->psz_location));
    
    qDebug() << "Opening URL: " << url;
    
    // Check if we can open it
    if (!KProtocolManager::supportsOpening(url)) {
        qDebug() << "Unable to open URL.";
        delete d;
        return VLC_EGENERIC;
    }
    QMetaObject::invokeMethod(&d->plugin, "openUrl", Q_ARG(const QUrl&, url), Q_ARG(void*, (void*)d));
    qDebug() << "invoked";
    
    return VLC_SUCCESS;
}

/**
 * Stops the interface. 
 */
static void Close(vlc_object_t *obj)
{
    access_t *intf = (access_t *)obj;
    access_sys_t *sys = intf->p_sys;
    QMutexLocker locker(&sys->plugin.m_mutex);

    /* Free internal state */
    delete sys;
}

static int Seek(access_t *obj, uint64_t pos)
{
    
    qDebug() << "seeking to" << pos;
    access_t *intf = (access_t *)obj;
    access_sys_t *sys = intf->p_sys;
    QMutexLocker locker(&sys->plugin.m_mutex);
    sys->job->seek(pos);
    
    return VLC_SUCCESS;
}

static int Control(access_t *obj, int query, va_list arguments)
{
    qDebug() << "control query:" << query;
    
    access_t *intf = (access_t *)obj;
    access_sys_t *sys = intf->p_sys;
    QMutexLocker locker(&sys->plugin.m_mutex);
    
    Q_UNUSED(sys);
    
    bool *b = (bool*)va_arg(arguments, bool*);
    int64_t *i = (int64_t*)va_arg(arguments, int64_t *);

    switch(query) {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_PAUSE:
            *b = true;
            break;
        case ACCESS_CAN_CONTROL_PACE:
        case ACCESS_CAN_FASTSEEK:
            *b = false;
            break;
        case ACCESS_GET_PTS_DELAY:
            *i = 300000;//###
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
            qWarning() << "unimplemented query:" << query;
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static block_t *Block(access_t *obj)
{
    
    qDebug() << "getting block...";
    access_t *intf = (access_t *)obj;
    access_sys_t *sys = intf->p_sys;
    QMutexLocker locker(&sys->plugin.m_mutex);
    

    if (sys->plugin.m_eof) {
        intf->info.b_eof = true;
    } else {
        sys->job->read(32768);
    }
    
    const QByteArray &buffer = sys->plugin.m_data;
    
    if (buffer.size() == 0)
        return 0;
    
    block_t *block = block_Alloc(buffer.size());
    memcpy(block->p_buffer, buffer.constData(), buffer.size());
    return block;
}

void KioPlugin::handleData(KIO::Job* job, const QByteArray& data)
{
    qDebug() << "getting data..." << data.size();
    
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(job);
    m_data.append(data);
}

void KioPlugin::handlePosition(KIO::Job* job, KIO::filesize_t pos)
{
    qDebug() << "handling pos:" << pos;
    Q_UNUSED(job);
    m_pos = pos;
}

void KioPlugin::handleResult(KJob* job)
{
    qDebug() << "got end";
    Q_UNUSED(job);
    m_eof = true;
}

KioPlugin::KioPlugin(): QObject()
{
    moveToThread(&m_thread);
}

void KioPlugin::openUrl(const QUrl& url, void* lol)
{
    qDebug() << Q_FUNC_INFO << url;
    access_sys_t *d = reinterpret_cast<access_sys_t*>(lol);
    d->job = KIO::open(url, QIODevice::ReadOnly);
    QObject::connect(d->job, SIGNAL(result(KJob*)), this, SLOT(handleResult(KJob*)));
    QObject::connect(d->job, SIGNAL(data(KIO::Job*, const QByteArray&)), this, SLOT(handleData(KIO::Job*, const QByteArray&)));
    QObject::connect(d->job, SIGNAL(position(KIO::Job*, KIO::filesize_t)), this, SLOT(handlePostion(KIO::Job*, KIO::filesize_t)));
    QObject::connect(d->job, SIGNAL(open(KIO::Job*)), this, SLOT(handleOpen(KIO::Job*)));
    
    d->job->addMetaData("UserAgent", QLatin1String("VLC KIO Plugin"));
    qDebug() << "returning";
    qApp->processEvents();
}

void KioPlugin::handleOpen(KJob* job)
{
    qDebug() << "open!";
    m_open.release(1);
}

