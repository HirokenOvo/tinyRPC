#include "RpcChannel.h"
#include "RpcHeader.pb.h"
#include "zookeeperUtil.h"
#include "WriteFstMp.h"
#include "LockQueue.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>

// static WriteFstMp<std::string, std::unique_ptr<LockQueue<std::string>>> host_list_cache;
static WriteFstMp<std::string, struct String_vector> host_list_cache;

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            RpcController *controller,
                            const google::protobuf::Message *request,
                            google::protobuf::Message *response,
                            google::protobuf::Closure *done,
                            int cnt)
{
    if (cnt)
    {
        printf("Rpc call %dth Failed,trying reconnecting...\n", cnt);
        std::cout << controller->ErrorText() << std::endl;
        switch (cnt)
        {
        case 1:
            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        case 2:
            std::this_thread::sleep_for(std::chrono::seconds(4));
            break;
        case 3:
            std::this_thread::sleep_for(std::chrono::seconds(8));
            break;
        default:
            if (controller)
                controller->SetMsg(controller->ErrorText());
            return;
        }
    }
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
        if (controller)
            controller->SetMsg("Serialize Request Failed!");
        CallMethod(method, controller, request, response, done, cnt + 1);
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
        if (controller)
            controller->SetMsg("Serialize Request Rpc Header Failed!");
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }

    // 连接待发送的rpc请求字符串
    std::string send_rpc_str = std::string(reinterpret_cast<char *>(&header_size), 4);
    send_rpc_str += header_str + args_str;

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        char buf[512] = {0};
        sprintf(buf, "Create Socket Failed! Errno:%d", errno);
        if (controller)
            controller->SetMsg(buf);
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }

    std::string method_path = "/" + service_name + "/" + method_name;
    auto linkZk = [&method_path]() -> ZkClient &
    {
        static ZkClient zkCli;
        static std::atomic_bool flag = 0;

        if (!flag)
        {
            flag = 1;
            zkCli.start(ZooLogLevel::ZOO_LOG_LEVEL_ERROR);
        }
        while (!zkCli.exist())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return zkCli;
    };

    auto &zkCli = linkZk();
    auto getHostData = [&zkCli, &method_path](WriteFstMp<std::string, struct String_vector> &cache) -> std::string
    {
        if (!cache.exist(method_path))
            zkCli.getChildrenList(method_path.c_str(), &cache);

        const auto strVec = cache.get(method_path);
        int idx = rand() % strVec.count;
        std::string path = method_path + "/" + std::string(strVec.data[idx]);
        return zkCli.getData(path.c_str());
    };
    std::string host_data = getHostData(host_list_cache);

    if (host_data == "")
    {
        if (controller)
            controller->SetMsg(method_path + " is not exist!");
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }
    int idx = host_data.find(":");
    if (idx == std::string::npos)
    {
        if (controller)
            controller->SetMsg(method_path + " address is invalid!");
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }

    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1).c_str());

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
        if (controller)
            controller->SetMsg(buf);
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }

    // 发送请求
    if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1)
    {
        close(clientfd);
        char buf[512] = {0};
        sprintf(buf, "Send Failed! Errno:%d", errno);
        if (controller)
            controller->SetMsg(buf);
        CallMethod(method, controller, request, response, done, cnt + 1);
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
        if (controller)
            controller->SetMsg(buf);
        CallMethod(method, controller, request, response, done, cnt + 1);
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
        if (controller)
            controller->SetMsg(buf);
        CallMethod(method, controller, request, response, done, cnt + 1);
        return;
    }

    if (rpcType == tinyRPC::MessageType::RPC_TYPE_ERROR)
    {
        // FIXME: 考虑递归重传,记录cnt重传次数
        if (controller)
            controller->SetMsg("Rpc Request Error!");
        CallMethod(method, controller, request, response, done, cnt + 1);
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
            if (controller)
                controller->SetMsg(buf);
            CallMethod(method, controller, request, response, done, cnt + 1);
            return;
        }
    }

    close(clientfd);
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller,
                            const google::protobuf::Message *request,
                            google::protobuf::Message *response,
                            google::protobuf::Closure *done)
{

    CallMethod(method, dynamic_cast<RpcController *>(controller), request, response, done, 0);
}