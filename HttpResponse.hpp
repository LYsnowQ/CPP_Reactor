#pragma once


#include "Buffer.h"


//用来组织回复给客户端的数据块
typedef void (*responseBody)(const char* fileName,struct Buffer*sendBuffer,int socket);

enum HttpStatusCode
{
    Unknown = 0,
    OK = 200,
    MovePermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404,
    ServerError = 501
};

struct ResponseHeader
{
    char key[32];
    char value[128];
};

struct HttpResponse
{
    //状态行：状态码、状态描述、版本
    int statusCode;
    char statusMsg[128];
    char fileName[128];
    //响应头 - 键值对
    struct ResponseHeader* headers;
    int headerNum;

    responseBody sendDataFunc;
};


struct HttpResponse* httpResponseInit();

void httpResponseDestory(struct HttpResponse* response);

void HttpResponseAddHeader(struct HttpResponse* response,const char* key,const char* value);

void httpResponsePrepareMsg(struct HttpResponse* response, struct Buffer* sendBuffer, int socket);


