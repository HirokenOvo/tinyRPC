#include "example.pb.h"
#include "RpcChannel.h"
#include "RpcController.h"

int main()
{
    std::string str;
    while (getline(std::cin, str))
    {
        example::ServiceRpc_Stub stub(new RpcChannel());
        example::Request args;
        example::Response response;
        args.set_msg(str);
        RpcController ctrler;
        stub.Get(&ctrler, &args, &response, nullptr);

        if (!ctrler.Failed())
            std::cout << "rpc invoke success: " << response.handledmsg() << std::endl;
        else
            std::cout << "rpc invoke failed" << ' ' << ctrler.ErrorText() << std::endl;
    }
}