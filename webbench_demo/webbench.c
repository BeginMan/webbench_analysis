/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 *
 */
#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

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
char host[MAXHOSTNAMELEN];          // 服务器端ip， 在sys/param.h中，max hostname size，MAC OS X 是256

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

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
    timerexpired=1;
}

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
                tmp=strrchr(optarg,':');
                proxyhost=optarg;
                
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
                *tmp='\0';              // ???
                proxyport=atoi(tmp+1);  //去掉前面的:, 如果:9050
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
    
    /* Copyright */
    fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
            "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
            );
    
    build_request(argv[optind]);    // optind为最后一个元素索引，argv[optind]表示最后一个参数，URL.
    
    /* print bench info */
    printf("\nBenchmarking: ");
    
    switch(method)
    {
        case METHOD_GET:
        default:
            printf("GET");break;
        case METHOD_OPTIONS:
            printf("OPTIONS");break;
        case METHOD_HEAD:
            printf("HEAD");break;
        case METHOD_TRACE:
            printf("TRACE");break;
    }
    printf(" %s",argv[optind]);
    switch(http10)
    {
        case 0: printf(" (using HTTP/0.9)");break;
        case 2: printf(" (using HTTP/1.1)");break;
    }
    printf("\n");
    if(clients==1) printf("1 client");
    else
        printf("%d clients",clients);
    
    printf(", running %d sec", benchtime);
    if(force) printf(", early socket close");
    if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
    if(force_reload) printf(", forcing reload");
    printf(".\n");
    return bench();
}

void build_request(const char *url)
{
    char tmp[10];
    int i;
    
    bzero(host,MAXHOSTNAMELEN);
    bzero(request,REQUEST_SIZE);
    
    if(force_reload && proxyhost!=NULL && http10<1) http10=1;
    if(method==METHOD_HEAD && http10<1) http10=1;
    if(method==METHOD_OPTIONS && http10<2) http10=2;    // HTTP/1.1 new add
    if(method==METHOD_TRACE && http10<2) http10=2;      // HTTP/1.1 new add
    
    switch(method)
    {
        default:
        case METHOD_GET: strcpy(request,"GET");break;
        case METHOD_HEAD: strcpy(request,"HEAD");break;
        case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
        case METHOD_TRACE: strcpy(request,"TRACE");break;
    }
		  
    strcat(request," ");
    
    if(NULL==strstr(url,"://"))     // 检查 url的正确性
    {
        fprintf(stderr, "\n%s: is not a valid URL.\n",url);
        exit(2);
    }
    if(strlen(url)>1500)            // 控制url在最大长度
    {
        fprintf(stderr,"URL is too long.\n");
        exit(2);
    }
    if(proxyhost==NULL)
        if ( 0 != strncasecmp("http://", url, 7))
        {
            fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
            exit(2);
        }
    /* protocol/host delimiter */
    // 这段代码用于检查URL的合法性，如http://www.baidu.com/ 而非 http://www.baidu.com
    
    // strstr(url,"://")的值 ://www.a.com
    // strstr(url,"://")-url 为url对应位置索引，这里是4
    // strstr(url,"://")-url+3, 则表示url从索引为7处开始的字符串，即去掉http://,剩余www.baidu.com/ 的部分
    i=strstr(url,"://")-url+3;
    
    
    if(strchr(url+i,'/')==NULL) {   // 这里规定，URL要以/结尾，如http://xx.xxx.xx.xx:8000/api/doc
        fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }
    
    if(proxyhost==NULL)
    {
        /* get port from hostname */
        // 如url 为 http://www.baidu.com:8000/api 例子
        if(index(url+i,':')!=NULL && index(url+i,':') < index(url+i,'/'))   // 保证端口合法性
        {
            // 如 webbench -t 2 http://www.baidu.com:9000/api, 则host:www.baidu.com. 端口:9000
            // 获取主机名
            // strchr(url+i,':')为: :8000/api
            // strchr(url+i,':')-url 表示到:8000/api的索引位置
            // strchr(url+i,':')-url-i 则表示cp的内容恰好是去掉http://和:8000/api后的中间部分，得到的值：www.baidu.com
            strncpy(host, url+i, strchr(url+i,':')-url-i);
            
            // 获取端口
            // index(url+i,':')+1 得到的是 8000/api
            // strchr(url+i, '/') 得到的是 /api
            // strchr(url+i,'/')-index(url+i,':')-1 得到的数字是cp 8000/api的前四个字符，8000
            bzero(tmp,10);
            strncpy(tmp, index(url+i,':')+1, strchr(url+i,'/')-index(url+i,':')-1);
            proxyport=atoi(tmp);
            if(proxyport==0) proxyport=80;
        }
        else
        {
            // 如 webbench -t 2 http://www.baidu.com/, 则host:www.baidu.com. 端口:80
            strncpy(host, url+i, strcspn(url+i,"/"));
        }
        /*printf("Host=%s, port:%d\n",host, proxyport);*/
        strcat(request+strlen(request), url+i+strcspn(url+i,"/"));
    }
    else
    {
        // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
        strcat(request,url);
    }
    
    if(http10==1)
        strcat(request," HTTP/1.0");
    else if (http10==2)
        strcat(request," HTTP/1.1");
    strcat(request,"\r\n");
    if(http10>0)
        strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
    
    if(proxyhost==NULL && http10>0)
    {
        strcat(request,"Host: ");
        strcat(request,host);
        strcat(request,"\r\n");
    }
    
    if(force_reload && proxyhost!=NULL)
    {
        strcat(request,"Pragma: no-cache\r\n");
    }
    
    if(http10>1)
        strcat(request,"Connection: close\r\n");
    /* add empty line at end */
    if(http10>0) strcat(request,"\r\n");
//    printf("Req=%s\n",request);
}

/* vraci system rc error kod */
static int bench(void)
{
    int i,j,k;
    pid_t pid=0;
    FILE *f;
    
    /* check avaibility of target server */
    i=Socket(proxyhost==NULL?host:proxyhost,proxyport);
    if(i<0) {
        fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);
    /* create pipe */
    if(pipe(mypipe))
    {
        perror("pipe failed.");
        return 3;
    }
    
    /* not needed, since we have alarm() in childrens */
    /* wait 4 next system clock tick */
    /*
     cas=time(NULL);
     while(time(NULL)==cas)
     sched_yield();
     */
    
    /* fork childs */
    for(i=0;i<clients;i++)
    {
        pid=fork();
        if(pid <= (pid_t) 0)        // (pid_t) 0 类型转换
        {
            /* child process or error*/
            sleep(1); /* make childs faster ????*/
            break;
        }
    }
    
    if( pid< (pid_t) 0)
    {
        fprintf(stderr,"problems forking worker no. %d\n",i);
        perror("fork failed.");
        return 3;
    }
    
    if(pid== (pid_t) 0)
    {
        /* I am a child */
        // 子进程调用benchcore 发起请求
        if(proxyhost==NULL)
            benchcore(host,proxyport,request);
        else
            benchcore(proxyhost,proxyport,request);
        
        /* write results to pipe */
        f=fdopen(mypipe[1],"w");
        if(f==NULL)
        {
            perror("open pipe for writing failed.");
            return 3;
        }
        /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
        // 格式化写入管道， %d %d %d 依次是speed(成功数),failed(失败数),bytes(字节数)
        fprintf(f,"%d %d %d\n",speed,failed,bytes);
        fclose(f);
        return 0;
    } else                  // 父进程开始读管道数据进行统计
    {
        f=fdopen(mypipe[0],"r");
        if(f==NULL)
        {
            perror("open pipe for reading failed.");
            return 3;
        }
        setvbuf(f,NULL,_IONBF,0);  // 设置文件流缓冲区为无缓冲，直接从流中操作，数据更加及时更快些
        speed=0;
        failed=0;
        bytes=0;
        
        while(1)
        {
            pid=fscanf(f,"%d %d %d",&i,&j,&k);      // 格式化从管道读取流
            if(pid<2)
            {
                fprintf(stderr,"Some of our childrens died.\n");
                break;
            }
            speed+=i;
            failed+=j;
            bytes+=k;
            /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
            if(--clients==0) break;
        }
        fclose(f);
        
        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
               (int)((speed+failed)/(benchtime/60.0f)),
               (int)(bytes/(float)benchtime),
               speed,
               failed);
    }
    return i;
}

void benchcore(const char *host,const int port,const char *req)
{
    /*
     * 子进程请求处理函数
     */
    ssize_t rlen;       // 作者原：int类型，我觉得改成ssize_t更好些
    char buf[1500];
    int s,i;
    struct sigaction sa;
    
    /* setup alarm signal handler */
    sa.sa_handler=alarm_handler;  // 注册alarm_handler自定义函数，使用timerexpired全局变量跳出循环
    sa.sa_flags=0;
    if(sigaction(SIGALRM,&sa,NULL))     // 闹钟超时产生SIGALRM信号,则注册alarm_handler处理函数
        exit(3);                        // 成功则退出
    
    //调用alarm函数设定一个闹钟，告诉内核在seconds秒之后给当前进程发SIGALRM信号
    //这里处理的是请求超时问题
    alarm(benchtime);
    
    rlen=strlen(req);
    
nexttry:while(1)
{
    if(timerexpired)        // 超时则返回
    {
        if(failed>0)
        {
            /* fprintf(stderr,"Correcting failed by signal\n"); */
            failed--;
        }
        return;
    }
    
    s=Socket(host,port);
    
    if(s<0) { failed++; continue;}
    
    //write()返回实际写入的字节数. 当有错误发生时则返回-1, 错误代码存入errno 中.
    //注意不管成功失败都要记得close(s)
    if(rlen != write(s, req, rlen)) {failed++; close(s); continue;}
    
    if(http10==0)       // 针对http0.9做的特殊处理
        if(shutdown(s,1)) { failed++;close(s);continue;}
    
    if(force==0)        // 全局变量force表示是否需要等待读取从server返回的数据，0表示要等待读取
    {
        /* read all available data from socket */
        while(1)
        {
            if(timerexpired) break;         // 超时 break
            i=read(s,buf,1500);             // 从socket读取返回数据
            /* fprintf(stderr,"%d\n",i); */
            // read返回实际读取到的字节数, 如果返回0, 表示已到达文件尾或是无可读取的数据,当有错误发生时则返回-1
            if(i<0) 
            { 
                failed++;
                close(s);
                goto nexttry;
            }
            else
                if(i==0) break;
                else
                    bytes+=i;
        }
    }
    // 若顺利关闭则返回0, 发生错误时返回-1.
    if(close(s)) {failed++;continue;}   // 也就是close异常时..
    speed++;
}
}
