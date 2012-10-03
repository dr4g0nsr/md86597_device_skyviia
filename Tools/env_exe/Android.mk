LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES:= \
	env_exe.c 

LOCAL_SHARED_LIBRARIES := \
	libenv_lib 

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE:=env_exe

include $(BUILD_EXECUTABLE)

