#include "HttpResponse.h"
#include "Buffer.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>

#define ResHeaderSize 16

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
