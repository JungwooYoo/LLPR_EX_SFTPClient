#ifndef COMMONVALUES_H
#define COMMONVALUES_H

#include <QObject>
#include <QLabel>
#include "dataclass.h"
#include "ftpclient.h"
#include "sftpclient.h"

class commonvalues : public QObject
{
    Q_OBJECT
public:
    explicit commonvalues(QObject *parent = nullptr);

    /*        Center  value            */
       static int center_count;
       static QList<CenterInfo> center_list;
       //static QList<FtpClient *> clientlist;
       static QList<SftpClient *> clientlist;
       enum FTPRetryType
       {
           FTPRetry_ON = 0x00,
           FTPRetry_OFF = 0x01
       };
       static int ftpretry;
       static QString FTPSavePath;

       static int loglevel;
};

#endif // COMMONVALUES_H
