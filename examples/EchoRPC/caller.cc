#include "example.pb.h"
#include "RpcChannel.h"
#include "RpcController.h"

int main(int argc, char **argv)
{
    Config::loadCfgFile(argc, argv, Config::cfgType::client);
    std::string str;
    int op;
    while (std::cout << "==============================\n"
                     << "Echo:1; Add:2; Quit:3" << std::endl,
           std::cin >> op)
    {
        if (op != 1 && op != 2)
            exit(0);
        std::cin.ignore();
        getline(std::cin, str);
        example::ServiceRpc_Stub stub(new RpcChannel());
        example::Request args;
        example::Response response;
        args.set_msg(str);
        RpcController ctrler;
        if (op & 1)
            stub.Get(&ctrler, &args, &response, nullptr);
        else
            stub.Add(&ctrler, &args, &response, nullptr);

        if (!ctrler.Failed())
            std::cout << "rpc invoke success: " << response.handledmsg() << std::endl;
        else
            std::cout << "rpc invoke failed" << ' ' << ctrler.ErrorText() << std::endl;
    }
}
