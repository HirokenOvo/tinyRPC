#pragma once

#include <vector>
#include <string>

/** @code
 * +-------------------+------------------+------------------+
 * | prependable bytes |  readable bytes  |  writable bytes  |
 * |                   |     (CONTENT)    |                  |
 * +-------------------+------------------+------------------+
 * |                   |                  |                  |
 * 0      <=      readerIndex   <=   writerIndex    <=     size
 * @endcode
 */
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;     // 头长度
    static const size_t kInitiableSize = 1024; // 缓冲区长度

    explicit Buffer(size_t initiableSize = kInitiableSize);

    // 获取对应数据段的长度
    size_t readableBytes() const;
    size_t writableBytes() const;
    size_t prependableBytes() const;

    // 将[readerIndex,readerIndex+min(len,readableBytes)]的数据转为string读出
    std::string retrieveAllToString();
    std::string retrieveToString(size_t len);
    // 读出后更新readerIndex
    void retrieve(size_t len);
    void retrieveAll();

    // 通过扩容保证缓冲空间够用
    void ensureWritableBytes(size_t len);
    // 把[data,data+len]部分的数据添加至writable缓冲区中
    void append(const char *data, size_t len);

    char *getReadableBegin();
    const char *getReadableBegin() const;
    char *getWritableBegin();
    const char *getWritableBegin() const;

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // 返回数组起始地址
    char *begin();
    const char *begin() const;

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};