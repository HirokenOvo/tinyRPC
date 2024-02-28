#include "include/Thread.h"
#include "include/CurrentThread.h"

#include <memory>
#include <semaphore.h>

std::atomic_int Thread::numCreated_{0};

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

Thread::Thread(std::function<void()> &&func, const std::string &name)
    : started_{false}, joined_{false}, tid_{0}, func_{func}, name_{name}
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
        thread_->detach();
}

void Thread::start() // 一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem; // 线程间同步操作用
    sem_init(&sem, false, 0);

    thread_ = std::make_shared<std::thread>([&]()
                                            {
        tid_=CurrentThread::getTid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数
        func_(); });

    // 等待上面新线程获取到tid值，防止新线程还未初始化完主线程就访问新线程信息
    sem_wait(&sem);
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

bool Thread::getStarted() const { return started_; }
pid_t Thread::getTid() const { return tid_; }
const std::string &Thread::getName() const { return name_; }
int Thread::getNumCreated() { return numCreated_; };