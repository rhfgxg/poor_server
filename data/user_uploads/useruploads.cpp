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

// 文件第一次上传，返回生成的文件ID
QJsonObject UserUploads::handleInitialUploadRequest(QJsonObject request)
{/* 做文件上传的准备工作
  * 客户端上传本次文件的文件名，客户端id，用户ID，文件总大小
  */
    QJsonObject response;   // 相应数据
    response["type"] = "INITIAL_UPLOAD";    // 响应数据类型

    QString file_name = request["file_name"].toString(); // 文件名
    QString file_path = request["file_path"].toString();// 文件路径
    qint64 total_size = request["total_size"].toInt();   // 总大小
    QString user_id = request["user_id"].toString();  // 用户ID
    QString client_id = request["client_id"].toString(); // 客户端ID

    if (!database.isOpen())
    {
        qDebug() << "数据库连接未打开";

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;

    }

    // 检查是否有同名文件存在
    QSqlQuery query(database);
    query.prepare("SELECT file_id FROM completed_files WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":file_name", file_name);
    query.bindValue(":user_id", user_id);
    if (!query.exec())
    {
        qDebug() << "查询文件失败:" << query.lastError();
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";

        return response;
    }

    QString file_id = QUuid::createUuid().toString(QUuid::WithoutBraces);   // 分配文件ID

    if (query.next()) // 找到同名文件
    {/* 出现同名文件
      * 为同名文件添加后缀，查看有几个同名文件，后缀n = 同名文件数+1
      * 第二个同名文件 = 文件名(2)
      */
        QString new_file_name;
       file_name = new_file_name;
    }

    // 分配文件ID
    file_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 在文件上传表，创建文件上传记录
    query.prepare("INSERT INTO completed_files (file_id, file_name, user_id, file_status, upload_time) VALUES (:file_id, :file_name, :user_id, 'uploading', NULL)");
    query.bindValue(":file_id", file_id);
    query.bindValue(":file_name", file_name);
    query.bindValue(":user_id", user_id);
    if (!query.exec())
    {
        qDebug() << "创建文件记录失败:" << query.lastError();

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端创建数据库对应数据失败";
        return response;
    }

    // 在文件表 新建数据项，初始化上传状态记录
    query.prepare("INSERT INTO upload_status_files (file_id, user_id, client_id, last_upload_time, missing_parts) VALUES (:file_id, :user_id, :client_id, NULL, '')");
    query.bindValue(":file_id", file_id);
    query.bindValue(":user_id", user_id);
    query.bindValue(":client_id", client_id);
    if (!query.exec())
    {
        qDebug() << "创建上传状态记录失败:" << query.lastError();

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端创建数据库对应数据失败";
        return response;
    }

    // 响应数据
    response["status"] = "SUCCESS";
    response["message"] = "创建成功";
    response["file_id"] = file_id;
    // 以下数据是为了实现由初始化结果直接调用执行函数，间接返回给客户端的数据
    response["file_path"] = file_path;  // 客户端本地文件路径
    response["user_id"] = user_id;  // 用户ID
    response["total_size"] = total_size;  // 用户ID
    return response;
}

// 处理文件切片上传
QJsonObject UserUploads::uploadChunk(QJsonObject request)
{/* 获取时间，用户，路径一起保存在数据库
  * 文件路径：date/用户名/年/月
  * 函数返回的数据不会响应给客户端：本函数执行过于频繁，每一条都响应会增加网络负担
  * 客户端获取文件上传状态：使用心跳机制（另一个函数）查询文件上传状态
  */
    QJsonObject response;   // 相应数据
    response["type"] = "UPLOAD_CHUNK";  // 响应数据类型

    QString file_id = request["file_id"].toString();    // 文件ID
    QString base64_data = request["filedata"].toString();    // 文件块数据
    qint64 offset = request["offset"].toInt();  // 已发送文件大小
    QString user_id = request["user_id"].toString();   // 用户ID
    qint64 total_size = request["total_size"].toInt(); // 文件总大小

    // 解码 base64数据
    QByteArray fileData = QByteArray::fromBase64(base64_data.toUtf8());

// 构建文件保存路径
    QString dataDir = QCoreApplication::applicationDirPath() + "/data";
    QString userUploadsDir = dataDir + "/user_uploads/" + user_id;
    // 获取当前时间，并作为路径的一部分：2024_07
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

//            response["status"] = "FAILURE"; // 任务创建失败
//            response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
            return response;
        }
    }

    // 打开文件，如果文件不存在就新建文件
    QFile file(savePath);
    if (!file.open(QIODevice::Append))
    {
        qDebug() << "文件保存失败:" << savePath;
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
        return response;
    }

    // 定位偏移量
    if (!file.seek(offset))
    {
        qDebug() << "文件定位失败:" << savePath;
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
        return response;
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

    // 返回的数据被抛弃，仅是为了格式统一
//    response["status"] = "SUCCESS";
//    response["message"] = "";
    return response;
}

// 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
void UserUploads::updateUploadStatus(const QString &file_id, qint64 offset, qint64 bytesWritten, qint64 total_size, const QString &user_id, const QString &savePath)
{
    if (!database.isOpen())
    {
        qDebug() << "数据库连接未打开";
        return;
    }

    QSqlQuery query(database);

    query.prepare("UPDATE upload_status_files SET last_upload_time = :last_upload_time, missing_parts = :missing_parts WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":last_upload_time", QDateTime::currentDateTime());
    query.bindValue(":file_name", file_id);
    query.bindValue(":user_id", user_id);

    QString missingParts;
    if (offset + bytesWritten < total_size)
    {
        missingParts = QString("%1 %2").arg(offset + bytesWritten + 1).arg(total_size - 1);
    }
    else
    {
        missingParts = "";
    }
    query.bindValue(":missing_parts", missingParts);

    if (!query.exec())
    {
        qDebug() << "更新上传状态失败:" << query.lastError();
        return;
    }

    checkFileCompletion(file_id, total_size, user_id, savePath);
}

// 检查文件是否完整上传
void UserUploads::checkFileCompletion(const QString &file_id, qint64 total_size, const QString &user_id, const QString &savePath)
{
    if (!database.isOpen())
    {
        qDebug() << "数据库连接未打开";
        return;
    }

    QSqlQuery query(database);

    query.prepare("SELECT missing_parts FROM upload_status_files WHERE file_name = :file_name AND user_id = :user_id");
    query.bindValue(":file_id", file_id);
    query.bindValue(":user_id", user_id);

    if (!query.exec())
    {
        qDebug() << "查询上传状态失败:" << query.lastError();
        return;
    }

    if (query.next())
    {
        QString missingParts = query.value("missing_parts").toString();

        if (missingParts.isEmpty())
        {
            query.prepare("UPDATE completed_files SET file_status = 'normal', actual_file_path = :actual_file_path, upload_time = :upload_time WHERE file_name = :file_name AND user_id = :user_id");
            query.bindValue(":actual_file_path", savePath);
            query.bindValue(":upload_time", QDateTime::currentDateTime());
            query.bindValue(":file_id", file_id);
            query.bindValue(":user_id", user_id);

            if (!query.exec())
            {
                qDebug() << "更新文件状态失败:" << query.lastError();
                return;
            }

            query.prepare("DELETE FROM upload_status_files WHERE file_name = :file_name AND user_id = :user_id");
            query.bindValue(":file_id", file_id);
            query.bindValue(":user_id", user_id);

            if (!query.exec())
            {
                qDebug() << "删除上传状态失败:" << query.lastError();
                return;
            }

            qDebug() << "文件已完整上传，更新状态并删除上传记录:" << savePath;
        }
    }
}

