#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <type_traits>
#include <vector>

namespace reactor::base
{
    class Buffer
    {
    public:
        static const size_t kCheapPrepend = 8;//为2进制协议预留请求头
        static const size_t kInitialSize = 1024;


        explicit Buffer();
        ~Buffer() = default;

        size_t readableBytes()const;
        size_t writeableBytes()const;        
        size_t prependableBytes()const;

        const char* peek() const;
        
        std::string::size_type find(const std::string_view substr) const;
        std::string::size_type find(const std::string_view substr,size_t offset) const;

        void append(const std::string& str);
        void append(const char* data,size_t len);
        
        void retrieve(size_t len);
        void retrieveAll();
        
        std::string_view getStringView(size_t len) const; 
        std::string_view getStringView(size_t offset,size_t len) const; 
        std::string retrieveAsString(size_t len);
        std::string retrieveAllString(); 
        
        ssize_t readFd(int fd, int* saved_errno);
        ssize_t writeFd(int fd);

    private:
        char* begin_();
        const char* begin_()const;
        char* beginWrite_();
        

        void ensureWriteableBytes_(size_t len);
        void makeSpace_(size_t len);
    private:
        std::vector<char> buffer_;
        uint32_t readIndex_;
        uint32_t writeIndex_;
    };

}//namespace reactoer::base
