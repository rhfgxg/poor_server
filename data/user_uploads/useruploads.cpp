#include "useruploads.h"
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QUuid>

UserUploads::UserUploads(QObject *parent) :
    QObject(parent)
{

}

// 初始化数据库
void UserUploads::setDatabase(const QSqlDatabase &db)
{
    database = db;
}

// 处理文件切片上传
void UserUploads::uploadChunk(const QString &file_id, const QString &base64_data, qint64 offset, qint64 total_size, const QString &user_id)
{
// 获取时间，用户，路径一起保存在数据库
// 文件路径：date/用户名/年/月
    // 解码 base64数据
    QByteArray fileData = QByteArray::fromBase64(base64_data.toUtf8());

// 构建文件保存路径
    QString dataDir = QCoreApplication::applicationDirPath() + "/data";
    QString userUploadsDir = dataDir + "/user_uploads/" + user_id;
    // 获取当前时间，并作为路径的一部分，2024_07
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString yearMonthDir = QString::number(currentDateTime.date().year()) + "_" + QString::number(currentDateTime.date().month());
    QString savePath = userUploadsDir + "/" + yearMonthDir + "/" + file_id;   // 最终保存路径

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

    // 打开文件，如果文件不存在就新建文件
    QFile file(savePath);
    if (!file.open(QIODevice::Append))
    {
        qDebug() << "文件保存失败:" << savePath;
        return;
    }

    // 定位偏移量
    if (!file.seek(offset))
    {
        qDebug() << "文件定位失败:" << savePath;
        return;
    }

    qint64 bytesWritten = file.write(fileData); // 从偏移量开始写入文件数据进行保存
    file.close();

    if (bytesWritten == -1)
    {
        qDebug() << "写入文件失败:" << savePath;
    }
    else
    {
        qDebug() << "文件切片保存成功:" << savePath << ", 字节写入:" << bytesWritten;
        updateUploadStatus(file_id, offset, bytesWritten, total_size, user_id, savePath);
    }
}

// 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
void UserUploads::updateUploadStatus(const QString &file_id, qint64 offset, qint64 bytesWritten, qint64 total_size, const QString &user_id, const QString &savePath)
{
    if (!database.isOpen()) {
        qDebug() << "数据库连接未打开";
        return;
    }

    QSqlQuery query(database);

    query.prepare("UPDATE upload_status_files SET last_upload_time = :last_upload_time, missing_parts = :missing_parts WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":last_upload_time", QDateTime::currentDateTime());
    query.bindValue(":file_name", file_id);
    query.bindValue(":user_id", user_id);

    QString missingParts;
    if (offset + bytesWritten < total_size) {
        missingParts = QString("%1 %2").arg(offset + bytesWritten + 1).arg(total_size - 1);
    } else {
        missingParts = "";
    }
    query.bindValue(":missing_parts", missingParts);

    if (!query.exec()) {
        qDebug() << "更新上传状态失败:" << query.lastError();
        return;
    }

    checkFileCompletion(file_id, total_size, user_id, savePath);
}

// 检查文件是否完整上传
void UserUploads::checkFileCompletion(const QString &file_id, qint64 total_size, const QString &user_id, const QString &savePath)
{
    if (!database.isOpen()) {
        qDebug() << "数据库连接未打开";
        return;
    }

    QSqlQuery query(database);

    query.prepare("SELECT missing_parts FROM upload_status_files WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":file_id", file_id);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "查询上传状态失败:" << query.lastError();
        return;
    }

    if (query.next()) {
        QString missingParts = query.value("missing_parts").toString();

        if (missingParts.isEmpty()) {
            query.prepare("UPDATE completed_files SET file_status = 'normal', actual_file_path = :actual_file_path, upload_time = :upload_time WHERE file_name = :file_name AND user_id = :user_id");
            query.bindValue(":actual_file_path", savePath);
            query.bindValue(":upload_time", QDateTime::currentDateTime());
            query.bindValue(":file_id", file_id);
            query.bindValue(":user_id", user_id);

            if (!query.exec()) {
                qDebug() << "更新文件状态失败:" << query.lastError();
                return;
            }

            query.prepare("DELETE FROM upload_status_files WHERE file_name = :file_name AND user_id = :user_id");
            query.bindValue(":file_id", file_id);
            query.bindValue(":user_id", user_id);

            if (!query.exec()) {
                qDebug() << "删除上传状态失败:" << query.lastError();
                return;
            }

            qDebug() << "文件已完整上传，更新状态并删除上传记录:" << savePath;
        }
    }
}

// 文件第一次上传，返回生成的文件ID
QString UserUploads::handleInitialUploadRequest(const QString &file_name, qint64 total_size, const QString &user_id, const QString &client_id)
{
    if (!database.isOpen()) {
        qDebug() << "数据库连接未打开";
        return "";
    }

    // 检查文件是否存在
    QSqlQuery query(database);
    query.prepare("SELECT file_id FROM completed_files WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":file_name", file_name);
    query.bindValue(":user_id", user_id);
    if (!query.exec()) {
        qDebug() << "查询文件失败:" << query.lastError();
        return "";
    }

    QString file_id;
    if (query.next()) {
        file_id = query.value("file_id").toString();
    } else {
        // 分配文件ID
        file_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        // 创建文件记录
        query.prepare("INSERT INTO completed_files (file_id, file_name, user_id, file_status, upload_time) VALUES (:file_id, :file_name, :user_id, 'uploading', NULL)");
        query.bindValue(":file_id", file_id);
        query.bindValue(":file_name", file_name);
        query.bindValue(":user_id", user_id);
        if (!query.exec()) {
            qDebug() << "创建文件记录失败:" << query.lastError();
            return "";
        }

        // 创建上传状态记录
        query.prepare("INSERT INTO upload_status_files (file_id, user_id, last_upload_time, missing_parts) VALUES (:file_id, :user_id, NULL, '')");
        query.bindValue(":file_id", file_id);
        query.bindValue(":user_id", user_id);
        if (!query.exec()) {
            qDebug() << "创建上传状态记录失败:" << query.lastError();
            return "";
        }
    }

    // 发送文件ID给客户端
    return file_id;
}
