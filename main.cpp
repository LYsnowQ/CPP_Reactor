#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "Log.h"
#include "TcpServer.h"



int main(int argc ,const char** argv)
{
    if(argc < 3)
    {
        printf("app port path");
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    
    chdir(argv[2]);
    LOG_DEBUG("资源定向成功，开始启动服务器...");
    struct TcpServer* server = tcpServerInit(port, 4);
    if(server == NULL)
    {
        fprintf(stderr, "服务器初始化失败，请检查端口占用和启动参数。\n");
        return -1;
    }
    tcpServerRun(server);

    return 0;
}
