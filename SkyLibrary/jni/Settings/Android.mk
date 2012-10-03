LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Settings_jni.cpp \
	../../common/Settings_api.cpp \
	../../common/Settings_ref.cpp 

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/../../include

LOCAL_LDLIBS := -lm

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libcutils \
	libutils \
	libicuuc 

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE := libSettingsUtil_jni

LOCAL_MODULE_TAGS := user

include $(BUILD_SHARED_LIBRARY)
