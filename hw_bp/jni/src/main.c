// SPDX-FileCopyrightText: 2026-present The Kaidev Core developers (Linux-BP)
// SPDX-Author: Kaidevon <github.com/Kaidevon>
// SPDX-License-Identifier: Apache-2.0
// Linux-BP/main.cpp
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
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// System call header file
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/unistd.h>

// Linux kernel header files
#include <linux/hw_breakpoint.h>


static struct hw_breakpoint_attr* current_hw_bp_attr = NULL;
static FILE* g_out_fp = NULL;

/**
 * @brief Help information
 * @prog_name The executable name of the program itself
 */
static void print_usage(const char* prog_name);

/**
 * @brief Parse permission type
 * @str The string to be parsed into a permission code
 * @return Returns the parsed hardware breakpoint permission code
 */
static uint32_t parse_bp_type(const char* str);

/**
 * @brief Parse breakpoint length
 * @str The string to be parsed into a length
 * @return Returns the parsed numerical value
 */
static int parse_bp_len(const char* str);

/** 
 * @brief Handle interrupt signals
 */
static void sigint_handler(int signo);

/** 
 * @brief Execute the breakpoint
 * @pid The pid to set the hardware breakpoint on
 * @bp_type Hardware breakpoint type
 * @bp_addr Hardware breakpoint address
 * @bp_len  Hardware breakpoint length
 */
static int run_watch(pid_t pid, uint32_t bp_type, uint64_t bp_addr, uint32_t bp_len);

/**
 * @brief Output event
 * @sample Event sampler
 */
static void print_event(const struct hw_breakpoint_sample* sample);

/**
 * @brief Print version
 */
static void print_version(void);

/**
 * @brief Cleanup and exit
 */
static void cleanup(void);

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"pid",     required_argument, 0, 'p'},  // Process id
        {"type",    required_argument, 0, 't'},  // Breakpoint type
        {"len",     required_argument, 0, 'l'},  // Breakpoint length
        {"output",  required_argument, 0, 'o'},  // Output to file
        {"version", no_argument,       0, 'v'},  // Display version and copyright information
        {"help",    no_argument,       0, 'h'},  // Help
        {0, 0, 0, 0}  // Sentinel value
    };

    int opt;  // Option value
    static pid_t target_pid = 0;     // Target process pid 
    static uint32_t hw_bp_type = 0;  // Hardware breakpoint type
    static uint64_t hw_bp_addr = 0;  // Hardware breakpoint address
    static uint32_t hw_bp_len = sizeof(int);  // Hardware breakpoint length
    char output_path[256] = {0};     // Output path
    while ((opt = getopt_long(argc, argv, "p:t:l:o:vh", long_options, NULL)) != -1) {
        switch (opt) {
            // Process id
            case 'p':
                target_pid = atoi(optarg);
                break;
            // Hardware breakpoint type
            case 't':
                hw_bp_type = parse_bp_type(optarg);
                if (hw_bp_type < 0) {
                    fprintf(stderr, "[err] Invalid breakpoint type: %s\n", optarg);
                    return 1;
                }
                break;
            // Hardware breakpoint length
            case 'l':
                hw_bp_len = parse_bp_len(optarg);
                if (hw_bp_len < 0) {
                    fprintf(stderr, "[err] Invalid breakpoint length: %s\n", optarg);
                    return 1;
                }
                break;
            // Output path
            case 'o':
                strncpy(output_path, optarg, sizeof(output_path));
                break;
            // Display version and copyright information
            case 'v':
                print_version();
                break;
            // Help
            case 'h':
                print_usage(argv[0]);
                return 0;
            // Other
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check required parameters
    if (optind >= argc) {
        fprintf(stderr, "[err] Missing address argument\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* addr_arg = argv[optind];

    // Parse address argument
    if (strncmp(addr_arg, "0x", 2) == 0 || strncmp(addr_arg, "0X", 2) == 0) {
        // Hexadecimal address
        hw_bp_addr = strtoull(addr_arg, NULL, 16);
    } else {
        fprintf(stderr, "[err] Address must be in hex format (0x...) or use -p <pid> for symbol resolution\n");
        return 1;
    }

    // Set up signal handling
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // Open output file
    if (output_path[0] != '\0') {
        g_out_fp = fopen(output_path, "w");
        if (!g_out_fp) {
            fprintf(stderr, "[err] Cannot open output file: %s\n", strerror(errno));
            return 1;
        }
    } else {
        g_out_fp = stdout;
    }

    // Run main loop
    int ret = run_watch(target_pid, hw_bp_type, hw_bp_addr, hw_bp_len);

    // Close output file
    if (output_path[0] != '\0' && g_out_fp) {
        fclose(g_out_fp);
    }

    return ret;
}

// Help information
static void print_usage(const char* prog_name) {
    printf("Usage: %s [OPTIONS] <address|symbol|var>\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -p, --pid <pid>        Target process ID (default: self)\n");
    printf("  -t, --type <type>      Breakpoint type: r(ead), w(rite), x(ecute) (default: rw)\n");
    printf("                         Allowed combinations: r, w, rw, x, rx\n");
    printf("                         NOT allowed: rwx (rw cannot be combined with x)\n");
    printf("  -l, --len <len>        Length: 1, 2, 4, 8 bytes (default: 4)\n");
    printf("  -o, --output <file>    Write output to file instead of stdout\n");
    printf("  -v, --version          View version and copyright information\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Address formats:\n");
    printf("  0x12345678             Hexadecimal address\n");
    printf("  symbol                 Symbol name (requires -p <pid>)\n");
    printf("  filename:offset        File name and offset\n");
    printf("\n");
    printf("Examples:\n");
    printf("  hw_bp 0x7fff12340000                    # Monitor address in self\n");
    printf("  hw_bp -p 1234 0x7fff12340000            # Monitor address in process 1234\n");
    printf("  hw_bp -p 1234 -t w my_var               # Monitor writes to 'my_var' in process 1234\n");
    printf("  hw_bp -p 1234 -t x 0x7fff12340000       # Monitor execute\n");
    printf("\n");
}

// Parse breakpoint type
static uint32_t parse_bp_type(const char* str) {
    uint32_t mode = 0;

    // Check read permission
    if (strstr(str, "r") != 0)
        mode |= HW_BREAKPOINT_R;
    
    // Check write permission
    if (strstr(str, "w") != 0)
        mode |= HW_BREAKPOINT_W;

    // Check execute permission
    if (strstr(str, "x") != 0)
        mode |= HW_BREAKPOINT_X;

    return mode;
}

// Parse breakpoint length
static int parse_bp_len(const char* str) {
    int len = atoi(str);
    if (len == 1 || len == 2 || len == 4 || len == 8) {
        return len;
    }
    return -1;
}

// Signal handling
static void sigint_handler(int signo) {
    (void)signo;
    cleanup();
    printf("\n[Exited the current hardware breakpoint]\n");
    exit(0);
}

// Execute breakpoint
static int run_watch(pid_t pid, uint32_t bp_type, uint64_t bp_addr, uint32_t bp_len) {
    // Prepare perf_event_attr
    struct perf_event_attr attr = {0};
    attr.bp_addr = bp_addr;
    attr.bp_type = bp_type;
    attr.bp_len = bp_len;

    // Create breakpoint
    struct hw_breakpoint_attr* bp = create_hw_breakpoint(attr, pid);
    if (!bp) {
        fprintf(stderr, "[err] Failed to create hardware breakpoint\n");
        return -1;
    }
    current_hw_bp_attr = bp;

    enable_hw_breakpoint(bp->hw_fd);

    // Main loop
    bool isRunning = 1;
    while (isRunning) {
        int ret = wait_hw_breakpoint(bp);
        if (ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[err] wait_hw_breakpoint failed: %s\n", strerror(errno));
            break;
        }

        if (ret > 0) {
            struct hw_breakpoint_sample sample = {0};
            int n = handler_hw_breakpoint(bp, &sample);
            if (n > 0) {
                print_event(&sample);
                isRunning = 0;
            }
        }
    }

    // Cleanup
    cleanup();
    return 0;
}

// Output breakpoint event
static void print_event(const struct hw_breakpoint_sample* sample) {
    const char* type_str = "";
    switch (current_hw_bp_attr -> attr.bp_type) {
        case HW_BREAKPOINT_R:
            type_str = "READ";
            break;
        case HW_BREAKPOINT_W:
            type_str = "WRITE";
            break;
        case HW_BREAKPOINT_RW: 
            type_str = "READ | WRITE";
            break;
        case HW_BREAKPOINT_X:
            type_str = "EXEC";
            break;
        default:
            type_str = "Unsupported breakpoint types: rwx.";
            break;
    }

    if (g_out_fp == stdout) {
        // Terminal output
#if defined(__arm__)
            const struct user_regs* r = &(sample->regs);
#else
           const struct user_regs_struct* r = &(sample->regs);
#endif
        // Define purple escape sequence
        const char* purple = "\033[35m";
        const char* reset = "\033[0m";
    
        int reg_count = 0;
    
#if defined(__i386__)
        // i386 registers
        printf("  %sEAX%s: 0x%08lx    %sEBX%s: 0x%08lx    %sECX%s: 0x%08lx    %sEDX%s: 0x%08lx\n",
           purple, reset, r->eax,
           purple, reset, r->ebx,
           purple, reset, r->ecx,
           purple, reset, r->edx);
        reg_count += 4;
        
        printf("  %sESI%s: 0x%08lx    %sEDI%s: 0x%08lx    %sEBP%s: 0x%08lx    %sESP%s: 0x%08lx\n",
           purple, reset, r->esi,
           purple, reset, r->edi,
           purple, reset, r->ebp,
           purple, reset, r->esp);
        reg_count += 4;
    
        printf("  %sEIP%s: 0x%08lx    %sEFLAGS%s: 0x%08lx    %sCS%s: 0x%08lx    %sSS%s: 0x%08lx\n",
           purple, reset, r->eip,
           purple, reset, r->eflags,
           purple, reset, r->xcs,
           purple, reset, r->xss);
        reg_count += 4;
    
#elif defined(__x86_64__)
        // x86_64 registers
        printf("  %sRAX%s: 0x%016lx  %sRBX%s: 0x%016lx  %sRCX%s: 0x%016lx  %sRDX%s: 0x%016lx\n",
           purple, reset, r->rax,
           purple, reset, r->rbx,
           purple, reset, r->rcx,
           purple, reset, r->rdx);
        reg_count += 4;
    
        printf("  %sRSI%s: 0x%016lx  %sRDI%s: 0x%016lx  %sRBP%s: 0x%016lx  %sRSP%s: 0x%016lx\n",
           purple, reset, r->rsi,
           purple, reset, r->rdi,
           purple, reset, r->rbp,
           purple, reset, r->rsp);
        reg_count += 4;
    
        printf("  %sR8 %s: 0x%016lx  %sR9 %s: 0x%016lx  %sR10%s: 0x%016lx  %sR11%s: 0x%016lx\n",
           purple, reset, r->r8,
           purple, reset, r->r9,
           purple, reset, r->r10,
           purple, reset, r->r11);
        reg_count += 4;
    
        printf("  %sR12%s: 0x%016lx  %sR13%s: 0x%016lx  %sR14%s: 0x%016lx  %sR15%s: 0x%016lx\n",
           purple, reset, r->r12,
           purple, reset, r->r13,
           purple, reset, r->r14,
           purple, reset, r->r15);
        reg_count += 4;
    
        printf("  %sRIP%s: 0x%016lx  %sCS%s: 0x%016lx  %sRFLAGS%s: 0x%016lx  %sSS%s: 0x%016lx\n",
           purple, reset, r->rip,
           purple, reset, r->cs,
           purple, reset, r->eflags,
           purple, reset, r->ss);
        reg_count += 4;
    
#elif defined(__arm__)
        // ARM 32-bit registers
        printf("  %sR0 %s: 0x%08lx    %sR1 %s: 0x%08lx    %sR2 %s: 0x%08lx    %sR3 %s: 0x%08lx\n",
           purple, reset, r->uregs[0],
           purple, reset, r->uregs[1],
           purple, reset, r->uregs[2],
           purple, reset, r->uregs[3]);
        reg_count += 4;
    
        printf("  %sR4 %s: 0x%08lx    %sR5 %s: 0x%08lx    %sR6 %s: 0x%08lx    %sR7 %s: 0x%08lx\n",
           purple, reset, r->uregs[4],
           purple, reset, r->uregs[5],
           purple, reset, r->uregs[6],
           purple, reset, r->uregs[7]);
        reg_count += 4;
    
        printf("  %sR8 %s: 0x%08lx    %sR9 %s: 0x%08lx    %sR10%s: 0x%08lx    %sR11%s: 0x%08lx\n",
           purple, reset, r->uregs[8],
           purple, reset, r->uregs[9],
           purple, reset, r->uregs[10],
           purple, reset, r->uregs[11]);
        reg_count += 4;
    
        printf("  %sR12%s: 0x%08lx    %sSP %s: 0x%08lx    %sLR %s: 0x%08lx    %sPC %s: 0x%08lx\n",
           purple, reset, r->uregs[12],
           purple, reset, r->uregs[13],
           purple, reset, r->uregs[14],
           purple, reset, r->uregs[15]);
        reg_count += 4;
    
        printf("  %sCPSR%s: 0x%08lx\n",
           purple, reset, r->uregs[16]);
        reg_count += 1;
    
#elif defined(__aarch64__)
        // ARM64 registers
        printf("  %sX0 %s: 0x%016lx  %sX1 %s: 0x%016lx  %sX2 %s: 0x%016lx  %sX3 %s: 0x%016lx\n",
           purple, reset, r->regs[0],
           purple, reset, r->regs[1],
           purple, reset, r->regs[2],
           purple, reset, r->regs[3]);
        reg_count += 4;
    
        printf("  %sX4 %s: 0x%016lx  %sX5 %s: 0x%016lx  %sX6 %s: 0x%016lx  %sX7 %s: 0x%016lx\n",
           purple, reset, r->regs[4],
           purple, reset, r->regs[5],
           purple, reset, r->regs[6],
           purple, reset, r->regs[7]);
        reg_count += 4;
    
        printf("  %sX8 %s: 0x%016lx  %sX9 %s: 0x%016lx  %sX10%s: 0x%016lx  %sX11%s: 0x%016lx\n",
           purple, reset, r->regs[8],
           purple, reset, r->regs[9],
           purple, reset, r->regs[10],
           purple, reset, r->regs[11]);
        reg_count += 4;
    
        printf("  %sX12%s: 0x%016lx  %sX13%s: 0x%016lx  %sX14%s: 0x%016lx  %sX15%s: 0x%016lx\n",
           purple, reset, r->regs[12],
           purple, reset, r->regs[13],
           purple, reset, r->regs[14],
           purple, reset, r->regs[15]);
        reg_count += 4;
    
        printf("  %sX16%s: 0x%016lx  %sX17%s: 0x%016lx  %sX18%s: 0x%016lx  %sX19%s: 0x%016lx\n",
           purple, reset, r->regs[16],
           purple, reset, r->regs[17],
           purple, reset, r->regs[18],
           purple, reset, r->regs[19]);
        reg_count += 4;
    
        printf("  %sX20%s: 0x%016lx  %sX21%s: 0x%016lx  %sX22%s: 0x%016lx  %sX23%s: 0x%016lx\n",
           purple, reset, r->regs[20],
           purple, reset, r->regs[21],
           purple, reset, r->regs[22],
           purple, reset, r->regs[23]);
        reg_count += 4;
    
        printf("  %sX24%s: 0x%016lx  %sX25%s: 0x%016lx  %sX26%s: 0x%016lx  %sX27%s: 0x%016lx\n",
           purple, reset, r->regs[24],
           purple, reset, r->regs[25],
           purple, reset, r->regs[26],
           purple, reset, r->regs[27]);
        reg_count += 4;
    
        printf("  %sX28%s: 0x%016lx  %sFP %s: 0x%016lx  %sLR %s: 0x%016lx  %sSP %s: 0x%016lx\n",
           purple, reset, r->regs[28],
           purple, reset, r->regs[29],
           purple, reset, r->regs[30],
           purple, reset, r->sp);
        reg_count += 4;
    
        printf("  %sPC %s: 0x%016lx  %sPSTATE%s: 0x%016lx\n",
           purple, reset, r->pc,
           purple, reset, r->pstate);
        reg_count += 2;
    
#else
        // Unknown platform
        printf("  %sWarning: Unknown architecture, cannot display registers%s\n",
           "\033[33m", reset);  // Yellow warning
#endif
    
        // End with separator line
        printf("  --------------------------------%s\n", reset);
    } else {
        // File output
        // Output registers according to platform
#if defined(__arm__)
            const struct user_regs* r = &(sample->regs);
#else
           const struct user_regs_struct* r = &(sample->regs);
#endif
        
        // Write configuration information
        fprintf(g_out_fp, "# Hardware breakpoint triggered\n");
        fprintf(g_out_fp, "# Target PID: %d\n", current_hw_bp_attr -> pid);
        fprintf(g_out_fp, "# Address: 0x%llx\n", current_hw_bp_attr -> attr.bp_addr);
        fprintf(g_out_fp, "# Type: %s\n", type_str);
        fprintf(g_out_fp, "# Length: %llu bytes\n", current_hw_bp_attr -> attr.bp_len);
        fprintf(g_out_fp, "# --------------------------------\n");

#if defined(__i386__)
        // i386 registers
        fprintf(g_out_fp,
            "  EAX: 0x%08lx    EBX: 0x%08lx    ECX: 0x%08lx    EDX: 0x%08lx\n",
            r->eax, r->ebx, r->ecx, r->edx);
        fprintf(g_out_fp,
            "  ESI: 0x%08lx    EDI: 0x%08lx    EBP: 0x%08lx    ESP: 0x%08lx\n",
            r->esi, r->edi, r->ebp, r->esp);
        fprintf(g_out_fp,
            "  EIP: 0x%08lx    EFLAGS: 0x%08lx    CS: 0x%08lx    SS: 0x%08lx\n",
            r->eip, r->eflags, r->xcs, r->xss);

#elif defined(__x86_64__)
        // x86_64 registers
        fprintf(g_out_fp,
            "  RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx  RDX: 0x%016lx\n",
            r->rax, r->rbx, r->rcx, r->rdx);
        fprintf(g_out_fp,
            "  RSI: 0x%016lx  RDI: 0x%016lx  RBP: 0x%016lx  RSP: 0x%016lx\n",
            r->rsi, r->rdi, r->rbp, r->rsp);
        fprintf(g_out_fp,
            "  R8: 0x%016lx  R9: 0x%016lx  R10: 0x%016lx  R11: 0x%016lx\n",
            r->r8, r->r9, r->r10, r->r11);
        fprintf(g_out_fp,
            "  R12: 0x%016lx  R13: 0x%016lx  R14: 0x%016lx  R15: 0x%016lx\n",
            r->r12, r->r13, r->r14, r->r15);
        fprintf(g_out_fp,
            "  RIP: 0x%016lx  CS: 0x%016lx  RFLAGS: 0x%016lx  SS: 0x%016lx\n",
            r->rip, r->cs, r->eflags, r->ss);

#elif defined(__arm__)
        // ARM 32-bit registers
        fprintf(g_out_fp,
            "  R0: 0x%08lx    R1: 0x%08lx    R2: 0x%08lx    R3: 0x%08lx\n",
            r->uregs[0], r->uregs[1], r->uregs[2], r->uregs[3]);
        fprintf(g_out_fp,
            "  R4: 0x%08lx    R5: 0x%08lx    R6: 0x%08lx    R7: 0x%08lx\n",
            r->uregs[4], r->uregs[5], r->uregs[6], r->uregs[7]);
        fprintf(g_out_fp,
            "  R8: 0x%08lx    R9: 0x%08lx    R10: 0x%08lx   R11: 0x%08lx\n",
            r->uregs[8], r->uregs[9], r->uregs[10], r->uregs[11]);
        fprintf(g_out_fp,
            "  R12: 0x%08lx   SP: 0x%08lx    LR: 0x%08lx    PC: 0x%08lx\n",
            r->uregs[12], r->uregs[13], r->uregs[14], r->uregs[15]);
        fprintf(g_out_fp,
            "  CPSR: 0x%08lx\n",
            r->uregs[16]);

#elif defined(__aarch64__)
        // ARM64 registers
        fprintf(g_out_fp,
            "  X0: 0x%016lx  X1: 0x%016lx  X2: 0x%016lx  X3: 0x%016lx\n",
            r->regs[0], r->regs[1], r->regs[2], r->regs[3]);
        fprintf(g_out_fp,
            "  X4: 0x%016lx  X5: 0x%016lx  X6: 0x%016lx  X7: 0x%016lx\n",
            r->regs[4], r->regs[5], r->regs[6], r->regs[7]);
        fprintf(g_out_fp,
            "  X8: 0x%016lx  X9: 0x%016lx  X10: 0x%016lx  X11: 0x%016lx\n",
            r->regs[8], r->regs[9], r->regs[10], r->regs[11]);
        fprintf(g_out_fp,
            "  X12: 0x%016lx  X13: 0x%016lx  X14: 0x%016lx  X15: 0x%016lx\n",
            r->regs[12], r->regs[13], r->regs[14], r->regs[15]);
        fprintf(g_out_fp,
            "  X16: 0x%016lx  X17: 0x%016lx  X18: 0x%016lx  X19: 0x%016lx\n",
            r->regs[16], r->regs[17], r->regs[18], r->regs[19]);
        fprintf(g_out_fp,
            "  X20: 0x%016lx  X21: 0x%016lx  X22: 0x%016lx  X23: 0x%016lx\n",
            r->regs[20], r->regs[21], r->regs[22], r->regs[23]);
        fprintf(g_out_fp,
            "  X24: 0x%016lx  X25: 0x%016lx  X26: 0x%016lx  X27: 0x%016lx\n",
            r->regs[24], r->regs[25], r->regs[26], r->regs[27]);
        fprintf(g_out_fp,
            "  X28: 0x%016lx  FP: 0x%016lx  LR: 0x%016lx  SP: 0x%016lx\n",
            r->regs[28], r->regs[29], r->regs[30], r->sp);
        fprintf(g_out_fp,
            "  PC: 0x%016lx  PSTATE: 0x%016lx\n",
            r->pc, r->pstate);

#else
        // Unknown platform
        fprintf(g_out_fp, "  [Unknown architecture, cannot display registers]\n");
#endif

        // End with separator line
        fprintf(g_out_fp, "  --------------------------------\n");
    }
}

// Cleanup
static void cleanup(void) {
    disable_hw_breakpoint(current_hw_bp_attr -> hw_fd);
    destroy_hw_breakpoint(current_hw_bp_attr);
}

// Print copyright and version information
static void print_version(void) {
    printf("Hardware-breakpoint Tool 1.0\n");

    // Definition of platform related macros
    #if defined(__i386__)
    printf("Compiled for x86_32\n");
    #elif defined(__x86_64__)
    printf("Compiled for x86_64\n");
    #elif defined(__arm__)
    printf("Compiled for armv7-a\n");
    #elif defined(__aarch64__)
    printf("Compiled for aarch64\n");
    #else
    #error "Unknown platform"
    #endif

    printf("Copyright (C) 2026-present The Kaidev Core Developers\n");
    printf("Author: Kaidevon <github.com/Kaidevon>\n");
    printf("License: Apache-2.0 <https://www.apache.org/licenses/LICENSE-2.0>\n");
    printf("This software is free software and can be freely modified and redistributed.\n");
    printf("No warranty is provided within the scope permitted by law.\n");
    return;
}

#ifdef __cplusplus
}
#endif