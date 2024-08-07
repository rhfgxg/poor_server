#include "useruploads.h"
#include <QFile>    // 使用文件
#include <QCoreApplication> // 获取当前执行文件路径
#include <QDateTime>    // 获取当前时间
#include <QDir> // 新建文件路径
#include <QUuid>    // 生成文件ID
#include <QJsonArray>
#include <QJsonDocument>
#include <QBitArray>

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
    QJsonObject response;   // 响应数据的json子包

    QString file_id = QUuid::createUuid().toString(QUuid::WithoutBraces);   // 分配文件ID
    QString file_name = request["file_name"].toString(); // 客户端展示文件名
    QString file_format = request["file_format"].toString();    // 文件拓展名：文件格式
    qint64 file_size = request["file_size"].toInt();   // 总大小
    QString account = request["account"].toString();  // 用户账号
    QString client_id = request["client_id"].toString(); // 客户端ID
    QString file_path = request["file_path"].toString();// 客户端源文件路径，间接传给客户端
    QString missing_parts;  // 缺失文件列表：位图
    {
        QVector<bool> bitmap;   // 位图
        QJsonArray jsonArray = request["missing_parts"].toArray();  // 获取上传的位图
        bitmap.reserve(jsonArray.size());   // 扩展容器空间

        for (const QJsonValue &value : jsonArray)   // 将jsonArray转换成vector
        {
            if (value.isBool())
            {
                bitmap.append(value.toBool());
            }
        }

        for (bool bit : bitmap) // 将vector转换成string
        {
            missing_parts.append(bit ? '1' : '0');
        }
    }

    QString client_display_path = "user/file";  // 客户端展示路径：
    QDateTime currentDateTime = QDateTime::currentDateTime();   // 当前时间
    QString upload_time = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");   // 文件上传时间

    // 构建文件保存路径
    QString actual_file_path;
    {
        QString dataDir = QCoreApplication::applicationDirPath() + "/data";
        QString userUploadsDir = dataDir + "/user_uploads/" + account;
        // 将当前时间作为路径的一部分：例如2024_07
        QString yearMonthDir = QString::number(currentDateTime.date().year()) + "_" + QString::number(currentDateTime.date().month());
        actual_file_path = userUploadsDir + "/" + yearMonthDir + "/" + file_id;   // 最终保存路径

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
    }

    // 新建文件并打开
    {
        QFile file(actual_file_path);
        if (file.exists())
        {
            qDebug() << "初始化文件上传：文件已存在:" << actual_file_path;
            response["status"] = "FAILURE"; // 任务创建失败
            response["message"] = "服务端新建文件失败，文件已存在";
            return response;
        }

        if (!file.open(QIODevice::WriteOnly ))
        {
            qDebug() << "初始化文件上传：新建文件失败失败:" << actual_file_path;
            response["status"] = "FAILURE"; // 任务创建失败
            response["message"] = "服务端新建文件失败，请稍候重试";
            return response;
        }
        file.close();   // 关闭新建的文件
    }

    if (!database.isOpen())
    {
        qDebug() << "初始化上传任务：数据库连接未打开";

        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;

    }

    QSqlQuery query(database);  // 数据库查询对象

    // 处理同名文件：添加后缀
    QString file_new_name = file_name;
    {
        // 检查是否有同名文件存在
        // 准备查询语句
        query.clear();  // 重置查询对象
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
        bool exact_match = false;   // 是否有同名文件
        QRegularExpression re("^" + QRegularExpression::escape(file_name) + "\\((\\d+)\\)$");

        while (query.next())
        {
            QString existing_file_name = query.value("file_name").toString();   // 查出的文件名
            if (existing_file_name == file_name)
            {// 如果查出同名文件
                exact_match = true; // 更新状态
            }
            else
            {// 如果名字不完全相同，处理文件名后缀：计算同名文件数量
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

        // 如果有同名文件，添加后缀
        if (exact_match || max_suffix > 0)
        {
            file_new_name =  file_name + "(" + QString::number(max_suffix + 1) + ")";
        }
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
    query.prepare("INSERT INTO upload_status_files VALUES (:file_id, :account, :client_id, :file_path, :actual_file_path, :file_size, :last_upload_time, :missing_parts)");
    query.bindValue(":file_id", file_id);   // 文件ID
    query.bindValue(":account", account);   // 用户ID
    query.bindValue(":client_id", client_id);   // 客户端ID
    query.bindValue(":file_path", file_path);   // 客户端保存文件路径
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
    response["file_id"] = file_id;  // 分配的文件ID
    // 以下数据是为了实现由初始化结果直接调用执行函数，间接返回给客户端的数据
    response["file_path"] = file_path;  // 客户端本地文件路径
    response["missing_parts"] = request["missing_parts"].toArray();  // 缺失文件列表

    return response;
}

// 处理文件切片上传
QJsonObject UserUploads::uploadChunk(QJsonObject request, QByteArray file_data)
{/*
  * 函数返回的数据不会响应给客户端：本函数执行过于频繁，每一条都响应会增加网络负担
  * 客户端获取文件上传状态：使用心跳机制（另一个函数）查询文件上传状态
  */
    // 上传的数据
    QString file_id = request["file_id"].toString();    // 文件ID
    qint64 offset = request["offset"].toInt();  // 文件块偏移地址

    QJsonObject response;   // 响应数据的json子包

    // 使用文件ID，查询需要的信息
    QSqlQuery query(database);
    query.prepare("SELECT actual_file_path FROM upload_status_files WHERE file_id = :file_id");
    query.bindValue(":file_id", file_id);

    // 查询失败
    if (!query.exec())
    {
        qDebug() << "上传任务：查询文件数据执行失败:" << query.lastError();
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }
    else if (!query.next())
    {
        qDebug() << "上传任务：查询文件数据失败:" << query.lastError();
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }

    QString actual_file_path = query.value("actual_file_path").toString(); // 服务端文件保存路径

    // 打开文件，如果文件不存在就结束
    QFile file(actual_file_path);
    if (!file.open(QIODevice::ReadWrite))
    {
        qDebug() << "文件上传：文件打开失败:";
//        response["status"] = "FAILURE"; // 任务创建失败
//        response["message"] = "服务端打开文件失败，请稍候重试";
        return response;
    }

    // 定位偏移量
    if (!file.seek(offset))
    {
        qDebug() << "文件上传：文件数据偏移定位失败:";
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "服务端数据库查询同名文件失败，请稍候重试";
        return response;
    }

    qint64 bytesWritten = file.write(file_data); // 从偏移量开始写入文件数据进行保存

    file.close();

    if (bytesWritten == -1)
    {
        qDebug() << "写入文件失败:" << actual_file_path;
        response["status"] = "FAILURE"; // 任务创建失败
        response["message"] = "文件写入失败，请稍候重试";
    }
    else
    {
        qDebug() << "文件切片保存成功:, 字节写入:" << bytesWritten;
        response["status"] = "SUCCESS";
        response["message"] = "文件上传成功";

        // 更新 upload_status_files 表中的相关信息
        updateUploadStatus(file_id, offset);
    }

    return response;
}

// 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
void UserUploads::updateUploadStatus(const QString &file_id, qint64 offset)
{/*参数：文件ID，偏移量(文件切片开始位置)，账号*/
    if (!database.isOpen())
    {
        qDebug() << "数据库连接未打开";
        return;
    }

    QString missingParts_new;   // 获取最新缺失文件块列表：位图
    {
        // 查询数据库中的位图
        QSqlQuery query(database);
        query.prepare("SELECT missing_parts FROM upload_status_files WHERE file_id = :file_id");
        query.bindValue(":file_id", file_id);

        // 查询失败
        if (!query.exec())
        {
            qDebug() << "上传任务更新数据：查询文件数据执行失败:" << query.lastError();
        }
        else if (!query.next())
        {
            qDebug() << "上传任务更新数据：查询文件数据失败:" << query.lastError();
        }

        QString missing_parts = query.value("missing_parts").toString(); // 文件缺失列表

        QVector<bool> bitVector;    // vector保存位图
        bitVector.reserve(missing_parts.size());
        for (int j = 0; j < missing_parts.size(); ++j)
        {
            bitVector.append(missing_parts.at(j) == '1');
        }

        qint32 block_size = 1024 * 1024;    // 文件块大小
        int i = offset / block_size;    // 下标

        if (i < bitVector.size())
        {
            bitVector[i] = true;
        }

        for (bool bit : bitVector)
        {
            missingParts_new.append(bit ? '1' : '0');
        }
    }

    QSqlQuery query(database);
    query.prepare("UPDATE upload_status_files SET last_upload_time = :last_upload_time, missing_parts = :missing_parts WHERE file_id = :file_id");
    query.bindValue(":file_id", file_id);   // 文件ID
    query.bindValue(":last_upload_time", QDateTime::currentDateTime()); // 更新上传时间
    query.bindValue(":missing_parts", missingParts_new);    // 更新缺失的文件块

    if (!query.exec())
    {
        qDebug() << "更新上传状态失败:" << query.lastError();
    }
}

// 检测文件是否完整上传：
QJsonObject UserUploads::checkFileCompletion(QJsonObject request)
{/* 完整上传：删除文件上传活动表的对应数据，更新文件表对应数据，响应客户端已经完整上传
  * 没有完整上传：向返回客户端 最新的缺失列表，客户端遍历最新的缺失列表
  */
    QString file_id = request["file_id"].toString();    // 文件ID

    QJsonObject response;   // 响应数据的json子包

    if (!database.isOpen())
    {
        qDebug() << "数据库连接未打开";
    }

    QSqlQuery query(database);  // 查询对象
    // 查询文件缺失列表
    query.prepare("SELECT missing_parts, file_path FROM upload_status_files WHERE file_id = :file_id");
    query.bindValue(":file_id", file_id);   // 文件ID

    if (!query.exec())
    {
        qDebug() << "文件检验：查询文件缺失列表失败" << query.lastError();
        response["status"] = "FAILURE";
        response["message"] = "文件检验失败：查询文件缺失列表失败";
    }

    if (query.next())
    {
        qDebug("检查文件上传结果");
        QString file_path = query.value("file_path").toString();    // 客户端文件保存路径
        QString missing_parts = query.value("missing_parts").toString(); // 获取当前缺失文件块列表

        // 将字符串转换为 QVector<bool>
        QVector<bool> bitVector;
        bitVector.reserve(missing_parts.size()); // 预留足够的空间以优化性能

        for (int j = 0; j < missing_parts.size(); ++j)  // 遍历字符串，转换为位图
        {
            bitVector.append(missing_parts.at(j) == '1');
        }

        // 遍历缺失列表
        for (bool bit : bitVector)
        {
            if (!bit)
            {
                // 如果没有完整上传：返回客户端 最新的缺失列表
                response["status"] = "FAILURE"; // 任务创建失败
                response["message"] = "文件未上传完成";
                response["file_id"] = file_id;  // 文件ID
                response["file_path"] = file_path;  // 客户端文件保存路径
                response["missing_parts"] = missing_parts;
                return response;
            }
        }

        // 如果缺失列表所有位置都是true：已上传
        // 更新 completed_files表的文件上传状态
        query.clear();  // 重置查询对象
        query.prepare("UPDATE completed_files SET file_status = :file_status WHERE file_id = :file_id");
        query.bindValue(":file_id", file_id);   // 文件ID
        query.bindValue(":file_status", "normal");   // 更新文件状态

        if (!query.exec())
        {
            qDebug() << "文件检验：更新文件状态失败:" << query.lastError();
            response["status"] = "FAILURE";
            response["message"] = "文件检验失败：更新文件状态失败";
        }

        // 删除 upload_status_files表的相关数据
        query.clear();  // 重置查询对象
        query.prepare("DELETE FROM upload_status_files WHERE file_id = :file_id");
        query.bindValue(":file_id", file_id);   // 文件ID

        if (!query.exec())
        {
            qDebug() << "文件检验：删除上传状态失败:" << query.lastError();
            response["status"] = "FAILURE";
            response["message"] = "文件检验失败：删除上传状态失败";
        }
        qDebug() << "文件检验：文件已完整上传";
        response["status"] = "SUCCESS";
        response["message"] = "文件完整上传";
    }

    return response;
}

