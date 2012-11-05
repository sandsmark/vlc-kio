#ifndef KIOPLUGIN_H
#define KIOPLUGIN_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <kio/global.h>

class KioPlugin : public QObject
{
    Q_OBJECT

public:
    void handleResult(KJob *job);
    void handleData(KJob *job, const QByteArray &data);
    void handlePosition(KJob *job, KIO::filesize_t pos);
    
    QByteArray m_data;
    KIO::filesize_t m_pos;
    bool m_eof;
};

#endif
