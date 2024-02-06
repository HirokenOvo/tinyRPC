#pragma once

namespace CurrentThread
{
    extern thread_local int t_cachedTid; // 当前线程的Tid

    void cacheTid(); // 获取Tid并缓存至t_cachedTid

    int tid(); // 返回Tid
}