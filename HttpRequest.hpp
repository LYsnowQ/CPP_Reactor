#pragma once

#include <string>
#include <map>
#include <vector>
#include "Buffer.hpp"
//#include "HttpResponse.hpp"
//#include "HttpResponse.hpp"




namespace reactor::net::protocol{


    class HttpRequest
    {

    public:
        enum class HttpRequestState {kParseReqLine,kParseReqHeaders,kParseReqBody,kParseDone};
        
        HttpRequest(base::Buffer* dataPackage);
        ~HttpRequest() = default;

        std::string getMethed();
        std::string getUrl();
        std::string version();

        std::vector<std::pair<std::string,std::string>> getHeader();
        
        std::string getBody();
        
        HttpRequestState getState();
           
    private:
        void parseHead_();
        void parseLine_();
        void parseBody_();

    private:

        reactor::base::Buffer* data_;
        std::string method_;
        std::string url_;
        std::string version_;
        std::vector<std::pair<std::string,std::string>>headers_;
        std::string body_;

        enum HttpRequestState curState_;
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
