#include "useruploads.h"
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>

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
void UserUploads::uploads(QString file_name, QString base64_date, QString user_id)
{
// 获取时间，用户，路径一起保存在数据库
// 文件路径：date/用户名/年/月
    QByteArray fileData = QByteArray::fromBase64(base64_date.toUtf8());

    // 保存文件
    QString exePath = QCoreApplication::applicationDirPath();   // 获取可执行文件地址
    QDateTime currentDateTime = QDateTime::currentDateTime();   // 获取当前的系统时间
    int currentYear = currentDateTime.date().year();    // 提取年份
    int currentMonth = currentDateTime.date().month();  // 提取月份

//    QString file_path = exePath + "/date/user_uploads/用户名/" + QString::number(currentYear) + "/" + QString::number(currentMonth) + "/"+ file_name; // 文件保存的地址
    QString file_path = "D:/Project";

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "文件保存失败:" << file_path;
            return;
    }
    qDebug("文件保存成功");
    file.write(fileData);
}

