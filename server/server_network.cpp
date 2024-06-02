#include "./server_network.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDataStream>

ServerNetwork::ServerNetwork(QObject *parent) :
    QTcpServer(parent)
{

}

void ServerNetwork::setDatabase(const QSqlDatabase &db)
{
    userManager.setDatabase(db);
}

// 传入的连接
// 这个函数是当服务器接收到一个新的客户端连接请求时，由QTcpServer自动调用的
// qintptr是一个Qt中用于表示指针或整数的类型，它保证了在32位和64位系统上的移植性
// socketDescriptor是一个整数，代表了新连接的套接字描述符。这个描述符是由操作系统提供的，用于唯一标识这个网络连接。
void ServerNetwork::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *clientSocket = new QTcpSocket(this);    // 将当前对象传入，保证生命周期相同
    // 用于将一个已存在的套接字描述符（socketDescriptor）赋予一个QTcpSocket对象（clientSocket）。
    // 这样做可以让QTcpSocket接管一个已经由操作系统创建并配置好的套接字，而不是通过QTcpSocket自身建立一个新的连接
    // 这一步骤允许服务器为每个新连接创建单独的处理逻辑，从而能够同时处理多个客户端的连接请求。
    if (!clientSocket->setSocketDescriptor(socketDescriptor))
    {// 如果设置失败，释放这个对象，并结束函数
        delete clientSocket;
        return;
    }

    // 服务器接收到数据
    connect(clientSocket, &QTcpSocket::readyRead, this, &ServerNetwork::onReadyRead);
    // 客户端断开连接
    connect(clientSocket, &QTcpSocket::disconnected, this, &ServerNetwork::onDisconnected);

    // 将连接的客户端，加入客户端列表
    qDebug("新客户端链接成功");
    clients[clientSocket] = "user1";
}

// 读取服务器收到的数据
void ServerNetwork::onReadyRead()
{
    qDebug("服务器收到数据");
    // 获取 发射信号激活此槽函数 的对象
    // 因为使用了一个列表管理连接的所有客户端，所以无法确定是那个客户端关联的tcp对象发射的信号

    // sender()：用于返回发出当前正在处理的信号的对象
    // qobject_cast<QTcpSocket>(sender()) 如果sender()返回的对象是一个（或派生自）QTcpSocket类型的对象，这个转换就会成功，并返回指向那个QTcpSocket对象的指针。
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket)
    {    // 如果sender()不是期望的类型，该操作将返回nullptr，因此在使用转换结果前，通常需要检查是否为nullptr以避免程序崩溃。
        return;
    }

    QByteArray data = clientSocket->readAll();  // 获取客户端发送的数据
    // 将一个包含JSON数据的 data解析成一个 QJsonDocument对象
    QJsonDocument requestDoc = QJsonDocument::fromJson(data);
    QJsonObject request = requestDoc.object();

    QString type = request["type"].toString();
    QJsonObject response;
// 如果是登录
    if (type == "LOGIN")
    {
        QString username = request["username"].toString();
        QString password = request["password"].toString();

        if (userManager.validateUser(username, password))
        {
            response["status"] = "success";
            response["message"] = "登录成功";
            // 获取客户端 IP 地址
            QString clientIp = clientSocket->peerAddress().toString();
            userManager.logLogin(username, clientIp);
        }
        else
        {
            response["status"] = "failure";
            response["message"] = "检查你的账号或密码";
        }
    }
// 如果是注册
    else if (type == "register")
    {
        QString username = request["username"].toString();
        QString password = request["password"].toString();
        QString email = request["email"].toString();

        if (userManager.registerUser(username, password, email))
        {
            response["status"] = "success";
            response["message"] = "注册成功";
        }
        else
        {
            response["status"] = "failure";
            response["message"] = "注册失败";
        }
    }

    QJsonDocument responseDoc(response);    // 将json对象转换回QJsonDocument对象，来方便发射数据
    clientSocket->write(responseDoc.toJson());  // 客户管理的发射函数
}

// 链接断开
void ServerNetwork::onDisconnected()
{
    // 获取 发射信号激活此槽函数 的对象
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());

    if (clientSocket)
    {
        clients.remove(clientSocket);   // 清理列表中的客户端
        clientSocket->deleteLater();    // 对象会在当前执行的代码块结束后，以及任何待处理的事件（如已排队的信号和槽）处理完毕后，适时被删除
    }
}

