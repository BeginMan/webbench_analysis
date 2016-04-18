//
//  demo.c
//  webbench_demo
//
//  Created by 方朋 on 16/4/18.
//  Copyright © 2016年 beginman. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
    char *url = "http://www.baidu.com:8000/api";
    char *p;
    char *m;
    char host[30];
    int i;
    p = strstr(url, "://");         // ://www.a.com
    printf("%s\n", p);
    printf("%ld\n", p-url);         // 4, start url index
    i = p-url+3;                    // 7
    m = strchr(url+i,'/');          // 判断url第七个字符是否是'/',检查url的合法性
    printf("%s\n", m);
    printf(":> %s\n", strchr(url+i,':'));
    printf("%ld\n", strchr(url+i,':')-url-i);
    strncpy(host, url+i, strchr(url+i,':')-url-i);
    printf("host > %s\n", host);
    
    printf("%s\n", index(url+i,':'));       // :8000/api,  /api
    printf("%ld\n", strchr(url+i,'/')-index(url+i,':')-1);
    
}