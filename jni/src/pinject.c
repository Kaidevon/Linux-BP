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

#ifdef __cplusplus
extern "C" {
#endif

#include "pinject.h"

// 标准库头文件

#include <stdlib.h>
#include <errno.h>
#include <string.h>

// 系统调用头文件
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/unistd.h>

// linux内核头文件
#include <linux/elf.h>

static pid_t tracee_pid = 0;

// 附加进程
int pinject_attach(pid_t pid) {
    // 附加进程
    int ret = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    if (ret < 0) {
        perror("ptrace(ATTACH) failed");
        return -1;
    }
    
    // 等待进程停止
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid failed");
        return -1;
    }
    
    // 验证进程确实停止了
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "Process did not stop as expected\n");
        return -1;
    }

    // 成功附加到 pid
    tracee_pid = pid;
    printf("[+] Attached to PID %d\n", pid);
    return 0;
}

// 脱离进程
int pinject_detach() {
    // 脱离附加进程
    int ret = ptrace(PTRACE_DETACH, tracee_pid, NULL, NULL);
    if (ret < 0) {
        perror("ptrace(DETACH) failed");
        return -1;
    }

    printf("[+] Detached from PID %d\n", tracee_pid);
    tracee_pid = 0;
    return 0;
}
pid_t get_tracee_pid() {
    return tracee_pid;
}

// 系统调用预热
int pinject_warmup() {
    int status = 0;

    // 初始化部分PTRACE状态
    ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL);
    waitpid(tracee_pid, &status, 0);

    // 完全初始化PTRACE状态
    ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL);
    waitpid(tracee_pid, &status, 0);

    return WIFSTOPPED(status);
}

// 获取或者写入寄存器
#if defined(__arm__)
    int pinject_get_regs(struct user_regs* regs) {
        if (tracee_pid <= 0 || regs == NULL) {
            return -1;
        }
        
        struct iovec iov;
        iov.iov_base = regs;
        iov.iov_len = sizeof(struct user_regs);
        
        if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov) < 0) {
            return -1;
        }
        return 0;
    }
    
    int pinject_set_regs(const struct user_regs* regs) {
        if (tracee_pid <= 0 || regs == NULL) {
            return -1;
        }
        
        struct iovec iov;
        iov.iov_base = (void*)regs;
        iov.iov_len = sizeof(struct user_regs);
        
        if (ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov) < 0) {
            return -1;
        }
        return 0;
    }
#else
    int pinject_get_regs(struct user_regs_struct* regs) {
        if (tracee_pid <= 0 || regs == NULL) {
            return -1;
        }
        
        struct iovec iov;
        iov.iov_base = regs;
        iov.iov_len = sizeof(struct user_regs_struct);
        
        if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov) < 0) {
            return -1;
        }
        return 0;
    }
    
    int pinject_set_regs(const struct user_regs_struct* regs) {
        if (tracee_pid <= 0 || regs == NULL) {
            return -1;
        }
        
        struct iovec iov;
        iov.iov_base = (void*)regs;
        iov.iov_len = sizeof(struct user_regs_struct);
        
        if (ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov) < 0) {
            return -1;
        }
        return 0;
    }
#endif

// 注入系统调用
long pinject_syscall(long syscall_num, int argc, long argv[]) {
#if defined(__aarch64__)
    struct user_regs_struct old_regs, new_regs;
    struct iovec iov;
    int status;
    long retval = -1;
    
    iov.iov_base = &old_regs;
    iov.iov_len = sizeof(old_regs);
    if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov) == -1) {
        perror("PTRACE_GETREGSET failed");
        return -1;
    }
    
    // 如果在内核空间，让当前系统调用完成
    if (old_regs.pc > 0x800000000000) {
        ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL);
        waitpid(tracee_pid, &status, 0);
        ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov);
    }
    
    memcpy(&new_regs, &old_regs, sizeof(struct user_regs_struct));
    new_regs.regs[8] = syscall_num;  // x8 存放系统调用号
    
    for (int i = 0; i < argc && i < 6; i++) {
        new_regs.regs[i] = argv[i];  // x0-x5 存放参数
    }
    new_regs.pstate = 0x0;
    
    long original_instruction = ptrace(PTRACE_PEEKTEXT, tracee_pid, (void*)old_regs.pc, NULL);
    if (original_instruction == -1 && errno) {
        perror("PTRACE_PEEKTEXT failed");
        return -1;
    }
    
    // SVC #0 指令
    if (ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.pc, (void*)0xD4000001) == -1) {
        perror("PTRACE_POKETEXT failed");
        return -1;
    }
    
    iov.iov_base = &new_regs;
    iov.iov_len = sizeof(new_regs);
    if (ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov) == -1) {
        perror("PTRACE_SETREGSET failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.pc, (void*)original_instruction);
        return -1;
    }
    
    // 系统调用进入
    if (ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL) == -1) {
        perror("PTRACE_SYSCALL 1 failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.pc, (void*)original_instruction);
        iov.iov_base = &old_regs;
        iov.iov_len = sizeof(old_regs);
        ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
        return retval;
    }
    
    waitpid(tracee_pid, &status, 0);
    if (WIFSTOPPED(status)) {
        // 系统调用退出
        if (ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL) == -1) {
            perror("PTRACE_SYSCALL 2 failed");
            ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.pc, (void*)original_instruction);
            iov.iov_base = &old_regs;
            iov.iov_len = sizeof(old_regs);
            ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
            return retval;
        }
        
        waitpid(tracee_pid, &status, 0);
        if (WIFSTOPPED(status)) {
            struct user_regs_struct result_regs;
            struct iovec iov_result = {&result_regs, sizeof(result_regs)};
            
            if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov_result) == -1) {
                perror("PTRACE_GETREGS after syscall failed");
            } else {
                retval = result_regs.regs[0];
            }
        }
    }
    
    ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.pc, (void*)original_instruction);
    iov.iov_base = &old_regs;
    iov.iov_len = sizeof(old_regs);
    ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
    
    return retval;
#elif defined(__arm__)
    struct user_regs old_regs, new_regs;
    struct iovec iov;
    int status;
    long retval = -1;
    
    iov.iov_base = &old_regs;
    iov.iov_len = sizeof(old_regs);
    if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov) == -1) {
        perror("PTRACE_GETREGSET failed");
        return -1;
    }
    
    // 如果在内核空间，让当前系统调用完成
    if (old_regs.uregs[15] > 0x80000000) {  // PC 在用户空间通常小于 0x80000000
        ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL);
        waitpid(tracee_pid, &status, 0);
        ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov);
    }
    
    memcpy(&new_regs, &old_regs, sizeof(struct user_regs));
    new_regs.uregs[7] = syscall_num;  // r7 存放系统调用号 (ARM EABI)
    
    for (int i = 0; i < argc && i < 6; i++) {
        new_regs.uregs[i] = argv[i];  // r0-r5 存放参数
    }
    
    // 保存原指令
    long original_instruction = ptrace(PTRACE_PEEKTEXT, tracee_pid, (void*)old_regs.uregs[15], NULL);
    if (original_instruction == -1 && errno) {
        perror("PTRACE_PEEKTEXT failed");
        return -1;
    }
    
    // SVC #0 指令 (ARM 32位)
    if (ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.uregs[15], (void*)0xEF000000) == -1) {
        perror("PTRACE_POKETEXT failed");
        return -1;
    }
    
    iov.iov_base = &new_regs;
    iov.iov_len = sizeof(new_regs);
    if (ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov) == -1) {
        perror("PTRACE_SETREGSET failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.uregs[15], (void*)original_instruction);
        return -1;
    }
    
    // 系统调用进入
    if (ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL) == -1) {
        perror("PTRACE_SYSCALL 1 failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.uregs[15], (void*)original_instruction);
        iov.iov_base = &old_regs;
        iov.iov_len = sizeof(old_regs);
        ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
        return retval;
    }
    
    waitpid(tracee_pid, &status, 0);
    
    if (WIFSTOPPED(status)) {
        // 系统调用退出
        if (ptrace(PTRACE_SYSCALL, tracee_pid, NULL, NULL) == -1) {
            perror("PTRACE_SYSCALL 2 failed");
            ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.uregs[15], (void*)original_instruction);
            iov.iov_base = &old_regs;
            iov.iov_len = sizeof(old_regs);
            ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
            return retval;
        }
        
        waitpid(tracee_pid, &status, 0);
        
        if (WIFSTOPPED(status)) {
            struct user_regs result_regs;
            struct iovec iov_result = {&result_regs, sizeof(result_regs)};
            
            if (ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &iov_result) == -1) {
                perror("PTRACE_GETREGS after syscall failed");
            } else {
                retval = result_regs.uregs[0];  // r0 存放返回值
                printf("系统调用返回: %ld\n", retval);
            }
        }
    }
    
    ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.uregs[15], (void*)original_instruction);
    iov.iov_base = &old_regs;
    iov.iov_len = sizeof(old_regs);
    ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &iov);
    
    return retval;
#elif defined(__i386__)
    struct user_regs_struct old_regs, new_regs;
    int status;
    long retval = -1;
    
    if (ptrace(PTRACE_GETREGS, tracee_pid, NULL, &old_regs) == -1) {
        perror("PTRACE_GETREGS failed");
        return -1;
    }
    
    // 保存原指令
    long original_instruction = ptrace(PTRACE_PEEKTEXT, tracee_pid, (void*)old_regs.eip, NULL);
    if (original_instruction == -1 && errno) {
        perror("PTRACE_PEEKTEXT failed");
        return -1;
    }
    
    // INT 0x80 指令
    if (ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.eip, (void*)0xCD80CD80) == -1) {
        perror("PTRACE_POKETEXT failed");
        return -1;
    }
    
    memcpy(&new_regs, &old_regs, sizeof(struct user_regs_struct));
    new_regs.eax = syscall_num;  // eax 存放系统调用号
    
    for (int i = 0; i < argc && i < 6; i++) {
        switch (i) {
            case 0: new_regs.ebx = argv[i]; break;
            case 1: new_regs.ecx = argv[i]; break;
            case 2: new_regs.edx = argv[i]; break;
            case 3: new_regs.esi = argv[i]; break;
            case 4: new_regs.edi = argv[i]; break;
            case 5: new_regs.ebp = argv[i]; break;
        }
    }
    
    if (ptrace(PTRACE_SETREGS, tracee_pid, NULL, &new_regs) == -1) {
        perror("PTRACE_SETREGS failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.eip, (void*)original_instruction);
        return -1;
    }
    
    // 执行系统调用
    if (ptrace(PTRACE_CONT, tracee_pid, NULL, NULL) == -1) {
        perror("PTRACE_CONT failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.eip, (void*)original_instruction);
        return retval;
    }
    
    waitpid(tracee_pid, &status, 0);
    
    if (WIFSTOPPED(status)) {
        if (ptrace(PTRACE_GETREGS, tracee_pid, NULL, &new_regs) == -1) {
            perror("PTRACE_GETREGS after syscall failed");
        } else {
            retval = new_regs.eax;
            printf("系统调用返回: %ld\n", retval);
        }
    }
    
    ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.eip, (void*)original_instruction);
    ptrace(PTRACE_SETREGS, tracee_pid, NULL, &old_regs);
    
    return retval;
#elif defined(__x86_64__)
    struct user_regs_struct old_regs, new_regs;
    int status;
    long retval = -1;
    
    if (ptrace(PTRACE_GETREGS, tracee_pid, NULL, &old_regs) == -1) {
        perror("PTRACE_GETREGS failed");
        return -1;
    }
    
    // 保存原指令
    long original_instruction = ptrace(PTRACE_PEEKTEXT, tracee_pid, (void*)old_regs.rip, NULL);
    if (original_instruction == -1 && errno) {
        perror("PTRACE_PEEKTEXT failed");
        return -1;
    }
    
    // SYSCALL 指令 (机器码: 0F 05)
    if (ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.rip, (void*)0x050F) == -1) {
        perror("PTRACE_POKETEXT failed");
        return -1;
    }
    
    memcpy(&new_regs, &old_regs, sizeof(struct user_regs_struct));
    new_regs.rax = syscall_num;  // rax 存放系统调用号
    
    for (int i = 0; i < argc && i < 6; i++) {
        switch (i) {
            case 0: new_regs.rdi = argv[i]; break;
            case 1: new_regs.rsi = argv[i]; break;
            case 2: new_regs.rdx = argv[i]; break;
            case 3: new_regs.r10 = argv[i]; break;  // 注意: r10 用于第4个参数
            case 4: new_regs.r8  = argv[i]; break;
            case 5: new_regs.r9  = argv[i]; break;
        }
    }
    
    if (ptrace(PTRACE_SETREGS, tracee_pid, NULL, &new_regs) == -1) {
        perror("PTRACE_SETREGS failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.rip, (void*)original_instruction);
        return -1;
    }
    
    // 执行系统调用
    if (ptrace(PTRACE_CONT, tracee_pid, NULL, NULL) == -1) {
        perror("PTRACE_CONT failed");
        ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.rip, (void*)original_instruction);
        return retval;
    }
    
    waitpid(tracee_pid, &status, 0);
    
    if (WIFSTOPPED(status)) {
        if (ptrace(PTRACE_GETREGS, tracee_pid, NULL, &new_regs) == -1) {
            perror("PTRACE_GETREGS after syscall failed");
        } else {
            retval = new_regs.rax;
            printf("系统调用返回: %ld\n", retval);
        }
    }
    
    ptrace(PTRACE_POKETEXT, tracee_pid, (void*)old_regs.rip, (void*)original_instruction);
    ptrace(PTRACE_SETREGS, tracee_pid, NULL, &old_regs);
    
    return retval;
#else
#error "Unsupported architecture"
#endif
}

// 注入函数调用
long pinject_funcall(uintptr_t func_addr, uintptr_t ret_addr, int argc, long argv[], int ret_sig){

    /**
     * 由于跨平台原因, 所以这个可能要延搁了。请见谅  
     */
    return 0;
}

// 注入shellcode
int inject_shellcode(pid_t pid, uintptr_t shellcode_addr, int ret_sig) {
    return 0;
}

#ifdef __cplusplus
}
#endif
