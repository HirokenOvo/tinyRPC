#include "RpcChannel.h"
#include "RpcHeader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller,
                            const google::protobuf::Message *request,
                            google::protobuf::Message *response,
                            google::protobuf::Closure *done)
{
    const google::protobuf::ServiceDescriptor *servDesc = method->service();
    std::string service_name = servDesc->name();
    std::string method_name = method->name();

    // 序列化请求内容
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
        args_size = args_str.size();
    else
    {
        controller->SetFailed("Serialize Request Failed!");
        return;
    }

    // 序列化rpc头
    tinyRPC::RpcHeader rpcHeader;
    rpcHeader.set_type(tinyRPC::MessageType::RPC_TYPE_REQUEST);
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_error(tinyRPC::ErrorCode::RPC_ERR_OK);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string header_str;
    if (rpcHeader.SerializeToString(&header_str))
        header_size = header_str.size();
    else
    {
        controller->SetFailed("Serialize Request Rpc Header Failed!");
        return;
    }

    // 连接待发送的rpc请求字符串
    std::string send_rpc_str = std::string(reinterpret_cast<char *>(&header_size), 4);
    send_rpc_str += header_str + args_str;

    // std::cout << "============================================" << std::endl;
    // std::cout << "header_size: " << header_size << std::endl;
    // std::cout << "rpc_header_str: " << header_str << std::endl;
    // std::cout << "service_name: " << service_name << std::endl;
    // std::cout << "method_name: " << method_name << std::endl;
    // std::cout << "args_str: " << args_str << std::endl;
    // std::cout << send_rpc_str.size() << std::endl;
    // std::cout << "============================================" << std::endl;

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        char buf[512] = {0};
        sprintf(buf, "Create Socket Failed! Errno:%d", errno);
        controller->SetFailed(buf);
        return;
    }

    /*
    FIXME:zookeeper获取服务节点的ip:port
    */
    std::string ip = "127.0.0.1";
    uint16_t port = 8000;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    server_addr.sin_port = htons(port);

    // 连接rpc服务节点
    if (connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        close(clientfd);
        char buf[512] = {0};
        sprintf(buf, "Connect Failed! Errno:%d", errno);
        controller->SetFailed(buf);
        return;
    }

    // 发送请求
    if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1)
    {
        close(clientfd);
        char buf[512] = {0};
        sprintf(buf, "Send Failed! Errno:%d", errno);
        controller->SetFailed(buf);
        return;
    }

    // 接收响应值
    char buf[1024] = {0};
    int recv_size = 0;
    if ((recv_size = recv(clientfd, buf, 1024, 0)) == -1)
    {
        close(clientfd);
        char buf[512] = {0};
        sprintf(buf, "Recv Failed! Errno:%d", errno);
        controller->SetFailed(buf);
        return;
    }
    // 从字符流前4个字节中读出协议头长度
    std::string recv_buf = std::string(buf, recv_size);
    header_size = 0;
    recv_buf.copy(reinterpret_cast<char *>(&header_size), 4, 0);

    header_str = recv_buf.substr(4, header_size);
    tinyRPC::MessageType rpcType;

    if (rpcHeader.ParseFromString(header_str))
        rpcType = rpcHeader.type();
    else
    {
        close(clientfd);
        char buf[512] = {0};
        sprintf(buf, "Response Header_str:%s Parse Failed!", header_str.c_str());
        controller->SetFailed(buf);
        return;
    }

    if (rpcType == tinyRPC::MessageType::RPC_TYPE_ERROR)
    {
        // FIXME: 考虑递归重传,记录cnt重传次数
        controller->SetFailed("Rpc Request Error!");
        return;
    }

    if (rpcType == tinyRPC::MessageType::RPC_TYPE_RESPONSE)
    {
        uint32_t args_size = rpcHeader.args_size();
        std::string args_str = recv_buf.substr(header_size + 4, args_size);
        // 反序列化接收到的数据
        if (!response->ParseFromArray(args_str.c_str(), args_size))
        {
            close(clientfd);
            char buf[512] = {0};
            sprintf(buf, "Response Parse Failed! Response_str:%s", buf);
            controller->SetFailed(buf);
            return;
        }
    }

    close(clientfd);
}