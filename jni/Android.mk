LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := hphphp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_touch
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/stealth/stealth.cpp \
    src/overlay/overlay.cpp \
    src/Android_touch/TouchHelperA.cpp

IM_SRC := $(wildcard $(LOCAL_PATH)/src/ImGui/*.cpp)
LOCAL_SRC_FILES += $(IM_SRC:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS := -llog -lEGL -lGLESv2 -lGLESv3 -landroid
LOCAL_CFLAGS += -O3 -fvisibility=hidden -Wno-unused-variable -Wno-unused-parameter
LOCAL_CPPFLAGS += -O3 -std=c++17 -fvisibility=hidden
LOCAL_LDFLAGS += -s

LOCAL_PIE := true
include $(BUILD_EXECUTABLE)
