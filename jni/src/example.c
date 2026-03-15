#include "pinject.h"
#include "sw_breakpoint.h"

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>

#define target_pid 5696 
#define breakpoint_addr 0x58e8051000

int example() {
    // 软件断点例子
    // 附加到目标进程
    printf("%p, 使用对齐运算： %p\n", breakpoint_addr, (void*)(breakpoint_addr & ~0xFFF));
    pinject_attach(target_pid);

    // 系统调用预热
    if (pinject_warmup()) {
        printf("系统调用预热成功!\n");
    }

    // 创建断点属性
    struct sw_breakpoint_attr* bp_attr = create_sw_breakpoint(
        target_pid, 
        breakpoint_addr, 
        SW_BREAKPOINT_NRW, 
        1
    );
    
    // 等待断点触发
    int wait_status = wait_sw_breakpoint(bp_attr);
    
    // 断点样本数据
    struct sw_breakpoint_sample bp_sample;
    
    // 处理断点
    handler_sw_breakpoint(bp_attr, &bp_sample, wait_status);
    
    // 打印结果
    #if defined(__aarch64__)
        printf("断点触发 PC:  %p\n", bp_sample.regs.pc);
        printf("断点触发 X0:  %p\n", bp_sample.regs.regs[0]);
    #endif
    
    // 结束软件断点
    
    return 0;
}