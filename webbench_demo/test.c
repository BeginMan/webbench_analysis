//
//  test.c
//  webbench_demo
//
//  Created by 方朋 on 16/4/17.
//  Copyright © 2016年 beginman. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>


/* values */
volatile int timerexpired=0;        //判断压测时长是否已经到达设定的时间
int speed=0;                        //记录进程成功得到服务器响应的数量
int failed=0;                       //记录失败的数量（speed表示成功数，failed表示失败数）
int bytes=0;                        //记录进程成功读取的字节数

/* globals */
int http10=1;                       //http版本，0表示http0.9，1表示http1.0，2表示http1.1

/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"

int method=METHOD_GET;              //默认请求方式为GET，也支持HEAD、OPTIONS、TRACE

int clients=1;                      //并发数目，默认只有1个进程发请求，通过-c参数设置
int force=0;                        //是否需要等待读取从server返回的数据，0表示要等待读取
int force_reload=0;                 //是否使用缓存，1表示不缓存，0表示可以缓存页面
int proxyport=80;                   //代理服务器的端口
char *proxyhost=NULL;               //代理服务器的ip
int benchtime=30;                   //压测时间，默认30秒，通过-t参数设置

/* internal */
int mypipe[2];                      //使用管道进行父进程和子进程的通信
char host[256];          // 服务器端ip， 在sys/param.h中，max hostname size，MAC OS X 是256

#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];         //所要发送的http请求


static const struct option long_options[]=
{
    {"force",   no_argument,        &force,         1},
    {"reload",  no_argument,        &force_reload,  1},
    {"time",    required_argument,  NULL,           't'},
    {"help",    no_argument,        NULL,           '?'},
    {"http09",  no_argument,        NULL,           '9'},
    {"http10",  no_argument,        NULL,           '1'},
    {"http11",  no_argument,        NULL,           '2'},
    {"get",     no_argument,        &method,        METHOD_GET},
    {"head",    no_argument,        &method,        METHOD_HEAD},
    {"options", no_argument,        &method,        METHOD_OPTIONS},
    {"trace",   no_argument,        &method,        METHOD_TRACE},
    {"version", no_argument,        NULL,           'V'},
    {"proxy",   required_argument,  NULL,           'p'},
    {"clients", required_argument,  NULL,           'c'},
    {NULL,      0,                  NULL,           0}
};

static void usage(void)
{
    fprintf(stderr,
            "webbench [option]... URL\n"
            "  -f|--force               Don't wait for reply from server.\n"
            "  -r|--reload              Send reload request - Pragma: no-cache.\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -p|--proxy <server:port> Use proxy server for request.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -9|--http09              Use HTTP/0.9 style requests.\n"
            "  -1|--http10              Use HTTP/1.0 protocol.\n"
            "  -2|--http11              Use HTTP/1.1 protocol.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  --options                Use OPTIONS request method.\n"
            "  --trace                  Use TRACE request method.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n"
            );
};

int main(int argc, char *argv[])
{
    int opt=0;              //getopt_long 返回的字符选项
    int options_index=0;    //getopt_long最后一个参数，一般为NULL
    char *tmp=NULL;
    
    if(argc==1)
    {
        usage();
        return 2;
    }
    
    while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF )
    {
        switch(opt)
        {
            case  0 : break;
            case 'f': force=1;break;
            case 'r': force_reload=1;break;
            case '9': http10=0;break;
            case '1': http10=1;break;
            case '2': http10=2;break;
            case 'V': printf(PROGRAM_VERSION"\n");exit(0);
            case 't': benchtime=atoi(optarg);break;
            case 'p':
                /* proxy server parsing server:port ,<server:port>*/
                printf("optarg:%s\n", optarg);
                
                tmp=strrchr(optarg,':');
                
                printf("tmp:%s\n", tmp);

                proxyhost=optarg;
                printf("new_optarg:%s\n", optarg);
                printf("proxyhost:%s\n", proxyhost);
                
                if(tmp==NULL)
                {
                    break;
                }
                if(tmp==optarg)
                {
                    fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                    return 2;
                }
                if(tmp==optarg+strlen(optarg)-1)
                {
                    fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                    return 2;
                }
                printf("o+1:%s\n", (optarg+strlen(optarg)-1));
                *tmp='\0';      // ???
                proxyport=atoi(tmp+1);      // 去掉前面的:, 如果:9050
                printf("port:%d, host=%s\n", proxyport, proxyhost);
                printf("*******************\n");
                break;
            case ':':
            case 'h':
            case '?': usage();return 2;break;
            case 'c': clients=atoi(optarg);break;
        }
    }
    
    if(optind==argc) {
        fprintf(stderr,"webbench: Missing URL!\n");
		      usage();
		      return 2;
    }
    
    if(clients==0) clients=1;
    if(benchtime==0) benchtime=60;
    
    printf("http10:%d\n"
           "method:%d\n"
           "clients:%d\n"
           "force:%d\n"
           "force_reload:%d\n"
           "proxyport:%d\n"
           "benchtime:%d\n"
           "proxyhost:%s\n",
           http10,method,clients,force,force_reload, proxyport, benchtime,proxyhost);
}



