#pragma once

#include <iostream>
#include <string>

// 时间类
//???这个类的作用不明
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t secondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
    std::string getYMD() const;
    int64_t secondsSinceEpoch() const { return secondsSinceEpoch_; }

private:
    int64_t secondsSinceEpoch_;
};