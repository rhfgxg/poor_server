#include "useruploads.h"
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QUuid>

UserUploads::UserUploads(const QSqlDatabase &db, QObject *parent) :
    database(db),
    QObject(parent)
{

}

// 文件第一次上传，返回生成的文件ID
QJsonObject UserUploads::handleInitialUploadRequest(QJsonObject request)
{/* 做文件上传的准备工作
  * 客户端上传本次文件的文件名，客户端id，用户ID，文件总大小
  * 获取时间，用户，路径一起保存在数据库
  * 文件路径：date/用户名/年/月
  */
    QJsonObject response;   // 相应数据
    response["type"] = "INITIAL_UPLOAD";    // 响应数据类型

    QString file_id = QUuid::createUuid().toString(QUuid::WithoutBraces);   // 分配文件ID
    QString file_name = request["file_name"].toString(); // 客户端展示文件名
    QString file_format = request["file_format"].toString();    // 文件拓展名：文件格式
    QString file_path = request["file_path"].toString();// 客户端源文件路径，间接传给客户端
    qint64 file_size = request["file_size"].toInt();   // 总大小
    QString account = request["account"].toString();  // 用户ID
    QString client_id = request["client_id"].toString(); // 客户端ID

    QString client_display_path = "user/file";  // 客户端展示路径：

    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString upload_time = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");   // 文件上传时间

    int totalBlocks = (file_size + (1024*1024) - 1) / (1024*1024); // 文件总块数
    QString missing_parts;  // 缺失文件块下标列表：保存数字0 1 2 3 n，使用空格分开，0表示第一块，乘切片大小得到偏移量
    for (int i = 0; i < totalBlocks; ++i)
    {
        missing_parts.append(QString::number(i) + " ");
    }
    // 去除最后一个空格
    if (!missing_parts.isEmpty())
    {
        missing_parts.chop(1);
    }

    // 构建文件保存路径
    QString dataDir = QCoreApplication::applicationDirPath() + "/data";
    QString userUploadsDir = dataDir + "/user_uploads/" + account;
    // 将当前时间作为路径的一部分：例如2024_07
    QString yearMonthDir = QString::number(currentDateTime.date().year()) + "_" + QString::number(currentDateTime.date().month());
    QString actual_file_path = userUploadsDir + "/" + yearMonthDir + "/" + file_id;   // 最终保存路径

    // 新建文件保存路径
    QDir dir(userUploadsDir + "/" + yearMonthDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qDebug() << "初始化文件上传：文件保存路径创建失败:" << dir.path();

            response["status"] = "FAILURE"; // 任务创建失败
            response["message"] = "服务端创建文件保存路径失败，请稍候重试";
            return response;
        }
    }

    // 打开文件，如果文件不存在就新建文件
    QFile file(actual_file_path);
    if (!file.open(QIODevice::Append))
    {
        qDebug() << "初始化文件上传：新建文件失败失败:" << actual_file_path;
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端新建文件失败，请稍候重试";
        return response;
    }
    file.close();   // 关闭文件

    if (!database.isOpen())
    {
        qDebug() << "初始化上传任务：数据库连接未打开";

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;

    }

    // 检查是否有同名文件存在
    QSqlQuery query(database);
    // 准备查询语句
    query.prepare("SELECT file_id, file_name FROM completed_files WHERE account = :account AND (file_name = :file_name OR file_name REGEXP CONCAT('^', :file_name, '\\\\([0-9]+\\\\)$'))");
    query.bindValue(":file_name", file_name);
    query.bindValue(":account", account);

    // 执行查询
    if (!query.exec())
    {
        qDebug() << "初始化上传任务：查询文件失败:" << query.lastError();
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }

    // 计算同名文件数量
    int max_suffix = 0;
    bool exact_match = false;
    QRegularExpression re("^" + QRegularExpression::escape(file_name) + "\\((\\d+)\\)$");

    while (query.next())
    {
        QString existing_file_name = query.value("file_name").toString();
        if (existing_file_name == file_name)
        {
            exact_match = true;
        }
        else
        {
            QRegularExpressionMatch match = re.match(existing_file_name);
            if (match.hasMatch())
            {
                int suffix = match.captured(1).toInt();
                if (suffix > max_suffix)
                {
                    max_suffix = suffix;
                }
            }
        }
    }

    // 如果找到同名文件，添加后缀
    QString file_new_name = "";
    if (exact_match || max_suffix > 0)
    {
        file_new_name =  file_name + "(" + QString::number(max_suffix + 1) + ")";
    }

    // 在 文件表，创建新的文件数据
    query.clear();  // 重置查询对象
    query.prepare("INSERT INTO completed_files VALUES (:file_id, :account, :file_name, :file_format, :file_size, :upload_time, :actual_file_path, :client_display_path, :file_status)");
    query.bindValue(":file_id", file_id);   // 文件ID
    query.bindValue(":account", account);   // 用户ID
    query.bindValue(":file_name", file_new_name);   // 客户端展示文件名：同名文件加后缀(n)
    query.bindValue(":file_format", file_format);   // 文件拓展名
    query.bindValue(":file_size", file_size);   // 文件总大小
    query.bindValue(":upload_time", upload_time);   // 文件上传时间
    query.bindValue(":actual_file_path", actual_file_path);   // 服务端保存文件路径
    query.bindValue(":client_display_path", client_display_path);   // 客户端展示路径
    query.bindValue(":file_status", "uploading");   // 文件状态：上传中

    if (!query.exec())
    {
        qDebug() << "初始化文件上传：创建文件记录失败:" << query.lastError();

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端创建数据库对应数据失败";
        return response;
    }

    // 在 文件上传活动 创建新的数据
    query.clear();  // 重置查询对象
    query.prepare("INSERT INTO upload_status_files VALUES (:file_id, :account, :client_id, :actual_file_path, :file_size, :last_upload_time, :missing_parts)");
    query.bindValue(":file_id", file_id);   // 文件ID
    query.bindValue(":account", account);   // 用户ID
    query.bindValue(":client_id", client_id);   // 客户端ID
    query.bindValue(":actual_file_path", actual_file_path);   // 服务端保存路径
    query.bindValue(":file_size", file_size);   // 文件总大小
    query.bindValue(":last_upload_time", upload_time);  // 上次上传事件
    query.bindValue(":missing_parts", missing_parts);   // 缺失文件块列表

    if (!query.exec())
    {
        qDebug() << "初始化文件上传：创建上传状态记录失败:" << query.lastError();

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
    return response;
}

// 处理文件切片上传
QJsonObject UserUploads::uploadChunk(QJsonObject request)
{/*
  * 函数返回的数据不会响应给客户端：本函数执行过于频繁，每一条都响应会增加网络负担
  * 客户端获取文件上传状态：使用心跳机制（另一个函数）查询文件上传状态
  */
    QJsonObject response;   // 相应数据
    response["type"] = "UPLOAD_CHUNK";  // 响应数据类型
    QString file_id = request["file_id"].toString();    // 文件ID
    QString base64_data = request["filedata"].toString();    // 文件块数据
    QByteArray file_data = QByteArray::fromBase64(base64_data.toUtf8());    // 解码 base64数据
    qint64 offset = request["offset"].toInt();  // 文件块偏移量

    // 使用文件ID，查询需要的信息
    QSqlQuery query(database);
    // 准备查询语句
    query.prepare("SELECT user_id, total_size, actual_file_path FROM completed_files WHERE file_id = :file_id");
    query.bindValue(":file_id", file_id);

    if (!query.exec())
    {
        qDebug() << "上传任务：查询文件数据执行失败:" << query.lastError();
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }
    else if (!query.next())
    {
        qDebug() << "上传任务：查询文件数据失败:" << query.lastError();
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }

    QString user_id = query.value("user_id").toString(); // 用户ID
    qint64 total_size = query.value("total_size").toInt(); // 文件大小
    QString savePath = query.value("actual_file_path").toString(); // 文件保存路径

    // 打开文件，如果文件不存在就结束
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "文件上传：文件打开失败:" << savePath;
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
        return response;
    }

    // 定位偏移量
    if (!file.seek(offset))
    {
        qDebug() << "文件上传：文件数据偏移定位失败:" << savePath;
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
        return response;
    }

    qint64 bytesWritten = file.write(file_data); // 从偏移量开始写入文件数据进行保存
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

