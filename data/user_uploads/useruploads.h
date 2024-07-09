#ifndef USERUPLOADS_H
#define USERUPLOADS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

class UserUploads : public QObject
{
    Q_OBJECT

public:
    explicit UserUploads(QObject *parent = nullptr);

    void setDatabase(const QSqlDatabase &db);   // 初始化数据库
    void uploadChunk(const QString &file_id, const QString &base64_data, qint64 offset, qint64 total_size, const QString &user_id);   // 处理切片上传文件
    // 处理第一次上传的文件请求，返回生成的文件id
    QString handleInitialUploadRequest(const QString &file_name, qint64 total_size, const QString &user_id, const QString &client_id);

private:
    // 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
    void updateUploadStatus(const QString &file_id, qint64 offset, qint64 bytesWritten, qint64 total_size, const QString &user_id, const QString &savePath);
    // 检查文件是否完整上传
    void checkFileCompletion(const QString &file_id, qint64 total_size, const QString &user_id, const QString &savePath);

    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USERUPLOADS_H
