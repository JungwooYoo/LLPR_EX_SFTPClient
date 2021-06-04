#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commonvalues.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle(Program_Version);

    m_maxlogline = 500;

    init();
}

MainWindow::~MainWindow()
{    
    delete qTimer;

    delete ui;
}

void MainWindow::init()
{
    pcfg = new config();
    init_config();    

    plog = new Syslogger(this,"mainwindow",true,commonvalues::loglevel);
    QString logstr = QString("Program Start(%1)").arg(Program_Version);
    plog->write(logstr,LOG_ERR); qDebug() << logstr;

    pmainthr = new mainthread(plog);

    pcenterdlg = new CenterDlg(pcfg);
    pconfigdlg = new configdlg(pcfg);

    initaction();

    init_mainthr();

    //QTimer init
    qTimer = new QTimer();
    connect(qTimer,SIGNAL(timeout()),this,SLOT(onTimer()));
    qTimer->start(1000);
    m_mseccount=0;
}

void MainWindow::initaction()
{
    connect(ui->actionCenter,SIGNAL(triggered()),this,SLOT(centerdlgview()));
    connect(ui->actionConfiguration,SIGNAL(triggered()),this,SLOT(configurationdlgview()));
}

void MainWindow::init_config()
{
    //check config file exists
    if( !pcfg->check())
    {
//        //create config file emptied
//        pcfg->create();

//        //load config file
//        pcfg->load();
//        //init config value
//        applycommon2config();

//        pcfg->save();
    }
    else
    {
        //load config file
        pcfg->load();
        //apply commonvalues
        applyconfig2common();

    }
}

void MainWindow::applyconfig2common()
{
    QString svalue;
    bool bvalue;
    uint uivalue;
    float fvalue;

    /*        Center  value            */
   //static QList<centerinfo> center_list;
//   if( pcfg->getuint("CENTER","Count",&uivalue)) { commonvalues::center_count = uivalue; }
//   for(int i=0; i < commonvalues::center_count; i++)
    {
        int i=0;
        QString title = "CENTER|LIST" + QString::number(i);
        while( ( svalue = pcfg->get(title,"IP") ) != NULL)
        {
            CenterInfo centerinfo;
            if( ( svalue = pcfg->get(title,"CenterName") ) != NULL ) { centerinfo.centername = svalue; }
            if( ( svalue = pcfg->get(title,"IP") ) != NULL ) { centerinfo.ip = svalue; }
            if( pcfg->getuint(title,"TCPPort",&uivalue)) { centerinfo.tcpport = uivalue; }
            if( pcfg->getuint(title,"AliveInterval",&uivalue)) { centerinfo.connectioninterval = uivalue; }
            if( pcfg->getuint(title,"ProtocolType",&uivalue)) { centerinfo.protocol_type = uivalue; }
            if( pcfg->getuint(title,"FTPPort",&uivalue)) { centerinfo.ftpport = uivalue; }
            if( ( svalue = pcfg->get(title,"FTPUserID") ) != NULL ) { centerinfo.userID = svalue; }
            if( ( svalue = pcfg->get(title,"FTPPassword") ) != NULL ) { centerinfo.password = svalue; }
            if( ( svalue = pcfg->get(title,"FTPPath") ) != NULL ) { centerinfo.ftpPath = svalue; }
            if( pcfg->getuint(title,"FileNameSelect",&uivalue)) { centerinfo.fileNameSelect = uivalue; }

            centerinfo.plblstatus = new QLabel();
            centerinfo.plblstatus->setText( QString("%1:%2")
                        .arg(centerinfo.ip).arg(centerinfo.ftpport));
            centerinfo.plblstatus->setStyleSheet("QLabel { background-color : red; }");

            commonvalues::center_list.append(centerinfo);

            i++;
            title = "CENTER|LIST" + QString::number(i);
        }
    }
    if( pcfg->getuint("CENTER","FTPRetry",&uivalue)) { commonvalues::ftpretry = uivalue; }
    if( ( svalue = pcfg->get("CENTER","FTPSavePath") ) != NULL){ commonvalues::FTPSavePath = svalue; }



  /*       Log Level value             */
   if( pcfg->getuint("LOG","Level",&uivalue)) { commonvalues::loglevel = uivalue; }

}

void MainWindow::init_mainthr()
{
    connect(pmainthr,SIGNAL(createcenter(int)),this,SLOT(createcenter(int)));
    connect(pmainthr,SIGNAL(connectcenter(int)),this,SLOT(connectcenter(int)));
    connect(pmainthr, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
    connect(pmainthr,SIGNAL(restart()),this,SLOT(restart()));
    pmainthr->start();
}

bool MainWindow::loglinecheck()
{
    bool brtn = false;
    int count = ui->teFTPlog->document()->lineCount();

    if ( count > m_maxlogline )
    {
        qDebug() << "teFTPlog clear";
        brtn = true;
    }
    return brtn;
}

void MainWindow::checkcenterstatus()
{
    int count = commonvalues::center_list.size();

    if( commonvalues::center_count != count )
    {
        if(commonvalues::center_count < count)
        {
            for(int i=commonvalues::center_count; i < count; i++)
            {
                ui->statusBar->addWidget(commonvalues::center_list.value(i).plblstatus);
            }

        }
        commonvalues::center_count = count;
    }

    count = commonvalues::center_list.size();
    for(int i = 0; i < count; i++)
    {
        commonvalues::center_list.value(i).plblstatus->setText( QString("%1:%2")
                                        .arg(commonvalues::center_list.at(i).ip).arg(commonvalues::center_list.value(i).ftpport));
        if(commonvalues::center_list.value(i).status )
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : green; }");

        }
        else
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : red; }");
        }
    }
}

void MainWindow::centerdlgview()
{
    pcenterdlg->show();
    pcenterdlg->raise();
}

void MainWindow::configurationdlgview()
{
    pconfigdlg->show();
    pconfigdlg->raise();
}

void MainWindow::createcenter(int index)
{
    /*
    FtpClient *pftpclient = new FtpClient(index,plog);
    connect(pftpclient, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));

    commonvalues::clientlist.append(pftpclient);
    QString logstr = QString("FTP Create : %1").arg(index);
    plog->write(logstr,LOG_NOTICE);
    logappend(logstr);
    qDebug() << logstr;
    */

    SftpClient *psftpclient = new SftpClient(index,plog);
    connect(psftpclient, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));

    commonvalues::clientlist.append(psftpclient);
    QString logstr = QString("SFTP Create : %1").arg(index);
    plog->write(logstr,LOG_NOTICE);
    logappend(logstr);
    qDebug() << logstr;
}

void MainWindow::connectcenter(int index)
{
    /*
    FtpClient *pftpclient = commonvalues::clientlist.value(index);

    if(!pftpclient->isRunning())
    {
        pftpclient->start(commonvalues::center_list.value(index));
        QString logstr = QString("FTP Connect : %1(ip:%2,port:%3,centerName:%4,ProtocolType:%5, FTP Retry:%6)").arg(index)
                .arg(pftpclient->config.ip).arg(pftpclient->config.ftpport).arg(pftpclient->config.centername)
                .arg(pftpclient->config.protocol_type).arg(commonvalues::ftpretry);
        plog->write(logstr,LOG_NOTICE);
        logappend(logstr);
        qDebug() << logstr;
    }
    */

    SftpClient *psftpclient = commonvalues::clientlist.value(index);

    if(!psftpclient->isRunning())
    {
        psftpclient->start(commonvalues::center_list.value(index));
        QString logstr = QString("FTP Connect : %1(ip:%2,port:%3,centerName:%4,ProtocolType:%5, FTP Retry:%6)").arg(index)
                .arg(psftpclient->config.ip).arg(psftpclient->config.ftpport).arg(psftpclient->config.centername)
                .arg(psftpclient->config.protocol_type).arg(commonvalues::ftpretry);
        plog->write(logstr,LOG_NOTICE);
        logappend(logstr);
        qDebug() << logstr;
    }
}

void MainWindow::logappend(QString logstr)
{
    bool brtn = loglinecheck();

    QString qdt = QDateTime::currentDateTime().toString("[HH:mm:ss:zzz]") + logstr;

    if(brtn)
    {
        ui->teFTPlog->setPlainText(qdt);
    }
    else ui->teFTPlog->append(qdt);

}

void MainWindow::restart()
{
    this->close();
}

void MainWindow::onTimer()
{
    if( m_mseccount%5 == 0)
    {
        //status bar -> display center connection status
        checkcenterstatus();
    }
    m_mseccount++;
}

void MainWindow::closeEvent(QCloseEvent *)
{
    if(pconfigdlg != NULL)pconfigdlg->close();
    if(pcenterdlg != NULL)pcenterdlg->close();

    qTimer->stop();
}
