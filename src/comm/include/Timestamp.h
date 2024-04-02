#pragma once

#include <iostream>
#include <string>

// 时间类
//???这个类的作用不明
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
    std::string getYMD() const;

private:
    int64_t microSecondsSinceEpoch_;
};