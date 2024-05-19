#include "echo.pb.h"
#include "InetAddress.h"
#include "RpcChannel.h"
#include "Timestamp.h"

#include <stdio.h>
#include <unistd.h>

static int kRequests;
#include <thread>
#include <mutex>
#include <condition_variable>

class CountDownLatch
{
public:
  explicit CountDownLatch(int count) : count_(count) {}

  void wait()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (count_ > 0)
    {
      condition_.wait(lock);
    }
  }

  void countDown()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    --count_;
    if (count_ == 0)
    {
      condition_.notify_all();
    }
  }

  int getCount() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  int count_;
};

class RpcClient : uncopyable
{
public:
  RpcClient(int id,
            CountDownLatch *allFinished)
      : id_(id),
        channel_(new RpcChannel),
        stub_(channel_.get()),
        allFinished_(allFinished),
        count_(0)
  {
  }

  void sendRequest()
  {
    echo::EchoRequest request;
    request.set_id(id_);
    request.set_cnt(count_);
    echo::EchoResponse *response = new echo::EchoResponse;
    RpcController ctrl;
    stub_.Echo(&ctrl, &request, response, nullptr);
    if (ctrl.Failed())
      std::cout << ctrl.ErrorText() << std::endl;
    replied(response);
  }

private:
  void replied(echo::EchoResponse *resp)
  {
    ++count_;
    // printf("client[%d] cnt[%d]\n", id_, count_);
    if (count_ < kRequests)
    {
      sendRequest();
    }
    else
    {
      std::cout << "RpcClient " << this << " finished\n";
      allFinished_->countDown();
    }
  }
  int id_;
  std::shared_ptr<RpcChannel> channel_;
  echo::EchoService::Stub stub_;
  CountDownLatch *allFinished_;
  int count_;
};

int main(int argc, char *argv[])
{
  if (argc > 1)
  {
    Config::loadCfgFile(argc, argv, Config::cfgType::client);
    std::cout << "pid = " << getpid() << std::endl;
    int nClients = argc > 2 ? atoi(argv[2]) : 1;
    kRequests = argc > 3 ? atoi(argv[3]) : 50000;
    std::cout << kRequests << std::endl;
    CountDownLatch allFinished(nClients);
    Timestamp start(Timestamp::now());

    std::cout << "start test...\n";
    // 创建nClients个客户端线程
    std::vector<std::thread> clientThreads;
    for (int i = 0; i < nClients; ++i)
    {
      clientThreads.emplace_back([&allFinished, i]()
                                 {
        RpcClient client(i, &allFinished);
          client.sendRequest(); });
    }

    // 等待所有客户端线程完成
    for (auto &thread : clientThreads)
    {
      thread.join();
    }
    allFinished.wait();
    std::cout << "all finished\n";
    Timestamp end(Timestamp::now());
    double seconds = static_cast<double>(end.secondsSinceEpoch() - start.secondsSinceEpoch());
    printf("%f seconds\n", seconds);
    printf("%.1f calls per second\n", 1.0 * nClients * kRequests / seconds);

    exit(0);
  }
  else
  {
    puts("Args : <config file path> [client nums] [thread nums]");
  }
}
