#include "user_login_account_manager.h"
#include <QSqlError>
#include <QDebug>
#include <QJsonObject>

UserLoginAccountManager::UserLoginAccountManager(const QSqlDatabase &db, QObject *parent) :
    QObject(parent),
    database(db)
{

}


// 用户登录，验证用户信息
QJsonObject UserLoginAccountManager::validateUser(QJsonObject request, QString client_ip)
{// todo 如果用户未注册，自动进行注册
    QString username = request["username"].toString();
    QString password = request["password"].toString();

    QJsonObject response;   // 响应数据
    response["type"] = "LOGIN"; // 响应数据类型

    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        // 响应数据
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "服务端数据库异常，请稍候重试";

        return response;
    }

    QSqlQuery query(database);
    query.prepare("SELECT password FROM users WHERE username = :username");
    query.bindValue(":username", username);
    /* 在使用参数化查询时，SQL语句使用占位符来表示实际值。这里的 :username 是一个占位符，它将在执行查询时被替换为实际的值。
     * 通过 query.bindValue(":username", username);，将变量 username 的值绑定到占位符 :username。
     * 这意味着当查询被执行时，:username 将被替换为 username 的实际值。
     * 防止SQL注入：参数化查询将用户输入作为数据处理，而不是直接插入到SQL语句中，从而避免了SQL注入攻击。
     */

    if (!query.exec())  // 执行SQL查询
    {
        qWarning() << "查询执行失败:" << query.lastError();
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "检查你的账号或密码";

        return response;
    }

    if (query.next())
    {
        QString storedPassword = query.value(0).toString();
        if(storedPassword == password) // 比较用户数据
        {
            response["status"] = "FAILURE"; // 登录失败
            response["message"] = "检查你的账号或密码";
            return response;
        }
    }

    QString user_id = query.value(0).toString();    // 记录用户ID，用来添加日志

    logLogin(user_id, client_ip);    // 添加到用户登录日志

    // todo：生成一个令牌，返回
    // 令牌单次链接有效
    QString tooken;

    response["status"] = "SUCCESS"; // 登录成功
    response["message"] = "登录成功";// 登录提示
    response["tooken"] = tooken;    // 令牌
    response["user_id"] = user_id;  // 用户ID
    return response;
}

// 注册用户
QJsonObject UserLoginAccountManager::registerUser(QJsonObject request, QString client_ip)
{
    QString username = request["username"].toString();
    QString password = request["password"].toString();
    QString email = request["email"].toString();

    QJsonObject response;   // 响应数据
    response["type"] = "RESISTER";  // 响应数据类型

    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        // 响应数据
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "服务端数据库异常，请稍候重试";

        return response;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO users (username, password_hash, email) VALUES (:username, :password, :email)");
    query.bindValue(":username", username);
    query.bindValue(":password", password); // 在实际应用中，请使用哈希密码存储
    query.bindValue(":email", email);

    if (!query.exec())
    {
        qWarning() << "插入用户失败:" << query.lastError();
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "检查你的账号或密码";
        return response;
    }

    // todo：记录日志等数据

    // 响应数据：注册成功
    response["status"] = "SUCCESS";
    response["message"] = "注册成功";
    return response;
}

// 记录登录日志
bool UserLoginAccountManager::logLogin(const QString &user_id, const QString &ip_address)
{
    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return false;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO login_logs (username, ip_address) VALUES (:username, :ip_address)");
    query.bindValue(":user_id", user_id);
    query.bindValue(":ip_address", ip_address);

    if (!query.exec())
    {
        qWarning() << "记录登录日志失败:" << query.lastError();
            return false;
    }

    return true;
}
