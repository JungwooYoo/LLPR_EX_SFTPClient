#include "commonvalues.h"

commonvalues::commonvalues(QObject *parent) : QObject(parent)
{

}

int commonvalues::center_count = 0;
QList<CenterInfo> commonvalues::center_list;
//QList<FtpClient *> commonvalues::clientlist;
QList<SftpClient *> commonvalues::clientlist;
int commonvalues::ftpretry = 1;
QString commonvalues::FTPSavePath = "/home/jinwoo/workQT/build-llpr_ex_ftpClient-Desktop_Qt_5_10_0_GCC_64bit-Debug/FTP_Trans";

int commonvalues::loglevel = 0;
