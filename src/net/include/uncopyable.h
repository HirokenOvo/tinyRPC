#pragma once

class uncopyable
{
public:
    uncopyable(const uncopyable &) = delete;
    uncopyable &operator=(const uncopyable &) = delete;

protected:
    uncopyable() = default;
    ~uncopyable() = default;
};