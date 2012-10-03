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

#include "display_ref.h"

#ifndef __DISP2CTRL__
#define __DISP2CTRL__

#define DISPLAY2_CURRENT_VERSION	"2009-08-14 2.01a"


class Display2_Image_Data
{
public:
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Pointer Information:
	Display2_Image_Data *	Next_ptr;
	Display2_Image_Data *	Previous_ptr;
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Image Information Information:
	uint32_t		ucImage_ID_num;
	uint8_t *	ucImage_pixel;
	uint32_t	uiImage_Width;
	uint32_t	uiImage_Height;
	uint32_t	uiImage_Pos_x;
	uint32_t	uiImage_Pos_y;
	//2009-05-18 Eric Fang-Cheng Liu
	//For checking the image is displayed now or not.
	uint8_t		bImage_Drawed;
};

class disp2_api
{
private:
	int test;
	int iGraphic_Device_fd;
	classSKYFB_GET_DISPLAY_INFO		sky_get_graphic_device_info;
	classSKYFB_API_DISPLAY_PARM		sky_api_display_parm;
	classSKYFB_API_DISPLAY_STATUS	sky_api_display_status;
	classSKYFB_API_DISPLAY_ADDR		sky_api_display_addr;
	classSKYFB_API_SCALAR_PARM		sky_api_scalar_parm;
	classSKY_SET_OSD_COMM			sky_set_osd_comm;
	classSKYFB_API_OSD_PARM			sky_api_osd_parm;
	classSKYFB_API_FT_PARM			sky_api_ft_param;
	classSKYFB_RF_FILL				sky_api_rf_fill;
	Display2_Image_Data			*	disp2_Image_curr;
	Display2_Image_Data 			* 	disp2_Image_Head_ptr;
	Display2_Image_Data			*	disp2_Image_prev;
	uint32_t						display2_width;
	uint32_t						display2_height;
	//2009-08-12 Eric Fang-Cheng Liu
	void						*	fb_mmap_addr;
	
	//Eric Fang-Cheng Liu ~ 2010-03-16
	//Display-2 Color Space Selection
	unsigned char					disp2_color_fmt_sel;


	//set_disp2_image_charateristic
	//
	//Input Parameter:
	//					uint32_t src_image_x
	//					uint32_t src_image_y
	//					uint32_t src_image_width
	//					uint32_t src_image_height
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return		set_disp2_xy_wh_parameter(uint32_t src_image_x,
														uint32_t src_image_y,
														uint32_t src_image_width,
														uint32_t src_image_height);


	//replace_Node_from_disp2_stack
	//
	//Input Parameter:
	//							Display2_Image_Data * Dest_ptr
	//							Display2_Image_Data * Src_ptr
	//					                                           
	//
	//Return:
	//
	//Status:					Reserved!!!
	Error_Return		replace_Node_from_disp2_stack(Display2_Image_Data * Dest_ptr,
															Display2_Image_Data * Src_ptr);


	//set_disp2_image_Width_Height
	//
	//Input Parameter:
	//					uint32_t src_image_width
	//					uint32_t src_image_height
	//
	//Return:
	//					Error_Return
	//Status: 			Reserved!!!
	Error_Return	set_disp2_paramter_Width_Height(uint32_t src_image_width,
														uint32_t src_image_height);
	
	
public:
	//Contstructor
	disp2_api();
	/*Desructor*/
	~disp2_api();
	
	
	//add_image_resource_to_display_2
	//
	//Input Parameter:
	//					image_src: Image Raw Data ARGB format, ex [ARGB][ARGB][ARGB]...
	//					                                           P1     P2    P3
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		add_image_resource_to_display_2_stack(uint8_t * image_src,
															 uint32_t src_image_width,
															 uint32_t src_image_height,
															 uint32_t src_image_pos_x,
															 uint32_t src_image_pos_y,
															 uint32_t src_image_id_num);

	
	//rm_image_from_disp2_stack
	//
	//Input Parameter:
	//					uint32_t target_image_id_num
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		rm_image_from_disp2_stack(uint32_t target_image_id_num);


	
	//replace_image_from_disp2_stack
	//
	//Input Parameter:
	//							image_src: Image Raw Data ARGB format, ex [ARGB][ARGB][ARGB]...
	//					                                           P1     P2    P3
	//
	//Return:
	//
	//Status:					Reserved!!!
	Error_Return		replace_image_from_disp2_stack(	 uint8_t * image_src,
															 uint32_t src_image_width,
															 uint32_t src_image_height,
															 uint32_t src_image_pos_x,
															 uint32_t src_image_pos_y,
															 uint32_t src_image_id_num);


	//draw_image_from_disp2_stack
	//
	//Input Parameter:
	//					uint32_t src_image_id_num
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		draw_image_from_disp2_stack(uint32_t src_image_id_num);
	
	
	//draw_image_from_disp2_stack
	//
	//Input Parameter:
	//					uint32_t src_image_id_num
	//					uint8_t   dsip2_mem_offset
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		draw_image_from_disp2_stack(uint32_t src_image_id_num, uint8_t   disp2_mem_offset);

	//draw_image_directly
	//
	//Input Parameter:
	//					image_src: Image Resource
	//					src_image_width: The width of the source image
	//					src_image_height: The height of the source image 
	//					src_image_pos_x: The X coordinate of the source image
	//					src_image_pos_y: The Y coordinate of the source image
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		draw_image_directly(uint8_t * image_src,
									uint32_t src_image_width,
									uint32_t src_image_height,
									uint32_t src_image_pos_x,
									uint32_t src_image_pos_y);

	//draw_image_directly
	//
	//Input Parameter:
	//					disp2_mem_offset: Display Address Offset (ARGB0, ARGB1, OSD and YUV)
	//					image_src: Image Resource
	//					src_image_width: The width of the source image
	//					src_image_height: The height of the source image 
	//					src_image_pos_x: The X coordinate of the source image
	//					src_image_pos_y: The Y coordinate of the source image
	//					
	//
	//Return:
	//
	//Status: 					Reserved!!!
	Error_Return		draw_image_directly(uint32_t disp2_mem_offset,
									uint8_t * image_src,
									uint32_t src_image_width,
									uint32_t src_image_height,
									uint32_t src_image_pos_x,
									uint32_t src_image_pos_y);

		
	//switch_to_android_fb
	//
	//Input Parameter:
	//
	//Return:						Error_Return
	//
	Error_Return		close_fb();
	
	//switch_to_fb
	//
	//Input Parameter:
	//
	//Return:						Error_Return
	Error_Return		open_fb();

	//reset_display2_env_par
	//
	//Description: Reset the display 2 environment parameters when the environment
	//			has been changed.
	//
	//Input Parameter:
	//
	//Return:						Error_Return
	Error_Return		reset_display2_env_par();

	//change_display2_env_resolution	
	//
	//Description: For designer to change the Display-2 Resolution	
	//	
	//Input Parameter:	
	//					disp2_width: The width of Display 2	
	//					disp2_height: The height of Display 2	
	//					disp2_pos_x: The X coordinate of Display 2	
	//					disp2_pos_y: The Y coordinate of Display 2	
	//			
	//	
	//Return:				Skymedi_Error_Return	
	Error_Return		change_display2_env_resolution(uint32_t disp2_width, uint32_t disp2_height, uint32_t disp2_pos_x, uint32_t disp2_pos_y);

	//change_display2_color_space	
	//
	//Description: For designer to change the Display-2 Resolution	
	//	
	//Input Parameter:	
	//					disp2_color_fmt: Follow Skyviia Kernel Video Driver Setting.
	//			
	//	
	//Return:				Skymedi_Error_Return	
	Error_Return		change_display2_color_space(uint32_t disp2_color_fmt);
	
	
	//set_display_2_on
	//
	//Input Parameter:
	//
	//Return:						Error_Return
	Error_Return set_display_2_on();
	
	//set_display_2_off
	//
	//Input Parameter:
	//
	//Return:						Error_Return
	Error_Return set_display_2_off();

	//set_disp2_image_position
	//
	//Input Parameter:
	//					uint32_t src_image_id_num
	//					uint32_t src_image_x
	//					uint32_t src_image_y
	//					
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return	set_image_position_on_disp2(uint32_t src_image_id_num,
									uint32_t src_image_x,
									uint32_t src_image_y);

	
	////////////////////////////////////////////////////////////////////
	//
	//Description:			
	//					Display-2 Global Alpha Value Setting
	//
	//Input:		
	//					unsigned char in_Global_Alpha:	0 ~ 255 (0x00 ~ 0xFF)
	//Outout: 
	//					Error_Return: Please refer to the description, [Error Message], below.
	//
	////////////////////////////////////////////////////////////////////
	Error_Return Set_disp2_Global_Alpha(unsigned char in_Global_Alpha);

	
	//Clean_displayed_image_on_disp2
	//
	//Input Parameter:
	//					uint32_t src_image_id_num
	//
	//Return:
	//
	//Status:					Reserved!!!
	Error_Return		Clean_part_disp2_drawing_board_by_Image_ID(uint32_t src_image_id_num);
		
	
		
	//Clean_displayed_image_on_disp2
	//
	//Input Parameter:
	//					uint32_t src_image_id_num
	//
	//Return:
	//
	//Status:					Reserved!!!
	Error_Return		Clean_part_disp2_drawing_board_by_Image_ID(uint32_t src_image_id_num, uint8_t	disp2_mem_offset);
	

	//Reset_disp2_drawing_board
	//
	//Input Parameter:
	//					NONE
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return	Clean_whole_disp2_drawing_board();
	
	
	//Reset_disp2_drawing_board
	//
	//Input Parameter:
	//					dsip2_mem_offset:		Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return	Clean_whole_disp2_drawing_board(uint8_t   disp2_mem_offset);


	//Reset_part_disp2_drawing_board
	//
	//Input Parameter:
	//					uint32_t target_x:		The cleaned start Position X
	//					uint32_t target_y:		The cleaned range start position Y
	//					uint32_t target_width:	The cleaned range Width
	//					uint32_t target_height:	The cleaned range Height
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return	Clean_part_disp2_drawing_board(uint32_t cln_start_x,
										uint32_t cln_start_y,
										uint32_t cln_range_width,
										uint32_t cln_range_height);

	
	//Reset_part_disp2_drawing_board
	//
	//Input Parameter:
	//					dsip2_mem_offset:		Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
	//					uint32_t target_x:		The cleaned start Position X
	//					uint32_t target_y:		The cleaned range start position Y
	//					uint32_t target_width:	The cleaned range Width
	//					uint32_t target_height:	The cleaned range Height
	//
	//Return:
	//					Error_Return
	//Status: 					Reserved!!!
	Error_Return	Clean_part_disp2_drawing_board(uint8_t disp2_mem_offset,
										uint32_t cln_start_x,
										uint32_t cln_start_y,
										uint32_t cln_range_width,
										uint32_t cln_range_height);
	
	
	//!!! WARNING !!!
	//
	//The Following function is for test purpose
	//
	//!!! WARNING !!!
	//Load_file_to_Phy_Memory
	//
	//Input Parameter: 
	//									fp: 			File Name
	//									offType:	Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
	//
	//Return:						Error_Return
	Error_Return Load_file_to_Phy_Memory(uint8_t * fn, uint8_t offType,
											uint32_t src_image_x, uint32_t src_image_y,
											uint32_t src_image_width, uint32_t src_image_height,
											uint32_t src_image_id);

	//Pint_Out_the_Disp2_Image_Stack_Info
	//
	//Input Parameter: 
	//					
	//
	//Return:			Error_Return
	Error_Return Pint_Out_the_Disp2_Image_Stack_Info();
};

#define DISPLAY_2_FOR_DISP2				0X01
#define DISPLAY_2_FOR_NORMAL_DISPLAY	0X02

#define DISPLAY_2_DEFAULT_WIDTH		1280
#define DISPLAY_2_DEFAULT_HEIGHT		720
#define DISPLAY_2_MAX_WIDTH			1280
#define DISPLAY_2_MAX_HEIGHT			720


//=========================================================================
//			[Error_Return] Defined
// define the message returned to the caller party
#define SKY_FRAME_BUFFER_OPEN_FAILED			0x00000001
#define SKY_FILE_CANT_OPENED					0x00000002
#define SKY_GET_FB_INFO_FAILED					0x00000003
#define SKY_SET_DISP_PARAM_FAILED				0x00000004
#define SKY_SET_DISP2_ON_FAILED					0x00000005
#define SKY_SET_DISP2_OFF_FAILED				0x00000006
#define SKY_WRONG_OFF_TYPE						0x00000007
#define SKY_CANT_MATCH_THE_MMAP					0x00000008
#define SKY_NO_SUCH_ID							0x00000009
#define SKY_FB_ALREADY_OPEN						0x0000000A
#define SKY_NO_DATA_INPUT						0x0000000B
#define SKY_DISP_WRONG_COLOR_SPACE				0x0000000C
#define SKY_DISP2_IMAGE_SIZE_ERROR				0x0000000D
#define SKY_DISP2_RF_FILL_ERROR					0x0000000E

#define SKY_PROCESS_ERROR						0X7FFFFFFF

#define SKY_PROCESS_SUCCESS						0x80000000


#endif

