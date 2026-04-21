#include "HttpResponse.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>

reactor::net::protocol::HttpResponse::HttpResponse(base::Buffer* buffer)
:ready_(15),response_(buffer)
{
    checkReady_();
}

void reactor::net::protocol::HttpResponse::setStateLine(const std::string& version,HttpStatusCode status,const std::string& statusMsg)
{
    if(version == "")
    {
        line_ = "HTTP/1.1 " + std::to_string(status) + " " + statusMsg + "\r\n";
    }
    else 
    {
        line_ = version + " " + std::to_string(status) + " " + statusMsg + "\r\n";
    }
}
 

void reactor::net::protocol::HttpResponse::addHeader(std::string key,std::string value)
{
    if(key == "" || value == "")
    { 
        throw std::system_error(
                    errno,
                    std::system_category(),
                    "didn't set key or value"
                );
    
    }
    headers_.emplace_back(std::move(key)+": " + std::move(value) + "\r\n");
}


void reactor::net::protocol::HttpResponse::addfile(std::string file)
{//只传文件名，后续发送时再打开处理
    files_.emplace_back(std::move(file));
}


uint16_t reactor::net::protocol::HttpResponse::getCheckReady()
{
    checkReady_();
    if(ready_ == static_cast<uint16_t>(ReadyCode::Ready))
    {
        std::cout<< "response complated\n";
        return 0;  
    }
    
    if(ready_ & static_cast<uint16_t>(ReadyCode::NoResponseLine))
    {
        std::cout<<"ResponseLine dosen't exist\n";
    }

    if(ready_ & static_cast<uint16_t>(ReadyCode::NoHeaders))
    {
        std::cout<<"ResponseHeaders dosen't exist\n";
    }

    if(ready_ & static_cast<uint16_t>(ReadyCode::NoFile))
    {
        std::cout<<"Responsefile dosen't exist\n";
    }

    if(ready_ & static_cast<uint16_t>(ReadyCode::NoResponseBuffer))
    {
        std::cout<<"ResponseBuffer dosen't exist\n";
    }
    return ready_;
}



void reactor::net::protocol::HttpResponse::checkReady_()
{
    if(!response_)
    {
        ready_ |= static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
    }
    else
    {
        ready_ |= ~static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
    }
    
    if(line_ == std::string())
    {
        ready_ |= static_cast<uint16_t>(ReadyCode::NoResponseLine);
    }
    else
    {
        ready_ |= ~static_cast<uint16_t>(ReadyCode::NoResponseLine);
    }
    
    if(headers_.empty())
    {
        ready_ |= static_cast<uint16_t>(ReadyCode::NoHeaders);
    }
    else
    {
        ready_ |= ~static_cast<uint16_t>(ReadyCode::NoHeaders);
    }
    
    if(files_.empty())
    {
        ready_ |= static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
    }
    else
    {
        ready_ |= ~static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
    }
}

#if 0
struct HttpResponse* httpResponseInit()
{
    struct HttpResponse* response = (struct HttpResponse*)malloc(sizeof(struct HttpResponse));
    response->headerNum = 0;
    int size = sizeof(struct HttpResponse)*ResHeaderSize;
    response->headers = (struct ResponseHeader*)malloc(size);
    response->statusCode = Unknown;


    bzero(response->headers,size);
    bzero(response->statusMsg,sizeof(response->statusMsg));
    bzero(response->fileName,sizeof(response->fileName));

    response->sendDataFunc = NULL;

    return response;
}


void httpResponseDestory(struct HttpResponse* response)
{
    if(response != NULL)
    {
        free(response->headers);
        free(response);
    }
}


void HttpResponseAddHeader(struct HttpResponse* response,const char* key,const char* value)
{
    if(response == NULL || key == NULL || value == NULL)
    {
        return;
    }
    strcpy(response->headers[response->headerNum].key,key);
    strcpy(response->headers[response->headerNum].value,value);
    response->headerNum++;
}


void httpResponsePrepareMsg(struct HttpResponse* response, struct Buffer* sendBuffer, int socket)
{
    //状态行 - 响应头 - 空行 - 回复数据
    
    char temp[1024] = {0};
    sprintf(temp, "HTTP/1.1 %d %s\r\n",response->statusCode,response->statusMsg);
    bufferAppendString(sendBuffer, temp);

    for(int i = 0; i < response->headerNum ; i++)
    {
        sprintf(temp,"%s: %s\r\n",response->headers[i].key, response->headers[i].value);
        bufferAppendString(sendBuffer, temp);
    }
    bufferAppendString(sendBuffer, "\r\n");
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuffer, socket);
#endif
    if(response->sendDataFunc != NULL)
    {
        response->sendDataFunc(response->fileName,sendBuffer,socket);
    }
}
#endif
