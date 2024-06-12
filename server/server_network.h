#ifndef SERVERNETWORK_H
#define SERVERNETWORK_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include "../user/user_login_account_manager.h" // 用户登录管理
#include "../data/user_uploads/useruploads.h"   // 用户上传数据

// 与客户端通信管理
// 在此文件管理所有与客户端的交互，然后交给对象的对应功能进行处理
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
    UserUploads useruploads;    // 图片上传管理

    QMap<QTcpSocket*, QString> clients; // 管理已经连接的客户端列表，客户端连接后，创建新的tcp指针，并保存在此map
};

#endif // SERVERNETWORK_H
