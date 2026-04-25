#pragma once


#include "Buffer.hpp"
#include <cstdint>
#include <string>
#include <sys/types.h>

namespace reactor::net::protocol{

    enum class ReadyCode:uint16_t
    {
        Ready = 0,
        NoResponseBuffer=0x01,
        NoResponseLine=0x02,
        NoHeaders=0x04,
        NoFile=0x08
    };

    class HttpResponse
    {
    public:
        HttpResponse(base::Buffer* buffer);

        void setStateLine(const std::string& version,uint32_t status,const std::string& statusMsg);
        
        void addHeader(std::string key,std::string value);

        void addfile(std::string file);

        uint16_t getCheckReady();
    private:
        void checkReady_();
    private:
        uint16_t ready_;
        base::Buffer* response_ = nullptr;
        std::string line_ = std::string();
        std::vector<std::pair<std::string,std::string>> headers_;
        std::vector<std::string> files_;
    };
}

#if 0
struct HttpResponse* httpResponseInit();

void httpResponseDestory(struct HttpResponse* response);

void HttpResponseAddHeader(struct HttpResponse* response,const char* key,const char* value);

void httpResponsePrepareMsg(struct HttpResponse* response, struct Buffer* sendBuffer, int socket);

#endif
