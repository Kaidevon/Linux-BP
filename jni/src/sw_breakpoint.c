// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (Linux-BP)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/sw_breakpoint.c
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

#include "sw_breakpoint.h"

// 标准库头文件
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

// 系统调用头文件
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/poll.h>
#include <sys/unistd.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

// Linux 内核头文件
#include <linux/types.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/elf.h>

// 依赖库文件
#include "pinject.h"

static long syscall_argv[3] = {0};

// 获取页权限
static int process_addrperm(pid_t pid, uint64_t addr) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    
    char line[1024];
    uint64_t start, end;
    char perms[5];
    int perm = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%" SCNx64 "-%" SCNx64 " %4s", &start, &end, perms) == 3) {
            if (addr >= start && addr < end) {
                fclose(fp);
                
                // 使用字符串表达式计算权限值
                // r=4, w=2, x=1, -=0
                for (int i = 0; i < 3; i++) {
                    switch (perms[i]) {
                        case 'r':
                            perm += 1;
                            break;
                        case 'w':
                            perm += 2;
                            break;
                        case 'x':
                            perm += 4;
                            break;
                        default:
                            break;  // '-' 或其他 = 0
                    }
                }
                
                return perm;
            }
        }
    }
    
    fclose(fp);
    return 0;  // 地址不在任何映射中
}

// 封装系统调用
static int p_mprotect(void *addr, size_t len, int prot) {
    // process mprotect
    syscall_argv[0] = (long)addr;
    syscall_argv[1] = (long)len;
    syscall_argv[2] = (long)prot;
    return (int)pinject_syscall(__NR_mprotect, 3, syscall_argv);
}

// 创建软件断点
struct sw_breakpoint_attr* create_sw_breakpoint(int pid, uint64_t addr, uint32_t type, uint32_t len) {
    if (get_tracee_pid() != pid) {
        // 确定目标和当前目标必须一致
        return NULL;
    }

    // 分配并清零软件断点资源句柄
    struct sw_breakpoint_attr* attr = malloc(sizeof(struct sw_breakpoint_attr));
    memset(attr, 0, sizeof(struct sw_breakpoint_attr));
    
    // 设置软件断点基础属性或者配置
    attr ->pid = pid;
    attr ->bp_addr = addr;
    attr ->bp_type = type;
    attr ->bp_len  = len;

    // 备份原数据
    attr -> original_pageperm = process_addrperm(pid, addr);
    attr -> original_addrdata = ptrace(PTRACE_PEEKTEXT, pid, (void*)addr, NULL);

    return attr;
}

// 安装断点并等待进程中断 
int wait_sw_breakpoint(struct sw_breakpoint_attr* attr) {
    if (get_tracee_pid() != attr -> pid) {
        // 确定目标和当前目标必须一致
        return -1;
    }
    
    // 安装代码断点
    // 如果同时安装INT3和NX断点 则强制失败
    if ((attr->bp_type & SW_BREAKPOINT_INT3) && (attr->bp_type & SW_BREAKPOINT_NX)) {
        return -1;
    }

    // 安装软件 不可操作 断点
    int addrperm = attr->original_pageperm;

    // 使用位清除操作
    if (attr->bp_type & SW_BREAKPOINT_NR) {
        addrperm &= ~PROT_READ;
    }
    if (attr->bp_type & SW_BREAKPOINT_NW) {
        addrperm &= ~PROT_WRITE;
    }

    // 对地址向下取整, 安装NP (no perm)断点
    p_mprotect((void*)(attr->bp_addr & ~0xFFF), (attr->bp_len + 0xFFF) & ~0xFFF, addrperm);
    
    // 安装INT3断点
    if (attr->bp_type & SW_BREAKPOINT_INT3 || attr->bp_type & SW_BREAKPOINT_NX) {
        // int3代码
        int int3_text[2];
        int3_text[1] = (int)((attr->original_addrdata >> 32) & 0xFFFFFFFF);
        
        #if defined(__aarch64__)
            int3_text[0] = 0xD4200000;
        #elif defined(__arm__)
            int3_text[0] = 0x0000BE01;
        #elif defined(__x86_64__) || defined(__i386__)
            int3_text[0] = 0xCC;
        #endif

        ptrace(PTRACE_POKETEXT, attr -> pid, attr -> bp_addr, *(long*)int3_text);
    }
    ptrace(PTRACE_CONT, attr->pid, NULL, NULL);
    
    // 中断进程中断并过滤指定信号
    int status = -1;
    waitpid(attr -> pid, &status, 0);
    return WSTOPSIG(status);
}

// 处理中断信号
int handler_sw_breakpoint(struct sw_breakpoint_attr* attr, struct sw_breakpoint_sample* sample, int sig) {
    if (get_tracee_pid() != attr -> pid) {
        // 确定目标和当前目标必须一致
        return -1;
    }

    // 设置采样器的中断信号
    sample -> signal = sig;

    // 处理INT3(TRAP) 中断
    if (sig == SIGTRAP) {
        // 获取寄存器
        if (pinject_get_regs(&sample -> regs) < 0) {
            // 如果获取寄存器失败
            return -1;
        }

        // 恢复现场
        ptrace(PTRACE_POKETEXT, attr->pid, attr -> bp_addr, attr -> original_addrdata);
        
        // 增加采样计数器
        sample -> hit_count ++;
    }
        
    // 处理SIGSEGV 中断
    if (sig == SIGSEGV) {
        // 获取寄存器
        if (pinject_get_regs(&sample -> regs) < 0) {
            // 如果获取寄存器失败
            return -1;
        }

        // 恢复现场
        p_mprotect((void*)attr -> bp_addr, (attr->bp_len + 0xFFF) & ~0xFFF, attr -> original_pageperm);
        
        // 增加采样计数器
        sample -> hit_count ++;
    }

    return 1;
}

void destroy_sw_breakpoint(struct sw_breakpoint_attr* attr) {
    // 销毁软件断点
    if (get_tracee_pid() != attr -> pid) {
        free(attr);
        return;
    }

    // 脱离附加
    pinject_detach();
    free(attr);
    return;
}

#ifdef __cplusplus
}
#endif
