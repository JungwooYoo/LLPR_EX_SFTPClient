#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "config.h"
#include "configdlg.h"
#include "centerdlg.h"
#include "syslogger.h"
#include "mainthread.h"
#include "ftpclient.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void init();
    void initaction();
    void init_config();
    void applyconfig2common();
    void init_mainthr();
    bool loglinecheck();
    void checkcenterstatus();

private slots:
    void centerdlgview();
    void configurationdlgview();

    void createcenter(int index);
    void connectcenter(int index);
    void logappend(QString logstr);
    void restart();

    void onTimer();
    void closeEvent(QCloseEvent *);

private:
#define Program_Version  "LLPR_EX_FTPClient v1.0.7 (date: 2020/09/11)"
    Ui::MainWindow *ui;

    configdlg *pconfigdlg;
    CenterDlg *pcenterdlg;

    QTimer *qTimer;
    int m_mseccount;
    //class
    config *pcfg;
    mainthread *pmainthr;
    Syslogger *plog;

    int m_maxlogline;
};

#endif // MAINWINDOW_H
