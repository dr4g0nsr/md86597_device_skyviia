$(call inherit-product, build/target/product/generic.mk)

# Overrides
PRODUCT_NAME := skyviia_frenzy
PRODUCT_DEVICE := sv886x
PRODUCT_BRAND := skyviia
PRODUCT_MANUFACTURER := skyviia
PRODUCT_MODEL := Android on sv8860

# specified locales
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

#PRODUCT_PROPERTY_OVERRIDES += \
#        xxxxx.xxx.xxxx=ooooooooo \
#        xxxxx.xxx.xxxx=ooooooooo \
PRODUCT_PROPERTY_OVERRIDES += \
        ro.sf.lcd_density=360

# work around files!
#PRODUCT_COPY_FILES += \
#    device/skyviia/sv886x/sys_power/ac_online:sys_power/ac_online \
#    device/skyviia/sv886x/sys_power/battery_capacity:sys_power/battery_capacity \
#    device/skyviia/sv886x/sys_power/battery_health:sys_power/battery_health \
#    device/skyviia/sv886x/sys_power/battery_present:sys_power/battery_present \
#    device/skyviia/sv886x/sys_power/battery_status:sys_power/battery_status \
#    device/skyviia/sv886x/sys_power/battery_technology:sys_power/battery_technology \
#    device/skyviia/sv886x/system/etc/dhcpcd/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
#    device/skyviia/sv886x/init.rc:init.rc

