#include "ftpclient.h"
#include <QDir>
#include <QFile>
#include <QApplication>

#include "commonvalues.h"

FtpClient::FtpClient(int id, Syslogger *plogger,QThread *parent) : QThread(parent)
{
//    QString path = QApplication::applicationDirPath();
//    FTP_SEND_PATH = path + "/FTP_Trans";
    m_RetryInterval = 5; //sec
    m_pftp = new QFtp();
    m_iFTPTrans = 0;
    m_iFTPRenameFail = 0;
    m_iFTPTransFail = 0;

    centerid = id;

    connect(m_pftp, SIGNAL(commandFinished(int,bool)),
            this, SLOT(ftpCommandFinished(int,bool)));

    log = plogger;

    codec = QTextCodec::codecForName("eucKR");
    encoderw = codec->makeEncoder( QTextCodec::IgnoreHeader);
}

FtpClient::~FtpClient()
{
    //log->deleteLater();
}

bool FtpClient::start(CenterInfo configinfo)
{
    config = configinfo;
    brun = true;
    m_iFTPTransFail = 0;
    QThread::start();
    return brun;
}

void FtpClient::run()
{
    ScanSendDataFiles();

    int lastCount = 0;
    bool isRetry = false;
    QDateTime lastTime = QDateTime::currentDateTime().addDays(-1);

    while(brun)
    {
        if( config.protocol_type == CenterInfo::Remote)
        {
            msleep(500);
            continue;
        }
        else if(sendFileList.Count() == 0)
        {
            ScanSendDataFiles();
            msleep(500);

            if(sendFileList.Count() == 0 )
                continue;
        }

        //check connection  && connect
        QFtp::State cur_state = m_pftp->state();
        if(cur_state == QFtp::Unconnected || cur_state == QFtp::Closing )
            connect2FTP();

        SendFileInfo sendFile = sendFileList.GetFirstFile();
        if(sendFile.filename.isNull() || sendFile.filename.isEmpty())
        {
            msleep(50);
            continue;
        }

        if(isRetry)
        {
            if(lastTime.secsTo(QDateTime::currentDateTime()) < m_RetryInterval )
            {
                msleep(10);
                continue;
            }
        }

        //FTP Send
        lastTime = QDateTime::currentDateTime();
        QFile file(sendFile.filepath);
        if(!file.open(QIODevice::ReadOnly)) continue;

        QString fname = sendFile.filename.mid(0,1); //H,X
        QString rname = sendFile.filename.mid(1); //~~~.jpg , ~~~.txt

        if(sendFile.filename.mid(0,1).compare("M") != 0 )
            m_pftp->put(&file,QString::fromLatin1(codec->fromUnicode(rname)),QFtp::Binary);
        else
            m_pftp->put(&file,QString::fromLatin1(codec->fromUnicode(sendFile.filename)),QFtp::Binary);
        m_iFTPTrans = 0x00;
        while(m_iFTPTrans == 0x00 && brun)
        {
            //2초동안 전송을 못 하면 실패
            if(lastTime.secsTo(QDateTime::currentDateTime()) > 5 )
                break;
            msleep(100);
        }
        file.close();

        if( m_iFTPTrans > 0)
        {
            m_iFTPTransFail = 0;

            QString logstr = QString("FTP파일 전송성공 : %1").arg(sendFile.filepath);
            log->write(logstr,LOG_NOTICE); qDebug() << logstr;            
            emit logappend(logstr);

            if(sendFile.filename.mid(0,1).compare("M") != 0)
            {               
                m_pftp->rename(QString::fromLatin1(codec->fromUnicode(rname)),QString::fromLatin1(codec->fromUnicode(sendFile.filename)));
                m_iFTPRename = 0x00;
                while(m_iFTPRename == 0x00 && brun)
                {
                    //2초동안 전송을 못 하면 실패
                    if(lastTime.secsTo(QDateTime::currentDateTime()) > 2 )
                    {
                        m_iFTPRename = -1;
                        break;
                    }
                    msleep(10);
                }
                if(m_iFTPRename > 0 )
                {
                    QString logstr = QString("FTP파일 이름변경 성공 : %1 -> %2").arg(rname).arg(sendFile.filename);
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);
                }
                else
                {
                    m_iFTPTrans = -1;
                    m_iFTPRenameFail++;
                    QString logstr = QString("FTP파일 이름변경 실패 : %1 -> %2").arg(rname).arg(sendFile.filename);
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);
                }
            }

            if(m_iFTPTrans > 0 || m_iFTPRenameFail > 3)
            {
                sendFileList.RemoveFirstFile(sendFile);
                m_iFTPRenameFail = 0;
                isRetry = false;
            }
            else
            {
                m_pftp->close();
                isRetry = true;
            }
        }
        else
        {
            m_iFTPTransFail++;
            QString logstr = QString("FTP파일 전송실패(fcnt:%1) : %2").arg(m_iFTPTransFail).arg(sendFile.filepath);
            log->write(logstr,LOG_NOTICE); qDebug() << logstr;
            emit logappend(logstr);

            //m_pftp->abort();
            m_pftp->close();
            isRetry = true;
        }

        if(lastCount != sendFileList.Count())
        {
            lastCount = sendFileList.Count();
            QString logstr = QString("FTP전송 파일 리스트 : %1").arg(lastCount);
            log->write(logstr,LOG_NOTICE); qDebug() << logstr;
            emit logappend(logstr);
        }
        msleep(100);

    }
}

void FtpClient::stop()
{
    brun = false;
}

bool FtpClient::connect2FTP()
{
    QString ipaddr = config.ip.trimmed();
    int port = config.ftpport;
    QString useName = config.userID;
    QString password = config.password;
    QString path = config.ftpPath;

    m_pftp->connectToHost(ipaddr,port);

    if (!useName.isEmpty())
        m_pftp->login(useName, password);
    else
        m_pftp->login();

    if (!path.isNull() && !path.isEmpty() )
        m_pftp->cd(path);
    //m_pftp->state();
}

//void FtpClient::CleanSendFilepath(QList<commonvalues::CenterInfo> list)
//{
//    try
//    {
//        QDir dir(FTP_SEND_PATH);
//        QString paths = dir.entryList(QDir::AllDirs);

//        foreach (QString path , paths)
//        {
//            bool isFind = false;
//            QString ftpName = path;
//            foreach (CenterInfo center ,list)
//            {
//                if (center.centername == ftpName)
//                {
//                    isFind = true;
//                    break;
//                }
//            }

//            if (!isFind) dir.remove(path);
//        }
//    }
//    catch ( ... )
//    {
//        qDebug() << QString("CleanSendFilepath");
//    }

//}

void FtpClient::ScanSendDataFiles()
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
    sendFileList.ClearAll();

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
        SendFileInfo info;
        QString fpath = filepath.absoluteFilePath();
        if( info.ParseFilepath(fpath))
        {
            if( sendFileList.AddFile(info))
            {
                logstr = QString("FTP전송 데이터 추가 : %1").arg(fpath);
                log->write(logstr,LOG_NOTICE);  qDebug() <<  logstr;

            }
            else DeleteFile(fpath);
        }
        else
            DeleteFile(fpath);

        if( sendFileList.Count() > MAXFTPCOUNT )
        {
            break;
        }
    }

    if( sendFileList.Count() > 0)
    {
        logstr = QString("FTP전송 파일 리스트 : %1").arg(sendFileList.Count());
        log->write(logstr,LOG_NOTICE);  qDebug() <<  logstr;
    }

}

void FtpClient::DeleteFile(QString filepath)
{
    try
    {
        QFile file(filepath);
        file.remove();
    }
    catch( ... )
    {
        qDebug() << QString("DeleteFile exception");
    }
}

bool FtpClient::SendVehicleInfo(QString fename, QByteArray ftpData)
{
    bool result = true;

    SendFileInfo sendFile;
                //sendFile.passDateTime = dateTime;

    if(sendFile.SaveFile(QString("%1/%2").arg(commonvalues::FTPSavePath).arg(config.centername),fename,ftpData))
    {
        if( !sendFileList.AddFile(sendFile))
        {
            result = false;
            QString logstr = QString("FTP전송 파일추가 실패 : %1, %2").arg(sendFileList.Count()).arg(sendFile.filepath);
            log->write(logstr, LOG_NOTICE); qDebug() << logstr;
            emit logappend(logstr);
            DeleteFile(sendFile.filename);
        }
        else
        {
            QString logstr = QString("FTP전송 파일추가 : %1, %2").arg(sendFileList.Count()).arg(sendFile.filepath);
            log->write(logstr, LOG_NOTICE);  qDebug() << logstr;
            emit logappend(logstr);
        }
    }
    else
    {
        QString logstr = QString("FTP전송 파일 저장실패 : %1").arg(sendFile.filepath);
        log->write(logstr, LOG_NOTICE); qDebug() << logstr;
    }

    return result;
}

void FtpClient::ftpCommandFinished(int id, bool error)
{
    if (m_pftp->currentCommand() == QFtp::Put )
    {
        if(error)
        {
            m_iFTPTrans = -1; //fail
            qDebug() << m_pftp->errorString();
        }
        else
            m_iFTPTrans = 1; //success
    }
    else if(m_pftp->currentCommand() == QFtp::Rename )
    {
        if(error)
        {
            m_iFTPRename = -1; //fail
            qDebug() << m_pftp->errorString();
        }
        else
            m_iFTPRename = 1; //success
    }
    else if(m_pftp->currentCommand() == QFtp::Close )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
        }

    }
    else if(m_pftp->currentCommand() == QFtp::ConnectToHost )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
        }

    }
    else if(m_pftp->currentCommand() == QFtp::Login )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
        }

    }
}

///////////////////// SendFileInfo  ///////////////////////////////////////

bool FtpClient::SendFileInfo::SaveFile(QString path, QString fname, QByteArray filedata)
{
    try
    {
        filename = fname;
        filepath = QString("%1/%2").arg(path).arg(fname);

        QDir dir(path);
        if (!dir.exists())
            dir.mkpath(path);

        QFile file(filepath);

        if (file.exists()) return false;

        if(file.open(QIODevice::WriteOnly))
        {
            QDataStream out(&file);
            int flen = filedata.length();
            int wflen = 0;
            while( flen > wflen )
            {
                int wlen = out.writeRawData(filedata.data(),flen);
                if(wlen <= 0)
                    break;
                wflen += wlen;
            }
            //            out << filedata;
             file.close();

            if(flen > wflen)
                return false;
        }
        else
            return false;
    }
    catch ( ... )
    {
        qDebug() << QString("SaveFile Expection : %1/%2").arg(path).arg(fname);
        return false;
    }
    return true;
}

bool FtpClient::SendFileInfo::ParseFilepath(QString _filepath)
{
    try
    {
        filename = _filepath.mid(_filepath.lastIndexOf('/') + 1);
//        QString filename2 = filename.mid(0, filename.lastIndexOf(".jpg"));

//        int index = filename2.indexOf("H");
//        if (index != 0)
//        {
//            index = filename2.indexOf("MS");
//            if (index != 0)
//                    return false;
//        }
        filepath = _filepath;


    }
    catch (...)
    {
        qDebug() << QString("ParseFilepath Expection : %1").arg(filepath);
        return false;
    }
    return true;
}


bool FtpClient::SendFileInfoList::AddFile(FtpClient::SendFileInfo data)
{
    bool brtn = true;
    mutex.lock();
    try
    {
        if( fileList.count() >= MAX_FILE)
        {
            SendFileInfo file = GetFirstFile();
            if(!file.filename.isNull() && !file.filename.isEmpty()) RemoveFirstFile(file);
        }
        fileList.append(data);
    }
    catch( ... )
    {
        qDebug() << QString("AddFile Expection");
        brtn = false;
    }
    mutex.unlock();
    return brtn;

}

FtpClient::SendFileInfo FtpClient::SendFileInfoList::GetFirstFile()
{
    SendFileInfo fileinfo;
    mutex.lock();
    if (fileList.count() > 0)
        fileinfo = fileList.first();
//    else
//        fileinfo = NULL;
    mutex.unlock();
    return fileinfo;
}

void FtpClient::SendFileInfoList::RemoveFirstFile(FtpClient::SendFileInfo data)
{
    mutex.lock();
    try
    {
        QFile file(data.filepath);
        file.remove();
        fileList.removeFirst();//   .removeOne(data);
        for(int index=0; index < fileList.count(); index++)
        {
              if( data.filepath.compare(fileList[index].filepath) == 0 )
              {
                fileList.removeAt(index);
                break;
              }

        }
    }
    catch( ... )
    {
        qDebug() << QString("RemoveFile Expection");
    }
    mutex.unlock();
}

void FtpClient::SendFileInfoList::ClearAll()
{
    mutex.lock();
    fileList.clear();
    mutex.unlock();
}

int FtpClient::SendFileInfoList::Count()
{
    return fileList.count();
}


