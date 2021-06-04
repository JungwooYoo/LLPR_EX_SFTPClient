#include "mainthread.h"
#include "commonvalues.h"

mainthread::mainthread(Syslogger *plogger,QThread *parent) : QThread(parent)
{
    log = plogger;
}

mainthread::~mainthread()
{

}

bool mainthread::start()
{
    QThread::start();
    brun = true;

    return brun;
}

void mainthread::run()
{
    QString logstr;
    int checkcount = 0;

    logstr = QString("mainthread start");
    log->write(logstr,LOG_ERR); qDebug() << logstr;

    while(brun)
    {
        try
        {
            if( checkcount%200 == 0)
            {
                center_check();
            }
            checkcount++;



        }
        catch ( ... )
        {
            logstr = QString("--------- Error mainthread -----------");
            log->write(logstr,LOG_ERR);  qDebug() << logstr;
        }

        msleep(10);

    }
    logstr = QString("mainthread stop");
    log->write(logstr,LOG_ERR); qDebug() << logstr;
}

void mainthread::stop()
{
    brun = false;
}

void mainthread::center_check()
{
    int count = commonvalues::center_list.size();
    int client_count = commonvalues::clientlist.size();

    //check center count or create center class
    if( client_count != count)
    {
        if(client_count > count)
        {
            int index;
            for(int i=client_count; i > count; i-- )
            {
                index = i-1;
                commonvalues::clientlist.value(index)->stop();

                quint64 closecount = 300;
                while(commonvalues::clientlist.value(index)->isRunning())
                {
                    msleep(10);
                    closecount--;
                    if(closecount < 1)
                    {
                        break;
                    }
                }
                QString logstr = QString("FTP Remove : %1").arg(i);
                qDebug() << logstr;
                log->write(logstr,LOG_NOTICE);

                commonvalues::clientlist.removeAt(index);
            }
        }
        else // client_count < count
        {
            for(int i=client_count; i < count; i++)
            {
                emit createcenter(i);
                msleep(100);
            }
        }
    }

    client_count = commonvalues::clientlist.size();
    for(int i=0; i < client_count; i++)
    {
        //check ip, port, localname

        if( commonvalues::center_list.value(i).ip.compare(commonvalues::clientlist.value(i)->config.ip) != 0
                || commonvalues::center_list[i].ftpport != (int)commonvalues::clientlist.value(i)->config.ftpport
    || commonvalues::center_list[i].centername.compare(commonvalues::clientlist.value(i)->config.centername) != 0)
        {
            commonvalues::clientlist.value(i)->stop();

            quint64 closecount = 300;
            while(commonvalues::clientlist.value(i)->isRunning())
            {
                msleep(10);
                closecount--;
                if(closecount < 1)
                {
                    break;
                }
            }
//            QString logstr = QString("FTP STOP : %1").arg(i);
//            qDebug() << logstr;
//            log->write(logstr,LOG_NOTICE);
            emit connectcenter(i);
            msleep(100);
        }
    }

    //update ftp send status
    client_count = commonvalues::clientlist.size();
    for(int i=0; i < client_count; i++)
    {
        if(commonvalues::clientlist.value(i)->m_iFTPTransFail > 0
                || commonvalues::center_list.value(i).protocol_type == CenterInfo::Remote  )
        {
            commonvalues::center_list[i].status = false;
        }
        else
            commonvalues::center_list[i].status = true;
    }

    if( commonvalues::ftpretry > 0)
    {
        if(!ftpcheck())
        {
            QString logstr = QString("Center 재시작");
            log->write(logstr,LOG_NOTICE);
            emit logappend(logstr);

            msleep(100);
            emit restart();

//            int index;
//            for(int i=client_count; i > count; i-- )
//            {
//                index = i-1;
//                commonvalues::clientlist.value(index)->stop();

//                quint64 closecount = 300;
//                while(commonvalues::clientlist.value(index)->isRunning())
//                {
//                    msleep(10);
//                    closecount--;
//                    if(closecount < 1)
//                    {
//                        break;
//                    }
//                }
//                QString logstr = QString("FTP Remove : %1").arg(i);
//                qDebug() << logstr;
//                log->write(logstr,LOG_NOTICE);

//                commonvalues::clientlist.removeAt(index);
//            }
//            commonvalues::clientlist.clear();
        }
    }
}

bool mainthread::ftpcheck()
{
    bool brtn = true;
    int client_count = commonvalues::clientlist.size();
    for(int i=0; i < client_count; i++)
    {
        int iFTPTransFail = commonvalues::clientlist.value(i)->m_iFTPTransFail;
        if(iFTPTransFail > 20 )  //약30분
        {
            QString logstr = QString("FTP 실패초과(id:%1) %2").arg(i).arg(iFTPTransFail);
            log->write(logstr,LOG_NOTICE);
            emit logappend(logstr);
            brtn = false;
            break;
        }

    }

    return brtn;
}
