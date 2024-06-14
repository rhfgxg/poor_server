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
    explicit UserLoginAccountManager(const QSqlDatabase &db, QObject *parent = nullptr);

    bool validateUser(const QString &username, const QString &password) const;  // 验证用户信息，参数：用户名，密码
    bool registerUser(const QString &username, const QString &password, const QString &email);  // 注册用户，参数：用户名，密码，邮箱
    bool logLogin(const QString &username, const QString &ipAddress);   // 记录用户登录日志，参数：用户名，IP地址

private:
    QSqlDatabase database;   // MySQL 数据库连接
};

#endif // USER_LOGIN_ACCOUNT_MANAGER_H

