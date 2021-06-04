#ifndef FTPCLIENT_H
#define FTPCLIENT_H

#include <QObject>
#include <QThread>
#include <QFtp>
#include <QMutex>

#include <QTextCodec>

#include "syslogger.h"
#include "dataclass.h"


class FtpClient : public QThread
{
    Q_OBJECT

public:
    class SendFileInfo
    {
    public:
        SendFileInfo()
        {
            filename = "";
            filepath = "";
        }

        bool SaveFile(QString path, QString fname, QByteArray filedata);
        bool ParseFilepath(QString _filepath); //센터로 보낼 데이터인지 파일명을 체크해서 리턴함.

    public:
        QString filename;
        QString filepath;
    };

    class SendFileInfoList
    {
    public:
        bool AddFile(SendFileInfo data);
        SendFileInfo GetFirstFile();
        void RemoveFirstFile(SendFileInfo data);
        void ClearAll();
        int Count();
    public:
        QList<SendFileInfo> fileList;
        const int MAX_FILE = 10000;        
        QMutex mutex;

    };

public:
    explicit FtpClient(int id, Syslogger *plogger,QThread *parent = nullptr);
    ~FtpClient();

    bool start(CenterInfo configinfo);
    void run();
    void stop();
    bool connect2FTP();
//    void CleanSendFilepath( QList<CenterInfo> list);
    void ScanSendDataFiles();
    void DeleteFile(QString filepath);

    bool SendVehicleInfo(QString fename, QByteArray ftpData);

signals:
    void logappend(QString logstr);

public slots:
    void ftpCommandFinished(int id,bool error);
public:
    bool brun;
    int centerid;
    Syslogger *log;
    int m_loglevel;
    const int MAXFTPCOUNT = 500;
    SendFileInfoList sendFileList;
    int m_RetryInterval;
    //QString FTP_SEND_PATH;
    CenterInfo config;
    QFtp *m_pftp;
    int m_iFTPTrans;
    int m_iFTPTransFail;
    int m_iFTPRename;
    int m_iFTPRenameFail;

    QTextCodec * codec;
    QTextEncoder *encoderw;

};

#endif // FTPCLIENT_H
