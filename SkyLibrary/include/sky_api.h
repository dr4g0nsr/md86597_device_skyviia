/* linux/drivers/video/skyviia/sky_api.h
 *
 * Copyright (C) 2009 Skyviia, Inc. 
 * Author: JimmyHung <jimmyhung@skyviia.com.tw>
 *
 */

#ifndef __SKY_API_H__
#define __SKY_API_H__

/* Display IOCTL functions */
enum {
	SKYFB_GET_DISPLAY_INFO 			= 0xffff1000,
	SKYFB_SET_DISPLAY_PARM,			//0xffff1001
	SKYFB_SET_DISPLAY_STATUS,		//0xffff1002
	SKYFB_SET_DISPLAY_ADDR,			//0xffff1003
	SKYFB_SET_SCALAR_PARM,			//0xffff1004
	SKYFB_SET_OSD_COMM,				//0xffff1005
	SKYFB_SET_OSD_PARM,				//0xffff1006
	SKYFB_SET_OSD_STATUS,			//0xffff1007
	SKYFB_FORMAT_TRANSFORM,			//0xffff1008
	SKYFB_WAIT_VSYNC,				//0xffff1009
	SKYFB_GET_DISPLAY_ADDR,			//0xffff100A
	SKYFB_SET_BRIGHTNESS,			//0xffff100B
	SKYFB_SET_CONTRAST,				//0xffff100C
	SKYFB_LOCK_DISPLAY,				//0xffff100D
	SKYFB_UNLOCK_DISPLAY,			//0xffff100E
	SKYFB_SET_SATURATION_HUE,		//0xffff100F
	SKYFB_GET_IMAGE_PARM, 			//0xffff1010
	SKYFB_SET_SCALE_RES,			//0xffff1011
	SKYFB_GET_SCALE_RES,			//0xffff1012
	SKYFB_RECOVER_DISPLAY,			//0xffff1013
	SKYFB_GET_OUTPUT_DEVICE,		//0xffff1014
	SKYFB_GET_1080_CAP,				//0xffff1015
	SKYFB_SET_MODE_1080,			//0xffff1016
	SKYFB_SET_MODE_720,				//0xffff1017
	SKYFB_STOP_ANIMATION,			//0xffff1018
	SKYFB_VIDEO_THUMBNAIL,			//0xffff1019
	SKYFB_CHECK_OUTPUT_DEVICE,		//0xffff101A
	SKYFB_SET_OUTPUT_DEVICE,		//0xffff101B
	SKYFB_GET_LOCK_STATUS,			//0xffff101C
	SKYFB_SET_MODE_ONLY,			//0xffff101D
	SKYFB_COPY_ANIMATION,			//0xffff101E
	SKYFB_GET_MODE,				    //0xffff101F
	SKYFB_GET_VIDEO_SETMODE_STATUS, //0xffff1020
	SKYFB_SET_VIDEO_RATIO,			//0xffff1021
	SKYFB_SET_DEDICATED_MODE,		//0xffff1022
	SKYFB_SET_DEINTERLACE_STATUS,	//0xffff1023
	SKYFB_SET_YPBPR_MODE,			//0xffff1024
	SKYFB_GET_SUPPORT_MODES,	    //0xffff1025
	SKYFB_SET_VIDEO_RES,			//0xffff1026
	SKYFB_SET_VIDEO_ZOOM_CMD,		//0xffff1027
	SKYFB_SET_32PULLDOWN_ADDR,		//0xffff1028
	SKYFB_SET_VIDEO_ARGB_FMT,		//0xffff1029
	SKYFB_SET_SUPERDISPLAY_STATUS,	//0xffff102A
	SKYFB_SET_PHOTO_ZOOM_CMD,		//0xffff102B
	SKYFB_GET_DISP_FPS,				//0xffff102C
	SKYFB_SET_PHOTO_ROTATION_CMD,	//0xffff102D
	SKYFB_SET_CVBS_TYPE,			//0xffff102E
	SKYFB_GET_REAL_DISPLAY,			//0xffff102F
	SKYFB_SET_HDMI_MODE,			//0xffff1030
	SKYFB_IGNORE_VIDEO_US,			//0xffff1031
	SKYFB_SET_LED_BLINK_PWM,			//0xffff1032
	SKYFB_SET_LED_BLINK,			//0xffff1033
	SKYFB_SET_FIXED_VIDEO_POSITION,	//0xffff1034
	SKYFB_SCALAR_FOR_ANDROID_APP,		//0xffff1035
	SKYFB_SET_SCART_TYPE,			//0xffff1036
	SKYFB_GET_SCART_TYPE
};

/* 2D IOCTL functions */
enum {
	SKYFB_2D_BITBLT = 0xffff2000,
	SKYFB_2D_LINE_DRAW,
	SKYFB_2D_RECTANGLE_FILL,
	SKYFB_2D_PATTERN_FILL,
	SKYFB_2D_ROP,
	SKYFB_2D_ROTATION,
	SKYFB_CURSOR_SET_BITMAP,
	SKYFB_CURSOR_SET_PARM,
	SKYFB_2D_FORMAT,		//ARGB or RGB565
};

/* TR IOCTL functions */
enum {
	SKYFB_TR = 0xffff3000,
	SKYFB_GET_TR_INFO,
	SKYFB_SET_TR_OP,
	SKYFB_SET_TR_YUV,
};

/* DMA IOCTL functions */
enum {
	SKYFB_DMA = 0xffff4000,
};

/* OSD IOCTL functions */
enum {
	SKYFB_OSD_INIT = 0xffff5000,
	SKYFB_OSD_FILL,
	SKYFB_OSD_ERASE,
};

/* COMMON DEFINES */
enum {
	SKYFB_FALSE = 0,
	SKYFB_TRUE,
};

enum {
	SKYFB_OFF = 0,
	SKYFB_ON,
};

enum {
	SKYFB_DISP1 = 0,
	SKYFB_DISP2,
};

enum {
	SKYFB_BKG = 0,
	SKYFB_OSD,
};

enum {
	SKYFB_SCALAR_DISPLAY = 0,
	SKYFB_SCALAR_MEMORY,
};

enum {
	SKYFB_NONE_SET = 0,
	SKYFB_SET,
};

enum {
	SKYFB_SCALAR_NONE = 0,
	SKYFB_SCALAR_UP,
	SKYFB_SCALAR_DOWN,
};

enum {	
	INPUT_FORMAT_422 = 0,
	INPUT_FORMAT_420,
	INPUT_FORMAT_RGB888,
	INPUT_FORMAT_ARGB,
	INPUT_FORMAT_YCC420,
	INPUT_FORMAT_RGB565,
	INPUT_FORMAT_MAX,
};

enum {
	RATIO_ORIGINAL = 0,
	RATIO_FIT,
	RATIO_1_1,
	RATIO_4_3,
	RATIO_16_9,
};

enum {
	SKYFB_OSD1 = 0,
	SKYFB_OSD2,
	SKYFB_OSD3,
	SKYFB_OSD5,
	SKYFB_OSD4,	//ECO use
};

enum {
	SKYFB_HDMI = 0,
	SKYFB_YPBPR,
	SKYFB_CVBS_SVIDEO,
	SKYFB_DSUB,
	SKYFB_LCD,
	SKYFB_DVI,
};

enum {
	BB_DIR_START_UL = 0,//up left
	BB_DIR_START_UR,	//up right
	BB_DIR_START_DL,	//down left
	BB_DIR_START_DR,	//down right
};

enum {
	AB_FROM_REG = 0,	//alpha blending value from register
	AB_FROM_SRC,		//alpha blending value from source
	AB_FROM_DST,		//alpha blending value from destination 
};

enum {
	AV_FROM_REG = 0,	//alpha value from register
	AV_FROM_SRC,		//alpha value from source
	AV_FROM_PAT,		//alpha value from pattern
	AV_FROM_DST,		//alpha value from destination
	AV_FROM_G2D,		//alpha value depend on 2d command
};

enum {
	MODE_1080P_60HZ = 1,
	MODE_1080P_50HZ,
	MODE_1080P_30HZ,
	MODE_1080P_24HZ,
	MODE_1080I_30HZ,
};

enum {
	G2D_ARGB_FORMAT = 0,
	G2D_RGB565_FORMAT,
};

//for video
enum {
	ZOOM_NORMAL = 0,
	ZOOM_READY,
	ZOOM_LEVEL1,
	ZOOM_LEVEL2,
};

//for photo
enum {
	ZOOM_NONE = 0,
	ZOOM_X1,
	ZOOM_X2,
	ZOOM_X3,
	ZOOM_X4,
	ZOOM_X5,
	ZOOM_X6,
	ZOOM_X7,
	ZOOM_X8,
	ZOOM_X9,
	ZOOM_X10,
	ZOOM_X11,
	ZOOM_X12,
};

enum {
	CMD_ZOOM_IN = 0,
	CMD_ZOOM_OUT,
	CMD_PAN_TOP,
	CMD_PAN_DOWN,
	CMD_PAN_LEFT,
	CMD_PAN_RIGHT,
	CMD_RESET,
	CMD_NORMAL,
};

enum {
	CMD_ROTATION_90 = 0,
	CMD_ROTATION_180,
	CMD_ROTATION_270,
	CMD_ROTATION_360,
	CMD_ROTATION_RESET,
};

/* HDCP IOCTL functions */
enum {
     SKYFB_HDCP_GET_STATUS = 0xffff8000,
	SKYFB_HDCP_RE_AUTH,
#ifdef CONFIG_BURN_KEY
	SKYFB_HDCP_KEY_CHECK  = 0xffff8100, 
	SKYFB_HDCP_KEY_BURN      = 0xffff8200,
	SKYFB_HDCP_RESULT_CHECK  = 0xffff8300
#endif	
};

enum{
	SKYFB_HDMI_GET_AUDIO_INFO = 0xffff7000, 
};

struct skyfb_api_display_info {
	uint32_t fb_base_addr;
	uint32_t fb_size;
	uint32_t video_offset;
	uint32_t video_size;
	uint32_t uncached_offset;
	uint32_t uncached_size;
	uint32_t width;		//screen width
	uint32_t height;		//screen height
	uint32_t rgb0_offset;	//offset is based on fb_base_addr
	uint32_t rgb1_offset;	//offset is based on fb_base_addr
	uint32_t osd_offset;	//offset is based on fb_base_addr
	uint32_t y_offset;		//offset is based on fb_base_addr
	uint32_t u_offset;		//offset is based on fb_base_addr
	uint32_t v_offset;		//offset is based on fb_base_addr
};

struct skyfb_api_display_parm {
	uint32_t display;		//display1 or display2
	uint32_t input_format;	//ARGB, YCC420
	uint32_t start_x;		//display 2 only
	uint32_t start_y;		//display 2 only
	uint32_t width_in;		//if no scalar, width_in = width_out
	uint32_t height_in;		//if no scalar, height_in = height_out
	uint32_t width_out;		//display 1 only
	uint32_t height_out;	//display 1 only
	uint32_t stride;
	uint32_t alpha;		//YCC420 global ahpha, display 2 only
	uint32_t y_addr;		//YCC420, ARGB
	uint32_t u_addr;		//YCC420
	uint32_t v_addr;		//none use in ARGB and YCC420 mode
};

struct skyfb_api_display_status {
	uint32_t display;		//display1 or display2
	uint32_t status;		//on/off
};

struct skyfb_api_display_addr {
	uint32_t display;		//display1 or display2
	uint32_t y_addr;		//YCC420, ARGB
	uint32_t u_addr;		//YCC420
	uint32_t v_addr;		//none use in ARGB and YCC420 mode
};

struct skyfb_api_scalar_parm {
	uint32_t width_in;
	uint32_t height_in;
	uint32_t width_out;
	uint32_t height_out;
	uint32_t stride_in;		//scalar to memory only
	uint32_t stride_out;	//scalar to memory only
	uint32_t y_addr_in;		//scalar to memory only
	uint32_t u_addr_in;		//scalar to memory only
	uint32_t y_addr_out;	//scalar to memory only
	uint32_t u_addr_out;	//scalar to memory only
	uint32_t pseudo;		//if true, just get result width and height
	uint32_t scale_to;		//display or memory
	uint32_t scale_mode;	//original ratio or fit in resolution
};

struct skyfb_api_osd_common {
	uint32_t addr;
	uint32_t size;
	uint32_t palette_change;	//if true then replace palette
	uint32_t red;			//r3<<24 | r2<<16 | r1<<8 | r0
	uint32_t green1;		//g3<<24 | g2<<16 | g1<<8 | g0
	uint32_t green2;		//g7<<24 | g6<<16 | g5<<8 | g4
	uint32_t blue;			//b3<<24 | b2<<16 | b1<<8 | b0
};

struct skyfb_api_osd_parm {
	uint32_t index;		//select osd 1~5;
	uint32_t alpha;		//alpha 0~15
	uint32_t status;		//select osd on/off
	uint32_t start_x;
	uint32_t start_y;
	uint32_t width;
	uint32_t height;
};

struct skyfb_api_brightness_parm {	
	uint32_t display;		//display1 or display2
	uint32_t brightness;	//0~255
};

struct skyfb_api_contrast_parm {
	uint32_t display;		//display1 or display2
	uint32_t contrast;		//0~255
};

struct skyfb_api_saturation_hue_parm {	
	uint32_t display;		//display1 or display2
	uint32_t saturation;	//0~127
	uint32_t hue;			//0~100
};

struct skyfb_api_image_parm {	
	uint32_t display;		//display1 or display2
	uint32_t brightness;	//0~255
	uint32_t contrast;		//0~255
	uint32_t saturation;	//0~127
	uint32_t hue;			//0~100
};

struct skyfb_api_video_thumbnail {
	uint32_t flag;
	uint32_t start_x;
	uint32_t start_y;
	uint32_t width;
	uint32_t height;
	uint32_t mode;
	uint32_t realw;		//UI don't care this
	uint32_t realh;		//UI don't care this
};

struct skyfb_api_ft_parm {	//format transform, only valid for YCC420
	uint32_t src_y_addr;
	uint32_t src_u_addr;
	uint32_t dst_y_addr;	//ARGB output address
	uint32_t src_stride;
	uint32_t dst_stride;
	uint32_t width;
	uint32_t height;
	uint32_t alpha;		//optional, set to output alpha
};

struct skyfb_api_32pd_parm {	//3:2 pull down second source address
	uint32_t y_addr;
	uint32_t u_addr;
};

//2D API structure
struct skyfb_api_bitblt {
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t width;
	uint32_t height;
	uint32_t src_stride;
	uint32_t dst_stride;
	uint32_t direction;			//0~3, need auto detection???
	uint32_t alpha_value_from;
	uint32_t alpha_value;
	uint32_t alpha_blend_status;
	uint32_t alpha_blend_from;
	uint32_t alpha_blend_value;
};

struct skyfb_api_line_draw {
	uint32_t dst_addr;
	uint32_t dst_stride;
	uint32_t start_x;
	uint32_t start_y;
	uint32_t end_x;
	uint32_t end_y;
	uint32_t color;			//[31:0]ARGB
	uint32_t alpha_value_from;
	uint32_t alpha_value;	
};

struct skyfb_api_rectangle_fill {
	uint32_t dst_addr;
	uint32_t dst_stride;
	uint32_t width;
	uint32_t height;
	uint32_t color;			//[31:0]ARGB
	uint32_t alpha_value_from;
	uint32_t alpha_value;
	uint32_t alpha_blend_status;
	uint32_t alpha_blend_from;
	uint32_t alpha_blend_value;
};

struct skyfb_api_pattern_fill {
	uint32_t dst_addr;
	uint32_t dst_stride;
	uint32_t width;
	uint32_t height;
	uint32_t fg_color;			//[31:0]ARGB
	uint32_t bg_color;			//[31:0]ARGB
	uint32_t value1;			//[31:0]line4~line1, each [7:0] pixel 8~1
	uint32_t value2;			//[31:0]line8~line5, each [7:0] pixel 8~1
	uint32_t alpha_value_from;
	uint32_t alpha_value;
	uint32_t alpha_blend_status;
	uint32_t alpha_blend_from;
	uint32_t alpha_blend_value;
};

struct skyfb_api_cursor_parm {
	uint32_t status;			//on/off
	uint32_t xpos;				//0~width-1
	uint32_t ypos;				//0~height-1
	uint32_t alpha;			//0~15
};

struct skyfb_api_dma {
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t size;
};

struct skyfb_api_osd {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t data_addr;
	uint32_t block;
	uint32_t alpha;
};

//HDMI audio info
struct hdmiAudioInfo{
  uint8_t bType;
  uint8_t bSRate;
  uint8_t bRes;
};

struct hdmiSupportAudioInfo_t
{
  uint8_t bNum;
  struct hdmiAudioInfo bAudInfo[15];
}__attribute__((packed));

struct hdmiSupportModes_t {
	uint32_t x_res;
	uint32_t y_res;
	uint32_t fps;
	uint32_t interlace;
};

struct skyfb_api_display_out_parm {
	int interface;	//HDMI , YPbPr , CVBS/S-Video or D-SUB
};

struct skyfb_api_CVBS_type_parm {
	int type;	//0:NTSC mode 2:PAL mode
};

struct skyfb_api_display_mode_parm {
	int mode;	//1080P 60 Hz, 1080P 50 Hz.........
};

struct skyfb_api_real_display_parm {
	uint32_t output;	//width<<16+height
};

struct skyfb_api_YPbPr_mode_parm {
	uint32_t mode;	//1080P 60Hz, 1080P 50HZ.........
};

struct skyfb_api_HDMI_mode_parm {
	uint32_t mode;	//1080P 60Hz, 1080P 50HZ.........
};

// Audio API structure
// Audio output API
struct skyfb_api_audio_digital_out_parm {	
	uint32_t interface;	//HDMI PCM or S/PDIF PCM
};

// Audio channels API (for set 1 channel)
struct skyfb_api_audio_ktv_out_parm {	
	uint32_t interface;	//HDMI PCM or S/PDIF PCM
};

/* Audio IOCTL function */
enum {
	SNDCTL_DSP_INTERFACE = 0x19810528
};
enum {
	SNDCTL_KTV_MODE = 0x20100621
};

void api_get_display_info(struct skyfb_api_display_info *d_info);
int api_set_display_parm(struct skyfb_api_display_parm *d_parm);
int api_set_display_addr(struct skyfb_api_display_addr *d_addr);
uint32_t api_set_scalar_parm(struct skyfb_api_scalar_parm *s_parm);
void api_format_transform(struct skyfb_api_ft_parm *ft_parm);
void set_display_status(int dev, int status);
void api_2d_bitblt(struct skyfb_api_bitblt *bb);
void api_2d_rectangle_fill(struct skyfb_api_rectangle_fill *rf);
void dynamic_set_mode(uint32_t mode, int clear, int init, int scale);
void select_hdmi_mode(void);
void select_ypbpr_mode(void);

#endif	/* __SKY_API_H__ */

