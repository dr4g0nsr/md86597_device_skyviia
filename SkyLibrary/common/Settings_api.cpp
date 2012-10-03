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
 //#ifndef __SKY_LINUX_DEF__
//#include "stdafx.h"
//#endif

#ifndef __SKY_LINUX_DEF__
#define __SKY_LINUX_DEF__
#endif

#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>


#include<stdlib.h>
#include <string.h>

#include "Settings_api.h"

#define LOG_TAG "SettingsUtil_jni"
#include <utils/Log.h>

settings_api::settings_api()
{
	iGraphic_Device_fd = -1;
};

Error_Return		settings_api::setScaleSize(unsigned int width_out/*must less than 1920*/, unsigned int height_out/*must less than 1080*/)
{
	//uint32_t width_height = width_out<<16 | height_out;
	
	
	LOGW("set_scale");
	//Get the Device Fd
           if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");

                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        ioctl(iGraphic_Device_fd, SKYFB_SET_SCALE_RES , &width_out);
        ioctl(iGraphic_Device_fd, SKYFB_RECOVER_DISPLAY, &width_out);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};
unsigned int		settings_api::getSourceWidth()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
       
       //Get the Graphic Device Information
          if(ioctl(iGraphic_Device_fd, SKYFB_GET_DISPLAY_INFO, &sky_get_graphic_device_info) == -1)
           {

		     LOGW("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
                     return SKY_GET_FB_INFO_FAILED;

           }
		  	close(iGraphic_Device_fd);
          	return sky_get_graphic_device_info.width;
};

unsigned int		settings_api::getSourceHeight()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
       
       //Get the Graphic Device Information
          if(ioctl(iGraphic_Device_fd, SKYFB_GET_DISPLAY_INFO, &sky_get_graphic_device_info) == -1)
           {

		     LOGW("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
                     return SKY_GET_FB_INFO_FAILED;

           }
       	   close(iGraphic_Device_fd); 
           return sky_get_graphic_device_info.height;
};

unsigned int		settings_api::getScaleWidth()
{
	uint32_t width_height;
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
           ioctl(iGraphic_Device_fd, SKYFB_GET_SCALE_RES , &width_height);
		   close(iGraphic_Device_fd);
		   return width_height;
           //return (width_height >> 16) & 0xffff;
};

unsigned int		settings_api::getScaleHeight()
{
	uint32_t width_height;
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        ioctl(iGraphic_Device_fd, SKYFB_GET_SCALE_RES , &width_height);
		close(iGraphic_Device_fd);
        return width_height & 0xffff;
};

unsigned int		settings_api::getImageBrightness(unsigned int display)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display = display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_image_parm.brightness;
};

unsigned int		settings_api::getImageContrast(unsigned int display)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display = display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_image_parm.contrast;
};

unsigned int		settings_api::getImageSaturation(unsigned int display)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display = display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_image_parm.saturation;
};

unsigned int		settings_api::getImageHue(unsigned int display)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display = display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_image_parm.hue;
};

unsigned int		settings_api::setImageBrightness(unsigned int display,unsigned int brightness)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_brightness_parm.display 	= display;
        sky_api_brightness_parm.brightness 	= brightness;
        ioctl(iGraphic_Device_fd, SKYFB_SET_BRIGHTNESS,&sky_api_brightness_parm);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setImageContrast(unsigned int display,unsigned int contrast)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_contrast_pram.display 		= display;
        sky_api_contrast_pram.contrast 		= contrast;
        ioctl(iGraphic_Device_fd, SKYFB_SET_CONTRAST,&sky_api_contrast_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setImageSaturation(unsigned int display,unsigned int saturation)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display 		= display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
        sky_api_saturation_hue_pram.display 	= display;
        sky_api_saturation_hue_pram.hue		= sky_api_image_parm.hue;
        sky_api_saturation_hue_pram.saturation 	= saturation;
        ioctl(iGraphic_Device_fd, SKYFB_SET_SATURATION_HUE,&sky_api_saturation_hue_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setImageHue(unsigned int display,unsigned int hue)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_image_parm.display 		= display;
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_IMAGE_PARM, &sky_api_image_parm)==-1)
        {
        	LOGW("SKYFB_GET_IMAGE_PARM ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
        sky_api_saturation_hue_pram.display 	= display;
        sky_api_saturation_hue_pram.hue 	= hue;
        sky_api_saturation_hue_pram.saturation  = sky_api_image_parm.saturation;
        ioctl(iGraphic_Device_fd, SKYFB_SET_SATURATION_HUE,&sky_api_saturation_hue_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::AudioInit(unsigned int audio_output_mode,unsigned int audio_ktv_mode)
{
	if((audio_fd = open(SKYFB_AUDIO_DEVICE_NAME_1, O_WRONLY | O_NONBLOCK)) == -1)
           {
     		LOGW("Open frame buffer device failed\n");
                return SKY_FRAME_BUFFER_OPEN_FAILED;

           }
	  // set audio output mode
      sky_api_audio_digital_out_pram.interface 	= audio_output_mode;
      ioctl(audio_fd, SNDCTL_DSP_INTERFACE,&sky_api_audio_digital_out_pram);
	  // set audio ktv mode
	  skyfb_api_audio_ktv_out_parm.interface 	= audio_ktv_mode;
      ioctl(audio_fd, SNDCTL_KTV_MODE,&skyfb_api_audio_ktv_out_parm);
	  fsync(audio_fd);
      close(audio_fd);
      return SKY_PROCESS_SUCCESS;
}
unsigned int		settings_api::setAudioDigitalOut(unsigned int mode)
{
	if((audio_fd = open(SKYFB_AUDIO_DEVICE_NAME_1, O_WRONLY | O_NONBLOCK)) == -1)
           {
     		LOGW("Open frame buffer device failed\n");
                return SKY_FRAME_BUFFER_OPEN_FAILED;

           }
      sky_api_audio_digital_out_pram.interface 	= mode;
      ioctl(audio_fd, SNDCTL_DSP_INTERFACE,&sky_api_audio_digital_out_pram);
	  fsync(audio_fd);
      close(audio_fd);
      return SKY_PROCESS_SUCCESS;
};

int		settings_api::setDisplayOut(unsigned int mode)
{
    int checkmode;
	if(mode >= 0xFFFF0000) {
		checkmode = 1;
        LOGW("Check displayOutput !!!\n");
	}
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return -1;

                     }

           }
        sky_api_display_out_pram.interface 		= mode;
        if(ioctl(iGraphic_Device_fd, SKYFB_SET_OUTPUT_DEVICE, &sky_api_display_out_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return -1;
        }
		if(checkmode == 1) {
     		// Check changed video output is right
     		if(sky_api_display_out_pram.interface == 0xFF){
                LOGE("change video output failed\n");
     		   return -1;
     		}
		}
		close(iGraphic_Device_fd);
        return 0;
};

unsigned int		settings_api::getDisplayOut()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_OUTPUT_DEVICE, &sky_api_display_out_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_display_out_pram.interface;
};

unsigned int		settings_api::checkDisplayOut(unsigned int mode)
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_display_out_pram.interface 		= mode;
        if(ioctl(iGraphic_Device_fd, SKYFB_CHECK_OUTPUT_DEVICE, &sky_api_display_out_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setAudioKtvOut(unsigned int mode)
{
	if((audio_fd = open(SKYFB_AUDIO_DEVICE_NAME_2, O_WRONLY | O_NONBLOCK)) == -1)
           {
     		LOGW("Open frame buffer device failed\n");
                return SKY_FRAME_BUFFER_OPEN_FAILED;

           }
      skyfb_api_audio_ktv_out_parm.interface 	= mode;
      ioctl(audio_fd, SNDCTL_KTV_MODE,&skyfb_api_audio_ktv_out_parm);
	  fsync(audio_fd);
      close(audio_fd);
      return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setCVBStype(unsigned int type)
{
    //Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_CVBS_type_pram.type 		= type;
        ioctl(iGraphic_Device_fd, SKYFB_SET_CVBS_TYPE,&sky_api_CVBS_type_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setSCARTtype(unsigned int type)
{
    //Get the Device Fd
    int scart_type;
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)
                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        scart_type 		= type;
		LOGW("setSCARTtype %d \n",scart_type);		
        ioctl(iGraphic_Device_fd, SKYFB_SET_SCART_TYPE,&scart_type);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::getSCARTtype()
{

    int scart_type;
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;
                     }
           }
       
        ioctl(iGraphic_Device_fd, SKYFB_GET_SCART_TYPE,&scart_type);
		LOGW("getSCARTtype %d \n",scart_type);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::getDisplayMode()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_MODE, &sky_api_display_mode_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return (sky_api_display_mode_pram.mode >> 16)&0xFFFF;
};

unsigned int		settings_api::getRealDisplayWidth()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_REAL_DISPLAY, &sky_api_real_display_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return (sky_api_real_display_pram.output >> 16)&0xFFFF;
};

unsigned int		settings_api::getRealDisplayHeight()
{
	//Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        if(ioctl(iGraphic_Device_fd, SKYFB_GET_REAL_DISPLAY, &sky_api_real_display_pram)==-1)
        {
        	LOGW("SKYFB_SET_OUTPUT_DEVICE ioctl failed\n");
                return SKY_GET_FB_INFO_FAILED;
        }
		close(iGraphic_Device_fd);
        return sky_api_real_display_pram.output & 0xFFFF;
};

unsigned int		settings_api::setYPbPrMode(unsigned int mode)
{
    //Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_YPbPr_mode_pram.mode 		= mode;
        ioctl(iGraphic_Device_fd, SKYFB_SET_YPBPR_MODE,&sky_api_YPbPr_mode_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};

unsigned int		settings_api::setHDMIMode(unsigned int mode)
{
    //Get the Device Fd
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
           {
                     if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)

                     {
                     		LOGW("Open frame buffer device failed\n");
                                //printf("Open frame buffer device failed\n");
                                return SKY_FRAME_BUFFER_OPEN_FAILED;

                     }

           }
        sky_api_HDMI_mode_pram.mode 		= mode;
        ioctl(iGraphic_Device_fd, SKYFB_SET_HDMI_MODE,&sky_api_HDMI_mode_pram);
		close(iGraphic_Device_fd);
        return SKY_PROCESS_SUCCESS;
};


