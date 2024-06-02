#include "logs.h"

Logs::Logs()
{

}

void Logs::addLogs(QString log_txt, QString file_name)     // 添加日志，参数：日志内容，日志文件名
{
    QString exePath = QCoreApplication::applicationDirPath();   // 获取可执行文件地址
    QString bash_history_path = exePath + "/data/terminal/home/root/" + file_name; // 指令记录的地址: 执行文件地址+日志文件的相对地址+日志文件名

    QString user_name_str = "user_name";
    // 获取当前时间，转换成时间戳
    QDateTime date_time = QDateTime::currentDateTime();   // 获取当前日期和时间的 QDateTime 对象
    QString date_time_str = date_time.toString("yyyy-MM-dd HH:mm:ss");  // 时间转换成字符串

    QString log_new = user_name_str + "-" + date_time_str + ": " + log_txt + "\n";  // 拼接成完整的记录


    // 使用QFile对象打开文件，QIODevice::ReadWrite表示读写模式，QIODevice::Text指定文件是文本文件，QIODevice::Append指定每次写入文件时，数据都将追加到文件的当前结尾，而不是覆盖现有内容
    QFile file(bash_history_path);
    if (file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append))
    {
        // 移动到文件末尾
        file.seek(file.size());

        // 使用QTextStream进行文本读写操作更为方便
        QTextStream out(&file);

        // 检查是否已经是文件的最后一行，避免在空文件或已有内容后直接添加换行符
        if (!file.atEnd()) {
            out << "\n"; // 如果不是空文件且不在最后一行，先添加一个换行符
        }

        // 写入新的字符串
        out << log_new;

        // 清理工作
        file.close();
    }
    else
    {
        qDebug() << "Failed to open file: " << bash_history_path;
    }
}
