LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES:= \
	smb_umnt.c 

LOCAL_SHARED_LIBRARIES := \
	libenv_lib 

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE:=smb_umnt

include $(BUILD_EXECUTABLE)

