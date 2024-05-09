#include "echo.pb.h"

#include "Logger.h"
#include "EventLoop.h"
#include "RpcServer.h"

#include <unistd.h>

namespace echo
{

  class EchoServiceImpl : public EchoService
  {
  public:
    virtual void Echo(::google::protobuf::RpcController *controller,
                      const ::echo::EchoRequest *request,
                      ::echo::EchoResponse *response,
                      ::google::protobuf::Closure *done)
    {
      char buf[64] = {0};
      int id = request->id();
      int cnt = request->cnt();
      sprintf(buf, "client[%d] cnt[%d]", id, cnt);
      std::string res(buf);
      std::cout << res << std::endl;
      LOG_INFO("%s", res.c_str());
      response->set_payload(res);
      done->Run();
    }
  };

} // namespace echo

int main(int argc, char *argv[])
{
  Config::loadCfgFile(argc, argv, Config::cfgType::server);
  int nThreads = argc > 2 ? atoi(argv[2]) : 1;
  printf("pid = %d threads = %d\n", getpid(), nThreads);
  RpcServer server(nThreads);
  server.registerService(new echo::EchoServiceImpl());
  server.start();
}
