#include "./server_network.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDataStream>
#include <QSqlError>
#include <QFile>

ServerNetwork::ServerNetwork(const QSqlDatabase &db, QObject *parent) :
    QTcpServer(parent),
    database(db),
    user_manager(db),
    user_uploads(db)
{

}

// 传入的连接
/* 这个函数是当服务器接收到一个新的客户端连接请求时，由QTcpServer自动调用的
 * qintptr是一个Qt中用于表示指针或整数的类型，它保证了在32位和64位系统上的移植性
 * socketDescriptor是一个整数，代表了新连接的套接字描述符。这个描述符是由操作系统提供的，用于唯一标识这个网络连接。
 */
void ServerNetwork::incomingConnection(qintptr socket_descriptor)
{
    QTcpSocket *client_socket = new QTcpSocket(this);    // 将当前对象传入，保证生命周期相同

    // 创建套接字的副本，来进行后续操作
    if (!client_socket->setSocketDescriptor(socket_descriptor))
    {/* 用于将一个已存在的套接字描述符（socketDescriptor）赋予一个QTcpSocket对象（clientSocket）。
     * 这样做可以让QTcpSocket接管一个已经由操作系统创建并配置好的套接字，而不是通过QTcpSocket自身建立一个新的连接
     * 这一步骤允许服务器为每个新连接创建单独的处理逻辑，从而能够同时处理多个客户端的连接请求。
     */
        // 如果设置失败，释放这个对象，并结束函数
        delete client_socket;
        return;
    }

    // 服务器接收到数据
    connect(client_socket, &QTcpSocket::readyRead, this, &ServerNetwork::onReadyRead);
    // 客户端断开连接
    connect(client_socket, &QTcpSocket::disconnected, this, &ServerNetwork::onDisconnected);

    // 将连接的客户端，加入客户端列表
    qDebug("新客户端链接成功");
    clients[client_socket] = ""; // 备注：获取连接到此客户端套接字（即远程客户端）的 IP 地址
}

// 读取服务器收到的数据
void ServerNetwork::onReadyRead()
{/* 只负责转发数据，不进行具体执行
  * 接收的数据：根据数据类型，将 json数据包 转发对应执行函数
  * 响应数据：接收执行函数返回的 json数据包，进行序列化后发送给客户端
  */
    qDebug("服务端收到消息");
    // 获取 发射信号激活此槽函数 的对象
    QTcpSocket *client_socket = qobject_cast<QTcpSocket*>(sender());
    /* 因为使用了一个列表管理连接的所有客户端，所以无法确定是那个客户端关联的tcp对象发射的信号
     * sender()：用于返回发出当前正在处理的信号的对象
     * qobject_cast<QTcpSocket>(sender()) 如果sender()返回的对象是一个（或派生自）QTcpSocket类型的对象，这个转换就会成功，并返回指向那个QTcpSocket对象的指针。
     * 如果sender()不是期望的类型，该操作将返回nullptr，因此在使用转换结果前，通常需要检查是否为nullptr以避免程序崩溃。
     */

    if (!client_socket)
    {
        return;
    }

    // 获取并解析客户端发送的数据
    QByteArray data = client_socket->readAll();

    // 将一个包含JSON数据的 data解析成一个 QJsonDocument对象
    QJsonParseError parse_error;
    QJsonDocument request_doc = QJsonDocument::fromJson(data, &parse_error);

    if (parse_error.error != QJsonParseError::NoError)
    {
        qDebug() << "服务端套接字：上传数据的 JSON数据解析失败:" << parse_error.errorString();
        return;
    }

    QJsonObject request = request_doc.object();  // 接收的数据 解析成 json对象
    QJsonObject response;   // 服务端响应给客户端的数据

// 如果是新的链接
    if (request["type"].toString() == "CONNECT")
    {// todo：暂时由此函数执行，后续考虑由日志类处理，或转发 同类的log_connect函数
        QHostAddress client_ip = client_socket->peerAddress();    // 获取客户端ip
        QString client_ip_str = client_ip.toString();   // 将ip转QString
        QString client_id = request["client_id"].toString();    // 客户端id

        for (auto it = clients.begin(); it != clients.end(); ++it)  // 遍历套接字，找到目标套接字后，添加客户端id作为备注
        {
            QTcpSocket* socket = it.key();  // 获取客户端链接列表的 键值对 的键：链接客户端的套接字
            if (client_socket == socket)
            {
                clients[socket] = client_id;    // 添加客户端id作为备注
            }
        }
        log_client_connect(client_id, client_ip_str, "connected"); // 添加链接日志
        return; // 无响应数据
    }
// 如果是登录
    else if (request["type"].toString() == "LOGIN")
    {
        QString client_ip = client_socket->peerAddress().toString();

        response = user_manager.validateUser(request, client_ip);  // 由具体函数执行，返回响应数据
    }
// 如果是注册
    else if (request["type"].toString() == "RESISTER")
    {
        QString client_ip = client_socket->peerAddress().toString();

        response = user_manager.registerUser(request, client_ip);  // 由具体函数执行，返回响应数据
    }
// 如果的第一次上传文件
    else if (request["type"].toString() == "INITIAL_UPLOAD")
    {
        response = user_uploads.handleInitialUploadRequest(request);  // 由具体函数执行，返回响应数据
    }
    // 如果是切片上传（断点续传）
    else if (request["type"].toString() == "UPLOAD_CHUNK")
    {
        response = user_uploads.uploadChunk(request);  // 由具体函数执行，返回响应数据
        return; // 执行数量过多，出于网络数据包考虑，不进行响应
    }

    // 序列化响应数据，发送给客户端
    QJsonDocument response_doc(response);
    QByteArray message = response_doc.toJson();
    client_socket->write(message);  // 客户管理的发射函数
}

// 链接断开
void ServerNetwork::onDisconnected()
{
    // 获取 发射信号激活此槽函数 的对象
    QTcpSocket *client_socket = qobject_cast<QTcpSocket*>(sender());

    if (client_socket)
    {
        QString client_ip_str = client_socket->peerAddress().toString();   // 获取客户端ip转QString

        QString client_id;  // 客户端ID
        for (auto it = clients.begin(); it != clients.end(); ++it)  // 遍历套接字列表，找到目标套接字后，获取备注的客户端id
        {
            QTcpSocket* socket = it.key();  // 获取客户端链接列表的 键值对 的键：链接客户端的套接字
            if (client_socket == socket)
            {
                client_id = clients[socket];    // 添加客户端id作为备注
            }
        }

        log_client_connect(client_id, client_ip_str, "disconnected"); // 添加链接日志：断开

        clients.remove(client_socket);   // 清理列表中的客户端
        client_socket->deleteLater();    // 对象会在当前执行的代码块结束后，以及任何待处理的事件（如已排队的信号和槽）处理完毕后，适时被删除
    }
}

// 客户端链接日志
void ServerNetwork::log_client_connect(QString client_id, QString client_ip, QString status)
{
    QString now_time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");  // 获取当前日期和时间转换成字符串

    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO log_client_connection (client_id, now_time, ip_address, status) VALUES (:client_id, :now_time, :ip_address, :status)");
    query.bindValue(":client_id", client_id);
    query.bindValue(":now_time", now_time);
    query.bindValue(":ip_address", client_ip);
    query.bindValue(":status", status);

    if (!query.exec())
    {
        qWarning() << "server/server_network：插入连接日志失败:" << query.lastError();
        return;
    }
}
