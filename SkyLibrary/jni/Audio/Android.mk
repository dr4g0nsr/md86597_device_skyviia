LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Audio_jni.cpp

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/../../include

LOCAL_LDLIBS := -lm

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
	libutils
#	libandroid_runtime \
#	libcutils \
#	libicuuc \
#	libsgl

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE := libAudioUtil_jni

LOCAL_MODULE_TAGS := user

include $(BUILD_SHARED_LIBRARY)
