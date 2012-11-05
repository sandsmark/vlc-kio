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
#include <kio/global.h>

class KioPlugin : public QObject
{
    Q_OBJECT

public slots:
    void handleResult(KJob *job);
    void handleData(KJob *job, const QByteArray &data);
    void handlePosition(KJob *job, KIO::filesize_t pos);
    
    void requestData();
    
    QByteArray m_data;
    KIO::filesize_t m_pos;
    bool m_eof;
};

#endif
