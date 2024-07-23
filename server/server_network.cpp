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
                client_id = clients[socket];    // 获取备注的客户端id
            }
        }

        log_client_connect(client_id, client_ip_str, "disconnected"); // 添加链接日志：断开

        clients.remove(client_socket);   // 清理列表中的客户端
        client_socket->deleteLater();    // 对象会在当前执行的代码块结束后，以及任何待处理的事件（如已排队的信号和槽）处理完毕后，适时被删除
    }
}

// 读取服务器收到的数据
void ServerNetwork::onReadyRead()
{/* 只负责转发数据，不进行具体执行
  * 接收的数据：根据数据类型，将 json数据包 转发对应执行函数
  * 响应数据：接收执行函数返回的 json数据包，进行序列化后发送给客户端
  */
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

    QByteArray byte_array = client_socket->readAll(); // 收到数据

    // 将收到的数据添加到缓冲区中
    incompleteData[client_socket].append(byte_array);

    while (true)
    {
        QByteArray &buffer = incompleteData[client_socket];

        // 确认是否可以读取包头
        if (buffer.size() < sizeof(quint32))
        {
            qDebug() << "数据包太短，不够读取大小";
            break; // 数据不够，等待更多数据
        }

        QDataStream stream(buffer); // 序列化工具
        quint32 packet_size;    // 数据包大小
        stream.readRawData(reinterpret_cast<char*>(&packet_size), sizeof(packet_size)); // 读取数据包大小

        // 确认是否可以读取完整的数据包
        if (buffer.size() < static_cast<int>(packet_size))
        {
            qDebug() << "数据头保存大小" << packet_size << "实际大小" << buffer.size();
            break; // 数据不够，等待更多数据
        }

        // 提取一个完整的数据包
        QByteArray packet_data = buffer.mid(0, packet_size);
        buffer.remove(0, packet_size);  // 清除数据包

        // 反序列化数据包，然后发给处理客户端数据的函数
        Packet request = Packet::fromByteArray(packet_data);
        handlePacket(client_socket, request);
        break;
    }
}

// 数据包处理（根据数据包类型进行转发，然后接收和发送响应数据）
void ServerNetwork::handlePacket(QTcpSocket* client_socket, Packet request)
{
    Packet response;    // 响应数据包

// 如果是新的链接
    if (request.getType() == PacketType::CLIENT_CONNECT)
    {/* todo：暂时由此函数执行，后续考虑由日志类处理，或转发 同类的log_connect函数 */
        QString client_ip = client_socket->peerAddress().toString();   // 获取客户端ip然后转QString

        QString client_id = request.getJsonData()["client_id"].toString();    // 客户端id：获取数据包中的json数据包的字段

        for (auto it = clients.begin(); it != clients.end(); ++it)  // 遍历套接字，找到目标套接字后，添加客户端id作为备注
        {
            QTcpSocket* socket = it.key();  // 获取客户端链接列表的 键值对 的键：链接客户端的套接字
            if (client_socket == socket)
            {
                clients[socket] = client_id;    // 添加客户端id作为备注
            }
        }

        log_client_connect(client_id, client_ip, "connected"); // 添加日志内容 和 此条日志的活动：connected
        return; // 无响应数据
    }
// 如果是登录
    else if (request.getType() == PacketType::LOGIN)
    {
        QString client_ip = client_socket->peerAddress().toString();

        QJsonObject response_json = user_manager.validateUser(request.getJsonData(), client_ip);  // 由具体函数执行，返回响应数据
        response.setType(PacketType::LOGIN);    // 设置数据头
        response.setJsonData(response_json);    // 设置 json子数据包
        client_socket->write(response.toByteArray());  // 序列化后发射给客户端
    }
// 如果是注册
    else if (request.getType() == PacketType::RESISTER)
    {
        QString client_ip = client_socket->peerAddress().toString();

        QJsonObject response_json = user_manager.registerUser(request.getJsonData(), client_ip);  // 由具体函数执行，返回响应数据
        response.setType(PacketType::RESISTER);    // 设置数据头
        response.setJsonData(response_json);    // 设置 json子数据包
        client_socket->write(response.toByteArray());  // 序列化后发射给客户端
    }
//// 如果的第一次上传文件
    else if (request.getType() == PacketType::INITIAL_UPLOAD)
    {
        QJsonObject response_json = user_uploads.handleInitialUploadRequest(request.getJsonData());  // 由具体函数执行，返回响应数据
        response.setType(PacketType::INITIAL_UPLOAD);    // 设置数据头
        response.setJsonData(response_json);    // 设置 json子数据包
        client_socket->write(response.toByteArray());  // 序列化后发射给客户端
    }
//    // 如果是切片上传（断点续传）
    else if (request.getType() == PacketType::UPLOAD_CHUNK)
    {
        qDebug("文件切片");
        // 传入json数据包和文件数据
        QJsonObject response_json = user_uploads.uploadChunk(request.getJsonData(), request.getFileData());  // 由具体函数执行，返回响应数据
        if (response_json.isEmpty())
        {/* 执行次数过多，为了减少网络压力，不进行响应
          * 每隔一段时间 响应一个上传结果，返回缺失的文件块下标，文件ID
          */
            return;
        }
        response.setType(PacketType::UPLOAD_CHUNK);    // 设置数据头
        response.setJsonData(response_json);    // 设置 json子数据包
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
