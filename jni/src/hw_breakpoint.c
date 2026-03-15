// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (Linux-BP)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// PKaitch/hw_breakpoint.c
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

#include "hw_breakpoint.h"

// 标准库头文件
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

// Linux 内核头文件
#include <linux/types.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>


// 创建硬件断点
struct hw_breakpoint_attr* create_hw_breakpoint(struct perf_event_attr attr, pid_t pid) {
    // 配置硬件断点参数
    attr.size = sizeof(attr);               // 通知内核属性结构的长度
    attr.type = PERF_TYPE_BREAKPOINT;       // 事件类型：硬件断点
    attr.config = PERF_COUNT_SW_CPU_CLOCK;  // 清零
    attr.watermark = 0;    // 非水位线模式 / 事件计数模式
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_REGS_USER;  // 采样数据类型
    attr.sample_period = 1;  // 采样频率
    attr.wakeup_events = 1;  // 唤醒频率
    attr.precise_ip = 2; // 同步精度
    
    // 启用事件
    attr.disabled = 1;        // 创建期间禁用事件
    attr.exclude_kernel = 1;  // 排除内核空间的事件
    attr.exclude_hv = 1;      // 排除虚拟机监控程序的事件
    
    // 配置硬件断点环形缓冲区
    attr.sample_regs_user = ((1ULL << MAX_PERF_REGS) - 1);
    attr.mmap = 1;
    attr.comm = 1;
    attr.mmap_data = 1;
    attr.mmap2 = 1;
    
    // 发起系统调用
    int fd = syscall(__NR_perf_event_open, &attr, pid, -1, -1, PERF_FLAG_FD_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }
    
    // 获取内存页大小
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0) {
        close(fd);
        return NULL;
    }
    
    // 内存映射
    size_t buffer_size = 3 * page_size;
    void* buffer = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    
    // 定义元数据
    struct perf_event_mmap_page* meta = (struct perf_event_mmap_page*)buffer;
    
    // 验证缓冲区布局
    if (meta->data_offset >= buffer_size) {
        munmap(buffer, buffer_size);
        close(fd);
        return NULL;
    }
    
    // 分配 hw_breakpoint_attr 结构体
    struct hw_breakpoint_attr* bp_attr = (struct hw_breakpoint_attr*)malloc(sizeof(struct hw_breakpoint_attr));
    if (!bp_attr) {
        munmap(buffer, buffer_size);
        close(fd);
        return NULL;
    }
    
    // 初始化结构体
    memset(bp_attr, 0, sizeof(struct hw_breakpoint_attr));
    bp_attr->attr = attr;             // 断点配置
    bp_attr->pid = pid;               // 进程 ID
    bp_attr->hw_fd = fd;              // 文件描述符
    bp_attr->mmap_buffer = buffer;    // 硬件断点环形缓冲区
    bp_attr->mmap_size = buffer_size; // 缓冲区大小
    
    // 需要重置事件并启用硬件断点。
    
    return bp_attr;
}

// 等待硬件断点事件
int wait_hw_breakpoint(struct hw_breakpoint_attr* attr) {
    if (!attr) {
        return -1; // 无效指针
    }
    
    // 等待事件
    struct pollfd perf_poll = {0};
    perf_poll.fd = attr->hw_fd;
    perf_poll.events = POLLIN;
    
    int poll_result = poll(&perf_poll, 1, -1); // 无限期等待
    if (poll_result < 0) {
        return -1; // 轮询失败
    }
    
    if (poll_result == 0) {
        return 0; // 超时（但在此处不会发生，因为是无限期等待）
    }
    
    return 1; // 有事件发生
}

// 处理硬件断点事件
int handler_hw_breakpoint(struct hw_breakpoint_attr* attr, 
                    struct hw_breakpoint_sample *sample) {
    if (!attr || !sample) {
        return -1;
    }
    
    // 硬件断点环形缓冲区
    struct perf_event_mmap_page* ring_buffer = (struct perf_event_mmap_page*)attr->mmap_buffer;
    size_t ring_buffer_size = attr->mmap_size;
    
    if (!ring_buffer || ring_buffer_size <= 0) {
        return -1;
    }
    
    // 数据区域信息
    uintptr_t data_addr = (uintptr_t)ring_buffer + ring_buffer->data_offset;
    size_t data_size = ring_buffer->data_size;    // 数据大小
    uint64_t data_head = ring_buffer->data_head;  // 数据头部
    uint64_t data_tail = ring_buffer->data_tail;  // 数据尾部
    
    // 验证缓冲区布局
    if (ring_buffer->data_offset >= ring_buffer_size) {
        return -1;
    }
    
    if (data_size == 0) {
        return -1;
    }
    
    if (ring_buffer->data_offset + data_size > ring_buffer_size) {
        return -1;
    }
    
    // 没有新数据
    if (data_head == data_tail) {
        return 0;
    }
    
    // 处理所有待处理的事件
    int events_processed = 0;
    
    while (data_tail != data_head) {
        // 计算当前事件在环形缓冲区中的位置
        size_t offset_in_buffer = data_tail % data_size;
        uintptr_t event_start = data_addr + offset_in_buffer;
        
        // 读取事件头
        struct perf_event_header *hdr = (struct perf_event_header *)event_start;
        
        // 检查事件大小是否合理, 如果不合理则是坏数据
        if (hdr->size < sizeof(struct perf_event_header)) {
            data_tail += sizeof(struct perf_event_header);
    
            // 防止越界
            if (data_tail > data_head) {
                data_tail = data_head;
            }
    
            ring_buffer->data_tail = data_tail;  // 更新尾部
            continue;  // 继续处理，不返回
        }
        
        // 检查事件大小是否超过数据区域（上限检查）
        if (hdr->size > data_size) {
            // 跳过 data_size 大小的数据
            data_tail += data_size;
    
            // 防止越界
            if (data_tail > data_head) {
                data_tail = data_head;
            }
    
            // 更新尾部
            ring_buffer->data_tail = data_tail;
    
            // 继续处理
            continue;
        }
        
        // 检查事件是否完整（可能跨越缓冲区末尾）
        if (offset_in_buffer + hdr->size > data_size) {
            // 只消费当前页的剩余部分，然后返回
            size_t remaining = data_size - offset_in_buffer;
            data_tail += remaining;

            // 防止越界
            if (data_tail > data_head) {
                data_tail = data_head;
            }

            // 更新尾部
            ring_buffer->data_tail = data_tail;

            return events_processed;  // 返回已处理的事件数量
        }
        
        // 解析事件
        size_t event_offset = sizeof(struct perf_event_header);
        
        // 采样数据结构
        if (hdr->type == PERF_RECORD_SAMPLE) {
            // 提取 pid
            uint32_t __pid = *(uint32_t *)(event_start + event_offset);
            event_offset += 4;
            
            // 提取 tid
            uint32_t __tid = *(uint32_t *)(event_start + event_offset);
            event_offset += 4;
            
            // 提取 abi
            uint64_t __abi = *(uint64_t *)(event_start + event_offset);
            event_offset += 8;
            
            // 提取寄存器
            #if defined(__arm__)
                struct user_regs __regs;  // 用户空间寄存器
            #else
                struct user_regs_struct __regs;  // 用户空间寄存器结构
            #endif
            memcpy(&__regs, (const void*)(event_start + event_offset), sizeof(__regs));
            event_offset += sizeof(__regs);
            
            // 更新返回值参数
            sample -> pid  = __pid;
            sample -> tid  = __tid;
            sample -> abi  = __abi;
            sample -> regs = __regs;
            
            // 增加命中计数器
            sample -> hit_count++;
            
            // 增加事件计数器
            events_processed++;
        } else if (hdr->type == PERF_RECORD_LOST) {
            // PERF_RECORD_LOST，未处理
            uint64_t lost = *(uint64_t *)(event_start + event_offset);
        } else {
            // 其他类型的事件，未处理
        }
        
        // 移动到下一个事件
        data_tail += hdr->size;
        
        // 检查是否超过 data_head
        if (data_tail > data_head) {
            return -1;
        }
    }
    
    // 更新消费者指针
    ring_buffer->data_tail = data_tail;
    
    return events_processed;  // 返回事件计数器
}


// 禁用硬件断点
void disable_hw_breakpoint(int fd) {
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}

// 启用硬件断点
void enable_hw_breakpoint(int fd) {
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

// 销毁硬件断点
void destroy_hw_breakpoint(struct hw_breakpoint_attr* attr) {
    if (!attr) {
        return;
    }
    
    // 检查文件描述符是否有效
    if (attr->hw_fd >= 0) {
        ioctl(attr->hw_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(attr->hw_fd);
    }
    
    // 检查内存映射是否有效
    if (attr->mmap_buffer && attr->mmap_buffer != MAP_FAILED) {
        munmap(attr->mmap_buffer, attr->mmap_size);
    }
    
    // 释放硬件断点属性
    free(attr);
}

#ifdef __cplusplus
}
#endif