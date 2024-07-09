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
    userManager(db)
{

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
    clients[clientSocket] = clientSocket->peerAddress().toString();; // 备注置空
}

// 读取服务器收到的数据
void ServerNetwork::onReadyRead()
{
// 获取 发射信号激活此槽函数 的对象
    // 因为使用了一个列表管理连接的所有客户端，所以无法确定是那个客户端关联的tcp对象发射的信号
    // sender()：用于返回发出当前正在处理的信号的对象
    // qobject_cast<QTcpSocket>(sender()) 如果sender()返回的对象是一个（或派生自）QTcpSocket类型的对象，这个转换就会成功，并返回指向那个QTcpSocket对象的指针。
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket)
    {    // 如果sender()不是期望的类型，该操作将返回nullptr，因此在使用转换结果前，通常需要检查是否为nullptr以避免程序崩溃。
        return;
    }

// 获取并解析客户端发送的数据
    QByteArray data = clientSocket->readAll();
    // 将一个包含JSON数据的 data解析成一个 QJsonDocument对象
    QJsonParseError parseError;
    QJsonDocument requestDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        return;
    }
    QJsonObject request = requestDoc.object();  // 解析成 json对象

    QJsonObject response;   // 服务器响应数据

// 如果是新的链接
    if (request["type"].toString() == "CONNECT")
    {
        QHostAddress client_ip = clientSocket->peerAddress();    // 获取客户端ip
        QString client_ip_str = client_ip.toString();
        QString client_id = request["client_id"].toString();    // 客户端id

        for (auto it = clients.begin(); it != clients.end(); ++it)  // 遍历套接字，找到目标套接字后，添加客户端id作为备注
        {
            QTcpSocket* socket = it.key();
            if (clientSocket == socket)
            {
                clients[socket] = client_id;
                qDebug() << "新链接：" << clients;
            }
        }

        log_connect(client_id, client_ip_str); // 添加链接日志
        return;
    }
// 如果是登录
    else if (request["type"].toString() == "LOGIN")
    {
        response["type"] = "LOGIN";
        QString username = request["username"].toString();
        QString password = request["password"].toString();

        if (userManager.validateUser(username, password))
        {
            // 获取客户端 IP 地址
            QString clientIp = clientSocket->peerAddress().toString();
            userManager.logLogin(username, clientIp);
            // 生成一个令牌，返回
            // 令牌单次链接有效
            response["status"] = "SUCCESS"; // 结果
            response["message"] = "登录成功";
        }
        else
        {
            response["status"] = "FAILURE";
            response["message"] = "检查你的账号或密码";
        }
    }
// 如果是注册
    else if (request["type"].toString() == "RESISTER")
    {
        response["type"] = "RESISTER";
        QString username = request["username"].toString();
        QString password = request["password"].toString();
        QString email = request["email"].toString();

        if (userManager.registerUser(username, password, email))
        {
            response["status"] = "SUCCESS";
            response["message"] = "注册成功";
        }
        else
        {
            response["status"] = "FAILURE";
            response["message"] = "注册失败";
        }
    }
    // 如果的第一次上传文件
    else if (request["type"].toString() == "INITIAL_UPLOAD")
    {
        response["type"] = "INITIAL_UPLOAD";
        QString fileName = request["filename"].toString();
        qint64 totalSize = request["totalSize"].toInt();
        QString userId = request["userId"].toString();
        QString clientId = request["clientId"].toString();

        QString file_id = useruploads.handleInitialUploadRequest(fileName, totalSize, userId, clientId);    // 返回生成的文件id
        response["file_id"] = file_id;
    }
    // 如果是切片上传（断点续传）
    else if (request["type"].toString() == "UPLOAD_CHUNK")
    {
        response["type"] = "UPLOAD_CHUNK";
        QString file_id = request["file_id"].toString();  // 原始文件名
        QString base64Data = request["filedata"].toString();    // 文件块数据
        qint64 offset = request["offset"].toInt();  // 已发送文件大小
        qint64 totalSize = request["totalSize"].toInt();    // 文件总大小
        QString user_id = request["userId"].toString(); // 用户id

        useruploads.uploadChunk(file_id, base64Data, offset, totalSize, user_id);  // 传递给处理函数
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

void ServerNetwork::log_connect(QString client_id, QString client_ip)
{
    QDateTime connection_time = QDateTime::currentDateTime();   // 获取当前日期和时间的 QDateTime 对象
    QString connection_time_str = connection_time.toString("yyyy-MM-dd HH:mm:ss");  // 时间转换成字符串

    if (!database.isOpen())
    {
        qWarning() << "数据库未打开";
        return;
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO log_client_connection (client_user_id, connection_time, ip_address, status) VALUES (:client_id, :connection_time, :ip_address, :status)");
    query.bindValue(":client_id", client_id);
    query.bindValue(":connection_time", connection_time_str);
    query.bindValue(":ip_address", client_ip);
    query.bindValue(":status", "connected");

    if (!query.exec())
    {
        qWarning() << "server/server_network：插入连接日志失败:" << query.lastError();
        return;
    }
}
