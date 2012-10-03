LOCAL_PATH:= $(call my-dir)

#############################################################
#2011-01-28 Mingyu Li
#Add for Skymedi MediaParser 
#


#===============================================
#
#	Build MediaParser Library.
#
#	Prebuild Mode
#
#===============================================

##===============================================
##
##	Prebuild Cross-Compiled Sky MediaParser Lib.
##
##===============================================
include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm

#Prevent ProGuard doing Obfuscation and Optimization.
LOCAL_PROGUARD_ENABLED := disabled
				   
LOCAL_PREBUILT_LIBS := libMediaParser_jni.so
include $(BUILD_MULTI_PREBUILT)
LOCAL_SHARED_LIBRARIES := libMediaParser_jni
