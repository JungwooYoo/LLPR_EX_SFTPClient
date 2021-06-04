#include "commonvalues.h"

commonvalues::commonvalues(QObject *parent) : QObject(parent)
{

}

int commonvalues::center_count = 0;
QList<CenterInfo> commonvalues::center_list;
QList<FtpClient *> commonvalues::clientlist;
int commonvalues::ftpretry = 1;
QString commonvalues::FTPSavePath = "/media/data/FTP_Trans";

int commonvalues::loglevel = 0;
