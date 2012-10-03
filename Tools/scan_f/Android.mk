LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_SRC_FILES:= \
	scan/atsc_psip_section.c \
	scan/lnb.c \
	scan/scan.c \
	scan/section.c  \
	scan/dump-vdr.c \
	scan/dump-zap.c \
	scan/diseqc.c
		

LOCAL_SHARED_LIBRARIES := \
	libiconv \
	libcutils

	
#LOCAL CFLAGS := -DCONFIG_SKY_DVB
	
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) / include \
	external/libiconv/include
	
LOCAL_LDLIBS := -lm

LOCAL_MODULE:=scan_f

include $(BUILD_EXECUTABLE)

