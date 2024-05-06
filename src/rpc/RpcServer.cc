#include "Logger.h"
#include "zookeeperUtil.h"
#include "RpcServer.h"
#include "RpcHeader.pb.h"
#include <google/protobuf/descriptor.h>

RpcServer::RpcServer(int numThreads)
    : server_(&evtLoop_, InetAddress{static_cast<uint16_t>(atoi(Config::getCfg("serv_port").c_str())), Config::getCfg("serv_ip")}, "RpcServer")
{
    server_.setConnectionCallback(std::bind(&RpcServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(std::bind(&RpcServer::onMessage, this,
                                         std::placeholders::_1,
                                         std::placeholders::_2,
                                         std::placeholders::_3));
    setThreadNum(numThreads);
}

void RpcServer::setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

/**
 *                                    / Service* 服务对象指针
 * servicesMap:servName->serviceInfo |                                    / servDesc->method(0) 该服务提供的第i个方法
 *                                    \ methodMap:methodName->methodDesc |  ...
 *                                                                        \ servDesc->method(methodCnt-1)
 */
void RpcServer::registerService(google::protobuf::Service *serv)
{
    ServiceInfo servInfo;
    servInfo.service_ = serv;

    const google::protobuf::ServiceDescriptor *servDesc = serv->GetDescriptor();
    std::string servName = servDesc->name();
    int methodCnt = servDesc->method_count();

    LOG_INFO("service_name:%s has %d methods", servName.c_str(), methodCnt);

    for (int i = 0; i < methodCnt; i++)
    {
        const google::protobuf::MethodDescriptor *methodDesc = servDesc->method(i);
        std::string methodName = methodDesc->name();
        servInfo.methodsMap_[methodName] = methodDesc;
        LOG_INFO("service_name:%s,No.%d method_name:%s", servName.c_str(), i + 1, methodName.c_str());
    }
    servicesMap_[servName] = servInfo;
}

void RpcServer::start()
{
    // 在zookeeper上注册所有服务
    // FIXME:注册服务设置为定期任务，定期向zookeeper汇报本机的cpu和内存使用量
    std::string ip = Config::getCfg("serv_ip");
    std::string port = Config::getCfg("serv_port");
    std::string addr = ip + ":" + port;

    ZkClient zkCli;
    zkCli.start(ZooLogLevel::ZOO_LOG_LEVEL_ERROR);
    // service_name 为永久性节点 method_name 为临时性节点
    for (const auto &[serv_name, serv_info] : servicesMap_)
    {
        std::string serv_path = "/" + serv_name;
        zkCli.create(serv_path.c_str(), nullptr, -1);
        for (const auto &[method_name, _] : serv_info.methodsMap_)
        {
            std::string method_Ppath = serv_path + "/" + method_name;
            zkCli.create(method_Ppath.c_str(), nullptr, -1);
            std::string method_Tpath = serv_path + "/" + method_name + "/" + method_name;
            zkCli.create(method_Tpath.c_str(), addr.c_str(), addr.size(), ZOO_EPHEMERAL | ZOO_SEQUENCE);
        }
    }
    LOG_INFO("RpcServer start service at address[%s]", addr.c_str());

    server_.start();
    evtLoop_.loop();
}

void RpcServer::onConnection(const TcpConnectionSPtr &conn)
{
    if (conn->isConnected())
        LOG_INFO("Connection UP : %s", conn->getPeerAddress().toIpPort().c_str());
    else
        conn->shutdown();
}

void RpcServer::onMessage(const TcpConnectionSPtr &conn, Buffer *buffer, Timestamp time)
{
    // 接收网络传输的远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllToString();
    // 从字符流前4个字节中读出协议头长度
    uint32_t header_size = 0;
    recv_buf.copy(reinterpret_cast<char *>(&header_size), 4, 0);

    std::string header_str = recv_buf.substr(4, header_size);

    tinyRPC::RpcHeader rpcHeader;
    tinyRPC::MessageType rpcType;
    std::string servName, methodName;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(header_str))
    {
        // 数据头反序列化成功
        rpcType = rpcHeader.type();
        servName = rpcHeader.service_name();
        methodName = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        char buf[512] = {0};
        sprintf(buf, "header_str:%s parse failed!", header_str.c_str());
        sendErrorResponse(conn, buf);
        LOG_ERROR("header_str:%s parse failed!", header_str.c_str());
        return;
    }

    if (rpcType != tinyRPC::MessageType::RPC_TYPE_REQUEST)
    {
        sendErrorResponse(conn, "rpc is not a request!");
        LOG_ERROR("rpc is not a request!");
        return;
    }

    // 获取rpc参数的字符流
    std::string args_str = recv_buf.substr(header_size + 4, args_size);

    // std::cout << "============================================" << std::endl;
    // std::cout << "header_size: " << header_size << std::endl;
    // std::cout << "rpc_header_str: " << header_str << std::endl;
    // std::cout << "service_name: " << servName << std::endl;
    // std::cout << "method_name: " << methodName << std::endl;
    // std::cout << "args_str: " << args_str << std::endl;
    // std::cout << "============================================" << std::endl;

    auto it = servicesMap_.find(servName);
    if (it == servicesMap_.end())
    {
        char buf[512] = {0};
        sprintf(buf, "service:%s not exist!", servName.c_str());
        sendErrorResponse(conn, buf);
        LOG_ERROR("service:%s not exist!", servName.c_str());
        return;
    }
    /* FIXME:已有service->GetDescriptor()->FindMethodByName("Foo");
             可以直接FindMethodByName，可以省略method
    */
    const RpcServer::ServiceInfo &servInfo = it->second;
    auto mit = servInfo.methodsMap_.find(methodName);
    if (mit == servInfo.methodsMap_.end())
    {
        char buf[512] = {0};
        sprintf(buf, "method:%s not exist!", methodName.c_str());
        sendErrorResponse(conn, buf);
        LOG_ERROR("method:%s not exist!", methodName.c_str());
        return;
    }

    google::protobuf::Service *serv = servInfo.service_;                // 获取service对象
    const google::protobuf::MethodDescriptor *methodDesc = mit->second; // 获取method对象

    google::protobuf::Message *request = serv->GetRequestPrototype(methodDesc).New();
    google::protobuf::Message *response = serv->GetResponsePrototype(methodDesc).New();
    if (!request->ParseFromString(args_str))
    {
        char buf[512] = {0};
        sprintf(buf, "request parse failed,content:%s", args_str.c_str());
        sendErrorResponse(conn, buf);
        LOG_ERROR("request parse failed,content:%s", args_str.c_str());
        return;
    }

    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcServer,
                                                                    const TcpConnectionSPtr &,
                                                                    google::protobuf::Message *>(this, &RpcServer::sendRpcRespones,
                                                                                                 conn,
                                                                                                 response);
    serv->CallMethod(methodDesc, nullptr, request, response, done);
}

// Closure的回调操作，用于序列化rpc的响应和网络发送
void RpcServer::sendRpcRespones(const TcpConnectionSPtr &conn, google::protobuf::Message *response)
{
    // 序列化响应内容
    uint32_t args_size = 0;
    std::string args_str;
    if (response->SerializeToString(&args_str))
        args_size = args_str.size();
    else
    {
        sendErrorResponse(conn, "serialize response failed!");
        LOG_ERROR("serialize response failed!");
        return;
    }

    // 序列化rpc头
    tinyRPC::RpcHeader rpcHeader;
    rpcHeader.set_type(tinyRPC::MessageType::RPC_TYPE_RESPONSE);
    rpcHeader.set_error(tinyRPC::ErrorCode::RPC_ERR_OK);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string header_str;
    if (rpcHeader.SerializeToString(&header_str))
        header_size = header_str.size();
    else
    {
        sendErrorResponse(conn, "serialize rpc header failed!");
        LOG_ERROR("serialize rpc header failed!");
        return;
    }

    // 连接待发送的rpc响应字符串
    std::string send_rpc_str;
    send_rpc_str = std::string(reinterpret_cast<char *>(&header_size), 4);
    send_rpc_str += header_str + args_str;

    conn->send(send_rpc_str);

    conn->shutdown(); // 模拟http的短链接服务，由RpcServer主动断开连接
}

void RpcServer::sendErrorResponse(const TcpConnectionSPtr &conn, const std::string &error_msg)
{
    tinyRPC::RpcHeader rpcHeader;
    rpcHeader.set_type(tinyRPC::MessageType::RPC_TYPE_ERROR);

    uint32_t header_size = 0;
    std::string header_str;
    if (rpcHeader.SerializeToString(&header_str))
        header_size = header_str.size();
    else
    {
        sendErrorResponse(conn, "serialize rpc header failed!");
        LOG_ERROR("serialize rpc header failed!");
        return;
    }

    // 连接待发送的rpc响应字符串
    std::string send_rpc_str;
    send_rpc_str = (std::string((char *)&header_size), 4);
    send_rpc_str += header_str + error_msg;

    conn->send(send_rpc_str);

    conn->shutdown();
}