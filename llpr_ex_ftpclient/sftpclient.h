#ifndef SFTPCLIENT_H
#define SFTPCLIENT_H

#include <QObject>
#include <QThread>
#include <QFtp>
#include <QMutex>

#include <QTextCodec>
#include <QDirIterator>

#include "syslogger.h"
#include "dataclass.h"
#include "ftpclient.h"

#include <iostream>
#include <execinfo.h>
using namespace std;

#include <libssh2.h>
#include <libssh2_sftp.h>

class SftpClient : public QThread
{
    Q_OBJECT

public:
    explicit SftpClient(int id, Syslogger *plogger,QThread *parent = nullptr);
    ~SftpClient();

    bool start(CenterInfo configinfo);
    void run();
    void stop();

    bool sshinit();
    bool connectSocket();
    bool initSession();
    bool initSFTP();

    bool sendData();
    void closeSFTP();
    void closesession();

    void ScanSendDataFiles();

public:
    CenterInfo config;

    int m_iFTPTrans;
    int m_iFTPTransFail;
    int m_iFTPRename;
    int m_iFTPRenameFail;

    const char *username;
    const char *password;
    const char *localfile;
    const char *sftppath;

private:
    bool brun;
    int centerid;
    Syslogger *log;

    QTextCodec * codec;
    QTextEncoder *encoderw;

    unsigned long hostaddr;
    int sock;
    LIBSSH2_SESSION *session;

    FILE *local;
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    QString logstr;
    QString noCharfName;

    FtpClient::SendFileInfo sftp_sendfileInfo;
    FtpClient::SendFileInfoList sftp_sendfileInfolist;

signals:
    void logappend(QString logstr);

public slots:
};

#endif // SFTPCLIENT_H
