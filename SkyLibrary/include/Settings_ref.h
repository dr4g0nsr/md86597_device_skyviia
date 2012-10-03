/* 
 *
 * Copyright (C) 2009 Skymedi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
 
#ifndef __SKY_OSD_H__
#define __SKY_OSD_H__

#ifndef __SKY_LINUX_DEF__
//#define __SKY_LINUX_DEF__
#endif

//Define uint32_t
#ifndef uint32_t
#define uint32_t	unsigned long
#endif
//Define uint16_t
#ifndef uint16_t
#define uint16_t	unsigned short
#endif
//Define uint8_t
#ifndef uint8_t
#define uint8_t		unsigned char
#endif

#include "sky_api.h"

typedef uint32_t Error_Return;

typedef struct skyfb_api_display_info classSKYFB_GET_DISPLAY_INFO;
typedef struct skyfb_api_display_parm classSKYFB_API_DISPLAY_PARM;
typedef struct skyfb_api_display_status classSKYFB_API_DISPLAY_STATUS;
typedef struct skyfb_api_display_addr classSKYFB_API_DISPLAY_ADDR;
typedef struct skyfb_api_scalar_parm classSKYFB_API_SCALAR_PARM;
typedef struct skyfb_api_osd_common classSKY_SET_OSD_COMM;
typedef struct skyfb_api_osd_parm classSKYFB_API_OSD_PARM;
typedef struct skyfb_api_ft_parm classSKYFB_API_FT_PARM; 	//format transform, only valid for YCC420

/**********************************
*  2009/08/26 version
**********************************/
typedef struct skyfb_api_brightness_parm classSKYFB_API_BRIGHTNESS_PARM;
typedef struct skyfb_api_contrast_parm classSKYFB_API_CONTRAST_PARM;
typedef struct skyfb_api_saturation_hue_parm classSKYFB_API_SATURATION_HUE_PARM;
typedef struct skyfb_api_image_parm classSKYFB_API_IMAGE_PARM;
/*********************************/

/**********************************
*  2010/01/08 version
**********************************/
typedef struct skyfb_api_audio_digital_out_parm classSKYFB_API_AUDIO_DIGITAL_OUT_PARM;
/*********************************/

/**********************************
*  2010/04/09 version
**********************************/
typedef struct skyfb_api_display_out_parm classSKYFB_API_DISPLAY_OUT_PARM;
/*********************************/

/**********************************
*  2010/06/20 version
**********************************/
typedef struct skyfb_api_audio_ktv_out_parm classSKYFB_API_AUDIO_KTV_OUT_PARM;
/*********************************/

/**********************************
*  2010/12/09 version
**********************************/
typedef struct skyfb_api_CVBS_type_parm classSKYFB_API_CVBS_TYPE_PARM;
typedef struct skyfb_api_display_mode_parm classSKYFB_API_DISPLAY_MODE_PARM;;
/*********************************/

/**********************************
*  2010/12/21 version
**********************************/
typedef struct skyfb_api_real_display_parm classSKYFB_API_REAL_DISPLAY_PARM;;
/*********************************/

/**********************************
*  2011/01/05 version
**********************************/
typedef struct skyfb_api_YPbPr_mode_parm classSKYFB_API_YPBPR_MODE_PARM;
typedef struct skyfb_api_HDMI_mode_parm classSKYFB_API_HDMI_MODE_PARM;;
/*********************************/


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Begin of Device Define
//
#define		SKYFB_DEVICE_NAME_1				"/dev/graphics/fb0"//"/dev/fb0"
#define		SKYFB_DEVICE_NAME_2				"/dev/graphics/fb0"
////////////////////////////////////////////////////////////////////////////////////////
//Begin of Audio Device Define
#define 	SKYFB_AUDIO_DEVICE_NAME_1			"/dev/dsp"
#define 	SKYFB_AUDIO_DEVICE_NAME_2			"/dev/mixer"
//End of Device Define
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define		SKY_PLAY_BAR_PATH				"/data/system/"

#define		PROT_READ						0x1							/* page can be read */
#define		PROT_WRITE						0x2							/* page can be written */
#define		MAP_SHARED						0x01						/* Share changes */

#define		O_RDONLY						00000000
#define		O_WRONLY						00000001
#define		O_RDWR							00000002
#define 	O_NONBLOCK         					00004000

#endif
