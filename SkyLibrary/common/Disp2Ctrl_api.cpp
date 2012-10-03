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
//#ifndef __SKY_LINUX_DEF__
//#include "stdafx.h"
//#endif

#ifndef __SKY_LINUX_DEF__
#define __SKY_LINUX_DEF__
#endif

//#define __ANDROID_MESSAGE_PRINT__
//#define __DISP2CTRL_DEBUG_MESSAGE__
//#define __DEBUG_IMAGE_SHIFT__

#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>


#include<stdlib.h>
#include <string.h>

//For Get Log macro
#ifdef __ANDROID_MESSAGE_PRINT__
#define LOG_TAG "Display 2" 
#include <utils/Log.h>
#endif

#ifndef __USE_BIDIRECTION_LINKED_LIST__
#define __USE_BIDIRECTION_LINKED_LIST__
#endif



#include "Disp2Ctrl_api.h"


//==================================================
//
//
//	Public Definition
//
//
//==================================================

disp2_api::disp2_api()
{
	disp2_Image_curr = NULL;
	disp2_Image_Head_ptr = NULL;
	disp2_Image_prev = NULL;
	iGraphic_Device_fd = -1;

	#ifdef __USE_DISP2__
	//2009-05-26 Eric Fang-Cheng Liu
	//Open Framebuffer FD
	open_fb();
	#endif
}

/*
	2009-05-27 Eric Fang-Cheng Liu

	Description:
		disp2_api Destructor

	Input:	None
		
	Output:	None
		
*/
disp2_api::~disp2_api()
{
	Display2_Image_Data * free_check_ptr;
	Display2_Image_Data * next_free_ptr;
	
	//iGraphic_Device_fd = -1;
	
	if(this->disp2_Image_Head_ptr != NULL)
	{
	
		free_check_ptr = this->disp2_Image_Head_ptr;
		next_free_ptr = this->disp2_Image_Head_ptr->Next_ptr;

#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Before Freeing Memory from disp2_api\n");
		LOGD("disp2_Image_curr address: 0x%x\n", (uint32_t) this->disp2_Image_curr);
		LOGD("disp2_Image_Head_ptr address: 0x%x\n", (uint32_t) this->disp2_Image_Head_ptr);
#else	
		printf("Before Freeing Memory from disp2_api\n");
		printf("disp2_Image_curr address: 0x%x\n", (uint32_t) this->disp2_Image_curr);
		printf("disp2_Image_Head_ptr address: 0x%x\n", (uint32_t) this->disp2_Image_Head_ptr);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
		/*Start fee memory*/
		while(1)
		{
			free(free_check_ptr->ucImage_pixel);
			free(free_check_ptr);
	
			if(next_free_ptr == NULL)
			{
				break;
			}
			
			free_check_ptr = next_free_ptr;
			next_free_ptr = next_free_ptr->Next_ptr;
			
		}
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Finish Freeing Memory from disp2_api\n");
		LOGD("disp2_Image_curr address: 0x%x\n", (uint32_t) this->disp2_Image_curr);
		LOGD("disp2_Image_Head_ptr address: 0x%x\n", (uint32_t) this->disp2_Image_Head_ptr);
#else
		printf("Finish Freeing Memory from disp2_api\n");
		printf("disp2_Image_curr address: 0x%x\n", (uint32_t) this->disp2_Image_curr);
		printf("disp2_Image_Head_ptr address: 0x%x\n", (uint32_t) this->disp2_Image_Head_ptr);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	}

	this->close_fb();
}



//add_image_resource_to_display_2
//
//Input Parameter:
//					image_src: Image Raw Data ARGB format, ex [ARGB][ARGB][ARGB]...
//					                                           P1     P2    P3
//
//Return:
//
//Status: 					Reserved!!!
Error_Return		disp2_api::add_image_resource_to_display_2_stack(uint8_t * image_src,
																		  uint32_t src_image_width,
																		  uint32_t src_image_height,
																		  uint32_t src_image_pos_x,
																		  uint32_t src_image_pos_y,
																		  uint32_t src_image_id_num)
{
	Display2_Image_Data * temp_sorted_ptr;
	uint32_t uiImage_Size_Volume;
	
	uiImage_Size_Volume = src_image_width * src_image_height * 4;

	
#ifdef __DEBUG_IMAGE_SHIFT__
	FILE * lfp;			//Local File Pointer
	char filename[24];
#endif

	//Need to check the image size is lower than 
	if((src_image_width + src_image_pos_x) > this->display2_width ||
	(src_image_pos_y + src_image_height) > this->display2_height)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Oops, Image size too larger");
#else
		printf("Oops, Image size too larger");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_DISP2_IMAGE_SIZE_ERROR;
	}

	
	//allocate a new memory block
	disp2_Image_curr = (Display2_Image_Data *)malloc(sizeof(Display2_Image_Data));
	disp2_Image_curr->Next_ptr = NULL;
	
	//ARGB = 4 bytes
	//Image Volume = image_width * image_height * [ARGB]
	disp2_Image_curr->ucImage_pixel = (uint8_t *) malloc(uiImage_Size_Volume);
	//Copy the image content to my buffer
	memcpy(disp2_Image_curr->ucImage_pixel, image_src, uiImage_Size_Volume);



#ifdef __DEBUG_IMAGE_SHIFT__

	sprintf(filename, "/sdcard/%d.argb",src_image_id_num);
	//File Opened
	if((lfp = fopen((const char *)filename, "wb")) == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Open %s failed", filename);
#else
		printf("Open %s failed", filename);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_FILE_CANT_OPENED;
	}
	fwrite(disp2_Image_curr->ucImage_pixel, 1, uiImage_Size_Volume, lfp);
	fclose(lfp);
	
#endif
	
	
	//Get the Image ID Number
	disp2_Image_curr->ucImage_ID_num = src_image_id_num;
	//Image Width
	disp2_Image_curr->uiImage_Width = src_image_width;
	//Image Height
	disp2_Image_curr->uiImage_Height = src_image_height;
	//Image X Position
	disp2_Image_curr->uiImage_Pos_x = src_image_pos_x;
	//Image Y Position
	disp2_Image_curr->uiImage_Pos_y = src_image_pos_y;
	//2009-05-18 Eric Fang-Cheng Liu
	//Image Drawed Disable
	disp2_Image_curr->bImage_Drawed = 0x00;
	
	
#ifdef __USE_BIDIRECTION_LINKED_LIST__

	//Check Header Pointer
	if(disp2_Image_Head_ptr == NULL)
	{//Null
		//Initialize Image Header Pointer
		disp2_Image_Head_ptr = disp2_Image_curr;
		disp2_Image_Head_ptr->Previous_ptr = NULL;
	}
	else
	{
		for(temp_sorted_ptr = disp2_Image_Head_ptr; temp_sorted_ptr != NULL; )
		{
			//If ucImage_ID_num is less than the Newest input, check the next one
			//or reset the last node if the current checked Node is the last one.
			if(disp2_Image_curr->ucImage_ID_num > temp_sorted_ptr->ucImage_ID_num)
			{
				//reset the last node if the current checked Node is the last one.
				if(temp_sorted_ptr->Next_ptr == NULL)
				{
					temp_sorted_ptr->Next_ptr = disp2_Image_curr;
					disp2_Image_curr->Previous_ptr = temp_sorted_ptr;
					break;
				}
				temp_sorted_ptr = temp_sorted_ptr->Next_ptr;
			}
			//2009-05-13 Eric Fang-Cheng Liu
			//Check if equal, Replace the old one
			else if(disp2_Image_curr->ucImage_ID_num == temp_sorted_ptr->ucImage_ID_num)
			{
				//Replace the old one
				replace_Node_from_disp2_stack(temp_sorted_ptr, disp2_Image_curr);
				break;
			}
			else
			{
				//If the current Pointer (temp_D2ID_ptr_checker) is the Header
				//Then we need to replace the Header with the newest
				//one (disp2_Image_curr)
				if(temp_sorted_ptr == disp2_Image_Head_ptr)
				{
					temp_sorted_ptr->Previous_ptr = disp2_Image_curr;
					disp2_Image_curr->Next_ptr = temp_sorted_ptr;
					//Replace Header Ptr
					disp2_Image_Head_ptr = disp2_Image_curr;
					disp2_Image_Head_ptr->Previous_ptr = NULL;
				}
				else
				{
					//Replace the Node with the newest input one.
					disp2_Image_curr->Next_ptr = temp_sorted_ptr;
					temp_sorted_ptr->Previous_ptr->Next_ptr = disp2_Image_curr;
					disp2_Image_curr->Previous_ptr = temp_sorted_ptr->Previous_ptr;
					temp_sorted_ptr->Previous_ptr = disp2_Image_curr;
				}
				break;
			}
		}
	}
	
#else
	//Check Header Pointer
	if(disp2_Image_Head_ptr == NULL)
	{//Null
		//Initialize Image Header Pointer
		disp2_Image_Head_ptr = disp2_Image_curr;
		disp2_Image_Head_ptr->Previous_ptr = NULL;
	}
	else
	{
		//Link the Next Pointer of "Previous Node" to "Current Node"
		disp2_Image_prev->Next_ptr = disp2_Image_curr;
		//Link the Previous Pointer of "Current Node" to "Previous Node"
		disp2_Image_curr->Previous_ptr = disp2_Image_prev;
	}
	
	disp2_Image_prev = disp2_Image_curr;
	
#endif
	//Set temp_D2ID_ptr_checker to NULL
	temp_sorted_ptr = NULL;

	return SKY_PROCESS_SUCCESS;
};



//rm_image_from_disp2_stack
//
//Input Parameter:
//					uint32_t src_image_id_num
//
//Return:
//
//Status:					Reserved!!!
Error_Return		disp2_api::rm_image_from_disp2_stack(uint32_t target_image_id_num)
{
	Display2_Image_Data *rm_checker_ptr;
	//Check Header
	if(disp2_Image_Head_ptr == NULL)
	{
		return SKY_PROCESS_ERROR;
	}
	
	rm_checker_ptr = disp2_Image_Head_ptr;
	
	while(1)
	{
		if(rm_checker_ptr->ucImage_ID_num == target_image_id_num)
		{
			//TODO: Remove the image data
			rm_checker_ptr->Next_ptr->Previous_ptr = rm_checker_ptr->Previous_ptr;
			rm_checker_ptr->Previous_ptr->Next_ptr = rm_checker_ptr->Next_ptr;
			rm_checker_ptr->Next_ptr = NULL;
			rm_checker_ptr->Previous_ptr = NULL;
			free(rm_checker_ptr->ucImage_pixel);
			free(rm_checker_ptr);
			//Set rm_checker_ptr to NULL
			rm_checker_ptr = NULL;
			break;
		}

		if(rm_checker_ptr->Next_ptr == NULL)
		{
			//Set rm_checker_ptr to NULL
			rm_checker_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}

		rm_checker_ptr = rm_checker_ptr->Next_ptr;
	}
	
	return SKY_PROCESS_SUCCESS;
}



//replace_image_from_disp2_stack
//
//Input Parameter:
//					Display2_Image_Data * Dest_ptr (If NULL, it will search the whole linked list data)
//					image_src: Image Raw Data ARGB format, ex [ARGB][ARGB][ARGB]...
//					                                           P1     P2    P3
//
//Return:
//
//Status:					Reserved!!!
Error_Return		disp2_api::replace_image_from_disp2_stack(uint8_t * image_src,
														 uint32_t src_image_width,
														 uint32_t src_image_height,
														 uint32_t src_image_pos_x,
														 uint32_t src_image_pos_y,
														 uint32_t src_image_id_num)
{
	Display2_Image_Data * replace_check_ptr;
	uint32_t uiImage_Size_Volume;
	
	//2009-08-14 Eric Fang-Cheng Liu
	//When data is null, return error.
	if(disp2_Image_Head_ptr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGE("Data buffer is null");
#else
	//Debug purpose
	printf("Data buffer is null");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_NO_DATA_INPUT;
	}
	
	replace_check_ptr = disp2_Image_Head_ptr;
	
	while(1)
	{
		if(replace_check_ptr->ucImage_ID_num == src_image_id_num)
		{
			//TODO: Remove the image data
			free(replace_check_ptr->ucImage_pixel);
	
			uiImage_Size_Volume = src_image_width * src_image_height * 4;
				
			//ARGB = 4 bytes
			//Image Volume = image_width * image_height * [ARGB]
			replace_check_ptr->ucImage_pixel = (uint8_t *) malloc(uiImage_Size_Volume);
			//Copy the image content to my buffer
			memcpy(replace_check_ptr->ucImage_pixel, image_src, uiImage_Size_Volume);
				
			//Image Width
			replace_check_ptr->uiImage_Width = src_image_width;
			//Image Height
			replace_check_ptr->uiImage_Height = src_image_height;
			//Image X Position
			replace_check_ptr->uiImage_Pos_x = src_image_pos_x;
			//Image Y Position
			replace_check_ptr->uiImage_Pos_y = src_image_pos_y;
			//2009-05-18 Eric Fang-Cheng Liu
			//Image Drawed Disable
			replace_check_ptr->bImage_Drawed = 0x00;

			
			//Set replace_check_ptr to NULL;
			replace_check_ptr = NULL;
			break;
		}

		if(replace_check_ptr->Next_ptr == NULL)
		{
			//Set replace_check_ptr to NULL;
			replace_check_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}

		replace_check_ptr = replace_check_ptr->Next_ptr;
	}
	
	return SKY_PROCESS_SUCCESS;
}


//draw_image_from_disp2_stack
//
//Input Parameter:
//					uint32_t src_image_id_num
//
//Return:
//
//Status: 					Reserved!!!
Error_Return		disp2_api::draw_image_from_disp2_stack(uint32_t src_image_id_num)
{	
	//2009-05-26 Eric Fang-Cheng Liu
	//Force to fill the YUV offset.
	return draw_image_from_disp2_stack(src_image_id_num , 0x04);
}



//draw_image_from_disp2_stack
//
//Input Parameter:
//					uint32_t src_image_id_num
//
//Return:
//
//Status: 					Reserved!!!
Error_Return		disp2_api::draw_image_from_disp2_stack(uint32_t src_image_id_num, uint8_t   disp2_mem_offset)
{
	//
	uint32_t lmemoffset;//Local Physical Memory Offset
	Display2_Image_Data *search_node_ptr;
	uint32_t i;
	uint32_t j;
	
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGD("Draw");
#else
	//Debug purpose
	printf("Draw");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	//2009-08-14 Eric Fang-Cheng Liu
	//When data is null, return error.
	if(disp2_Image_Head_ptr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGE("Data buffer is null");
#else
	//Debug purpose
	printf("Data buffer is null");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_NO_DATA_INPUT;
	}
	
	search_node_ptr = disp2_Image_Head_ptr;
	
	while(1)
	{
		if(search_node_ptr->ucImage_ID_num == src_image_id_num)
		{
			//TODO: Fill the Display 2 Drawing Panel in Memory with the Image Source.
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("Image ID: %d\n", search_node_ptr->ucImage_ID_num);
			LOGD("Image Pos X: %d\n", search_node_ptr->uiImage_Pos_x);
			LOGD("Image Pos Y: %d\n", search_node_ptr->uiImage_Pos_y);
			LOGD("Image Width: %d\n", search_node_ptr->uiImage_Width);
			LOGD("Image Height: %d\n", search_node_ptr->uiImage_Height);
#else
			printf("Image ID: %d\n", search_node_ptr->ucImage_ID_num);
			printf("Image Pos X: %d\n", search_node_ptr->uiImage_Pos_x);
			printf("Image Pos Y: %d\n", search_node_ptr->uiImage_Pos_y);
			printf("Image Width: %d\n", search_node_ptr->uiImage_Width);
			printf("Image Height: %d\n", search_node_ptr->uiImage_Height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

			//
			//Memory Offset
			switch(disp2_mem_offset)
			{
				////////////////////////////////////////////////////////////////////////////////////////////////
				//rgb0_offset
				case 0x01:
					
					lmemoffset = sky_get_graphic_device_info.rgb0_offset;
					
					break;
				
				
				////////////////////////////////////////////////////////////////////////////////////////////////	
				//rgb1_offset
				case 0x02:
					
					lmemoffset = sky_get_graphic_device_info.rgb1_offset;
					
					break;
				
				
				////////////////////////////////////////////////////////////////////////////////////////////////
				//osd_offset
				case 0x03:
					
					lmemoffset = sky_get_graphic_device_info.osd_offset;
					
					break;
				
				
				////////////////////////////////////////////////////////////////////////////////////////////////
				//y_offset
				case 0x04:
					
					lmemoffset = sky_get_graphic_device_info.y_offset;
					
					break;
				
				
				////////////////////////////////////////////////////////////////////////////////////////////////
				default:
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
					LOGD("Wrong Type\n");
#else
					printf("Wrong Type\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
					return SKY_WRONG_OFF_TYPE;
			}

			j = 0;
			
			for(i = search_node_ptr->uiImage_Pos_y; i < (search_node_ptr->uiImage_Pos_y + search_node_ptr->uiImage_Height); i++)
			{
				memcpy((void *)((uint32_t)this->fb_mmap_addr + lmemoffset + (i * display2_width * 4 + search_node_ptr->uiImage_Pos_x * 4)),
					(void *) (search_node_ptr->ucImage_pixel + j),
					(search_node_ptr->uiImage_Width * 4));

				j = j + search_node_ptr->uiImage_Width * 4;
			}
			
			//2009-05-18 Eric Fang-Cheng Liu
			//Set Image Drawed Enable
			search_node_ptr->bImage_Drawed = 0x01;
			
			search_node_ptr = NULL;

			break;
		}
		
		if(search_node_ptr->Next_ptr == NULL)
		{
			search_node_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}
		
		search_node_ptr = search_node_ptr->Next_ptr;
	}
	
	return SKY_PROCESS_SUCCESS;
}




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
Error_Return		disp2_api::draw_image_directly(uint8_t * image_src,
															  uint32_t src_image_width,
															  uint32_t src_image_height,
															  uint32_t src_image_pos_x,
															  uint32_t src_image_pos_y)
{	
	//2009-05-26 Eric Fang-Cheng Liu
	//Force to fill the YUV offset.
	return draw_image_directly(0x04, image_src, src_image_width, src_image_height, src_image_pos_x, src_image_pos_y);
}



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
Error_Return		disp2_api::draw_image_directly(uint32_t disp2_mem_offset,
															  uint8_t * image_src,
															  uint32_t src_image_width,
															  uint32_t src_image_height,
															  uint32_t src_image_pos_x,
															  uint32_t src_image_pos_y)
{
	//
	uint32_t lmemoffset;//Local Physical Memory Offset
	uint32_t i;
	uint32_t j;
	
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGD("Draw Directly");
#else
	//Debug purpose
	printf("Draw Directly");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	//Need to check the image size is lower than 
	if((src_image_width + src_image_pos_x) > this->display2_width ||
	(src_image_pos_y + src_image_height) > this->display2_height)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Oops, Image size too larger");
#else
		printf("Oops, Image size too larger");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_DISP2_IMAGE_SIZE_ERROR;
	}
	
	//TODO: Fill the Display 2 Drawing Panel in Memory with the Image Source.
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("Image Pos X: %d\n", src_image_pos_x);
	LOGD("Image Pos Y: %d\n", src_image_pos_y);
	LOGD("Image Width: %d\n", src_image_width);
	LOGD("Image Height: %d\n", src_image_height);
#else
	printf("Image Pos X: %d\n", src_image_pos_x);
	printf("Image Pos Y: %d\n", src_image_pos_y);
	printf("Image Width: %d\n", src_image_width);
	printf("Image Height: %d\n", src_image_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	//
	//Memory Offset
	switch(disp2_mem_offset)
	{
		////////////////////////////////////////////////////////////////////////////////////////////////
		//rgb0_offset
		case 0x01:
					
			lmemoffset = sky_get_graphic_device_info.rgb0_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////	
		//rgb1_offset
		case 0x02:
					
			lmemoffset = sky_get_graphic_device_info.rgb1_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//osd_offset
		case 0x03:
					
			lmemoffset = sky_get_graphic_device_info.osd_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//y_offset
		case 0x04:
					
			lmemoffset = sky_get_graphic_device_info.y_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		default:
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("Wrong Type\n");
#else
			printf("Wrong Type\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
			return SKY_WRONG_OFF_TYPE;
	}

	j = 0;
			
	for(i = src_image_pos_y; i < (src_image_pos_y + src_image_height); i++)
	{
		memcpy((void *)((uint32_t)this->fb_mmap_addr + lmemoffset + (i * display2_width * 4 + src_image_pos_x* 4)),
			(void *) (image_src + j),
			(src_image_width* 4));

		j = j + src_image_width * 4;
	}
	
	return SKY_PROCESS_SUCCESS;
}






//switch_to_android_fb
//
//Input Parameter:
//
//Return:						Error_Return
//
Error_Return		disp2_api::close_fb()
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	Clean_whole_disp2_drawing_board(0x04);

	if(this->fb_mmap_addr != NULL && (unsigned int)this->fb_mmap_addr < 0xFFFFFFFF &&
	(unsigned int)this->fb_mmap_addr > 0)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		//Debug purpose
		LOGD("Display-2 Mumap");
#else
		//Debug purpose
		printf("Display-2 Mumap");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

		munmap(this->fb_mmap_addr, this->sky_get_graphic_device_info.fb_size);
	}
		
	//Close Device
	if (iGraphic_Device_fd != -1) {
		close(iGraphic_Device_fd);
	}

	iGraphic_Device_fd = -1;
	
	return SKY_PROCESS_SUCCESS;
}


//switch_to_fb
//
//Input Parameter:
//
//Return:						Error_Return
Error_Return		disp2_api::open_fb()
{
	
	//////////////////////////////////////////////////
	//
	//Check the Frame buffer device is opended or not.
	//
	//(check the Frame buffer FD(File Description).
	//If you open it successfully, you will retrieve a FD number.)
	//
#ifdef __SKY_LINUX_DEF__
	if(iGraphic_Device_fd != -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("FB already opened\n");
#else
		printf("FB already opened\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_FB_ALREADY_OPEN;
	}
	//
	//////////////////////////////////////////////////
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__	
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("**********************************************************");
	LOGD("*");
	LOGD("*\tHello!! Welcome to use Display 2 Controller");
	LOGD("*\tCurrent Version: %s", DISPLAY2_CURRENT_VERSION);
	LOGD("*");
	LOGD("**********************************************************");
#else
	printf("**********************************************************\n");
	printf("*\n");
	printf("*\tHello!! Welcome to use Display 2 Controller\n");
	printf("*\tCurrent Version: %s\n", DISPLAY2_CURRENT_VERSION);
	printf("*\n");
	printf("**********************************************************");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	
	////////////////////////////////////////////////
	//
	//Get the Device Fd
	//
	if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_1, O_RDWR)) == -1)
	{
		if((iGraphic_Device_fd = open(SKYFB_DEVICE_NAME_2, O_RDWR)) == -1)
		{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("Open frame buffer device failed\n");
#else
			printf("Open frame buffer device failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
			return SKY_FRAME_BUFFER_OPEN_FAILED;
		}
	}
	//
	////////////////////////////////////////////////
	
	////////////////////////////////////////////////
	//
	//Get the Graphic Device Information
	if(ioctl(iGraphic_Device_fd, SKYFB_GET_DISPLAY_INFO, &sky_get_graphic_device_info) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#else
		printf("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_GET_FB_INFO_FAILED;
	}
	//
	////////////////////////////////////////////////
	
	//////////////////////////////////////////////////
	//2009-08-11 Eric Fang-Cheng Liu
	//Can use full-size
	//
	//2009-05-18 Eric Fang-Chen Liu
	//Set the display2 drawing panel (display2_width * display2_height)
	//How to set?
	//ex. When you want to set a panel with 720x480, you can just
	//display2_width = 720;
	//display2_height = 480;
	//display2_width = sky_get_graphic_device_info.width;
	//display2_height = sky_get_graphic_device_info.height;
	display2_width = DISPLAY_2_DEFAULT_WIDTH;
	display2_height = DISPLAY_2_DEFAULT_HEIGHT;
	//////////////////////////////////////////////////

	//Eric Fang-Cheng Liu ~ 2010-03-16
	//Set the Display-2 Color Space to ARGB,
	//This will follow Skyviia Kernel Video Driver Setting.
	this->disp2_color_fmt_sel = INPUT_FORMAT_ARGB;

	//////////////////////////////////////////////////
	//
	//Clean the display-2 panel
	Clean_whole_disp2_drawing_board(0x04);
	//
	//////////////////////////////////////////////////
	
	//////////////////////////////////////////////////
	//
	//Set up display 2 initialization state and parameters
	//
	//Set the display 2 parameters
	sky_api_display_parm.display = SKYFB_DISP2;
	sky_api_display_parm.input_format = INPUT_FORMAT_ARGB;
	sky_api_display_parm.start_x = (sky_get_graphic_device_info.width - display2_width) / 2;	//Original Point X position
	sky_api_display_parm.start_y = (sky_get_graphic_device_info.height - display2_height) / 2;	//Original Point Y position
	sky_api_display_parm.width_in = display2_width;
	sky_api_display_parm.height_in = display2_height;
	sky_api_display_parm.stride = display2_width;
	sky_api_display_parm.alpha = 0x00;//Control by image alpha //0xff;
	sky_api_display_parm.y_addr = sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset;
	sky_api_display_parm.u_addr = 0; //sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset + 320	* 480;
	sky_api_display_parm.v_addr = 0;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	//
	//////////////////////////////////////////////////
	
	//
	//2009-08-28 Eric Fang-Cheng Liu
	//
	//Set Display 2 Off Currently
	set_display_2_off();
	//
	/////////////////////////////////
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("display fb = %d\n", iGraphic_Device_fd);
	LOGD("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	LOGD("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	LOGD("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	LOGD("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	LOGD("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	LOGD("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	LOGD("display2_width: %d\n", display2_width);
	LOGD("display2_height: %d\n", display2_height);
#else
	printf("display fb = %d\n", iGraphic_Device_fd);
	printf("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	printf("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	printf("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	printf("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	printf("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	printf("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	printf("display2_width: %d\n", display2_width);
	printf("display2_height: %d\n", display2_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	if((signed long) this->fb_mmap_addr <= 0 || (unsigned int)this->fb_mmap_addr == 0xFFFFFFFF
	|| this->fb_mmap_addr == NULL)
	{

#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("MMAP Display-2 Addr.\n");
#else
		printf("MMAP Display-2 Addr.\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		//////////////////////////////////////////////////
		//
		//----------------------------
		//Prepare the Memory "Base Address", and "Address Offset"
		//
		//Base Address
		//this->fb_mmap_addr = mmap(0, sky_get_graphic_device_info.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, iGraphic_Device_fd, 0);
		this->fb_mmap_addr = mmap(0, SKYFB_FBSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, iGraphic_Device_fd, 0);
	}
	
	if((signed long) this->fb_mmap_addr <= 0 || (unsigned int)this->fb_mmap_addr == 0xFFFFFFFF
	|| this->fb_mmap_addr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Sorry, I can not get the right fb mmap!!\n");
#else
		printf("Sorry, I can not get the right fb mmap!!\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_CANT_MATCH_THE_MMAP;
	}
	//
	//------------------------------
	//
	//////////////////////////////////////////////////
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	LOGD("The gotten MMAP: %x\n", this->fb_mmap_addr);
#else
	printf("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	printf("The gotten MMAP: %x\n", this->fb_mmap_addr);
#endif
#endif // __DISP2CTRL_DEBUG_MESSAGE__
	
	return SKY_PROCESS_SUCCESS;
	
#endif
}

//reset_display2_env_par
//
//Description: Reset the display 2 environment parameters when the environment
//			has been changed.
//
//Input Parameter:
//
//Return:		
Error_Return		disp2_api::reset_display2_env_par()
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	////////////////////////////////////////////////
	//
	//Get the Graphic Device Information
	if(ioctl(iGraphic_Device_fd, SKYFB_GET_DISPLAY_INFO, &sky_get_graphic_device_info) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#else
		printf("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_GET_FB_INFO_FAILED;
	}
	//
	////////////////////////////////////////////////

	if((display2_width == sky_get_graphic_device_info.width &&
	display2_height == sky_get_graphic_device_info.height) ||
	((display2_width == DISPLAY_2_DEFAULT_WIDTH &&
	display2_height == DISPLAY_2_DEFAULT_HEIGHT) &&
	(sky_get_graphic_device_info.width > DISPLAY_2_MAX_WIDTH ||
	sky_get_graphic_device_info.height> DISPLAY_2_MAX_HEIGHT)))
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("No need to update Width (%d) or Height (%d)\n", display2_width, display2_height);
#else
		printf("No need to update Width (%d) or Height (%d)\n", display2_width, display2_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

		return SKY_PROCESS_SUCCESS;
	}
	
	//////////////////////////////////////////////////
	//2009-08-11 Eric Fang-Cheng Liu
	//Can use full-size
	//
	//2009-05-18 Eric Fang-Chen Liu
	//Set the display2 drawing panel (display2_width * display2_height)
	//How to set?
	//ex. When you want to set a panel with 720x480, you can just
	//display2_width = 720;
	//display2_height = 480;
	//Before setting the width and height, we need to check the current device
	//width and height
	if(this->sky_get_graphic_device_info.width <= DISPLAY_2_MAX_WIDTH &&
	this->sky_get_graphic_device_info.height <= DISPLAY_2_MAX_HEIGHT)
	{
		display2_width = sky_get_graphic_device_info.width;
		display2_height = sky_get_graphic_device_info.height;
	}
	else
	{
		display2_width = DISPLAY_2_DEFAULT_WIDTH;
		display2_height = DISPLAY_2_DEFAULT_HEIGHT;
	}
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("Update Width (%d) or Height (%d)\n", display2_width, display2_height);
#else
	printf("Update Width (%d) or Height (%d)\n", display2_width, display2_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	//////////////////////////////////////////////////

	//Eric Fang-Cheng Liu ~ 2010-03-16
	//Set the Display-2 Color Space to ARGB,
	//This will follow Skyviia Kernel Video Driver Setting.
	this->disp2_color_fmt_sel = INPUT_FORMAT_ARGB;

	//////////////////////////////////////////////////
	//
	//Clean the display-2 panel
	Clean_whole_disp2_drawing_board(0x04);
	//
	//////////////////////////////////////////////////

	
	//////////////////////////////////////////////////
	//
	//Set up display 2 initialization state and parameters
	//
	//Set the display 2 parameters
	sky_api_display_parm.display = SKYFB_DISP2;
	sky_api_display_parm.input_format = INPUT_FORMAT_ARGB;
	sky_api_display_parm.start_x = (sky_get_graphic_device_info.width - display2_width) / 2;	//Original Point X position
	sky_api_display_parm.start_y = (sky_get_graphic_device_info.height - display2_height) / 2;	//Original Point Y position
	sky_api_display_parm.width_in = display2_width;
	sky_api_display_parm.height_in = display2_height;
	sky_api_display_parm.stride = display2_width;
	sky_api_display_parm.alpha = 0x00;//Control by image alpha //0xff;
	sky_api_display_parm.y_addr = sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset;
	sky_api_display_parm.u_addr = 0; //sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset + 320	* 480;
	sky_api_display_parm.v_addr = 0;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	//
	//////////////////////////////////////////////////

	//
	//2009-08-28 Eric Fang-Cheng Liu
	//
	//Set Display 2 Off Currently
	set_display_2_off();
	//
	/////////////////////////////////
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("display fb = %d\n", iGraphic_Device_fd);
	LOGD("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	LOGD("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	LOGD("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	LOGD("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	LOGD("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	LOGD("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	LOGD("display2_width: %d\n", display2_width);
	LOGD("display2_height: %d\n", display2_height);
#else
	printf("display fb = %d\n", iGraphic_Device_fd);
	printf("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	printf("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	printf("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	printf("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	printf("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	printf("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	printf("display2_width: %d\n", display2_width);
	printf("display2_height: %d\n", display2_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	//////////////////////////////////////////////////
	//
	//----------------------------
	//Prepare the Memory "Base Address", and "Address Offset"
	//
	//Base Address
	//this->fb_mmap_addr = mmap(0, sky_get_graphic_device_info.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, iGraphic_Device_fd, 0);
	
	if((signed long) this->fb_mmap_addr <= 0 || (unsigned int)this->fb_mmap_addr == 0xFFFFFFFF
	|| this->fb_mmap_addr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("fb mmap error!!\n");
#else
		printf("fb mmap error!!\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_CANT_MATCH_THE_MMAP;
	}
	//
	//------------------------------
	//
	//////////////////////////////////////////////////
		
	return SKY_PROCESS_SUCCESS;
}



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
Error_Return		disp2_api::change_display2_env_resolution(uint32_t disp2_width, uint32_t disp2_height, uint32_t disp2_pos_x, uint32_t disp2_pos_y)
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	////////////////////////////////////////////////
	//
	//Get the Graphic Device Information
	if(ioctl(iGraphic_Device_fd, SKYFB_GET_DISPLAY_INFO, &sky_get_graphic_device_info) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#else
		printf("SKYFB_GET_DISPLAY_INFO ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_GET_FB_INFO_FAILED;
	}
	//
	////////////////////////////////////////////////

	if(display2_width == disp2_width &&
	display2_height == disp2_height)
	{
		return SKY_PROCESS_SUCCESS;
	}

	//Need to check the image size is lower than 
	if((((disp2_width + disp2_pos_x) > this->sky_get_graphic_device_info.width ||
	(disp2_height + disp2_pos_y) > this->sky_get_graphic_device_info.height) &&
	this->sky_get_graphic_device_info.width <= DISPLAY_2_MAX_WIDTH && this->sky_get_graphic_device_info.height <= DISPLAY_2_MAX_HEIGHT) || 
	((disp2_width + disp2_pos_x) > DISPLAY_2_MAX_WIDTH ||
	(disp2_height + disp2_pos_y) > DISPLAY_2_MAX_HEIGHT))
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Oops, you setting is too large, width (%d), height (%d), X (%d), Y (%d)", disp2_width, disp2_height, disp2_pos_x, disp2_pos_y);
#else
		printf("Oops, you setting is too large, width (%d), height (%d), X (%d), Y (%d)", disp2_width, disp2_height, disp2_pos_x, disp2_pos_y);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	
	//////////////////////////////////////////////////
	//2009-08-11 Eric Fang-Cheng Liu
	//Can use full-size
	//
	//2009-05-18 Eric Fang-Chen Liu
	//Set the display2 drawing panel (display2_width * display2_height)
	//How to set?
	//ex. When you want to set a panel with 720x480, you can just
	//display2_width = 720;
	//display2_height = 480;
	display2_width = disp2_width;
	display2_height = disp2_height;
	//////////////////////////////////////////////////

	//////////////////////////////////////////////////
	//
	//Set up display 2 initialization state and parameters
	//
	//Set the display 2 parameters
	sky_api_display_parm.display = SKYFB_DISP2;

	//Eric Fang-Cheng Liu ~ 20100316
	//Check Current Color Space and set for Kernel Display.
	if(this->disp2_color_fmt_sel == INPUT_FORMAT_RGB565)
	{
		sky_api_display_parm.input_format = INPUT_FORMAT_RGB565;
	}
	else
	{//Default ARGB
		sky_api_display_parm.input_format = INPUT_FORMAT_ARGB;
	}

	//////////////////////////////////////////////////
	//
	//Clean the display-2 panel
	Clean_whole_disp2_drawing_board(0x04);
	//
	//////////////////////////////////////////////////
	
	sky_api_display_parm.start_x = disp2_pos_x;
	sky_api_display_parm.start_y = disp2_pos_y;
	sky_api_display_parm.width_in = display2_width;
	sky_api_display_parm.height_in = display2_height;
	sky_api_display_parm.stride = display2_width;
	sky_api_display_parm.alpha = 0x00;//Control by image alpha //0xff;
	sky_api_display_parm.y_addr = sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset;
	sky_api_display_parm.u_addr = 0; //sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset + 320	* 480;
	sky_api_display_parm.v_addr = 0;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	//
	//////////////////////////////////////////////////
	 
	//
	//2009-08-28 Eric Fang-Cheng Liu
	//
	//Set Display 2 Off Currently
	set_display_2_off();
	//
	/////////////////////////////////
	
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("display fb = %d\n", iGraphic_Device_fd);
	LOGD("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	LOGD("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	LOGD("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	LOGD("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	LOGD("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	LOGD("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	LOGD("display2_width: %d\n", display2_width);
	LOGD("display2_height: %d\n", display2_height);
#else
	printf("display fb = %d\n", iGraphic_Device_fd);
	printf("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	printf("sky_api_display_parm.start_x: %d\n", sky_api_display_parm.start_x);
	printf("sky_api_display_parm.start_y: %d\n", sky_api_display_parm.start_y);
	printf("sky_api_display_parm.width_in: %d\n", sky_api_display_parm.width_in);
	printf("sky_api_display_parm.height_in: %d\n", sky_api_display_parm.height_in);
	printf("sky_api_display_parm.stride: %d\n", sky_api_display_parm.stride);
	printf("display2_width: %d\n", display2_width);
	printf("display2_height: %d\n", display2_height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	//////////////////////////////////////////////////
	//
	//----------------------------
	//Prepare the Memory "Base Address", and "Address Offset"
	//
	//Base Address
	//this->fb_mmap_addr = mmap(0, sky_get_graphic_device_info.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, iGraphic_Device_fd, 0);
	
	if((signed long) this->fb_mmap_addr <= 0 || (unsigned int)this->fb_mmap_addr == 0xFFFFFFFF
	|| this->fb_mmap_addr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("fb mmap error!!\n");
#else
		printf("fb mmap error!!\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_CANT_MATCH_THE_MMAP;
	}
	//
	//------------------------------
	//
	//////////////////////////////////////////////////
	
	
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	LOGD("The gotten MMAP: %x\n", this->fb_mmap_addr);
#else
	printf("The gotten Fb_base_addr: %x\n", sky_get_graphic_device_info.fb_base_addr);
	printf("The gotten MMAP: %x\n", this->fb_mmap_addr);
#endif
#endif // __DISP2CTRL_DEBUG_MESSAGE__
	
	return SKY_PROCESS_SUCCESS;
}



//change_display2_color_space	
//
//Description: For designer to change the Display-2 Resolution	
//	
//Input Parameter:	
//					disp2_color_fmt: Follow Skyviia Kernel Video Driver Setting.
//			
//	
//Return:				Skymedi_Error_Return	
Error_Return		disp2_api::change_display2_color_space(uint32_t disp2_color_fmt)
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	if(disp2_color_fmt > INPUT_FORMAT_MAX)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Code Monkey: Houston, We got the Wrong Color Format problem\n");
#else
		printf("Code Monkey: Houston, We got the Wrong Color Format problem\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_DISP_WRONG_COLOR_SPACE;
	}
	
	//
	//2009-08-28 Eric Fang-Cheng Liu
	//
	//Set Display 2 Off Currently
	set_display_2_off();
	//
	/////////////////////////////////
	
	//////////////////////////////////////////////////
	//
	//Clean the display-2 panel
	Clean_whole_disp2_drawing_board(0x04);
	//
	//////////////////////////////////////////////////
	
	//////////////////////////////////////////////////
	//
	//Set up display 2 initialization state and parameters
	//
	//Set the display 2 parameters
	sky_api_display_parm.display = SKYFB_DISP2;

	//Eric Fang-Cheng Liu ~ 20100316
	//Check Current Color Space and set for Kernel Display.
	sky_api_display_parm.input_format = disp2_color_fmt;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	//
	//////////////////////////////////////////////////
	
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("The gotten Color Format is: %d\n", sky_api_display_parm.input_format);
#else
	printf("The gotten Color Format is: %d\n", sky_api_display_parm.input_format);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

	//////////////////////////////////////////////////
	//
	//----------------------------
	//Prepare the Memory "Base Address", and "Address Offset"
	//
	//Base Address
	//this->fb_mmap_addr = mmap(0, sky_get_graphic_device_info.fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, iGraphic_Device_fd, 0);
	
	if((signed long) this->fb_mmap_addr <= 0 || (unsigned int)this->fb_mmap_addr == 0xFFFFFFFF
	|| this->fb_mmap_addr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("fb mmap error!!\n");
#else
		printf("fb mmap error!!\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_CANT_MATCH_THE_MMAP;
	}
	//
	//------------------------------
	//
	//////////////////////////////////////////////////
	
	return SKY_PROCESS_SUCCESS;
}





//set_display_2_on
//
//Input Parameter:
//
//Return:						Error_Return
Error_Return disp2_api::set_display_2_on()
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	///////////////////////////////////
#ifdef __DISP2CTRL_DEBUG_MESSAGE__

#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("set_display_2_on\n");
#else
	printf("set_display_2_on\n");
#endif

#endif //__DISP2CTRL_DEBUG_MESSAGE__
	//////////////////////////////////
	
	//set display 2 to display off
	sky_api_display_status.display = SKYFB_DISP2;
	sky_api_display_status.status = SKYFB_ON;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_STATUS, &sky_api_display_status) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_STATUS ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_STATUS ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		
		return SKY_SET_DISP2_ON_FAILED;
	}
	
	return SKY_PROCESS_SUCCESS;
}



//set_display_2_off
//
//Input Parameter:
//
//Return:						Error_Return
Error_Return disp2_api::set_display_2_off()
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}

	///////////////////////////////////
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
	
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("set_display_2_on\n");
#else
		printf("set_display_2_on\n");
#endif
	
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	//////////////////////////////////

	
	//set display 2 to display off
	sky_api_display_status.display = SKYFB_DISP2;
	sky_api_display_status.status = SKYFB_OFF;
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_STATUS, &sky_api_display_status) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_STATUS ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_STATUS ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		
		return SKY_SET_DISP2_OFF_FAILED;
	}
	
	return SKY_PROCESS_SUCCESS;
}


//set_image_position_on_disp2
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
Error_Return	disp2_api::set_image_position_on_disp2(uint32_t src_image_id_num,
													uint32_t src_image_x,
													uint32_t src_image_y)
{
	Display2_Image_Data *search_node_ptr;
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	//2009-08-14 Eric Fang-Cheng Liu
	//When data is null, return error.
	if(disp2_Image_Head_ptr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGE("Data buffer is null");
#else
	//Debug purpose
	printf("Data buffer is null");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_NO_DATA_INPUT;
	}
	
	search_node_ptr = disp2_Image_Head_ptr;
	
	while(1)
	{
		if(search_node_ptr->ucImage_ID_num == src_image_id_num)
		{
			//TODO: Set the target Image's Width & Height.

			search_node_ptr->uiImage_Pos_y = src_image_y;
			search_node_ptr->uiImage_Pos_x = src_image_x;
			

			search_node_ptr = NULL;

			break;
		}
		
		if(search_node_ptr->Next_ptr == NULL)
		{
			search_node_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}
		
		search_node_ptr = search_node_ptr->Next_ptr;
	}
	
	
	
	return SKY_PROCESS_SUCCESS;
}



//Reset_whole_disp2_drawing_board
//
//Input Parameter:
//					dsip2_mem_offset:		Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
//
//Return:
//					Error_Return
//Status: 					Reserved!!!
Error_Return	disp2_api::Clean_whole_disp2_drawing_board()
{
	//2009-05-26 Eric Fang-Cheng Liu
	//Force to Clean YUV offset
	return Clean_whole_disp2_drawing_board(0x04);
}


//Reset_whole_disp2_drawing_board
//
//Input Parameter:
//					dsip2_mem_offset:		Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
//
//Return:
//					Error_Return
//Status: 					Reserved!!!
Error_Return	disp2_api::Clean_whole_disp2_drawing_board(uint8_t   disp2_mem_offset)
{
	uint32_t lmemoffset;//Local Physical Memory Offset
	uint32_t i;
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	


	/*
	//
	//Memory Offset
	switch(disp2_mem_offset)
	{
		////////////////////////////////////////////////////////////////////////////////////////////////
		//rgb0_offset
		case 0x01:
					
			lmemoffset = sky_get_graphic_device_info.rgb0_offset;
				
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////	
		//rgb1_offset
		case 0x02:
					
			lmemoffset = sky_get_graphic_device_info.rgb1_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//osd_offset
		case 0x03:
					
			lmemoffset = sky_get_graphic_device_info.osd_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//y_offset
		case 0x04:
					
			lmemoffset = sky_get_graphic_device_info.y_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		default:
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("Wrong Type");
#else
			printf("Wrong Type");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
			return SKY_WRONG_OFF_TYPE;
	}
	//End of "Base Address" and "Memory Offset"
	//--------------------------

	if(this->disp2_color_fmt_sel == INPUT_FORMAT_RGB565)
	{
		memset((void *)((uint32_t) this->fb_mmap_addr + lmemoffset), 0x00, (display2_width * display2_height * 2));
	}
	else
	{//Default ARGB
		memset((void *)((uint32_t) this->fb_mmap_addr + lmemoffset), 0x00, (display2_width * display2_height * 4));
	}
	*/

	
	
	if(this->disp2_color_fmt_sel == INPUT_FORMAT_RGB565)
	{
		memset(&sky_api_rf_fill, 0, sizeof(classSKYFB_RF_FILL));
		//this->sky_api_rf_fill.dst_addr = sky_api_display_parm.y_addr;
		this->sky_api_rf_fill.dst_addr = sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset;
		this->sky_api_rf_fill.width = this->display2_width >> 1;
		this->sky_api_rf_fill.height = this->display2_height;
		this->sky_api_rf_fill.dst_stride = this->display2_width >> 1;
		this->sky_api_rf_fill.color = 0x00;
		this->sky_api_rf_fill.alpha_value_from = AV_FROM_REG;
		this->sky_api_rf_fill.alpha_value = 0x00;
		this->sky_api_rf_fill.alpha_blend_status = SKYFB_FALSE;
		this->sky_api_rf_fill.alpha_blend_from = 0;
		this->sky_api_rf_fill.alpha_blend_value = 0x00;
	
		if(ioctl(this->iGraphic_Device_fd, SKYFB_2D_RECTANGLE_FILL, &this->sky_api_rf_fill) == -1)
		{
			return SKY_DISP2_RF_FILL_ERROR;
			
		}
	}
	else
	{
		memset(&sky_api_rf_fill, 0, sizeof(classSKYFB_RF_FILL));
		//this->sky_api_rf_fill.dst_addr = sky_api_display_parm.y_addr;
		this->sky_api_rf_fill.dst_addr = sky_get_graphic_device_info.fb_base_addr + sky_get_graphic_device_info.y_offset;
		this->sky_api_rf_fill.width = this->display2_width;
		this->sky_api_rf_fill.height = this->display2_height;
		this->sky_api_rf_fill.dst_stride = this->display2_width;
		this->sky_api_rf_fill.color = 0x00;
		this->sky_api_rf_fill.alpha_value_from = AV_FROM_REG;
		this->sky_api_rf_fill.alpha_value = 0x00;
		this->sky_api_rf_fill.alpha_blend_status = SKYFB_FALSE;
		this->sky_api_rf_fill.alpha_blend_from = 0;
		this->sky_api_rf_fill.alpha_blend_value = 0x00;

		if(ioctl(this->iGraphic_Device_fd, SKYFB_2D_RECTANGLE_FILL, &this->sky_api_rf_fill) == -1)
		{
			return SKY_DISP2_RF_FILL_ERROR;
		}
	}
	

		
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("In Clean whole\n");
#else
	printf("In Clean whole\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	return SKY_PROCESS_SUCCESS;
}



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
Error_Return	disp2_api::Clean_part_disp2_drawing_board(uint32_t cln_start_x,
											uint32_t cln_start_y,
											uint32_t cln_range_width,
											uint32_t cln_range_height)
{
	//2009-05-26 Eric Fang-Cheng Liu
	//Force to Clean YUV Offset
	return Clean_part_disp2_drawing_board(0x04, cln_start_x, cln_start_y, cln_range_width, cln_range_height);
}



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
Error_Return	disp2_api::Clean_part_disp2_drawing_board(uint8_t  disp2_mem_offset,
																	uint32_t cln_start_x,
																	uint32_t cln_start_y,
																	uint32_t cln_range_width,
																	uint32_t cln_range_height)
{
	uint32_t lmemoffset;//Local Physical Memory Offset
	uint32_t i;
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}

	//Need to check the image size is lower than 
	if((cln_range_width + cln_start_x) > this->display2_width ||
	(cln_range_height + cln_start_y) > this->display2_height)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Oops, Clean range too larger");
#else
		printf("Oops, Clean range too larger");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_DISP2_IMAGE_SIZE_ERROR;
	}


	//
	//Memory Offset
	switch(disp2_mem_offset)
	{
		////////////////////////////////////////////////////////////////////////////////////////////////
		//rgb0_offset
		case 0x01:
					
			lmemoffset = sky_get_graphic_device_info.rgb0_offset;
				
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////	
		//rgb1_offset
		case 0x02:
					
			lmemoffset = sky_get_graphic_device_info.rgb1_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//osd_offset
		case 0x03:
					
			lmemoffset = sky_get_graphic_device_info.osd_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		//y_offset
		case 0x04:
					
			lmemoffset = sky_get_graphic_device_info.y_offset;
					
			break;
				
				
		////////////////////////////////////////////////////////////////////////////////////////////////
		default:
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("Wrong Type");
#else
			printf("Wrong Type");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
			return SKY_WRONG_OFF_TYPE;
	}
	//End of "Base Address" and "Memory Offset"
	//--------------------------

	if(this->disp2_color_fmt_sel == INPUT_FORMAT_RGB565)
	{
		unsigned int temp_clean_range_widht = 0;
		temp_clean_range_widht = ((cln_range_width >> 1) + 1) << 1;
		/*
			Check the last Coordinate X of the Clean Range is larger than the last Coordinate X of Display 2
		
			the last Coordinate X of the Clean Range:	(temp_clean_range_widht + cln_start_x)
			the last Coordinate X of Display-2:			this->display2_width (0 is the first )

				=== For the Display-2 panel ===
				
			it's left and up coordinate position = (0,0)
			right and bottom coordinate position = (Dsiplay-2 Widht, Display-2 Height)
		*/
		if((temp_clean_range_widht + cln_start_x) > this->display2_width)
		{//If larger than the range of Display-2, update the clean start coodinate.
			cln_start_x = this->display2_width - temp_clean_range_widht;
		}

#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Range-Clean width: %d", temp_clean_range_widht);
#else
		printf("Range-Clean width: %d", temp_clean_range_widht);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		
		memset(&sky_api_rf_fill, 0, sizeof(classSKYFB_RF_FILL));
		this->sky_api_rf_fill.dst_addr = sky_api_display_parm.y_addr + ((cln_start_x * 4) + (cln_start_y * this->display2_width * 4));
		this->sky_api_rf_fill.width = temp_clean_range_widht >> 1;
		this->sky_api_rf_fill.height = cln_range_height;
		this->sky_api_rf_fill.dst_stride = this->display2_width >> 1;
		this->sky_api_rf_fill.color = 0x00;
		this->sky_api_rf_fill.alpha_value_from = AV_FROM_REG;
		this->sky_api_rf_fill.alpha_value = 0x00;
		this->sky_api_rf_fill.alpha_blend_status = SKYFB_FALSE;
		this->sky_api_rf_fill.alpha_blend_from = 0;
		this->sky_api_rf_fill.alpha_blend_value = 0x00;
	
		if(ioctl(this->iGraphic_Device_fd, SKYFB_2D_RECTANGLE_FILL, &this->sky_api_rf_fill) == -1)
		{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("2D tool - Rectangle Fill - Failed!!.");
#else
			printf("2D tool - Rectangle Fill - Failed!!.");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

			return SKY_DISP2_RF_FILL_ERROR;
			
		}
	}
	else
	{
		memset(&sky_api_rf_fill, 0, sizeof(classSKYFB_RF_FILL));
		this->sky_api_rf_fill.dst_addr = sky_api_display_parm.y_addr + ((cln_start_x * 2) + (cln_start_y * this->display2_width * 2));
		this->sky_api_rf_fill.width = cln_range_width;
		this->sky_api_rf_fill.height = cln_range_height;
		this->sky_api_rf_fill.dst_stride = this->display2_width;
		this->sky_api_rf_fill.color = 0x00;
		this->sky_api_rf_fill.alpha_value_from = AV_FROM_REG;
		this->sky_api_rf_fill.alpha_value = 0x00;
		this->sky_api_rf_fill.alpha_blend_status = SKYFB_FALSE;
		this->sky_api_rf_fill.alpha_blend_from = 0;
		this->sky_api_rf_fill.alpha_blend_value = 0x00;

		if(ioctl(this->iGraphic_Device_fd, SKYFB_2D_RECTANGLE_FILL, &this->sky_api_rf_fill) == -1)
		{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("2D tool - Rectangle Fill - Failed!!.");
#else
			printf("2D tool - Rectangle Fill - Failed!!.");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__

			return SKY_DISP2_RF_FILL_ERROR;
		}
	}

	/*
	if(this->disp2_color_fmt_sel == INPUT_FORMAT_RGB565)
	{
		for(i = cln_start_y; i < (cln_start_y + cln_range_height); i++)
		{
			memset((void *)((uint32_t)this->fb_mmap_addr + lmemoffset + (i * display2_width * 2 + cln_start_x * 2)),
				 0x00,
				(cln_range_width * 2));
		}
	}
	else
	{//Default ARGB
		for(i = cln_start_y; i < (cln_start_y + cln_range_height); i++)
		{
			memset((void *)((uint32_t)this->fb_mmap_addr + lmemoffset + (i * display2_width * 4 + cln_start_x * 4)),
				 0x00,
				(cln_range_width * 4));
		}
	}
	*/
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("In Clean part\n");
#else
	printf("In Clean part\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	return SKY_PROCESS_SUCCESS;
}


//Clean_displayed_image_on_disp2
//
//Input Parameter:
//					uint32_t src_image_id_num
//
//Return:
//
//Status: 					Reserved!!!
Error_Return		disp2_api::Clean_part_disp2_drawing_board_by_Image_ID(uint32_t src_image_id_num)
{
	//2009-05-26 Eric Fang-Cheng Liu
	//Force to clean YUV offset
	return Clean_part_disp2_drawing_board_by_Image_ID(src_image_id_num, 0x04);
}




//Clean_displayed_image_on_disp2
//
//Input Parameter:
//					uint32_t src_image_id_num
//
//Return:
//
//Status: 					Reserved!!!
Error_Return		disp2_api::Clean_part_disp2_drawing_board_by_Image_ID(uint32_t src_image_id_num, uint8_t   disp2_mem_offset)
{
	//
	uint32_t lmemoffset;//Local Physical Memory Offset
	Display2_Image_Data *search_node_ptr;
	
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	//2009-08-14 Eric Fang-Cheng Liu
	//When data is null, return error.
	if(disp2_Image_Head_ptr == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	//Debug purpose
	LOGE("Data buffer is null");
#else
	//Debug purpose
	printf("Data buffer is null");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_NO_DATA_INPUT;
	}
	
	search_node_ptr = disp2_Image_Head_ptr;
	
	while(1)
	{
		if(search_node_ptr->ucImage_ID_num == src_image_id_num)
		{
			//TODO: Fill the Display 2 Drawing Panel in Memory with the Image Source.

			Clean_part_disp2_drawing_board(disp2_mem_offset, search_node_ptr->uiImage_Pos_x, search_node_ptr->uiImage_Pos_y,
											search_node_ptr->uiImage_Width, search_node_ptr->uiImage_Height);
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
			LOGD("In Clean part by Image ID\n");
#else
			printf("In Clean part by Image ID\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
			search_node_ptr->bImage_Drawed = 0x00;
			search_node_ptr = NULL;

			break;
		}
		
		if(search_node_ptr->Next_ptr == NULL)
		{
			search_node_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}
		
		search_node_ptr = search_node_ptr->Next_ptr;
	}
	
	return SKY_PROCESS_SUCCESS;
}








//==================================================
//
//
//	private Definition
//
//
//==================================================

//set_disp2_xy_wh_parameter
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
Error_Return	disp2_api::set_disp2_xy_wh_parameter(uint32_t src_image_x,
													uint32_t src_image_y,
													uint32_t src_image_width,
													uint32_t src_image_height)
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}


	//Set the display 2 parameters
	//Position X
	//Position Y
	//Image Width
	//Image Height
	sky_api_display_parm.start_x = src_image_x;//(sky_get_graphic_device_info.width - image_width) / 2;	//Original Point X position
	sky_api_display_parm.start_y = src_image_y;//(sky_get_graphic_device_info.height - image_height) / 2;	//Original Point Y position
	sky_api_display_parm.width_in = src_image_width;
	sky_api_display_parm.height_in = src_image_height;
	//stide needs to be set after the Width_in changing.
	sky_api_display_parm.stride = src_image_width;
	
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	
	
	return SKY_PROCESS_SUCCESS;
}





//replace_Node_from_disp2_stack
//
//Input Parameter:
//					Display2_Image_Data * Dest_ptr
//					Display2_Image_Data * Src_ptr
//
//Return:
//
//Status:					Reserved!!!
Error_Return		disp2_api::replace_Node_from_disp2_stack(Display2_Image_Data * Dest_ptr,
														 			Display2_Image_Data * Src_ptr)
{
	uint32_t uiImage_Size_Volume;

	
	if(Dest_ptr == NULL || Dest_ptr->ucImage_pixel == NULL ||
	Src_ptr == NULL || Src_ptr->ucImage_pixel == NULL)
	{
		return SKY_PROCESS_ERROR;
	}
	
	//TODO: Remove the image data
	free(Dest_ptr->ucImage_pixel);
	
	uiImage_Size_Volume = Src_ptr->uiImage_Width * Src_ptr->uiImage_Height * 4;
				
	//ARGB = 4 bytes
	//Image Volume = image_width * image_height * [ARGB]
	Dest_ptr->ucImage_pixel = (uint8_t *) malloc(uiImage_Size_Volume);
	//Copy the image content to my buffer
	memcpy(Dest_ptr->ucImage_pixel, Src_ptr->ucImage_pixel, uiImage_Size_Volume);
				
	//Image Width
	Dest_ptr->uiImage_Width = Src_ptr->uiImage_Width;
	//Image Height
	Dest_ptr->uiImage_Height = Src_ptr->uiImage_Height;
	//Image X Position
	Dest_ptr->uiImage_Pos_x = Src_ptr->uiImage_Pos_x;
	//Image Y Position
	Dest_ptr->uiImage_Pos_y = Src_ptr->uiImage_Pos_y;
	//2009-05-18 Eric Fang-Cheng Liu
	//Image Drawed Disable
	Dest_ptr->bImage_Drawed = 0x00;
	
	return SKY_PROCESS_SUCCESS;
}




//set_disp2_image_Width_Height
//
//Input Parameter:
//					uint32_t src_image_width
//					uint32_t src_image_height
//
//Return:
//					Error_Return
//Status: 					Reserved!!!
Error_Return	disp2_api::set_disp2_paramter_Width_Height(uint32_t src_image_width,
																uint32_t src_image_height)
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}


	//Set the display 2 parameters
	//Image Width
	//Image Height
	sky_api_display_parm.width_in = src_image_width;
	sky_api_display_parm.height_in = src_image_height;
	//stide needs to be set after the Width_in changing.
	sky_api_display_parm.stride = src_image_width;
	
	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}
	
	
	return SKY_PROCESS_SUCCESS;
}



////////////////////////////////////////////////////////////////////
//
//Description:			
//					Display-2 Global Alpha Value Setting
//
//Input:		
//					unsigned char	in_Global_Alpha:	0 ~ 255(0x00 ~ 0xFF)
//Outout:	
//					Error_Return: Please refer to the description, [Error Message], below.
//
////////////////////////////////////////////////////////////////////
Error_Return disp2_api::Set_disp2_Global_Alpha(unsigned char in_Global_Alpha)
{
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}

	sky_api_display_parm.display = SKYFB_DISP2;
	sky_api_display_parm.alpha = in_Global_Alpha & 0xFF;//Control by image alpha //0xff;

	if(ioctl(iGraphic_Device_fd, SKYFB_SET_DISPLAY_PARM, &sky_api_display_parm) == -1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#else
		printf("SKYFB_SET_DISPLAY_PARM ioctl failed\n");
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_SET_DISP_PARAM_FAILED;
	}


	return SKY_PROCESS_SUCCESS;
}








//!!! WARNING !!!
//
//The Following functions are only for test purpose
//
//!!! WARNING !!!

//Load_file_to_Phy_Memory
//
//Input Parameter: 
//					fn: 			File Name
//					offType:		Offset Type (0x01: rgb0_offset, 0x02: rgb1_offset, 0x03: osd_offset, 0x04: y_offset)
//
//Return:			Error_Return
Error_Return disp2_api::Load_file_to_Phy_Memory(uint8_t * fn, uint8_t offType,
													uint32_t src_image_x, uint32_t src_image_y,
													uint32_t src_image_width, uint32_t src_image_height,
													uint32_t src_image_id)
{
	uint32_t	lfz;	//Local File Size
	FILE * lfp;			//Local File Pointer
	uint32_t lmemoffset;//Local Physical Memory Offset
	uint8_t * temp_color_buffer;
	
	
	//Preven the Framebuffer from failed-opened
	if(iGraphic_Device_fd == -1)
	{
		return SKY_FRAME_BUFFER_OPEN_FAILED;
	}
	
	//File Opened
	if((lfp = fopen((const char *)fn, "rb")) == NULL)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Open %s failed", fn);
#else
		printf("Open %s failed", fn);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		return SKY_FILE_CANT_OPENED;
	}
	
	//----------------------------
	//Check the file size.
	fseek(lfp, 0, SEEK_END);
	lfz = ftell(lfp);
	rewind(lfp);
	//fseek(lfp, 0,SEEK_SET );  //back to offset
	//----------------------------
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
	LOGD("Open %s success\n", fn);
#else
	printf("Open %s success\n", fn);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
	
	temp_color_buffer = (uint8_t *)malloc(lfz);
	
	
	//Check end of file?
	
	fread((void*)(temp_color_buffer), lfz, 1, lfp);
	
	
	add_image_resource_to_display_2_stack((uint8_t *)(temp_color_buffer), src_image_width, src_image_height, src_image_x, src_image_y, src_image_id);
	//draw_image_from_disp2_stack(src_image_id, 0x04);
	
	free(temp_color_buffer);
	
	//Close file
	fclose(lfp);
	//
	return SKY_PROCESS_SUCCESS;
}

//Pint_Out_the_Disp2_Image_Stack_Info
//
//Input Parameter: 
//					
//
//Return:			Error_Return
Error_Return disp2_api::Pint_Out_the_Disp2_Image_Stack_Info()
{
	Display2_Image_Data * search_node_ptr;

	search_node_ptr = disp2_Image_Head_ptr;

	while(1)
	{
#ifdef __DISP2CTRL_DEBUG_MESSAGE__
#ifdef __ANDROID_MESSAGE_PRINT__
		LOGD("Image ID: %d\n", search_node_ptr->ucImage_ID_num);
		LOGD("Image Pos X: %d\n", search_node_ptr->uiImage_Pos_x);
		LOGD("Image Pos Y: %d\n", search_node_ptr->uiImage_Pos_y);
		LOGD("Image Width: %d\n", search_node_ptr->uiImage_Width);
		LOGD("Image Height: %d\n", search_node_ptr->uiImage_Height);
#else
		printf("Image ID: %d\n", search_node_ptr->ucImage_ID_num);
		printf("Image Pos X: %d\n", search_node_ptr->uiImage_Pos_x);
		printf("Image Pos Y: %d\n", search_node_ptr->uiImage_Pos_y);
		printf("Image Width: %d\n", search_node_ptr->uiImage_Width);
		printf("Image Height: %d\n", search_node_ptr->uiImage_Height);
#endif
#endif //__DISP2CTRL_DEBUG_MESSAGE__
		
		if(search_node_ptr->Next_ptr == NULL)
		{
			search_node_ptr = NULL;
			
			return SKY_NO_SUCH_ID;
		}
		
		search_node_ptr = search_node_ptr->Next_ptr;
	}

	return SKY_PROCESS_SUCCESS;
}







