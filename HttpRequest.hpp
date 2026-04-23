#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include "Buffer.hpp"
#include "CoreStatus.hpp"




namespace reactor::net::protocol{


    class HttpRequest
    {

    public:        
        static constexpr size_t kMaxBodyBytes = 1024 * 1024; // 1MB

        struct ParseResult
        {
            core::StatusCode code;
            std::unique_ptr<HttpRequest> request;
            std::string message;
            bool tooLarge = false;
        };

        static ParseResult parseRequest(base::Buffer* data, std::unique_ptr<HttpRequest>* inFlight = nullptr);

        HttpRequest(const HttpRequest&) = delete;
        HttpRequest& operator=(const HttpRequest&) = delete;
        

        std::string getMethed();
        std::string getUrl();
        std::string version();

        std::vector<std::pair<std::string,std::string>> getHeader();
        
        std::string getBody();
        
           
    private:
        enum class HttpRequestState{
            kIdle,
            kParseReqLine,
            kParseReqLineFailed,
            kParseReqHeaders,
            kParseReqHeadersFailed,
            kParseReqBody,
            kParseReqBodyFailed,
            kParseDone
        };

        explicit HttpRequest(base::Buffer* data);
        
        core::StatusCode parse_();

        core::StatusCode parseHead_();
        core::StatusCode parseLine_();
        core::StatusCode parseBody_();
        std::optional<std::string> findHeader_(std::string_view key) const;
        
    private:

        reactor::base::Buffer* data_;
        std::string method_;
        std::string url_;
        std::string version_;
        std::vector<std::pair<std::string,std::string>>headers_;
        std::string body_;
        bool bodyTooLarge_ = false;

        HttpRequestState curState_;
    };


}//namespace reactor::net::protocol

#if 0
//初始化
struct HttpRequest* httpRequestInit();
//重置
void httpRequestReset(struct HttpRequest* request);
void httpRequestResetEx(struct HttpRequest* request);
void httpRequestDestory(struct HttpRequest* request);

//获取处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* request);

//添加请求头
void httpRequestAddHeader(struct HttpRequest* request,const char* key,const char* value);

//根据key得到请求头value
char* httpRequestGetHeader(struct HttpRequest* request,const char* key);

//解析请求行
bool parseHttpRequestLine(struct HttpRequest* request,struct Buffer* readBuffer);
//解析请求头
bool parseHttpRequestHead(struct HttpRequest* request,struct Buffer* readBuffer);
//解析http请求协议
bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuffer,struct HttpResponse* response, struct Buffer* sendBuffer, int socket);

//解码字符串
void decodeMsg(char* from,char* to);
//解析文件类型
const char* getFileType(const char* name);
//处理http请求协议
bool processHttpRequest(struct HttpRequest* request,struct HttpResponse* response);

//发送文件
void sendFile(const char* fileName,struct Buffer* sendBuffer ,int cfd);
//发送目录
void sendDir(const char* dirName,struct Buffer* sendBuffer, int cfd);
#endif
