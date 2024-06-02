#ifndef USER_LOGIN_ACCOUNT_MANAGER_H
#define USER_LOGIN_ACCOUNT_MANAGER_H

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

// 用户账号登录
class UserLoginAccountManager : public QObject
{
    Q_OBJECT

public:
    explicit UserLoginAccountManager(QObject *parent = nullptr);

    bool validateUser(const QString &username, const QString &password) const;  // 验证用户信息
    bool registerUser(const QString &username, const QString &password, const QString &email);
    bool logLogin(const QString &username, const QString &ipAddress);

    void setDatabase(const QSqlDatabase &db);

private:
    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USER_LOGIN_ACCOUNT_MANAGER_H

