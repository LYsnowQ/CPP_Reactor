#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include "Buffer.hpp"




namespace reactor::net::protocol{


    class HttpRequest
    {

    public:        
        static std::pair<std::unique_ptr<HttpRequest>,std::string> parseRequest(base::Buffer* data);

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
        
        bool parse_();

        bool parseHead_();
        bool parseLine_();
        bool parseBody_();
        
    private:

        reactor::base::Buffer* data_;
        std::string method_;
        std::string url_;
        std::string version_;
        std::vector<std::pair<std::string,std::string>>headers_;
        std::string body_;

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
