#include <QCoreApplication>
// 使用 shell 脚本
//#include <QProcess>
// 服务器相关
#include "./server/server_network.h"
// 数据库相关
#include <QtSql/QSqlDatabase>
#include <QSqlError>

void server_init(ServerNetwork &server);   // 服务器初始化
void sql_link(QSqlDatabase &db);    // 数据库连接

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    sql_link(db);

    ServerNetwork server(db);
    server_init(server);

    return a.exec();
}

void server_init(ServerNetwork &server)
{
    if (!server.listen(QHostAddress::Any, 1234))
    {
        qCritical() << "启动服务器失败:" << server.errorString();
        exit(1);
    }
}

void sql_link(QSqlDatabase &db)
{
    db.setHostName("127.0.0.1");    // 数据库地址
    db.setPort(3306);   // 数据库端口
    db.setDatabaseName("poor"); // 数据库名
    db.setUserName("root"); // 用户名
    db.setPassword("159357");   // 密码

    if (!(db.open()))
    {
        qDebug()<< "数据库打开失败："<<db.lastError().text();
        exit(1);
    }
}

//void shell(/*QString name, QString email, QString captcha*/)
//{
//    QString name = "rhfgxg";
//    QString email = "m13411806653@163.com";
//    QString captcha = "123456";
//    QString url = "/home/lhw/bin/HearthStone/email_captcha_enroll.sh";  // 注册验证码

//    QStringList urll;
//    urll << url << name << email << captcha;

//    QProcess* p = new QProcess();
//    p->start("/bin/bash", urll);
//}
