#pragma once

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionSPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionSPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionSPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionSPtr &)>;
using MessageCallback = std::function<void(const TcpConnectionSPtr &,
                                           Buffer *,
                                           Timestamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionSPtr &, size_t)>;