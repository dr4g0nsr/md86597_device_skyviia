LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES:= \
	upgrade.c 

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_PROGUARD_ENABLED := disabled
	
LOCAL_MODULE:=upgrade

include $(BUILD_EXECUTABLE)

