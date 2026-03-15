// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (PKaitch)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/sw_breakpoint.h
// 本文件根据 Apache 许可证 2.0 版（"许可证"）授权；
// 除非符合许可证规定，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据本许可证分发的软件
// 是按"原样"分发的，没有任何明示或暗示的担保或条件。
// 请参阅许可证了解具体的权限和限制条款。

#ifndef SW_BREAKPOINT_H
#define SW_BREAKPOINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <linux/types.h>


// 平台相关宏定义
#if defined(__i386__)
#include <i686-linux-android/asm/perf_regs.h>
#define MAX_PERF_REGS PERF_REG_X86_64_MAX
#elif defined(__x86_64__)
#include <x86_64-linux-android/asm/perf_regs.h>
#define MAX_PERF_REGS PERF_REG_X86_64_MAX
#elif defined(__arm__)
#include <arm-linux-androideabi/asm/perf_regs.h>
#define MAX_PERF_REGS PERF_REG_ARM_MAX
#elif defined(__aarch64__)
#include <aarch64-linux-android/asm/perf_regs.h>
#define MAX_PERF_REGS PERF_REG_ARM64_MAX
#else
#error "Unknown platform"  // 未知的平台
#endif

// 断点长度枚举
enum {
    SW_BREAKPOINT_LEN_4 = 4,
	SW_BREAKPOINT_LEN_8 = 8
};

// 断点类型枚举
enum {
    SW_BREAKPOINT_EMPTY = 0,  // 空
    SW_BREAKPOINT_NR    = 1,  // 禁止读取操作
    SW_BREAKPOINT_NW    = 2,  // 禁止写入操作
    SW_BREAKPOINT_NX    = 4,  // 禁止执行操作
    SW_BREAKPOINT_INT3  = 8,  // INT3软件断点
    SW_BREAKPOINT_NRW   = SW_BREAKPOINT_NR | SW_BREAKPOINT_NW,  // 读写断点
};
// 软件断点属性结构体
struct sw_breakpoint_attr {
    uint32_t original_pageperm;    // 原始内存页权限
    uint64_t original_addrdata;    // 原始地址代码
    pid_t pid;                     // 进程 ID
    uint64_t bp_addr;  // 断点地址
    uint32_t bp_type;  // 断点类型
    uint32_t bp_len;   // 断点大小
    int signal;
};

// 用户存储的软件断点采样数据类型
struct sw_breakpoint_sample {
    uint64_t addr; // 实际触发断点的地址
    int signal;  // 中断信号
    
    #if defined(__arm__)
    struct user_regs regs;  // 用户空间寄存器
    #else
    struct user_regs_struct regs;  // 用户空间寄存器结构
    #endif

    uint64_t hit_count; // 命中计数器
};

/**
 * @brief 创建软件断点
 * @pid  目标进程pid
 * @addr 断点地址
 * @type 断点类型
 * @len  断点长度
 * @return 成功返回软件断点结构体, 失败返回NULL
 */
struct sw_breakpoint_attr* create_sw_breakpoint(int pid, uint64_t addr, uint32_t type, uint32_t len);

/**
 * @brief 关闭软件断点
 * @return void
 */
void disable_sw_breakpoint(struct sw_breakpoint_attr* attr, struct sw_breakpoint_sample* sample);

/**
 * @brief 启用软件断点
 * @return void
 */
void enable_sw_breakpoint(struct sw_breakpoint_attr* attr);

/**
 * @brief 安装软件断点并等待进程中断
 * @attr  软件断点配置结构体
 * @return 成功返回使进程中断的信号, 失败返回-1
 */
int wait_sw_breakpoint(struct sw_breakpoint_attr* attr);

/**
 * @brief 处理软件断点中断
 * @attr  软件断点配置结构体
 * @sample 软件断点采样器
 * @status 中断信号
 * @return 成功返回目标进程的寄存器并写入采样器
 */
int handler_sw_breakpoint(struct sw_breakpoint_attr* attr, struct sw_breakpoint_sample* sample, int status);

/**
 * @brief 销毁软件断点
 * @attr 软件断点配置结构体
 * @return void
 */
void destroy_sw_breakpoint(struct sw_breakpoint_attr* attr);

#ifdef __cplusplus
}
#endif

#endif // ! sw_breakpoint.h