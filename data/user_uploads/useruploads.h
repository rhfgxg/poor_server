#ifndef USERUPLOADS_H
#define USERUPLOADS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QByteArray>

class UserUploads : public QObject
{
    Q_OBJECT

public:
    explicit UserUploads(const QSqlDatabase &db, QObject *parent = nullptr);

    void setDatabase(const QSqlDatabase &db);   // 初始化数据库
    // 处理第一次上传的文件请求，返回生成的文件id
    QJsonObject handleInitialUploadRequest(QJsonObject request);
    // 处理切片上传文件，上传完成后，如果完整上传，调用checkFileCompletion，否则调用updateUploadStatus更新数据库
    QJsonObject uploadChunk(QJsonObject request, QByteArray file_data);
    // 检测文件完整上传：删除文件上传活动表的对应数据，更新文件表对应数据
    QJsonObject checkFileCompletion(QJsonObject request);
private:
    // 每次上传切片时更新 upload_status_files 表中的相关信息，包括最近上传时间和缺失块信息
    void updateUploadStatus(const QString &file_id, qint64 offset);


    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USERUPLOADS_H
