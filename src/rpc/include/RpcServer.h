#pragma once

#include "Config.h"
#include "TcpServer.h"
#include "google/protobuf/service.h"

class RpcServer
{
public:
    RpcServer(int numThreads);

    void registerService(google::protobuf::Service *);

    // 启动rpc服务节点
    void start();

private:
    // 新的socket连接回调
    void onConnection(const TcpConnectionSPtr &conn);
    // 已建立连接用户的读写回调
    void onMessage(const TcpConnectionSPtr &conn, Buffer *buf, Timestamp time);
    // Closure的回调，用于序列化rpc的响应和网络发送
    void sendRpcRespones(const TcpConnectionSPtr &, google::protobuf::Message *);
    void sendErrorResponse(const TcpConnectionSPtr &conn, const std::string &error_msg);
    void setThreadNum(int numThreads);

    EventLoop evtLoop_;
    TcpServer server_;
    struct ServiceInfo
    {
        google::protobuf::Service *service_;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> methodsMap_;
    };

    std::unordered_map<std::string, ServiceInfo> servicesMap_;
};
