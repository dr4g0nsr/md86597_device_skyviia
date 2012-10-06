# config.mk
# 
# Product-specific compile-time definitions.
#
TARGET_BOOTLOADER_BOARD_NAME := SV8860

# The generic product target doesn't have any hardware-specific pieces.
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_NO_RADIOIMAGE := true
#HAVE_HTC_AUDIO_DRIVER := true
#BOARD_USES_GENERIC_AUDIO := true
#BOARD_USES_GENERIC_OSSAUDIO := true
BOARD_USES_ALSA_AUDIO := true
#BUILD_WITH_ALSA_UTILS := true

TARGET_CPU_ABI := armeabi
USE_CAMERA_STUB := true

#BOARD_HAVE_BLUETOOTH := true
#TARGET_HARDWARE_3D := false

# define mtd size
#BOARD_SYSTEMIMAGE_MAX_SIZE := 0
#BOARD_USERDATAIMAGE_MAX_SIZE := 0

# The size of a block that can be marked bad.
#BOARD_FLASH_BLOCK_SIZE := 0

# config Wi-Fi
# WPA_SUPPLICANT_VERSION := 0.6.10 // skyviia old config
BOARD_WPA_SUPPLICANT_DRIVER := WEXT
WPA_SUPPLICANT_VERSION      := VER_0_6_X
WPA_BUILD_SUPPLICANT := true
# [BEGIN] Willie modify for change wifi path [2010-10-14]
# old version
#WIFI_DRIVER_MODULE_PATH := ""
WIFI_DRIVER_MODULE_PATH := "/system/lib/modules/rt3370sta.ko"
# [END]
WIFI_DRIVER_MODULE_ARG := ""
# [BEGIN] Willie modify for change wifi path [2010-10-14]
# old version
#WIFI_DRIVER_MODULE_NAME := ""
WIFI_DRIVER_MODULE_NAME := "rt3370sta"
# [END]
WIFI_FIRMWARE_LOADER := ""

# Skyviia
# config for BT download
SKYVIIA_BUILD_BTDOWNLOAD := false
SKYVIIA_HAVE_NO_BATTERY := true
