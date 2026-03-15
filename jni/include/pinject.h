// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (PKaitch)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/pinject.h
// 本文件根据 Apache 许可证 2.0 版（"许可证"）授权；
// 除非符合许可证规定，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据本许可证分发的软件
// 是按"原样"分发的，没有任何明示或暗示的担保或条件。
// 请参阅许可证了解具体的权限和限制条款。

#ifndef PINJECT_H  // Process Inject
#define PINJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>

/**
 * @brief 附加到目标进程
 * @param pid 目标进程ID
 * @return 成功返回0，失败返回-1
 */
int pinject_attach(pid_t pid);

/**
 * @brief 从目标进程分离
 * @return 成功返回0，失败返回-1
 */
int pinject_detach(void);
/**
 * @brief 系统调用预热
 * @return 成功返回1, 失败返回0
 */
int pinject_warmup();
/**
 * @brief 获取当前被跟踪进程的PID
 * @return 被跟踪进程ID，未跟踪时返回0
 */
pid_t get_tracee_pid(void);

/**
 * @brief 获取目标进程寄存器
 * @param regs 用于存储寄存器的结构体指针
 * @return 成功返回0，失败返回-1
 */
#if defined(__arm__)
int pinject_get_regs(struct user_regs* regs);
#else
int pinject_get_regs(struct user_regs_struct* regs);
#endif

/**
 * @brief 设置目标进程寄存器
 * @param regs 包含新寄存器值的指针
 * @return 成功返回0，失败返回-1
 */
#if defined(__arm__)
int pinject_set_regs(const struct user_regs* regs);
#else
int pinject_set_regs(const struct user_regs_struct* regs);
#endif

/**
 * @brief 注入系统调用
 * 
 * @param syscall_num 系统调用代码
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 成功返回返回值, 失败返回-1, 并打印错误信息
 * 
 * @note 遵循迪米特法则，所以使用时包装
 */
long pinject_syscall(long syscall_num, int argc, long argv[]);

/**
 * @brief 向目标进程注入函数调用
 * @param pid 目标进程ID
 * @param func_addr 要调用的函数地址
 * @param ret_addr 返回地址
 * @param argc 参数数量
 * @param argv 参数数组
 * @param ret_sig 期望的返回信号
 * @return 成功返回x0寄存器值，失败返回-1
 * 
 * @warning 应为跨平台原因，会在下一个版本提交
 */
long pinject_funcall(uintptr_t func_addr, uintptr_t ret_addr, 
                    int argc, long argv[], int ret_sig);

/**
 * @brief 向目标进程注入shellcode
 * @param pid 目标进程ID
 * @param shellcode_addr shellcode地址
 * @param ret_sig 返回信号
 * @return 成功返回0，失败返回-1
 * 
 * @warning 应为跨平台原因，会在下一个版本提交
 */
int pinject_shellcode(pid_t pid, uintptr_t shellcode_addr, int ret_sig);

#ifdef __cplusplus
}
#endif

#endif // ！pinject.h