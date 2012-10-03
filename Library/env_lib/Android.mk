LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	env_lib.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_LDLIBS := -lm

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils 
	
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libenv_lib

LOCAL_MODULE_TAGS := user

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_SHARED_LIBRARY)
