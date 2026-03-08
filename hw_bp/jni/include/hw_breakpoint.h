// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (Linux-BP)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// Linux-BP/hw_breakpoint.h
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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


// Definition of platform related macros
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
#error "Unknown platform architecture"
#endif

// Hardware breakpoint attrible structure
struct hw_breakpoint_attr {
    int hw_fd;                     // Hardware breakpoint file descriptor
    void* mmap_buffer;             // ring buffer
    size_t mmap_size;              // ring buffer size
    pid_t pid;                     // Process ID
    struct perf_event_attr attr;   // Breakpoint configuration attrible
};

// User stored hardware breakpoint sampling data type
struct hw_breakpoint_sample {
    uint32_t pid;  // Process id
    uint32_t tid;  // Task id
    uint64_t abi;  // Application Binary Interface
    uint64_t addr; // Actual breakpoint trigger address
    uint64_t time; // Hardware breakpoint occurrence time
    #if defined(__arm__)
    struct user_regs regs;  // User space Register
    #else
    struct user_regs_struct regs;  // User space Register
    #endif
};

/**
 * @brief Create hardware breakpoint
 * @attr Information that needs to be configured
 * @pid The PID of the target process that needs to be configured
 * @return Successfully returned hardware breakpoint attribute structure pointer, failed returned NULL
 */
struct hw_breakpoint_attr* create_hw_breakpoint(struct perf_event_attr attr, pid_t pid);

/**
 * @brief Waiting for hardware breakpoint event
 * @attr Hardware breakpoint attribute structure pointer
 * @return Success returns 1 (with event), 0 (timeout), failure returns -1
 */
int wait_hw_breakpoint(struct hw_breakpoint_attr* attr);

/**
 * @brief Handle hardware breakpoint events and parse ring buffers
 * @attr Hardware breakpoint attribute structure pointer
 * @pid Specify the PID of the process (-1 represents all processes)
 * @tid Receive pointer for task ID that triggers hardware breakpoint
 * @abi The receive pointer of the process architecture that triggers hardware breakpoints
 * @regs Register receiving pointer that triggers hardware breakpoint
 * @return If successful, return the count of successfully processed events and write the return value into a formal parameter. If it fails, return -1
 */
int handler_hw_breakpoint(struct hw_breakpoint_attr* attr, struct hw_breakpoint_sample *sample);

/**
 * @brief Disable hardware breakpoints
 * @fd Hardware breakpoint file descriptor
 * @return void
 */
void disable_hw_breakpoint(int fd);

/**
 * @brief Enable hardware breakpoints
 * @fd Hardware breakpoint file descriptor
 * @return void
 */
void enable_hw_breakpoint(int fd);

/**
 * @brief Destroy hardware breakpoints
 * @param attr Hardware breakpoint attribute structure pointer
 */
void destroy_hw_breakpoint(struct hw_breakpoint_attr* attr);

#ifdef __cplusplus
}
#endif

#endif // ! hw_breakpoint.h