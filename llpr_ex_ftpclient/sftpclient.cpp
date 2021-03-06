#include "sftpclient.h"
#include <QDir>
#include <QFile>
#include <QApplication>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "commonvalues.h"

SftpClient::SftpClient(int id, Syslogger *plogger,QThread *parent) : QThread(parent)
{
    int index = commonvalues::center_list.count();
    centerid = id;
    log = plogger;

    codec = QTextCodec::codecForName("eucKR");
    encoderw = codec->makeEncoder(QTextCodec::IgnoreHeader);

    m_iFTPTrans = 0;
    m_iFTPTransFail = 0;
    m_iFTPRename = 0;
    m_iFTPRenameFail = 0;
}

SftpClient::~SftpClient()
{
    closesession();
    closeSFTP();
}

bool SftpClient::start(CenterInfo configinfo)
{
    config = configinfo;
    const char *ipcvt;
    QByteArray cvt;

    ipcvt = config.ip.trimmed().toLocal8Bit();
    hostaddr = inet_addr(ipcvt);

    brun = true;
    QThread::start();

    return brun;
}

void SftpClient::run()
{
    int lastCount = 0;
    bool isRetry = false;
    QDateTime lastTime = QDateTime::currentDateTime().addDays(-1);

    username = config.userID.toStdString().c_str();
    password = config.password.toStdString().c_str();
    sftppath = config.ftpPath.toStdString().c_str();

    sshinit();
    connectSocket();
    initSession();

    while(brun)
    {
        if(isRetry)
        {
            closesession();

            if(!sshinit())
            {
                msleep(1000);
                continue;
            }
            else if(!connectSocket())
            {
                msleep(1000);
                continue;
            }
            else if(!initSession())
            {
                msleep(1000);
                continue;
            }
            else
            {
                isRetry = false;
            }
        }

        if(sftp_sendfileInfolist.Count() == 0)
        {
            ScanSendDataFiles();
            msleep(500);

            if(sftp_sendfileInfolist.Count() == 0)
                continue;
        }
        else if(sftp_sendfileInfolist.Count() > 0)
        {
            sftp_sendfileInfo = sftp_sendfileInfolist.GetFirstFile();
            if(sftp_sendfileInfo.filename.isNull() || sftp_sendfileInfo.filename.isEmpty())
            {
                msleep(50);
                continue;
            }

            if(isRetry)
            {
                if(lastTime.secsTo(QDateTime::currentDateTime()) < 2000)
                {
                    msleep(10);
                    continue;
                }
            }

            QByteArray cvt;
            QString fpath = QString("%1/%2/").arg(commonvalues::FTPSavePath).arg(config.centername);

            lastTime = QDateTime::currentDateTime();
            QFile file(sftp_sendfileInfo.filepath);
            if(!file.open(QIODevice::ReadOnly))
            {
                continue;
            }

            QChar char1 = sftp_sendfileInfo.filename.at(0);
            QChar char2 = sftp_sendfileInfo.filename.at(1);
            QChar char3 = sftp_sendfileInfo.filename.at(2);

            if(char3.isLetter() == true)
            {
                noCharfName = sftp_sendfileInfo.filename.mid(3);
            }
            else if(char2.isLetter() == true)
            {
                noCharfName = sftp_sendfileInfo.filename.mid(2);
            }
            else if (char1.isLetter() == true)
            {
                noCharfName = sftp_sendfileInfo.filename.mid(1);
            }
            else
            {
                noCharfName = sftp_sendfileInfo.filename;
            }

            fpath.append(sftp_sendfileInfo.filename);
            cvt = fpath.toLocal8Bit();
            localfile = cvt.data();

            local = fopen(localfile, "rb");

            if(!local)
            {
                logstr = QString("SFTP : ?????? ?????? ??????..");
                continue;
            }
            else
            {
                logstr = QString("SFTP : ?????? ?????? ??????!!");
            }

            log->write(logstr,LOG_NOTICE);
            qDebug() << logstr;
            emit logappend(logstr);


            if(initSFTP())
            {
                if(sendData())
                {
                    if(sftp_session != NULL && sftp_handle != NULL)
                    {
                        LIBSSH2_SFTP_ATTRIBUTES attrs;
                        if(libssh2_sftp_fstat_ex(sftp_handle, &attrs, 0) < 0)
                        {
                            logstr = QString("SFTP : libssh2_sftp_fstat fail..");
                            log->write(logstr, LOG_NOTICE);
                            qDebug() << logstr;
                            emit logappend(logstr);
                        }
                        else
                        {
                            libssh2_sftp_seek64(sftp_handle, attrs.filesize);
                        }

                        if(attrs.filesize > 0)
                        {
                            cvt = noCharfName.toLocal8Bit();
                            const char* srcname = cvt.data();

                            cvt = sftp_sendfileInfo.filename.toLocal8Bit();
                            const char* dstname = cvt.data();

                            m_iFTPRename = libssh2_sftp_rename(sftp_session, srcname, dstname);

                            if(m_iFTPRename == 0)
                            {
                                logstr = QString("SFTP ?????? ???????????? ??????!! : %1 -> %2").arg(noCharfName).arg(sftp_sendfileInfo.filename);
                                log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                                emit logappend(logstr);
                            }
                            else
                            {
                                logstr = QString("SFTP ?????? ???????????? ??????.. : %1 -> %2").arg(noCharfName).arg(sftp_sendfileInfo.filename);
                                log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                                emit logappend(logstr);

                                // ?????? ?????? ??????
                                cvt = noCharfName.toLocal8Bit();
                                const char* delfilename = cvt.data();

                                libssh2_sftp_unlink(sftp_session, delfilename);

                                logstr = QString("SFTP ?????? ?????? ?????? : %1").arg(noCharfName);
                                log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                                emit logappend(logstr);
                            }

                            file.close();
                        }
                    }

                    sftp_sendfileInfolist.RemoveFirstFile(sftp_sendfileInfo);

                    if(lastCount != sftp_sendfileInfolist.Count())
                    {
                        lastCount = sftp_sendfileInfolist.Count();
                        logstr = QString("SFTP ?????? ?????? ????????? : %1").arg(lastCount);
                        log->write(logstr, LOG_NOTICE);
                        qDebug() << logstr;
                        emit logappend(logstr);

                        msleep(500);
                    }

                    isRetry = false;
                }
                else
                {
                    isRetry = true;
                }
            }
            else
            {
                isRetry = true;
            }
        }
    }
}

void SftpClient::stop()
{
    brun = false;
}

bool SftpClient::sshinit()
{
    bool sshSuc = true;
    int sshinit = 0;

    sshinit = libssh2_init(0);
    if(sshinit != 0)
    {
        logstr = QString("SFTP : libssh2 ????????? ??????..");
        sshSuc = false;
    }
    else
    {
        logstr = QString("SFTP : libssh2 ????????? ??????!!");
        sshSuc = true;
    }

    log->write(logstr,LOG_NOTICE);
    qDebug() << logstr;
    emit logappend(logstr);

    return sshSuc;
}

bool SftpClient::connectSocket()
{
    bool connectSock = true;
    struct sockaddr_in sin;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = hostaddr;
    if(::connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0)
    {
        logstr = QString("SFTP : Socket ?????? ??????..");
        connectSock = false;
    }
    else
    {
        logstr = QString("SFTP : Socket ?????? ??????!!");
        connectSock = true;
    }

    log->write(logstr,LOG_NOTICE);
    qDebug() << logstr;
    emit logappend(logstr);

    return connectSock;

}

bool SftpClient::initSession()
{
    bool sessionSuc = true;
    int sessioninit = 0;
    int auth_pw = 1;

    session = libssh2_session_init();
    if(!session)
    {
        sessionSuc = false;
    }

    libssh2_session_set_blocking(session, 1);
    sessioninit = libssh2_session_handshake(session, sock);
    if(sessioninit)
    {
        logstr = QString("SFTP : SSH Session ?????? ??????..");
        sessionSuc = false;
    }
    else
    {
        logstr = QString("SFTP : SSH Session ?????? ??????!!");
        sessionSuc = true;
    }

    log->write(logstr,LOG_NOTICE);
    qDebug() << logstr;
    emit logappend(logstr);

    libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

    if(auth_pw)
    {
        if(libssh2_userauth_password(session, username, password))
        {
            logstr = QString("SFTP : ID/PW ?????? ??????..");
            closesession();

            sessionSuc = false;
        }
        else
        {
            logstr = QString("SFTP : ID/PW ?????? ??????!!");
            sessionSuc = true;
        }

        log->write(logstr,LOG_NOTICE);
        qDebug() << logstr;
        emit logappend(logstr);
    }

    return sessionSuc;
}

bool SftpClient::initSFTP()
{
    bool sftpSuc = false;

    sftp_session = libssh2_sftp_init(session);

    logstr = QString("SFTP : SFTP ?????????!!");
    log->write(logstr,LOG_NOTICE);
    qDebug() << logstr;
    emit logappend(logstr);

    QString Remotefilepath = QString("%1%2").arg(config.ftpPath).arg(noCharfName);
    QByteArray cvt;
    cvt = Remotefilepath.toLocal8Bit();
    const char* _sftppath = cvt.data();

    sftp_handle = libssh2_sftp_open(sftp_session, _sftppath,
                                    LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                    LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
                                    LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);
    if(!sftp_handle)
    {
        logstr = QString("SFTP : SFTP ?????? ??????!!");
        closesession();

        sftpSuc = false;
    }
    else
    {
        logstr = QString("SFTP : SFTP ?????? ??????!!");

        sftpSuc = true;
    }

    log->write(logstr,LOG_NOTICE);
    qDebug() << logstr;
    emit logappend(logstr);

    return sftpSuc;
}

bool SftpClient::sendData()
{
    bool sendSuc = false;
    int sendcount = 0;
    char mem[1024*100];
    size_t nread;
    char *ptr;

    // ?????? ??????
    do
    {
        nread = fread(mem, 1, sizeof(mem), local);
        if(nread <= 0)
        {
            // end of file
            break;
        }
        ptr = mem;

        do
        {
            // write data in a loop until we block
            sendcount = libssh2_sftp_write(sftp_handle, ptr, nread);
            if(sendcount < 0)
            {
                sendSuc = false;
                break;
            }
            else
                sendSuc = true;

            ptr += sendcount;
            nread -= sendcount;
        }
        while(nread);
    }
    while(sendcount > 0);

    return sendSuc;
}

void SftpClient::closeSFTP()
{
    libssh2_sftp_close(sftp_handle);
    libssh2_sftp_shutdown(sftp_session);
}

void SftpClient::closesession()
{
    if(session != nullptr)
    {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
    }

    session = nullptr;
}

void SftpClient::ScanSendDataFiles()
{
    QString logstr;
    QString path = QString("%1/%2").arg(commonvalues::FTPSavePath).arg(config.centername);
    QDir dir(path);
    try
    {
        if( !dir.exists())
        {
            dir.mkdir(path);
        }
    }
    catch( ... )
    {
        qDebug() << QString("ScanSendDataFiles-Directory Check  Exception");
        return;
    }

    QFileInfoList filepaths;
    sftp_sendfileInfolist.ClearAll();
/*
    try
    {
        QStringList filters;
        filters << "*.jpg" << "*.jpeg" << "*.txt";
        filepaths = dir.entryInfoList(filters);
    }
    catch( ... )
    {
        qDebug() << QString("ScanSendDataFiles-Search Entryile Exception");
        return;
    }

    foreach (QFileInfo filepath, filepaths)
    {
        QString fpath = filepath.absoluteFilePath();
        if( sftp_sendfileInfo.ParseFilepath(fpath))
        {
            if( sftp_sendfileInfolist.AddFile(sftp_sendfileInfo))
            {
                logstr = QString("SFTP ?????? ????????? ?????? : %1").arg(fpath);
                log->write(logstr,LOG_NOTICE);
                qDebug() <<  logstr;

            }
        }

        if( sftp_sendfileInfolist.Count() > 30 )
        {
            break;
        }
    }
*/

    QDirIterator dirIter(dir.absolutePath(), QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int filecount = 0;

    try
    {
        while(dirIter.hasNext())
        {
            dirIter.next();
            if(QFileInfo(dirIter.filePath()).isFile())
            {
                if(QFileInfo(dirIter.filePath()).suffix().toLower() == "jpg"
                        || QFileInfo(dirIter.filePath()).suffix().toLower() == "jpeg"
                        || QFileInfo(dirIter.filePath()).suffix().toLower() == "txt")
                {
                    filecount++;
                    QString fpath = dirIter.fileInfo().absoluteFilePath();
                    if(sftp_sendfileInfo.ParseFilepath(fpath))
                    {
                        sftp_sendfileInfolist.AddFile(sftp_sendfileInfo);
                    }
                    if(filecount > 10)
                    {
                        break;
                    }
                }
            }
        }
    }
    catch( ... )
    {
        logstr = QString("ScanSendDataFiles-Search Entryile Exception : %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr, LOG_NOTICE);
        return;
    }

    if( sftp_sendfileInfolist.Count() > 0)
    {
        logstr = QString("SFTP ?????? ?????? ????????? : %1").arg(sftp_sendfileInfolist.Count());
        log->write(logstr,LOG_NOTICE);
        qDebug() <<  logstr;
    }
}
