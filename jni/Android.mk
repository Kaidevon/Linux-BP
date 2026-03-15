LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := linux-bp

APP_STL := c++_static
LOCAL_CPPFLAGS += -std=c++17

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES := \
    src/main.c \
    src/hw_breakpoint.c \
    src/sw_breakpoint.c \
    src/pinject.c

include $(BUILD_EXECUTABLE)