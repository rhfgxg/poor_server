#include "user_login_account_manager.h"
#include <QSqlError>
#include <QDebug>
#include <QJsonObject>
#include <QRandomGenerator>

UserLoginAccountManager::UserLoginAccountManager(const QSqlDatabase &db, QObject *parent) :
    QObject(parent),
    database(db)
{

}


// 用户登录，验证用户信息
QJsonObject UserLoginAccountManager::validateUser(QJsonObject request, QString client_ip)
{// todo 如果用户未注册，自动进行注册
    QJsonObject response;   // 响应数据
    response["type"] = "LOGIN"; // 响应数据类型

    if (!database.isOpen())
    {
        qWarning() << "账号登陆：数据库未打开";
        // 响应数据
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "服务端数据库异常，请稍候重试";

        return response;
    }

    QString account = request["account"].toString();    // 账号
    QString password_hash = request["password_hash"].toString();  // md5加密的密码
    QString client_id = request["client_id"].toString();// 客户端ID，用来记录登录日志

    QSqlQuery query(database);
    query.prepare("SELECT user_id, password_hash FROM users WHERE account = :account");
    query.bindValue(":account", account);
    // 查询结果两列：user_id，password_hash
    /* 在使用参数化查询时，SQL语句使用占位符来表示实际值。这里的 :username 是一个占位符，它将在执行查询时被替换为实际的值。
     * 通过 query.bindValue(":username", username);，将变量 username 的值绑定到占位符 :username。
     * 这意味着当查询被执行时，:username 将被替换为 username 的实际值。
     * 防止SQL注入：参数化查询将用户输入作为数据处理，而不是直接插入到SQL语句中，从而避免了SQL注入攻击。
     */

    if (!query.exec())  // 执行SQL查询
    {/*语法执行出错：格式，权限，语法等*/
        qWarning() << "账号登陆：查询执行失败:" << query.lastError();

        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "检查你的账号或密码";
        return response;
    }

    if (query.next())   // 返回值：查询结果中是否有更多的行可读取
    {/* next作用
      * 遍历查出的多行数据
      * 初始指向第一行数据前一位，
      * 每次调用 next，query指向下一行
      * 遍历所有行需要循环使用
      * 当没有下一行时，next返回flash
      */

        if(query.value(1).toString() != password_hash) // 比较用户数据
        {/*如果密码错误*/
            response["status"] = "FAILURE"; // 登录失败
            response["message"] = "检查你的账号或密码";
            return response;
        }
    }
    else
    {
        // 没有找到账号
        response["status"] = "FAILURE"; // 登录失败
        response["message"] = "账号不存在";
        return response;
    }

    QString user_id = query.value(0).toString();    // 获取用户ID，用来添加日志

    logLogin(user_id, client_id);    // 添加到用户登录日志

    // todo：生成一个令牌，返回
    // 令牌单次链接有效
    QString tooken;

    response["status"] = "SUCCESS"; // 登录成功
    response["message"] = "登录成功";// 登录提示
    response["tooken"] = tooken;    // 令牌

    return response;
}

// 注册用户
QJsonObject UserLoginAccountManager::registerUser(QJsonObject request, QString client_ip)
{
    QJsonObject response;   // 响应数据
    response["type"] = "RESISTER";  // 响应数据类型

    if (!database.isOpen())
    {
        qWarning() << "注册账号：数据库未打开";
        // 响应数据
        response["status"] = "FAILURE"; // 注册失败
        response["message"] = "服务端数据库异常，请稍候重试";
        return response;
    }

    QString user_id = "";    // 用户ID
    QString account = "";    // 账号
    QString user_name = request["username"].toString(); // 用户名
    QString password_hash = request["password"].toString();  // 用户密码
    QString email = request["email"].toString();    // 用户邮箱
    QString phone_number = request["phone_number"].toString();   // 手机号码
    QString id_number = ""; // 身份证号
    QString registration_date = "";  // 注册日期
    QString last_login = ""; // 最后一次登录时间
    QString role = "user";  // 用户权限
    QString user_status = "";    // 用户状态：'inactive未实名', 'active正常', 'inactive_long长时间未登录', 'deleted注销', 'banned封禁中'
    QString avatar_url = ""; // 头像链接
//    QString client_id = request["client_id"].toString(); // 客户端ID

    QSqlQuery query(database);

    do {    // 生成唯一的11数字账号
        account = QString::number(QRandomGenerator::global()->bounded(static_cast<quint32>(1000000000), static_cast<quint32>(10000000000)));    // 生成11位随机数

        query.prepare("SELECT COUNT(*) FROM users WHERE account = :account");
        query.bindValue(":account", account);

        if (!query.exec() || !query.next()) // 数据库查询异常
        {
            qDebug() << "注册账号：生成账号时数据库异常:" << query.lastError().text();
            // 响应数据
            response["status"] = "FAILURE"; // 注册失败
            response["message"] = "服务端生成账号数据异常，请稍候重试";
            return response;
        }
    } while (query.value(0).toInt() > 0); // 确保账号唯一


    query.prepare("INSERT INTO users * VALUES (:user_id, :account, :user_name, :password_hash, :email, :phone_number, :id_number, :registration_date, :last_login, :role, :user_status, :avatar_url,)");
    query.bindValue(":user_id", user_id);   // 用户ID
    query.bindValue(":account", account);   // 账号
    query.bindValue(":user_name", user_name);   // 用户名
    query.bindValue(":password_hash", password_hash);   // 用户密码
    query.bindValue(":email", email);   // 用户邮箱
    query.bindValue(":phone_number", phone_number); // 手机号码
    query.bindValue(":id_number", id_number);   // 身份证号
    query.bindValue(":registration_date", registration_date);   // 注册日期
    query.bindValue(":last_login", last_login); // 最后一次登录时间
    query.bindValue(":role", role); // 用户权限
    query.bindValue(":user_status", user_status);   // 用户状态
    query.bindValue(":avatar_url", avatar_url); // 头像链接



    if (!query.exec())
    {
        qWarning() << "注册账号：插入用户失败:" << query.lastError();
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
bool UserLoginAccountManager::logLogin(const QString &user_id, const QString &client_id)
{
    if (!database.isOpen())
    {
        qWarning() << "登录日志：数据库未打开";
        return false;
    }
    QString login_time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");   // 登录时间
    QString device_info = "";   // 登录设备
    QString location = "";  // 登录地理位置

    QSqlQuery query(database);
    query.prepare("INSERT INTO log_user_login (user_id, login_time, client_id, device_info, location) VALUES (:user_id, :login_time, :client_id, :device_info, :location)");
    query.bindValue(":user_id", user_id);
    query.bindValue(":login_time", login_time);
    query.bindValue(":client_id", client_id);
    query.bindValue(":device_info", device_info);
    query.bindValue(":location", location);

    if (!query.exec())
    {
        qWarning() << "登录日志：记录登录日志失败:" << query.lastError();
            return false;
    }
    return true;
}
