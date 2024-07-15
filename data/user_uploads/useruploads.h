#ifndef USERUPLOADS_H
#define USERUPLOADS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>

class UserUploads : public QObject
{
    Q_OBJECT

public:
    explicit UserUploads(const QSqlDatabase &db, QObject *parent = nullptr);

    void setDatabase(const QSqlDatabase &db);   // 初始化数据库
    // 处理切片上传文件
    QJsonObject uploadChunk(QJsonObject request);
    // 处理第一次上传的文件请求，返回生成的文件id
    QJsonObject handleInitialUploadRequest(QJsonObject request);

private:
    // 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
    void updateUploadStatus(const QString &file_id, qint64 offset, qint64 bytesWritten, qint64 file_size, const QString &account, const QString &actual_file_path);
    // 检查文件是否完整上传
    void checkFileCompletion(const QString &file_id, qint64 file_size, const QString &account, const QString &actual_file_path);

    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USERUPLOADS_H
