// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (PKaitch)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/main.c
// 本文件根据 Apache 许可证 2.0 版（"许可证"）授权；
// 除非符合许可证规定，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据本许可证分发的软件
// 是按"原样"分发的，没有任何明示或暗示的担保或条件。
// 请参阅许可证了解具体的权限和限制条款。

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// ptrace 注入
#include "pinject.h"

// 标准库头文件
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>

// 系统调用头文件
#include <sys/signal.h>
#include <sys/unistd.h>
#include <sys/signal.h>

// Linux内核头文件

struct user_opt _user_opt = {0};

// 主循环
static int is_running = 1;

// 处理用户信号
void sigint_handler(int signum) {
    is_running = 0;
    
    printf("程序关闭中...\n");
    return;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"pid",     required_argument, 0, 'p'},  // 进程ID
        {"category",required_argument, 0, 'c'},  // 断点类别
        {"type",    required_argument, 0, 't'},  // 断点类型
        {"len",     required_argument, 0, 'l'},  // 断点长度
        {"max-count",required_argument,0, 'm'},  // 最大命中次数
        {"version", no_argument,       0, 'v'},  // 版本以及版权信息
        {"help",    no_argument,       0, 'h'},  // 帮助
        {0,         0,                 0,   0},  // 哨兵数值
    };

    // 操作值
    int opt;  

    // 用户操作断点默认结构体
    _user_opt.bp_len = 4;         // 大小默认为4字节, 即一个int
    _user_opt.max_count = -1;     // 命中计数器最大为无限制
    _user_opt.bp_category = 'h';  // 默认断点调试器类别为硬件断点调试器
    strcpy(_user_opt.bp_type, "rw");


    while ((opt = getopt_long(argc, argv, "p:c:t:l:m:vh", long_options, NULL)) != -1) {
        switch (opt) {
            // 进程ID
            case 'p':
                _user_opt.pid = atoi(optarg);
                break;

            // 断点类别
            case 'c':
                _user_opt.bp_category = optarg[0];
                if (_user_opt.bp_category != 's' && _user_opt.bp_category != 'h') {
                    printf("[err] 无效的断点类别: %s\n", optarg);
                }
                break;

            // 断点类型
            case 't':
                strncpy(_user_opt.bp_type, optarg, sizeof(_user_opt.bp_type));
                break;

            // 断点长度
            case 'l':
                _user_opt.bp_len = atoi(optarg);
                if (_user_opt.bp_len < 0) {
                    printf("[err] 无效的断点长度: %s\n", optarg);
                    return 1;
                }
                break;
            
            // 最大命中计数器
            case 'm':
                
                break;

            // 显示版本和版权信息
            case 'v':
                print_version();
                exit(0);

            // 帮助
            case 'h':
                print_usage(argv[0]);
                exit(0);

            // 其他
            default:
                print_usage(argv[0]);
                exit(0);
        }
    }

    // 检查所需参数
    if (optind >= argc) {
        printf("[err] 缺少地址参数\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* addr_arg = argv[optind];

    // 解析地址参数
    if (strncmp(addr_arg, "0x", 2) == 0 || strncmp(addr_arg, "0X", 2) == 0) {
        // 十六进制地址
        _user_opt.bp_addr = strtoull(addr_arg, NULL, 16);
    } else {
        printf("[err] 地址必须为十六进制格式 (0x…) \n");
        return 1;
    }

    // 设置信号处理
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // 执行观察循环
    int ret = run_watch(_user_opt);
    if (ret < 0) {
        printf("[err] 执行断点数据检测失败.\n");
    }
    printf("[进程正常退出]\n");
    return ret;
}

// 观察循环
int run_watch(struct user_opt opt) {
    if (opt.pid < 0) {
        return -1;
    }

    // 硬件断点
    if (opt.bp_category == 'h') {
        // 创建硬件断点
        struct perf_event_attr attr = {0};
        attr.bp_addr = opt.bp_addr;
        attr.bp_type = hw_type(opt.bp_type);
        attr.bp_len  = hw_len(opt.bp_len);
        _user_opt.hw_bp_attr = create_hw_breakpoint(attr, opt.pid);
        if (!_user_opt.hw_bp_attr) {
            printf("type: %s, val: %d\n", opt.bp_type, attr.bp_type);
            printf("[err] 创建硬件断点失败.\n");
            perror("");
            return -1;
        }
        
        // 启用硬件断点
        enable_hw_breakpoint(_user_opt.hw_bp_attr -> hw_fd);
        // 硬件断点循环
        struct hw_breakpoint_sample hw_sample = {0};
        while (is_running) {
            if (wait_hw_breakpoint(_user_opt.hw_bp_attr)) {
                // 处理硬件断点
                if (handler_hw_breakpoint(_user_opt.hw_bp_attr, &hw_sample) < 0) {
                    printf("处理失败\n");
                    continue;
                }

                // 输出断点信息
                printf("\033[2J\033[H");
                fflush(stdout);
                printf("Hardware Breakpoint:\n");
                print_regs(hw_sample.regs);
                printf("命中计数器: %" PRIu64 "\n", hw_sample.hit_count);
                printf("Task id: %d\n",   hw_sample.tid);
            }
        }
    }

    // 软件断点
    if (opt.bp_category == 's') {
        // 附加进程
        if (pinject_attach(opt.pid) < 0) {
            printf("[err] 附加进程失败.\n");
            return -1;
        }
        
        // 系统调用预热
        if (!pinject_warmup()) {
            printf("[err] 系统调用预热失败.\n");
            return -1;
        }

        // 创建软件断点
        _user_opt.sw_bp_attr = create_sw_breakpoint(opt.pid, opt.bp_addr, sw_type(opt.bp_type), opt.bp_len);
        if (!_user_opt.sw_bp_attr) {
            printf("[err] 创建软件断点失败.\n");
            return -1;
        }

        // 软件断点循环
        struct sw_breakpoint_sample sw_sample = {0};
        while (is_running) {
            pinject_warmup();
            int sig = wait_sw_breakpoint(_user_opt.sw_bp_attr);
            if (sig != SIGTRAP && sig != SIGSEGV) {
                continue;
            }
            // 处理软件断点
            if (!handler_sw_breakpoint(_user_opt.sw_bp_attr, &sw_sample, sig)) {
                printf("跳出\n");
                continue;
            }

            // 输出断点信息
            printf("\033[2J\033[H");
            fflush(stdout);
            printf("Software Breakpoint:\n");
            print_regs(sw_sample.regs);
            printf("中断时信号: %" PRIu32 "\n", sw_sample.signal);
            printf("命中计数器: %" PRIu64 "\n", sw_sample.hit_count);
            usleep(1000);
        }
    }
    
    // 销毁硬件断点
    if (_user_opt.bp_category == 'h') {
        printf("已销毁硬件断点.\n");
        destroy_hw_breakpoint(_user_opt.hw_bp_attr);
    }

    // 销毁软件断点
    if (_user_opt.bp_category == 's') {
        printf("已销毁软件断点.\n");
        destroy_sw_breakpoint(_user_opt.sw_bp_attr);
    }

    return 0;
}

#ifdef __cplusplus
}
#endif