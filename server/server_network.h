#ifndef SERVERNETWORK_H
#define SERVERNETWORK_H

#include <QTcpServer>
#include <QTcpSocket>
#include "../user/user_login_account_manager.h"
#include <QSqlDatabase>

// 与客户端通信管理
class ServerNetwork : public QTcpServer
{
    Q_OBJECT

public:
    explicit ServerNetwork(QObject *parent = nullptr);
    void setDatabase(const QSqlDatabase &db);   // 设置数据库

protected:
    void incomingConnection(qintptr socketDescriptor) override; // 传入的连接，override：显式重写基类的成员的标记

private slots:
    void onReadyRead(); // 准备读取数据
    void onDisconnected();  // 关闭连接

private:
    UserLoginAccountManager userManager; // 用户登录管理
    QMap<QTcpSocket*, QString> clients; // 管理已经连接的客户端列表，客户端连接后，创建新的tcp指针，并保存在此map
};

#endif // SERVERNETWORK_H
