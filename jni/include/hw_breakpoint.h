// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (PKaitch)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/hw_breakpoint.h
// 本文件根据 Apache 许可证 2.0 版（"许可证"）授权；
// 除非符合许可证规定，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据本许可证分发的软件
// 是按"原样"分发的，没有任何明示或暗示的担保或条件。
// 请参阅许可证了解具体的权限和限制条款。

#ifndef HW_BREAKPOINT_H
#define HW_BREAKPOINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <linux/types.h>
#include <linux/perf_event.h>


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
#error "未知的平台架构"
#endif

// 硬件断点属性结构体
struct hw_breakpoint_attr {
    int hw_fd;                     // 硬件断点文件描述符
    void* mmap_buffer;             // 环形缓冲区
    size_t mmap_size;              // 环形缓冲区大小
    pid_t pid;                     // 进程 ID
    struct perf_event_attr attr;   // 断点配置属性
};

// 用户存储的硬件断点采样数据类型
struct hw_breakpoint_sample {
    uint32_t pid;  // 进程 ID
    uint32_t tid;  // 任务 ID
    uint64_t abi;  // 应用程序二进制接口
    uint64_t addr; // 实际触发断点的地址
    uint64_t time; // 硬件断点发生的时间

    #if defined(__arm__)
    struct user_regs regs;  // 用户空间寄存器
    #else
    struct user_regs_struct regs;  // 用户空间寄存器结构
    #endif

    uint64_t hit_count; // 命中计数器
};

/**
 * @brief 创建硬件断点
 * @param attr 需要配置的信息
 * @param pid 需要配置的进程 ID
 * @return 成功返回硬件断点属性结构体指针，失败返回 NULL
 */
struct hw_breakpoint_attr* create_hw_breakpoint(struct perf_event_attr attr, pid_t pid);

/**
 * @brief 等待硬件断点事件
 * @param attr 硬件断点属性结构体指针
 * @return 成功返回 1（有事件），0（超时），失败返回 -1
 */
int wait_hw_breakpoint(struct hw_breakpoint_attr* attr);

/**
 * @brief 处理硬件断点事件并解析环形缓冲区
 * @param attr 硬件断点属性结构体指针
 * @param sample 接收硬件断点采样数据的结构体指针
 * @return 如果成功，返回成功处理的事件数量并将返回值写入参数中。如果失败，返回 -1
 */
int handler_hw_breakpoint(struct hw_breakpoint_attr* attr, struct hw_breakpoint_sample *sample);

/**
 * @brief 禁用硬件断点
 * @param fd 硬件断点文件描述符
 * @return void
 */
void disable_hw_breakpoint(int fd);

/**
 * @brief 启用硬件断点
 * @param fd 硬件断点文件描述符
 * @return void
 */
void enable_hw_breakpoint(int fd);

/**
 * @brief 销毁硬件断点
 * @param attr 硬件断点属性结构体指针
 */
void destroy_hw_breakpoint(struct hw_breakpoint_attr* attr);

#ifdef __cplusplus
}
#endif

#endif // ! hw_breakpoint.h