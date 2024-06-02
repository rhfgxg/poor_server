#ifndef LOGS_H
#define LOGS_H

#include <QObject>
#include <QCoreApplication> // 获取可执行文件的地址（debug文件夹），从而获取指令记录文件的地址
#include <QDateTime>    // 获取写入指令运行的时间
#include <QFile>    // 使用指令记录文件

class Logs
{
public:
    Logs();
    void addLogs(QString log_txt, QString file_name);   // 添加日志，参数：日志内容，日志文件名
};

#endif // LOGS_H
