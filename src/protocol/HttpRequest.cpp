#include "HttpRequest.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <cctype>
#include <charconv>
#include <system_error>
#include <sys/types.h>

reactor::net::protocol::HttpRequest::HttpRequest(base::Buffer* dataPackage)
:data_(dataPackage),method_(std::string()),url_(std::string()),version_(std::string()),curState_(HttpRequestState::kIdle)
{}

reactor::net::protocol::HttpRequest::ParseResult reactor::net::protocol::HttpRequest::parseRequest(
    base::Buffer* data,
    std::unique_ptr<HttpRequest>* inFlight)
{
    if(!data)
    {
        return {core::StatusCode::kInvalid, nullptr, "null request buffer", false};
    }
    if(data->readableBytes() == 0)
    {
        return {core::StatusCode::kAgain, nullptr, "empty request", false};
    }
    std::unique_ptr<HttpRequest> local;
    std::unique_ptr<HttpRequest>* holder = inFlight ? inFlight : &local;
    if(!(*holder))
    {
        holder->reset(new HttpRequest(data));
    }
    (*holder)->data_ = data;
    auto& res = *holder;

    const auto code = res->parse_();
    if(code != core::StatusCode::kOk)
    {
        switch(res->curState_)
        {
        case(HttpRequestState::kParseReqLineFailed):
            return {code, nullptr, "invalid request line", false};
        case(HttpRequestState::kParseReqHeadersFailed):
            return {code, nullptr, "invalid headers", false};
        case(HttpRequestState::kParseReqBodyFailed):
            return {code, nullptr, res->bodyTooLarge_ ? "body too large" : "invalid body", res->bodyTooLarge_};
        default:
            return {code, nullptr, "incomplete request", false};
        }
    }
    return {core::StatusCode::kOk, std::move(*holder), "ok", false};
}

std::string reactor::net::protocol::HttpRequest::getMethed()
{
    if (method_.length())
    {
        return method_;
    }
    return "";
}


std::string reactor::net::protocol::HttpRequest::getUrl()
{
    if (url_.length())
    {
        return url_;
    }
    return "";
}


std::string reactor::net::protocol::HttpRequest::version()
{
    if (version_.length())
    {
        return version_;
    }
    return "";
}


std::vector<std::pair<std::string,std::string>> reactor::net::protocol::HttpRequest::getHeader()
{
    return headers_;
}


std::string reactor::net::protocol::HttpRequest::getBody()
{
    return body_;
}

/*
uint32_t reactor::net::protocol::HttpRequest::getState()
{
    return curState_;
}
*/         
    
reactor::core::StatusCode reactor::net::protocol::HttpRequest::parse_()
{
    if(curState_ == HttpRequestState::kParseDone)
    {
        return core::StatusCode::kOk;
    }
    if(curState_ == HttpRequestState::kIdle || curState_ == HttpRequestState::kParseReqLine)
    {
        const auto code = parseLine_();
        if(code != core::StatusCode::kOk)
        {
            return code;
        }
        curState_ = HttpRequestState::kParseReqHeaders;
    }

    if(curState_ == HttpRequestState::kParseReqHeaders)
    {
        const auto code = parseHead_();
        if(code != core::StatusCode::kOk)
        {
            return code;
        }
        curState_ = HttpRequestState::kParseReqBody;
    }

    if(curState_ == HttpRequestState::kParseReqBody)
    {
        return parseBody_();
    }
    return core::StatusCode::kInvalid;
}

reactor::core::StatusCode reactor::net::protocol::HttpRequest::parseLine_()
{ 
    curState_ = HttpRequestState::kParseReqLine;

    auto lineEnd = data_->find("\r\n");
    
    if(lineEnd == std::string::npos) 
    {
        return core::StatusCode::kAgain;
    }

    const auto requestLineView = data_->getStringView(lineEnd);
    std::string requestLine(requestLineView.data(), requestLineView.size());

    const auto firstSpace = requestLine.find(' ');
    if(firstSpace == std::string::npos)
    {
        curState_ = HttpRequestState::kParseReqLineFailed;
        return core::StatusCode::kInvalid;
    }

    const auto secondSpace = requestLine.find(' ', firstSpace + 1);
    if(secondSpace == std::string::npos)
    {
        curState_ = HttpRequestState::kParseReqLineFailed;
        return core::StatusCode::kInvalid;
    }

    method_ = requestLine.substr(0, firstSpace);
    url_ = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    version_ = requestLine.substr(secondSpace + 1);

    if(method_.empty() || url_.empty() || version_.empty())
    {
        curState_ = HttpRequestState::kParseReqLineFailed;
        return core::StatusCode::kInvalid;
    }

    data_->retrieve(lineEnd + 2);
    return core::StatusCode::kOk;
}

reactor::core::StatusCode reactor::net::protocol::HttpRequest::parseHead_()
{
    curState_ = HttpRequestState::kParseReqHeaders;
    if(data_->find("\r\n\r\n") == std::string::npos)
    {
        return core::StatusCode::kAgain;
    }

    while(true)
    {
        auto index = data_->find("\r\n");
        if(index == std::string::npos)
        {
            return core::StatusCode::kAgain;
        }

        if(index == 0)
        {
            data_->retrieve(2);
            break;
        }

        std::string_view subView = data_->getStringView(index);
        std::string sub(subView.data(), subView.size());
        data_->retrieve(index + 2);
        
        // 分割请求头键值
        std::string::size_type subpos = sub.find(": "); 
        if (subpos == std::string::npos) 
        {
            curState_ = HttpRequestState::kParseReqHeadersFailed;
            return core::StatusCode::kInvalid;
        }

        std::string key = sub.substr(0,subpos); 
        std::string value = sub.substr(subpos+2);
        /*由于头部请求策略问题，其中有些参数多个值并非有单一拆分原则，具体如下：
         * 1.大部分请求头均是逗号隔离多个值。
         * 2.Set-Cookie由于内容支持逗号则其策略为多个值存储多次，且内部是使用；来区分的
         * 3.每个Cookie头只包含一组name = value
         * 基于以上情况，这里不做拆分，只做整行存储,具体情况交给上层处理
        while(subpos<sub.size()){
            subpos = sub.find(",",subpos)                     
            }
        */
        headers_.emplace_back(key,value);
    }
    return core::StatusCode::kOk;
}


reactor::core::StatusCode reactor::net::protocol::HttpRequest::parseBody_()
{
    curState_ = HttpRequestState::kParseReqBody;
    bodyTooLarge_ = false;

    const auto contentLengthOpt = findHeader_("Content-Length");
    if(!contentLengthOpt.has_value())
    {
        body_.clear();
        curState_ = HttpRequestState::kParseDone;
        return core::StatusCode::kOk;
    }

    size_t contentLength = 0;
    const auto& contentLengthValue = contentLengthOpt.value();
    const auto trimmedBegin = contentLengthValue.find_first_not_of(" \t");
    if(trimmedBegin == std::string::npos)
    {
        curState_ = HttpRequestState::kParseReqBodyFailed;
        return core::StatusCode::kInvalid;
    }
    const auto trimmedEnd = contentLengthValue.find_last_not_of(" \t");
    const auto trimmed = contentLengthValue.substr(trimmedBegin, trimmedEnd - trimmedBegin + 1);
    const auto begin = trimmed.data();
    const auto end = trimmed.data() + trimmed.size();
    const auto res = std::from_chars(begin, end, contentLength);
    if(res.ec != std::errc() || res.ptr != end)
    {
        curState_ = HttpRequestState::kParseReqBodyFailed;
        return core::StatusCode::kInvalid;
    }

    if(contentLength > kMaxBodyBytes)
    {
        bodyTooLarge_ = true;
        curState_ = HttpRequestState::kParseReqBodyFailed;
        return core::StatusCode::kInvalid;
    }

    if(data_->readableBytes() < contentLength)
    {
        return core::StatusCode::kAgain;
    }

    body_ = data_->retrieveAsString(contentLength);
    curState_ = HttpRequestState::kParseDone;
    return core::StatusCode::kOk;
}

std::optional<std::string> reactor::net::protocol::HttpRequest::findHeader_(std::string_view key) const
{
    auto toLower = [](std::string_view in)
    {
        std::string out;
        out.reserve(in.size());
        for(char c : in)
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    };

    const auto target = toLower(key);
    for(const auto& kv : headers_)
    {
        if(toLower(kv.first) == target)
        {
            return kv.second;
        }
    }
    return std::nullopt;
}



#if 0
#define HeaderSize 12

struct HttpRequest* httpRequestInit()
{
    struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request-> reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader)*HeaderSize);
    request->reqHeadersNum = 0;

    return request;
}

void httpRequestReset(struct HttpRequest* request)
{
    request-> curState = ParseReqLine;
    request->method = NULL;
    request->url = NULL;
    request->version = NULL;
    request->reqHeadersNum = 0;
}


void httpRequestResetEx(struct HttpRequest* request)
{
    free(request->url);
    free(request->method);
    free(request->version);
    if(request->reqHeaders != NULL)
    {
        for(int i=0;i<request->reqHeadersNum;i++)
        {
            free(request->reqHeaders[i].key);
            free(request->reqHeaders[i].value);
        }
        free(request->reqHeaders);
    }
    httpRequestReset(request);
}

void httpRequestDestory(struct HttpRequest* request)
{
    if(request != NULL)
    {
        httpRequestResetEx(request);
        free(request);
    }
}

enum HttpRequestState httpRequestState(struct HttpRequest* request)
{
    return request->curState;
}

void httpRequestAddHeader(struct HttpRequest* request,const char* key,const char* value)
{
    request->reqHeaders[request->reqHeadersNum].key = (char*)key;
    request->reqHeaders[request->reqHeadersNum].value = (char*)value;
    request->reqHeadersNum++;
}

char* httpRequestGetHeader(struct HttpRequest* request,const char* key)
{
    if(request != NULL)
    {
        for(int i = 0;i < request->reqHeadersNum; i++)
        {
            if(strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0)
            {
                return request->reqHeaders[i].value;
            }
        }
    } 
    return NULL;
}

char* splitRequestLine(const char* start,const char* end,const char* sub,char** ptr)
{
    char* space = (char*)end;
    if(sub != NULL)
    {
        space = memmem(start,end - start,sub,strlen(sub));
    }
    int lenSize = space - start;    
    char* temp = (char*)malloc(lenSize + 1);
    strncpy(temp, start, lenSize);
    temp[lenSize] = '\0';
    *ptr = temp;

    return space+1;
}

char* bufferFindCRLF(struct Buffer* buffer)
{
    // strstr() 遇到 \0 结束
    // memmem() 在大数据块中匹配子数据块
    char* ptr = memmem(buffer->data+buffer->readPos,bufferReadableSize(buffer),"\r\n",2);
    return ptr;
}

bool parseHttpRequestLine(struct HttpRequest* request,struct Buffer* readBuffer)
{
    char* end = bufferFindCRLF(readBuffer);
    if(end == NULL)
    {
        return false;
    }

    char* start = readBuffer->data+readBuffer->readPos;

    int lineSize = end - start;
    
    if(lineSize)
    {
        // 获取请求行：/xxx/x.txt HTTP/1.1
        char* point = splitRequestLine(start, end, " ",&request->method);
        point = splitRequestLine(point,end," ",&request->url);
        splitRequestLine(point, end, NULL, &request->version);

        // 请求路径
        /* start = space + 1;
        space = memmem(start,end - start," ",1);
        assert(space !=NULL);
        int urlSize = space - start;    
        request->url = (char*)malloc(urlSize + 1);
        strncpy(request->url, start, methodSize);
        request->method[urlSize] = '\0';
        
        // 协议版本
        start = space + 1;
        request->version = (char*)malloc(sizeof(end-start+1));
        strncpy(request->version, start,end-start);
        request->method[end-start + 1] = '\0';
        */

        readBuffer->readPos += lineSize + 2;
        request->curState = ParseReqHeaders;
        return true;
    }    

    return false;   
}

// 每次只处理一行
bool parseHttpRequestHead(struct HttpRequest* request,struct Buffer* readBuffer)
{
    char* end = bufferFindCRLF(readBuffer);
    if(end == NULL)
    {
        return false;
    }
    char* start = readBuffer->data + readBuffer->readPos;
    int linSize = end - start;
    char* middle = memmem(start,linSize,": ",2);
    if(middle != NULL)
    {
        char* key = malloc(middle - start + 1);
        strncpy(key,start,middle - start);
        key[middle - start] = '\0';
        char* value = malloc(end - middle - 1);
        strncpy(value,middle + 2,end - middle - 2);
        value[end -middle - 2]= '\0';
        
        httpRequestAddHeader(request, key, value);
        readBuffer->readPos += linSize + 2;
    }
    else 
    {
        //请求投被解析完，跳过空行
        readBuffer->readPos += 2;
        //修改解析状态
        // 忽略 POST 请求
        request->curState = ParseReqDone;
    }
    return true;
}

bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuffer, struct HttpResponse* response,struct Buffer* sendBuffer, int socket)
{
    bool flag = true;
    while(request->curState != ParseReqDone)
    {
        switch(request->curState)
        {
        case ParseReqLine:
            flag = parseHttpRequestLine(request,readBuffer);
            break;
        case ParseReqHeaders:
            flag = parseHttpRequestHead(request, readBuffer);    
            break;
        case ParseReqBody:
            // 当前版本不处理请求体，直接结束请求解析
            request->curState = ParseReqDone;
            break;
        default:
            break;
        }
        if(!flag)
        {
            return flag;
        }
        //判断是否解析完毕
        if(request->curState == ParseReqDone)
        {
            //根据解析原始数据对客户端做出相应
            processHttpRequest(request,response);
            //组织数据发送给客户端
            httpResponsePrepareMsg(response, sendBuffer, socket);    
        }
    }
    //状态复原
    request->curState = ParseReqLine;
    return flag;
}

int hexToDec(char c)
{
    if(c >= '0' && c <= '9')
        return c-'0';
    if(c >='a' && c <= 'f')
        return c - 'a'+10;
    if(c >= 'A' && c<= 'F')
        return c - 'A'+10;
    return 0;
}



void decodeMsg(char* from,char* to)
{
    for(;*from != '\0';to++ ,from++)
    {
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            *to = hexToDec(from[1])*16 + hexToDec(from[2]);  
            from +=2;
        }
        else
        {
            *to = *from;
        }
    }
}


const char* getFileType(const char* name)
{
    // 从右侧查找点号
    const char* dot = strrchr(name, '.');
    if(dot == NULL)
        return "text/plain; cherset=utf-8";
    if(strcmp(dot,".html") == 0 || strcmp(dot,".htm") == 0)
        return "text/html; charset=utf-8";
    if(strcmp(dot,".jpg") == 0 || strcmp(dot,".jpeg") == 0)
        return "image/jpeg";
    if(strcmp(dot,".gif") == 0)
        return "image/gif";
    if(strcmp(dot,".png") == 0)
        return "image/png";
    if(strcmp(dot,".css") == 0)
        return "text/css";
    if(strcmp(dot,".au") == 0)
        return "audio/basic";
    if(strcmp(dot,".wav") == 0)
        return "audio/wav";
    if(strcmp(dot,".avi") == 0)
        return "video/avi";
    if(strcmp(dot,".mpeg") == 0)
        return "video/mpg";
    if(strcmp(dot,".mp3") == 0)
        return "audio/mp3";
    if(strcmp(dot,".pic") == 0)
        return "application/x-pic";
    if(strcmp(dot,".gif") == 0)
        return "image/gif";
    return "text/plain; cherset=utf-8";
}



// 基于 GET 处理 HTTP
bool processHttpRequest(struct HttpRequest* request,struct HttpResponse* response)
{
    if(strcasecmp(request->method,"get") != 0)
    {
        return -1;
    }
    decodeMsg(request->url, request->url);    
    char* file = NULL;
    if(strcmp(request->url,"/") == 0)
    {
       file = "./"; 
    }
    else 
    {
        file = request->url + 1;
    }

    struct stat st;
    int ret = stat(file, & st);
    if(ret == -1)
    {
        strcpy(response->fileName,"404.html");
        response->statusCode = NotFound;
        strcpy(response->statusMsg,"NotFound");
   
        HttpResponseAddHeader(response, "Content-type", getFileType(response->fileName)); 
        response->sendDataFunc = sendFile;
        return 0;
    }
    
    strcpy(response->fileName,file);
    response->statusCode = OK;
    strcpy(response->statusMsg,"OK");
    
    if(S_ISDIR(st.st_mode))
    {
        //目录内容发给客户端
   
        HttpResponseAddHeader(response, "Content-type", getFileType(".html")); 
        response->sendDataFunc = sendDir;
        return 0;
    }
    else 
    {
        //文件内容发给客户端
   
        char temp[12] = {0};
        sprintf(temp,"%ld", st.st_size);
        HttpResponseAddHeader(response, "Content-type", getFileType(file)); 
        HttpResponseAddHeader(response, "Content-length", temp); 
        response->sendDataFunc = sendFile;
        return 0;
    }

    return false;
}

void sendFile(const char* fileName,struct Buffer* sendBuffer ,int cfd)
{

    int fd = open(fileName,O_RDONLY);
    if(fd < 0)
    {
        const char* msg = "<html><body><h1>404 Not Found</h1></body></html>";
        bufferAppendString(sendBuffer, msg);
#ifndef MSG_SEND_AUTO
        bufferSendData(sendBuffer, cfd);
#endif
        return;
    }
#if 0
    while(1) 
    {
        char buff[1024];
        int len = read(fd,buff,sizeof(buff));
        if(len > 0)
        {
            //send(cfd, buff, len, 0);
            bufferAppendData(sendBuffer, buff,len);
#ifndef MSG_SEND_AUTO
            bufferSendData(sendBuffer, cfd);
#endif
        }
        else if(len == 0)
        {
            break;
        }
        else 
        {
            close(fd);
            perror("read");
        }
    } 
#else
    off_t offset = 0;
    int size = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET); //重置读取指针
    while(offset < size)
    {
        int res = sendfile(cfd, fd, &offset, size - offset);   
        if(res == -1 && errno != EAGAIN)//如果是阻塞模式就不用担心fd的打开和cfd的上传节奏跟不上的问题
        {
            perror("sendfile");
            close(fd);
            return ;
        }
    }
#endif
    close(fd);
}

void sendDir(const char* dirName,struct Buffer* sendBuffer, int cfd)
{
    char buff[4096] = { 0 };
    sprintf(buff,"<html><head><title>%s</title></head><body><table>",dirName); 
    struct dirent **namelist;
    int dirnum = scandir(dirName,&namelist, NULL, alphasort);
    for (int i = 0; i < dirnum; i++)
    {
        // 获取文件名
        
        char* name = namelist[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath,"%s/%s",dirName,name);
        stat(subPath,&st);
        if(S_ISDIR(st.st_mode))
        {
            sprintf(buff+strlen(buff),"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    name,name,st.st_size);
        }
        else 
        {
            sprintf(buff+strlen(buff),"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    name,name,st.st_size);   
        }
        
        bufferAppendString(sendBuffer, buff);
        bufferSendData(sendBuffer, cfd);
        memset(buff,0,sizeof(buff));
        free(namelist[i]);
    }
    sprintf(buff,"</table></body></html>");
    bufferAppendString(sendBuffer, buff);
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuffer, cfd);
#endif
    free(namelist);
}
#endif
