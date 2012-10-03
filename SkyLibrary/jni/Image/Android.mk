LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Image_jni.cpp \
	../../common/Disp2Ctrl_api.cpp \
	../../common/display_ref.cpp

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/../../include \
	external/skyswjpeg \
	external/skyhwjpeg/global \
	external/skyjpeg

LOCAL_LDLIBS := -lm

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libcutils \
	libutils \
	libicuuc \
	libskyswjpeg \
	libskyhwjpeg \
	libskyjpeg

LOCAL_STATIC_LIBRARIES :=

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_MODULE := libImageUtil_jni

LOCAL_MODULE_TAGS := user

include $(BUILD_SHARED_LIBRARY)
