LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
	env_audio_exe.cpp \
	../../SkyLibrary/common/Settings_api.cpp \
	../../SkyLibrary/common/Settings_ref.cpp 
	
LOCAL_C_INCLUDES += $(LOCAL_PATH)/src \
										$(LOCAL_PATH)/../../Library/env_lib \
										$(LOCAL_PATH)/../../SkyLibrary/include
										
LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libcutils \
	libutils \
	libicuuc \
	libenv_lib  \
	libSettingsUtil_jni

LOCAL_PROGUARD_ENABLED := disabled
	
LOCAL_MODULE:=env_audio_exe

LOCAL_MODULE_TAGS := user

include $(BUILD_EXECUTABLE)

