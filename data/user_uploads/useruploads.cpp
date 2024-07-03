#include "useruploads.h"
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

UserUploads::UserUploads(QObject *parent) :
    QObject(parent)
{

}

// 初始化数据库
void UserUploads::setDatabase(const QSqlDatabase &db)
{
    database = db;
}

// 用户上传
void UserUploads::uploads(const QString &file_name, const QString &base64_date, const QString &user_id)
{
    qDebug() <<base64_date;
// 获取时间，用户，路径一起保存在数据库
// 文件路径：date/用户名/年/月
    // 解码 base64数据
    QByteArray fileData = QByteArray::fromBase64(base64_date.toUtf8());

    qDebug() << fileData;
// 构建文件保存路径
    QString dataDir = QCoreApplication::applicationDirPath() + "/data"; // 获取可执行文件路径拼接 data目录
    QString userUploadsDir = dataDir + "/user_uploads/" + user_id; // 用户上传文件目录
    // 时间
    QDateTime currentDateTime = QDateTime::currentDateTime(); // 获取当前时间
    QString yearMonthDir = QString::number(currentDateTime.date().year()) + "_" + QString::number(currentDateTime.date().month()); // 年/月目录

    QString savePath = userUploadsDir + "/" + yearMonthDir + "/" + file_name; // 最终保存路径

    // 新建文件保存路径
    QDir dir(userUploadsDir + "/" + yearMonthDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "文件上传：文件保存路径创建失败:" << dir.path();
            return;
        }
    }

    // 保存文件
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "文件保存失败:" << savePath;
        return; // 或者抛出异常或者返回错误码，具体根据需求而定
    }

    qint64 bytesWritten = file.write(fileData); // 保存文件
    file.close();   // 关闭文件

    if (bytesWritten == -1) // 如果保存文件失败
    {
        qDebug() << "写入文件失败:" << savePath;
        // 可以根据需要进行错误处理
    }
    else
    {
        qDebug() << "文件保存成功:" << savePath << ", 字节写入:" << bytesWritten;
        // 可以在这里添加更多的业务逻辑，比如记录上传日志等

        // 返回成功的状态或者执行其他操作
    }
}

