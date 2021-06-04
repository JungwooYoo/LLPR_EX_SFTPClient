#ifndef MAINTHREAD_H
#define MAINTHREAD_H

#include <QObject>
#include <QThread>
#include "syslogger.h"

class mainthread : public QThread
{
    Q_OBJECT
public:
    explicit mainthread(Syslogger *plogger,QThread *parent = nullptr);
    ~mainthread();
    bool start();
    void run();
    void stop();
    void center_check();
    bool ftpcheck();

signals:
    void createcenter(int index);
    void connectcenter(int index);
    void logappend(QString logstr);
    void restart();
public slots:

public:
    bool brun;
    Syslogger *log;
};

#endif // MAINTHREAD_H
