LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES:= \
	fwpack.c 

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE:=fwpack

include $(BUILD_HOST_EXECUTABLE)

