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
    query.prepare("SELECT password FROM users WHERE username = :username");
    query.bindValue(":username", username);
    // 在使用参数化查询时，SQL语句使用占位符来表示实际值。这里的 :username 是一个占位符，它将在执行查询时被替换为实际的值。
    // 通过 query.bindValue(":username", username);，将变量 username 的值绑定到占位符 :username。
    // 这意味着当查询被执行时，:username 将被替换为 username 的实际值。
    // 防止SQL注入：参数化查询将用户输入作为数据处理，而不是直接插入到SQL语句中，从而避免了SQL注入攻击。

    if (!query.exec())  // 执行SQL查询, 返回bool，flash表示失败
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
