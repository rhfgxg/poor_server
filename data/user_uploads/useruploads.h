#ifndef USERUPLOADS_H
#define USERUPLOADS_H

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

class UserUploads : public QObject
{
    Q_OBJECT

public:
    explicit UserUploads(QObject *parent = nullptr);

    void setDatabase(const QSqlDatabase &db);   // 初始化数据库
    void uploads(const QString &file_name, const QString &base64_date, const QString &user_id);     // 用户上传，参数：文件名、文件数据、上传的用户id

private:
    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USERUPLOADS_H
