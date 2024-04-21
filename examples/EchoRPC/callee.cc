#include "example.pb.h"
#include "RpcServer.h"
#include <sstream>

class foo : public example::ServiceRpc
{
public:
    void Get(::google::protobuf::RpcController *controller,
             const ::example::Request *request,
             ::example::Response *response,
             ::google::protobuf::Closure *done)
    {
        // 获取request相应数据
        std::string msg = request->msg();
        std::cout << "RPC_Server receive message: " << msg << std::endl;

        // 做本地业务
        std::string handledMsg = Get(msg);

        // 写入response
        response->set_handledmsg(handledMsg);
        response->mutable_resultcode()->set_errcode(0);
        response->mutable_resultcode()->set_errmsg("");

        // 回调 即RpcServer::sendRpcRespones,将response序列化发送回调用方
        done->Run();
    }

    void Add(::google::protobuf::RpcController *controller,
             const ::example::Request *request,
             ::example::Response *response,
             ::google::protobuf::Closure *done)
    {
        // 获取request相应数据
        std::string msg = request->msg();
        std::cout << "RPC_Server receive message: " << msg << std::endl;

        // 做本地业务
        std::string handledMsg = Add(msg);

        // 写入response
        response->set_handledmsg(handledMsg);
        response->mutable_resultcode()->set_errcode(0);
        response->mutable_resultcode()->set_errmsg("");

        // 回调 即RpcServer::sendRpcRespones,将response序列化发送回调用方
        done->Run();
    }

private:
    std::string Get(std::string str)
    {
        return "Handled Message:" + str;
    }
    std::string Add(std::string str)
    {
        std::stringstream ss(str);
        int a, b;
        ss >> a >> b;
        return std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(a + b);
    };
};

int main(int argc, char **argv)
{
    Config::loadCfgFile(argc, argv, Config::cfgType::server);
    RpcServer rpcServ(4);

    rpcServ.registerService(new foo());
    rpcServ.start();
}
