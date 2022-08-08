#pragma once

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionptr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionptr &)>;
using CloseCallback = std::function<void(const TcpConnectionptr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionptr &)>;
using MessageCallback = std::function<void(const TcpConnectionptr &,
                                           Buffer *,
                                           Timestamp)>;

using HighWaterMarkCallback = std::function<void(const TcpConnectionptr &, size_t)>;