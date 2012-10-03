# make file for new hardware from SKYVIIA SV8860
#


	
# copy configuration file for firmware packing
PRODUCT_COPY_FILES += \
    device/skyviia/sv886x/fw_config.txt:fw_config.txt \
    device/skyviia/sv886x/fw_config_mlc.txt:fw_config_mlc.txt \
    device/skyviia/sv886x/fw_config_mlc8k.txt:fw_config_mlc8k.txt

# include rules from the generic passion board
include device/skyviia/sv8860-common/AndroidBoardCommon.mk
