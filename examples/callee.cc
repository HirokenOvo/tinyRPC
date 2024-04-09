#include "example.pb.h"
#include "RpcServer.h"

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

        // 做本地业务
        std::string handledMsg = Get(msg);

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
};

int main()
{
    EventLoop evtLoop;
    InetAddress iAddr(8000);
    RpcServer rpcServ(&evtLoop, iAddr, 4);

    rpcServ.registerService(new foo());

    rpcServ.start();
    evtLoop.loop();
}