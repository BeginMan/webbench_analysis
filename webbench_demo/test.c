//
//  test.c
//  webbench_demo
//
//  Created by 方朋 on 16/4/17.
//  Copyright © 2016年 beginman. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

int main(int argc, char **argv)
{
    int opt;
    int other;
    char const *opts = "w:lt::012";     // who love what
    
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    
    struct option long_options[] = {
        {"who", required_argument, NULL, 'w'},  // 需要参数，往往返回 optarg，然后进行处理
        {"love", no_argument, NULL, 'l'},       // 不需要参数，往往程序内设定
        {"what", optional_argument, NULL, 't'}, // 可选
        {"other", no_argument, &other, 1},      // 将1写入other空间中,注意只能是int型指针
        {NULL, 0, NULL, 0} // or { 0,0,0,0 }    // 此数组的最后一个须将成员都置为0。
    };
    
    while ((opt = getopt_long(argc, argv, opts, long_options, NULL)) != -1) {
        switch (opt) {
             
            case '0':
            case '1':
            case '2':
                printf("012.....\n");
                break;
            
            case 'w':
                printf("opt: %s, other:%d\n", optarg, other);
                break;
                
            case 'l':
                printf("%s...\n", long_options[optind-1].name);
                break;
            
            case 't':
                printf("opt:%s, other:%d\n", optarg, other);
            
            case '?':
                break;
                
            default:
                printf("?? getopt returned character code 0%o ??\n", opt);
                break;
        }
        
        option_index = optind;
        printf("optind:%d, value:%s\n", option_index, argv[optind++]);
    }
    return 0;
}
