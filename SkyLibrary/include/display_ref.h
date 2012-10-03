/* 
 *
 * Copyright (C) 2009 Skyviia, Inc.
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
 
#ifndef __SKY_DISP2_H__
#define __SKY_DISP2_H__

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
typedef struct skyfb_api_rectangle_fill classSKYFB_RF_FILL;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Begin of Device Define
//
#define		SKYFB_DEVICE_NAME_1				"/dev/graphics/fb0"//"/dev/fb0"
#define		SKYFB_DEVICE_NAME_2				"/dev/graphics/fb0"//"/dev/graphics/fb1"
#define		SKYFB_FBSIZE							1920*1080*13
//End of Device Define
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define		SKY_PLAY_BAR_PATH				"/data/system/"

#define		PROT_READ						0x1							/* page can be read */
#define		PROT_WRITE						0x2							/* page can be written */
#define		MAP_SHARED						0x01						/* Share changes */

//#define		O_RDONLY						00000000
//#define		O_WRONLY						00000001
//#define		O_RDWR							00000002

//#define __OSD_SUPPORT__
//#define __USE_DISP2__

#endif

