// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (Linux-BP)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// Linux-BP/hw_breakpoint.c
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

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_breakpoint.h"

// Standard library header files
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

// System call header file
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/poll.h>
#include <sys/unistd.h>

// Linux kernel header files
#include <linux/types.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>


// Create hardware breakpoint
struct hw_breakpoint_attr* create_hw_breakpoint(struct perf_event_attr attr, pid_t pid) {
    // Configure hardware breakpoint parameters
    attr.size = sizeof(attr);               // Notify the length of the kernel attribute structure
    attr.type = PERF_TYPE_BREAKPOINT;       // Event type: Hardware breakpoint
    attr.config = PERF_COUNT_SW_CPU_CLOCK;  // zero
    attr.watermark = 0;    // Non waterline mode / Event counting mode
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_REGS_USER;  // Sampling data type
    attr.sample_period = 1;  // sampling frequency
    attr.wakeup_events = 1;  // Wake up  frequency
    attr.precise_ip = 2; // sync
    
    // Enable event
    attr.disabled = 1;        // Disabled during event creation
    attr.exclude_kernel = 1;  // Exclude events from kernel space
    attr.exclude_hv = 1;      // Exclude events from the virtual machine monitoring program
    
    // Configure hardware breakpoint ring buffer
    attr.sample_regs_user = ((1ULL << MAX_PERF_REGS) - 1);
    attr.mmap = 1;
    attr.comm = 1;
    attr.mmap_data = 1;
    attr.mmap2 = 1;
    
    // Start syscall
    int fd = syscall(__NR_perf_event_open, &attr, pid, -1, -1, PERF_FLAG_FD_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }
    
    // Get memory page size
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0) {
        close(fd);
        return NULL;
    }
    
    // memory mapping
    size_t buffer_size = 2 * page_size;
    void* buffer = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    
    // Define metadata
    struct perf_event_mmap_page* meta = (struct perf_event_mmap_page*)buffer;
    
    // Verify buffer layout
    if (meta->data_offset >= buffer_size) {
        munmap(buffer, buffer_size);
        close(fd);
        return NULL;
    }
    
    // Allocate hw_reakpoint_attr
    struct hw_breakpoint_attr* bp_attr = (struct hw_breakpoint_attr*)malloc(sizeof(struct hw_breakpoint_attr));
    if (!bp_attr) {
        munmap(buffer, buffer_size);
        close(fd);
        return NULL;
    }
    
    // Initialize the structure
    memset(bp_attr, 0, sizeof(struct hw_breakpoint_attr));
    bp_attr->attr = attr;             // Breakpoint configuration
    bp_attr->pid = pid;               // Process ID
    bp_attr->hw_fd = fd;              // file descriptor
    bp_attr->mmap_buffer = buffer;    // Hardware breakpoint ring buffer
    bp_attr->mmap_size = buffer_size; // buffer size
    
    // you need to reset the event and enable the hardware breakpoint.
    
    return bp_attr;
}

// Waiting for hardware breakpoint event
int wait_hw_breakpoint(struct hw_breakpoint_attr* attr) {
    if (!attr) {
        return -1; // invalid pointer
    }
    
    // waiting event
    struct pollfd perf_poll = {0};
    perf_poll.fd = attr->hw_fd;
    perf_poll.events = POLLIN;
    
    int poll_result = poll(&perf_poll, 1, -1); // endless waiting
    if (poll_result < 0) {
        return -1; // Poll failed
    }
    
    if (poll_result == 0) {
        return 0; // Timeout (but it won't happen here because of infinite waiting)
    }
    
    return 1; // There is an event
}

// Handling hardware breakpoint events
int handler_hw_breakpoint(struct hw_breakpoint_attr* attr, 
                    struct hw_breakpoint_sample *sample) {
    if (!attr || !sample) {
        return -1;
    }
    
    // hardware breakpoint ring buffer
    struct perf_event_mmap_page* ring_buffer = (struct perf_event_mmap_page*)attr->mmap_buffer;
    size_t ring_buffer_size = attr->mmap_size;
    
    if (!ring_buffer || ring_buffer_size <= 0) {
        return -1;
    }
    
    // data area information
    uintptr_t data_addr = (uintptr_t)ring_buffer + ring_buffer->data_offset;
    size_t data_size = ring_buffer->data_size;    // data size
    uint64_t data_head = ring_buffer->data_head;  // data header
    uint64_t data_tail = ring_buffer->data_tail;  // data end
    
    // Verify buffer layout
    if (ring_buffer->data_offset >= ring_buffer_size) {
        return -1;
    }
    
    if (data_size == 0) {
        return -1;
    }
    
    if (ring_buffer->data_offset + data_size > ring_buffer_size) {
        return -1;
    }
    
    // No new data
    if (data_head == data_tail) {
        return 0;
    }
    
    // Handle all pending events
    int events_processed = 0;
    
    while (data_tail != data_head) {
        // Calculate the position of the current event in the ring buffer
        size_t offset_in_buffer = data_tail % data_size;
        uintptr_t event_start = data_addr + offset_in_buffer;
        
        // Read event header
        struct perf_event_header *hdr = (struct perf_event_header *)event_start;
        
        // Check if the event size is reasonable
        if (hdr->size < sizeof(struct perf_event_header)) {
            return -1;
        }
        
        // Check if the event size exceeds the data area (upper bound check)
        if (hdr->size > data_size) {
            return -1;
        }
        
        // Check if the event is complete (possibly crossing the end of the buffer)
        if (offset_in_buffer + hdr->size > data_size) {
            // Only consume the remaining portion of the current page, and then return
            size_t remaining = data_size - offset_in_buffer;
            data_tail += remaining;

            // Prevent crossing boundaries
            if (data_tail > data_head) {
                data_tail = data_head;
            }

            // update tail
            ring_buffer->data_tail = data_tail;

            return events_processed;  // Return the number of processed events
        }
        
        // Event parsing
        size_t event_offset = sizeof(struct perf_event_header);
        
        // Sampling data structure
        if (hdr->type == PERF_RECORD_SAMPLE) {
            // Extract  pid
            uint32_t __pid = *(uint32_t *)(event_start + event_offset);
            event_offset += 4;
            
            // Extract  tid
            uint32_t __tid = *(uint32_t *)(event_start + event_offset);
            event_offset += 4;
            
            // Extract  abi
            uint64_t __abi = *(uint64_t *)(event_start + event_offset);
            event_offset += 8;
            
            // Extract regs
            #if defined(__arm__)
                struct user_regs __regs;  // User space Register
            #else
                struct user_regs_struct __regs;  // User space Register
            #endif
            memcpy(&__regs, (const void*)(event_start + event_offset), sizeof(__regs));
            event_offset += sizeof(__regs);
            
            // Update formal parameter return value
            sample -> pid  = __pid;
            sample -> tid  = __tid;
            sample -> abi  = __abi;
            sample -> regs = __regs;
            
            // Increase event counter
            events_processed++;
        } else if (hdr->type == PERF_RECORD_LOST) {
            // PERF_RECORD_LOST, not handled
            uint64_t lost = *(uint64_t *)(event_start + event_offset);
        } else {
            // Other types of events, not handled
        }
        
        // Move to the next event
        data_tail += hdr->size;
        
        // Check if it exceeds data_ head
        if (data_tail > data_head) {
            return -1;
        }
    }
    
    // Update consumer pointers
    ring_buffer->data_tail = data_tail;
    
    return events_processed;  // return event counter
}

// Disable hardware breakpoints
void disable_hw_breakpoint(int fd) {
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}

// Enable hardware breakpoints
void enable_hw_breakpoint(int fd) {
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

// Destroy hardware breakpoints
void destroy_hw_breakpoint(struct hw_breakpoint_attr* attr) {
    if (!attr) {
        return;
    }
    
    // Check if the file descriptor is valid
    if (attr->hw_fd >= 0) {
        ioctl(attr->hw_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(attr->hw_fd);
    }
    
    // Check if the memory mapping is valid
    if (attr->mmap_buffer && attr->mmap_buffer != MAP_FAILED) {
        munmap(attr->mmap_buffer, attr->mmap_size);
    }
    
    // Release hardware breakpoint attrible
    free(attr);
}

#ifdef __cplusplus
}
#endif