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
 
 #include "Settings_ref.h"

class settings_api
{
private:
	int iGraphic_Device_fd;
	classSKYFB_GET_DISPLAY_INFO		sky_get_graphic_device_info;
	classSKYFB_API_DISPLAY_PARM		sky_api_display_parm;
	classSKYFB_API_SCALAR_PARM		sky_api_scalar_parm;
	/******************************************
	*	2009/08/26 version.
	******************************************/
	classSKYFB_API_IMAGE_PARM		sky_api_image_parm;
	classSKYFB_API_BRIGHTNESS_PARM		sky_api_brightness_parm;
	classSKYFB_API_CONTRAST_PARM		sky_api_contrast_pram;
	classSKYFB_API_SATURATION_HUE_PARM	sky_api_saturation_hue_pram;
	/*****************************************/
	
	/******************************************
	*	2010/01/08 version.
	******************************************/
	int audio_fd;
	classSKYFB_API_AUDIO_DIGITAL_OUT_PARM		sky_api_audio_digital_out_pram;
	/*****************************************/

	/******************************************
	*	2010/04/09 version.
	******************************************/
	classSKYFB_API_DISPLAY_OUT_PARM		sky_api_display_out_pram;
	/*****************************************/
	
	/******************************************
	*	2010/06/20 version.
	******************************************/
	classSKYFB_API_AUDIO_KTV_OUT_PARM		skyfb_api_audio_ktv_out_parm;
	/*****************************************/

	/******************************************
	*	2010/12/09 version.
	******************************************/
	classSKYFB_API_CVBS_TYPE_PARM		sky_api_CVBS_type_pram;
	classSKYFB_API_DISPLAY_MODE_PARM		sky_api_display_mode_pram;
	/*****************************************/
	
	/******************************************
	*	2010/12/21 version.
	******************************************/
	classSKYFB_API_REAL_DISPLAY_PARM sky_api_real_display_pram;
	/*****************************************/

	/******************************************
	*	2011/01/05 version.
	******************************************/
	classSKYFB_API_YPBPR_MODE_PARM sky_api_YPbPr_mode_pram;
	classSKYFB_API_HDMI_MODE_PARM sky_api_HDMI_mode_pram;
	/*****************************************/
public:
	//Contstructor
	settings_api();
	//set_scale
	//
	//Input Parameter:
	//									width_out:
	//									height_out:
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return	setScaleSize(unsigned int width_out,unsigned int height_out);
	//get_source_width
	//
	//Input Parameter:
	//									
	//Return:					width
	//
	//Status: 					Reserved!!!
	unsigned int		getSourceWidth();
	//get_source_height
	//
	//Input Parameter:
	//									
	//Return:					height
	//
	//Status: 					Reserved!!!
	unsigned int		getSourceHeight();
	//get_source_width
	//
	//Input Parameter:
	//									
	//Return:					width
	//
	//Status: 					Reserved!!!
	unsigned int		getScaleWidth();
	//get_source_height
	//
	//Input Parameter:
	//									
	//Return:					height
	//
	//Status: 					Reserved!!!
	unsigned int		getScaleHeight();
	//get_image_brightness
	//
	//Input Parameter:
	//									
	//Return:					brightness
	//
	//Status: 					Reserved!!!
	unsigned int		getImageBrightness(unsigned int display);
	//get_image_contrast
	//
	//Input Parameter:
	//									
	//Return:					contrast
	//
	//Status: 					Reserved!!!
	unsigned int		getImageContrast(unsigned int display);
	//get_image_saturation
	//
	//Input Parameter:
	//									
	//Return:					saturation
	//
	//Status: 					Reserved!!!
	unsigned int		getImageSaturation(unsigned int display);
	//get_image_hue
	//
	//Input Parameter:
	//									
	//Return:					hue
	//
	//Status: 					Reserved!!!
	unsigned int		getImageHue(unsigned int display);
	//set_brightness
	//
	//Input Parameter:				display
	//						brightness			
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setImageBrightness(unsigned int display,unsigned int brightness);
	//set_contrast
	//
	//Input Parameter:				display
	//						contrast			
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setImageContrast(unsigned int display,unsigned int contrast);
	//set_saturation
	//
	//Input Parameter:				display
	//						saturation			
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setImageSaturation(unsigned int display,unsigned int saturation);
	//set_hue
	//
	//Input Parameter:				display
	//						hue			
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setImageHue(unsigned int display,unsigned int hue);

	//set_audio_digital_out
	//
	//Input Parameter:				audio_out_mode, audio_ktv_mode
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		AudioInit(unsigned int audio_output_mode,unsigned int audio_ktv_mode);
	
	//set_audio_digital_out
	//
	//Input Parameter:				mode
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setAudioDigitalOut(unsigned int mode);
	//set_display_out
	//
	//Input Parameter:				mode
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	int		setDisplayOut(unsigned int mode);
	//get_display_out
	//
	//Input Parameter:				
	//									
	//Return:					
	//							mode
	//Status: 					Reserved!!!
	unsigned int		getDisplayOut();
	//check_display_out
	//
	//Input Parameter:				mode
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		checkDisplayOut(unsigned int mode);
	//set_audio_channel_out
	//
	//Input Parameter:				mode
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setAudioKtvOut(unsigned int mode);

	//set_CVBS_type
	//
	//Input Parameter:				type
	//									
	//Return:					
	//
	//Status: 					Reserved!!!
	unsigned int		setCVBStype(unsigned int type);
	unsigned int		setSCARTtype(unsigned int type);	
	unsigned int		getSCARTtype();	
	
	//get_display_mode
	//
	//Input Parameter:				
	//									
	//Return:					
	//							mode
	//Status: 					Reserved!!!
	unsigned int		getDisplayMode();
	//get_real_display_width
	//
	//Input Parameter:				
	//									
	//Return:					
	//							width
	//Status: 					Reserved!!!
	unsigned int		getRealDisplayWidth();
	//get_real_display_height
	//
	//Input Parameter:				
	//									
	//Return:					
	//							height
	//Status: 					Reserved!!!
	unsigned int		getRealDisplayHeight();
	//set_YPbPr_mode
	//
	//Input Parameter:			mode	
	//									
	//Return:					
	//							
	//Status: 					Reserved!!!
	unsigned int		setYPbPrMode(unsigned int mode);

	//set_HDMI_mode
	//
	//Input Parameter:			mode	
	//									
	//Return:					
	//							
	//Status: 					Reserved!!!
	unsigned int		setHDMIMode(unsigned int mode);
};

#define DISPLAY_2_FOR_OSD				0X01
#define DISPLAY_2_FOR_NORMAL_DISPLAY			0X02


//=========================================================================
//			[Error_Return] Defined
// define the message returned to the caller party
#define SKY_FRAME_BUFFER_OPEN_FAILED	 0x00000001
#define SKY_FILE_CANT_OPENED			 0x00000002
#define SKY_GET_FB_INFO_FAILED			 0x00000003
#define SKY_SET_DISP_PARAM_FAILED		 0x00000004
#define SKY_SET_DISP2_ON_FAILED			 0x00000005
#define SKY_SET_DISP2_OFF_FAILED		 0x00000006
#define SKY_WRONG_OFF_TYPE				 0x00000007
#define SKY_CANT_MATCH_THE_MMAP			 0x00000008
#define SKY_NO_SUCH_ID					 0x00000009
#define SKY_FB_ALREADY_OPEN				 0x0000000A
#define SKY_NO_DATA_INPUT				 0x0000000B
#define SKY_PROCESS_SUCCESS				 0x80000000