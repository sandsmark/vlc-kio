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

#ifndef KIOPLUGIN_H
#define KIOPLUGIN_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QSemaphore>
#include <QtCore/QDebug>
#include <kio/global.h>
#include <kio/job.h>
#include <kio/filejob.h>
#include <vlc_common.h>
struct access_sys_t;
class KioPlugin : public QObject
{
    Q_OBJECT
public:
    KioPlugin();
    
public slots:
    void openUrl(const QUrl &url);
    void handleResult(KJob *job);
    void handleOpen(KIO::Job *job);
    void handleData(KIO::Job *job, const QByteArray &data);
    void handlePosition(KIO::Job *job, KIO::filesize_t pos);
    void read(uint64_t amount) { m_job->read(amount); }
    void seek(uint64_t position) { m_data.clear(); m_job->seek(position); }

public:
    QMutex m_mutex;
    QMutex m_launched;
    QByteArray m_data;
    KIO::filesize_t m_pos;
    KIO::FileJob *m_job;
    bool m_eof;
    bool m_waitingForData;
};

#endif
