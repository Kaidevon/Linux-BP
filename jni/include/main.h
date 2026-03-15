// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (PKaitch)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/main.h
// 本文件根据 Apache 许可证 2.0 版（"许可证"）授权；
// 除非符合许可证规定，否则您不得使用此文件。
// 您可以在以下网址获取许可证副本：
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// 除非适用法律要求或书面同意，否则根据本许可证分发的软件
// 是按"原样"分发的，没有任何明示或暗示的担保或条件。
// 请参阅许可证了解具体的权限和限制条款。

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

// 断点调试器头文件
#include "hw_breakpoint.h"
#include "sw_breakpoint.h"

// 标准库头文件
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// 系统调用头文件
#include <sys/user.h>

// linux系统头文件
#include <linux/hw_breakpoint.h>

// 用户操作
struct user_opt {
    pid_t pid;  // 进程ID
    uint32_t bp_category;  // 断点类别
    
    /**
     * @struct 断点类型
     * r 读取操作
     * w 写入操作
     * x 执行操作
     * i Int3断点 (仅软件断点支持)
     */
    char bp_type[3];       // 断点类型
    uint64_t bp_addr;      // 断点地址
    uint32_t bp_len;       // 断点长度

    struct hw_breakpoint_attr *hw_bp_attr;  // 硬件断点
    struct sw_breakpoint_attr *sw_bp_attr;  // 软件断点

    /**
     * @brief 最大命中次数
     * -1  无限制
     * 123 如果hit count 等于 123 那么结束观察
     * 99  如果hit count 等于 99 那么结束观察
     */
    int64_t max_count;
};

/**
 * @brief 执行断点循环
 * @opt 用户操作结构体
 * @return 失败返回-1, 成功使用 Ctrl + C 终止.
 */
int run_watch(struct user_opt opt);

// 打印用法
static void print_usage(const char* prog_name) {
    printf("用法: %s [选项] <地址>\n", prog_name);
    printf("选项:\n");
    printf("  -p, --pid <pid>        目标进程ID\n");
    printf("  -c, --category <cate>  断点类别: s(software) 或 h(hardware)\n");
    printf("  -t, --type <type>      断点类型: execute(x)/                                                                                                           b bread(r)/write(w)/rw\n");
    printf("  -l, --len <len>        断点长度: 1/2/4/8\n");
    printf("  -v, --version          显示版本信息\n");
    printf("  -h, --help             显示帮助信息\n");
    printf("\n示例:\n");
    printf("  软件断点: %s -c s -p 1234 -t rw -l 4096 - 0x401000\n", prog_name);
    printf("  硬件断点: %s -c h -p 1234 -l 1 0x401000\n",    prog_name);
}

// 打印版权和版本信息
static void print_version(void) {
    printf("Kaidev Linux-breakpoint 2.0\n");

    // 平台相关的宏定义
    #if defined(__i386__)
    printf("为 x86_32 编译\n");
    #elif defined(__x86_64__)
    printf("为 x86_64 编译\n");
    #elif defined(__arm__)
    printf("为 ARMv7-a 编译\n");
    #elif defined(__aarch64__)
    printf("为 aarch64 编译\n");
    #else
    #error "未知架构"
    #endif

    printf("版权所有 (C) 2026 至今 Kaidev Core 开发者团队\n");
    printf("作者：Kaidevon <github.com/Kaidevon>\n");
    printf("许可证：Apache-2.0 <https://www.apache.org/licenses/LICENSE-2.0>\n");
    printf("本软件为自由软件，可自由修改和重新分发。\n");
    printf("在法律允许的范围内，不提供任何担保。\n");
    return;
}


// 打印寄存器
#if defined(__arm__)
static void print_regs(struct user_regs r) {
#else
static void print_regs(struct user_regs_struct r) {
#endif
    // 在终端输出寄存器
    // 定义紫色转义序列
    const char* purple = "\033[35m";  // 紫色
    const char* reset = "\033[0m";    // 重置
    
    int reg_count = 0;
    
#if defined(__i386__)
    // i386 寄存器
    printf("  %sEAX%s: 0x%08lx    %sEBX%s: 0x%08lx    %sECX%s: 0x%08lx    %sEDX%s: 0x%08lx\n",
       purple, reset, r.eax,
       purple, reset, r.ebx,
       purple, reset, r.ecx,
       purple, reset, r.edx);
    reg_count += 4;
        
    printf("  %sESI%s: 0x%08lx    %sEDI%s: 0x%08lx    %sEBP%s: 0x%08lx    %sESP%s: 0x%08lx\n",
       purple, reset, r.esi,
       purple, reset, r.edi,
       purple, reset, r.ebp,
       purple, reset, r.esp);
    reg_count += 4;
    
    printf("  %sEIP%s: 0x%08lx    %sEFLAGS%s: 0x%08lx    %sCS%s: 0x%08lx    %sSS%s: 0x%08lx\n",
           purple, reset, r.eip,
           purple, reset, r.eflags,
           purple, reset, r.xcs,
           purple, reset, r.xss);
    reg_count += 4;
    
#elif defined(__x86_64__)
    // x86_64 寄存器
    printf("  %sRAX%s: 0x%016lx  %sRBX%s: 0x%016lx  %sRCX%s: 0x%016lx  %sRDX%s: 0x%016lx\n",
       purple, reset, r.rax,
       purple, reset, r.rbx,
       purple, reset, r.rcx,
       purple, reset, r.rdx);
    reg_count += 4;
    
    printf("  %sRSI%s: 0x%016lx  %sRDI%s: 0x%016lx  %sRBP%s: 0x%016lx  %sRSP%s: 0x%016lx\n",
       purple, reset, r.rsi,
       purple, reset, r.rdi,
       purple, reset, r.rbp,
       purple, reset, r.rsp);
    reg_count += 4;
    
    printf("  %sR8 %s: 0x%016lx  %sR9 %s: 0x%016lx  %sR10%s: 0x%016lx  %sR11%s: 0x%016lx\n",
       purple, reset, r.r8,
       purple, reset, r.r9,
       purple, reset, r.r10,
       purple, reset, r.r11);
    reg_count += 4;
    
    printf("  %sR12%s: 0x%016lx  %sR13%s: 0x%016lx  %sR14%s: 0x%016lx  %sR15%s: 0x%016lx\n",
       purple, reset, r.r12,
       purple, reset, r.r13,
       purple, reset, r.r14,
       purple, reset, r.r15);
    reg_count += 4;
    
    printf("  %sRIP%s: 0x%016lx  %sCS%s: 0x%016lx  %sRFLAGS%s: 0x%016lx  %sSS%s: 0x%016lx\n",
       purple, reset, r.rip,
       purple, reset, r.cs,
       purple, reset, r.eflags,
       purple, reset, r.ss);
    reg_count += 4;
    
#elif defined(__arm__)
    // ARM 32-bit 寄存器
    printf("  %sR0 %s: 0x%08lx    %sR1 %s: 0x%08lx    %sR2 %s: 0x%08lx    %sR3 %s: 0x%08lx\n",
       purple, reset, r.uregs[0],
       purple, reset, r.uregs[1],
       purple, reset, r.uregs[2],
       purple, reset, r.uregs[3]);
    reg_count += 4;
    
    printf("  %sR4 %s: 0x%08lx    %sR5 %s: 0x%08lx    %sR6 %s: 0x%08lx    %sR7 %s: 0x%08lx\n",
       purple, reset, r.uregs[4],
       purple, reset, r.uregs[5],
       purple, reset, r.uregs[6],
       purple, reset, r.uregs[7]);
    reg_count += 4;
    
    printf("  %sR8 %s: 0x%08lx    %sR9 %s: 0x%08lx    %sR10%s: 0x%08lx    %sR11%s: 0x%08lx\n",
       purple, reset, r.uregs[8],
       purple, reset, r.uregs[9],
       purple, reset, r.uregs[10],
       purple, reset, r.uregs[11]);
    reg_count += 4;
    
    printf("  %sR12%s: 0x%08lx    %sSP %s: 0x%08lx    %sLR %s: 0x%08lx    %sPC %s: 0x%08lx\n",
       purple, reset, r.uregs[12],
       purple, reset, r.uregs[13],
       purple, reset, r.uregs[14],
       purple, reset, r.uregs[15]);
    reg_count += 4;
    
    printf("  %sCPSR%s: 0x%08lx\n",
       purple, reset, r.uregs[16]);
    reg_count += 1;
    
#elif defined(__aarch64__)
    // ARM64 寄存器
    printf("  %sX0 %s: 0x%016lx  %sX1 %s: 0x%016lx  %sX2 %s: 0x%016lx  %sX3 %s: 0x%016lx\n",
       purple, reset, r.regs[0],
       purple, reset, r.regs[1],
       purple, reset, r.regs[2],
       purple, reset, r.regs[3]);
    reg_count += 4;
    
    printf("  %sX4 %s: 0x%016lx  %sX5 %s: 0x%016lx  %sX6 %s: 0x%016lx  %sX7 %s: 0x%016lx\n",
       purple, reset, r.regs[4],
       purple, reset, r.regs[5],
       purple, reset, r.regs[6],
       purple, reset, r.regs[7]);
    reg_count += 4;
    
    printf("  %sX8 %s: 0x%016lx  %sX9 %s: 0x%016lx  %sX10%s: 0x%016lx  %sX11%s: 0x%016lx\n",
       purple, reset, r.regs[8],
       purple, reset, r.regs[9],
       purple, reset, r.regs[10],
       purple, reset, r.regs[11]);
    reg_count += 4;
    
    printf("  %sX12%s: 0x%016lx  %sX13%s: 0x%016lx  %sX14%s: 0x%016lx  %sX15%s: 0x%016lx\n",
       purple, reset, r.regs[12],
       purple, reset, r.regs[13],
       purple, reset, r.regs[14],
       purple, reset, r.regs[15]);
    reg_count += 4;
    
    printf("  %sX16%s: 0x%016lx  %sX17%s: 0x%016lx  %sX18%s: 0x%016lx  %sX19%s: 0x%016lx\n",
       purple, reset, r.regs[16],
       purple, reset, r.regs[17],
       purple, reset, r.regs[18],
       purple, reset, r.regs[19]);
    reg_count += 4;
    
    printf("  %sX20%s: 0x%016lx  %sX21%s: 0x%016lx  %sX22%s: 0x%016lx  %sX23%s: 0x%016lx\n",
       purple, reset, r.regs[20],
       purple, reset, r.regs[21],
       purple, reset, r.regs[22],
       purple, reset, r.regs[23]);
    reg_count += 4;
    
    printf("  %sX24%s: 0x%016lx  %sX25%s: 0x%016lx  %sX26%s: 0x%016lx  %sX27%s: 0x%016lx\n",
       purple, reset, r.regs[24],
       purple, reset, r.regs[25],
       purple, reset, r.regs[26],
       purple, reset, r.regs[27]);
    reg_count += 4;
    
    printf("  %sX28%s: 0x%016lx  %sFP %s: 0x%016lx  %sLR %s: 0x%016lx  %sSP %s: 0x%016lx\n",
       purple, reset, r.regs[28],
       purple, reset, r.regs[29],
       purple, reset, r.regs[30],
       purple, reset, r.sp);
    reg_count += 4;
    
    printf("  %sPC %s: 0x%016lx  %sPSTATE%s: 0x%016lx\n",
       purple, reset, r.pc,
       purple, reset, r.pstate);
    reg_count += 2;

#else
    // 未知架构
    printf("  %s警告: 未知架构, 无法显示寄存器%s\n",
       "\033[33m", reset);  // Yellow warning
#endif
    
    // 以分隔线结束
    printf("  --------------------------------%s\n", reset);
}

// 权限字符串到硬件断点权限数值
static uint32_t hw_type(const char* str) {
    uint32_t mode = 0;

    // 检查读取权限
    if (strstr(str, "r") != 0)
        mode |= HW_BREAKPOINT_R;
    
    // 检查写入权限
    if (strstr(str, "w") != 0)
        mode |= HW_BREAKPOINT_W;

    // 检查执行权限
    if (strstr(str, "x") != 0)
        mode |= HW_BREAKPOINT_X;

    return mode;
}

// 长度数字转换为长度按位和
static int hw_len(int len) {
    if (len == 1 || len == 2 || len == 4 || len == 8) {
        return len;
    }
    return -1;
}

// 权限字符串到软件断点权限数值
static uint32_t sw_type(const char* str) {
    uint32_t mode = 0;

    // 检查读取权限
    if (strstr(str, "r") != 0)
        mode |= SW_BREAKPOINT_NR;
    
    // 检查写入权限
    if (strstr(str, "w") != 0)
        mode |= SW_BREAKPOINT_NW;

    // 检查执行权限
    if (strstr(str, "x") != 0)
        mode |= SW_BREAKPOINT_NX;

    // 检查Int3模式
    if (strstr(str, "i") != 0)
        mode |= SW_BREAKPOINT_INT3;
    return mode;
}

#ifdef __cplusplus
}
#endif

#endif  // ! main.h