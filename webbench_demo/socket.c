//
//  socket.c
//  webbench_demo
//
//  Created by 方朋 on 16/4/17.
//  Copyright © 2016年 beginman. All rights reserved.
// 这种代码注释风格值得借鉴

/***********************************************************************
 module:        socket.c
 program:       popclient
 compiler:      DEC RISC C compiler (Ultrix 4.1)
 environment:   DEC Ultrix 4.3
 description:   UNIX sockets code.
 ***********************************************************************/

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>


int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hostent *hp;
    
    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    
    inaddr = inet_addr(host);
    if (inaddr != INADDR_NONE) {        // 是正常的网络地址字符串
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    } else {                            // 是域名
        hp = gethostbyname(host);
        if (hp == NULL) {
            return -1;
        }
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    ad.sin_port = htons(clientPort);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return sock;
    }
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0) {
        return -1;
    }
    return sock;
}
