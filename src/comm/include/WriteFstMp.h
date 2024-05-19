#pragma once

#include <mutex>
#include <map>

// 写者优先的map
template <typename K, typename V>
class WriteFstMp
{
public:
    WriteFstMp()
        : readCnt_(0), writeCnt_(0)
    {
    }
    ~WriteFstMp() = default;

    bool exist(const K &key)
    {
        readLk_.lock();
        rmutex_.lock();
        readCnt_++;
        if (readCnt_ == 1)
            writeLk_.lock();
        rmutex_.unlock();
        readLk_.unlock();

        bool f = mp_.count(key);

        rmutex_.lock();
        readCnt_--;
        if (readCnt_ == 0)
            writeLk_.unlock();
        rmutex_.unlock();
        return f;
    }

    V get(const K &key)
    {
        readLk_.lock();
        rmutex_.lock();
        readCnt_++;
        if (readCnt_ == 1)
            writeLk_.lock();
        rmutex_.unlock();
        readLk_.unlock();

        V res = mp_[key];

        rmutex_.lock();
        readCnt_--;
        if (readCnt_ == 0)
            writeLk_.unlock();
        rmutex_.unlock();
        return res;
    }

    bool change(const K &key, const V &value)
    {
        wmutex_.lock();
        writeCnt_++;
        if (writeCnt_ == 1)
            readLk_.lock();
        wmutex_.unlock();
        writeLk_.lock();

        if (mp_.find(key) != mp_.end())
            mp_.erase(key);
        mp_[key] = value;

        writeLk_.unlock();
        wmutex_.lock();
        writeCnt_--;
        if (writeCnt_ == 0)
            readLk_.unlock();
        wmutex_.unlock();
        return true;
    }

private:
    std::map<K, V> mp_;
    std::mutex readLk_, writeLk_; // 给读/写者上锁
    std::mutex rmutex_, wmutex_;  // 锁对应的cnt
    int readCnt_, writeCnt_;      // 读者、写者的数量
};
