#ifndef SERVERNETWORK_H
#define SERVERNETWORK_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QtSql/QSqlQuery>
#include "../data/packet.h" // 自定义数据包
// 处理客户端信息
#include "../user/user_login_account_manager.h" // 用户登录管理
#include "../data/user_uploads/useruploads.h"   // 用户上传数据

// 与客户端通信管理
// 在此文件管理所有与客户端的交互，然后交给对象的对应功能进行处理
class ServerNetwork : public QTcpServer
{
    Q_OBJECT

public:
    explicit ServerNetwork(const QSqlDatabase &db, QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr socket_descriptor) override; // 传入的连接，override：显式重写基类的成员的标记

private slots:
    void onReadyRead(); // 读取客户端发来的数据包
    void onDisconnected();  // 关闭连接

private:
    void handlePacket(QTcpSocket* client_socket, Packet request);   // 处理客户端上传数据
    void log_client_connect(QString client_id, QString client_ip, QString status); // 添加客户端日志：链接或断开，参数：客户端ID，客户端IP，状态（链接或断开）

    UserLoginAccountManager user_manager; // 用户登录管理
    UserUploads user_uploads;    // 文件上传管理

    QSqlDatabase database;   // MySQL 数据库连接
    QMap<QTcpSocket*, QString> clients; // 管理已经连接的客户端列表，客户端连接后，创建新的tcp指针，并保存在此map

    QHash<QTcpSocket*, QByteArray> incompleteData;  // 用于存储所有 客户端接收的 数据包列表
};

#endif // SERVERNETWORK_H
