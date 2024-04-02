#include <unistd.h>
#include <sys/syscall.h>

#include "CurrentThread.h"

namespace CurrentThread
{
    thread_local int t_cachedTid = 0; // 当前线程的Tid

    // 获取Tid并缓存至t_cachedTid
    void cacheTid()
    {
        t_cachedTid = t_cachedTid == 0 ? static_cast<pid_t>(::syscall(SYS_gettid)) : t_cachedTid;
    }

    // 返回Tid
    int getTid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
            cacheTid();
        return t_cachedTid;
    }
}