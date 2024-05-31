#include "user_login_account_manager.h"
#include <QSqlError>
#include <QDebug>

UserLoginAccountManager::UserLoginAccountManager(QObject *parent) :
    QObject(parent)
{

}

void UserLoginAccountManager::setDatabase(const QSqlDatabase &db)
{
    database = db;
}

// 用户登录，验证用户信息
bool UserLoginAccountManager::validateUser(const QString &username, const QString &password) const
{
    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return false;
    }

    QSqlQuery query(database);
    query.prepare("SELECT password_hash FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec())
    {
        qWarning() << "查询执行失败:" << query.lastError();
        return false;
    }

    if (query.next())
    {
        QString storedPassword = query.value(0).toString();
        return storedPassword == password; // 在实际应用中，请使用哈希密码比较
    }

    return false;
}

// 注册用户
bool UserLoginAccountManager::registerUser(const QString &username, const QString &password, const QString &email)
{
    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return false;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO users (username, password_hash, email) VALUES (:username, :password, :email)");
    query.bindValue(":username", username);
    query.bindValue(":password", password); // 在实际应用中，请使用哈希密码存储
    query.bindValue(":email", email);

    if (!query.exec())
    {
        qWarning() << "插入用户失败:" << query.lastError();
        return false;
    }

    return true;
}

// 记录登录日志
// 传入用户名和ip
bool UserLoginAccountManager::logLogin(const QString &username, const QString &ipAddress)
{
    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return false;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO login_logs (username, ip_address) VALUES (:username, :ip_address)");
    query.bindValue(":username", username);
    query.bindValue(":ip_address", ipAddress);

    if (!query.exec())
    {
        qWarning() << "记录登录日志失败:" << query.lastError();
            return false;
    }

    return true;
}
