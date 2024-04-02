#include "Buffer.h"

#include <algorithm>
#include <sys/uio.h>
#include <type_traits>
#include <unistd.h>

Buffer::Buffer(size_t initiableSize)
	: buffer_(kCheapPrepend + initiableSize),
	  readerIndex_{kCheapPrepend},
	  writerIndex_{kCheapPrepend}
{
}

size_t Buffer::readableBytes() const { return writerIndex_ - readerIndex_; }
size_t Buffer::writableBytes() const { return buffer_.size() - writerIndex_; }
size_t Buffer::prependableBytes() const { return readerIndex_; }

// 将[readerIndex,readerIndex+min(len,readableBytes)]的数据转为string读出
std::string Buffer::retrieveToString(size_t len)
{
	std::string res(getReadableBegin(), len);
	retrieve(len);
	return res;
}
std::string Buffer::retrieveAllToString() { return retrieveToString(readableBytes()); }
// 读出后更新readerIndex
void Buffer::retrieve(size_t len)
{
	if (len < readableBytes())
		readerIndex_ += len;
	else
		retrieveAll();
}
void Buffer::retrieveAll()
{
	readerIndex_ = writerIndex_ = kCheapPrepend;
}

// 通过扩容保证缓冲空间够用
void Buffer::ensureWritableBytes(size_t len)
{
	if (writableBytes() < len)
		makeSpace(len);
}
// 把[data,data+len]部分的数据添加至writable缓冲区中
void Buffer::append(const char *data, size_t len)
{
	ensureWritableBytes(len);
	std::copy(data, data + len, getWritableBegin());
	writerIndex_ += len;
}

char *Buffer::getReadableBegin() { return const_cast<char *>(static_cast<const Buffer *>(this)->getReadableBegin()); }
const char *Buffer::getReadableBegin() const { return begin() + readerIndex_; }
char *Buffer::getWritableBegin() { return const_cast<char *>(static_cast<const Buffer *>(this)->getWritableBegin()); }
const char *Buffer::getWritableBegin() const { return begin() + writerIndex_; }

// 从fd上读取数据
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
	char extrabuf[65536] = {0}; // 64K额外空间开辟在栈上

	struct iovec iov[2];
	//{缓冲区起始地址,缓冲区长度}
	// 写入的缓冲区
	iov[0] = {getWritableBegin(), writableBytes()};
	// 写不下写到额外缓冲区
	iov[1] = {extrabuf, sizeof extrabuf};

	ssize_t n = readv(fd, iov, 2);
	if (n < 0)
		*saveErrno = errno;
	else if (n <= writableBytes()) // 可写缓冲区足够用
		writerIndex_ += n;
	else // 需要用到extrabuf
	{
		int lenExBuf = n - writableBytes(); // 填到extrabuf里的数据量
		writerIndex_ = buffer_.size();
		append(extrabuf, lenExBuf);
	}

	return n;
}
// 通过fd发送数据
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
	ssize_t n = ::write(fd, getReadableBegin(), readableBytes());
	if (n < 0)
		*saveErrno = errno;
	return n;
}

// 返回数组起始地址
char *Buffer::begin() { return const_cast<char *>(static_cast<const Buffer *>(this)->begin()); }
const char *Buffer::begin() const { return &*buffer_.begin(); }

void Buffer::makeSpace(size_t len)
{
	if (writableBytes() + prependableBytes() < len + kCheapPrepend)
		buffer_.resize(writerIndex_ + len);
	else
	{
		std::copy(getReadableBegin(), getWritableBegin(), begin() + kCheapPrepend);
		readerIndex_ = kCheapPrepend;
		writerIndex_ = readerIndex_ + readableBytes();
	}
}