#pragma once

#include "noncopyable.h"

#include <vector>
#include <string>
#include <algorithm>

class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitiaSize = 1024;
    explicit Buffer(size_t initialSize = kInitiaSize)
        : buffer_(kCheapPrepend + kInitiaSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
    }
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writeableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回缓冲区可读数据的起始地址
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; //应用只读取了可读缓冲区数据的一部分，就是len长度，还剩下readerIndex_到writerindex
        }
        else // len == readableBytes
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    //把onmessage函数上报的buffer数据转成string类型的
    std::string retrieveAllAsString()
    {
        return retrieveAstring(readableBytes());
    }

    std::string retrieveAstring(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); //上面把缓冲区中可读的数据已经读取出来，这里对缓冲区进行复位操作
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        if (writeableBytes() < len)
        {
            makeSpace(len); //扩容函数
        }
    }

    void makeSpace(size_t len)
    {
        if (writeableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    //把data， data + len内存上的数据，添加到writeable缓冲区中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    //从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd, int *savedErrno);

private:
    char *begin()
    {
        // it.operator*()
        return &*buffer_.begin(); // vetor底层数组首元素地址，也就是数组的起始地址
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};