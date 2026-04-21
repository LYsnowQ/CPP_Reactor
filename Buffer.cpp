
#include <cstddef>
#include <iterator>
#include <sys/uio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Buffer.hpp"


reactor::base::Buffer::Buffer()
:buffer_(kCheapPrepend+kInitialSize),
readIndex_(kCheapPrepend),
writeIndex_(kCheapPrepend)
{}


size_t reactor::base::Buffer::readableBytes()
{
    return writeIndex_ - readIndex_;
}


size_t reactor::base::Buffer::writeableBytes()
{
    return buffer_.size() - writeIndex_;
}


size_t reactor::base::Buffer::prependableBytes()
{
    return readIndex_;
}


const char* reactor::base::Buffer::peek() const
{
    return begin_() + readIndex_;
}


void reactor::base::Buffer::append(const std::string& str)
{
    append(str.data(),str.size());
}


void reactor::base::Buffer::append(const char* data,size_t len)
{
    ensureWriteableBytes_(len);
    std::copy(data,data+len,beginWrite_());
    writeIndex_ += len;
}
        
       
std::string reactor::base::Buffer::retrieveAsString(size_t len)
{
    std::string result(peek(),len);
    retrieve_(len);
    return result;
}


std::string reactor::base::Buffer::retrieveAllString()
{
    return retrieveAsString(readableBytes());
}
        
ssize_t reactor::base::Buffer::readFd(int fd, int* saved_errno)
{
    char extraBuf[1024*32];//32K额外缓冲区
    struct iovec vec[2];

    const size_t writeable = writeableBytes();
    vec[0].iov_base = begin_() + writeIndex_;
    vec[0].iov_len = writeable;
    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof(extraBuf);

    const int veccnt = (writeable < sizeof(extraBuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd,vec,veccnt);
    
    if(n < 0)
    {
        *saved_errno = errno;
    }
    else if(static_cast<size_t>(n) <= writeable)
    {
        writeIndex_ += n;
    }
    else 
    {
        writeIndex_ =  buffer_.size();
        append(extraBuf,n - writeable);
    }
    return n;
}


ssize_t reactor::base::Buffer::writeFd(int fd)
{
    const ssize_t n = ::write(fd, peek(), readableBytes());
    if(n > 0)
    {
        retrieve_(n);
    }
    return n;
}


char* reactor::base::Buffer::begin_()
{
    return &*buffer_.begin();        
}


const char* reactor::base::Buffer::begin_()const
{
    return &*buffer_.begin();
}


char* reactor::base::Buffer::beginWrite_()
{
    return begin_() + writeIndex_;
}


void reactor::base::Buffer::retrieve_(size_t len)
{
    if(len < readableBytes())
    {
        readIndex_ += len;
    }
    else
    {//覆盖原使用过的数据重复利用
        retrieveAll_();
    }
}


void reactor::base::Buffer::retrieveAll_()
{
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
}


void reactor::base::Buffer::ensureWriteableBytes_(size_t len)
{
    if(writeableBytes() < len)
    {
        makeSpace_(len);
    }
}


void reactor::base::Buffer::makeSpace_(size_t len)
{
    if(prependableBytes() + writeableBytes() < len + kCheapPrepend )
    {
        buffer_.resize(writeIndex_ + len);
    }
    else 
    {
        size_t readable = readableBytes();
        std::copy(begin_() + readIndex_,
                begin_()+ writeIndex_,
                begin_() + kCheapPrepend
                );
        readIndex_ = kCheapPrepend;
        writeIndex_ = readIndex_ + readable;
    }
}

