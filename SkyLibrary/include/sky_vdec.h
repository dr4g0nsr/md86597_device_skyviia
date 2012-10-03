/* linux/drivers/video/sky_vdec.h
 *
 * Copyright (C) 2010 Skyviia, Inc.
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

#ifndef __SKY_VDEC_H__
#define __SKY_VDEC_H__

#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */
#include <linux/soundcard.h>

//#define SKYVDEC_DEBUG

//#define USE_DPB2_256M


#if 1	/*johnnyke 20100818 for mplayer salloc reserve buffer 1MB, to avoid overlap*/

#if 0
#define DPB_MEM_ADDRESS ((uint32_t)((virt_to_phys((void *)reserved_video_start_addr)))+ (1<<20))
#define DPB_MEM_VIRT_ADDRESS ((void *)reserved_video_start_addr+ (1<<20))
#define DPB_MEM_SIZE ((uint32_t)((CONFIG_VIDEO_SIZE-1) << 20))
#else
#define DPB_MEM_ADDRESS ((uint32_t)((virt_to_phys((void *)reserved_video_start_addr))) )
#define DPB_MEM_VIRT_ADDRESS ((void *)reserved_video_start_addr)
#define DPB_MEM_SIZE ((uint32_t)((CONFIG_VIDEO_SIZE) << 20))

#define DPB2_MEM_ADDRESS ((uint32_t)((virt_to_phys((void *)reserved_video2_start_addr))) )
#define DPB2_MEM_VIRT_ADDRESS ((void *)reserved_video2_start_addr)
#define DPB2_MEM_SIZE ((uint32_t)((CONFIG_VIDEO2_SIZE) << 20))


#endif

#if 0
#define DISPLAY_MEM_ADDRESS ((uint32_t)((virt_to_phys((void *)reserved_video1_start_addr)))+ (1<<20))
#define DISPLAY_MEM_VIRT_ADDRESS ((void *)reserved_video1_start_addr+ (1<<20))
#define DISPLAY_MEM_SIZE ((uint32_t)((CONFIG_VIDEO1_SIZE-1) << 20))
#else
#define DISPLAY_MEM_ADDRESS ((uint32_t)((virt_to_phys((void *)reserved_video1_start_addr))))
#define DISPLAY_MEM_VIRT_ADDRESS ((void *)reserved_video1_start_addr)
#define DISPLAY_MEM_SIZE ((uint32_t)((CONFIG_VIDEO1_SIZE) << 20))
#endif

#else
#define DPB_MEM_ADDRESS ((uint32_t)(virt_to_phys((void *)reserved_video_start_addr)))
#define DPB_MEM_SIZE ((uint32_t)(CONFIG_VIDEO_SIZE << 20))
#endif

#define FIX_DROP_FLICKING	//Fuchun 2010.10.29

/*
 * Macros to help debugging
 */
#undef PDEBUG   /* undef it, just in case */
#ifdef SKYVDEC_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "skyvdec: " fmt, ## args)
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

typedef struct _vdec_alpha_blend 
{
	unsigned int blend_enable;
	unsigned int cur_blend_enable;
	unsigned int blend_sx;
	unsigned int blend_sy;
	unsigned int blend_w;
	unsigned int blend_h;
	unsigned int blend_ba;
	
	
}vdec_alpha_blend_t;

typedef struct _vdec_dec_info 
{
/*	//old V1 struct
	unsigned short stream_id;
	unsigned short standard;
	unsigned int buf_addr; 
	unsigned int buf_size;
	unsigned int vft;
	double pts;
*/
        unsigned int standard;
        unsigned int buf_size;
        unsigned char * buf_addr;
        unsigned int    buf_busaddr;    /*johnnyke 20100803*/
        unsigned int dec_flags;   // 0 : normal , 1 : usePeekOutput , 2 : resyn

        //int speed_mult;       /*johnnyke 20100524*/
        //int resync_video;     /*johnnyke 20100609*/

        unsigned int vft;
        double pts;
        int dvdnav;
	vdec_alpha_blend_t alpha_blend;
}vdec_dec_t;


typedef struct _vdec_thumbnail_resolution 
{
	unsigned int th_width;		// thumbnail width
	unsigned int th_height;	// thumbnail height
	
}vdec_th_res_t;

typedef struct _vdec_thumbnail_info
{
	unsigned int frame_valid;	// thumbnail is available or not 2:can't play
	unsigned int ARGB_addr;	// ARGB address or offset
	unsigned int width_out;	// output width
	unsigned int height_out;	// output height
	unsigned int vdec_init; //SkyViia_Vincent01182010 check whether the mplayer has called sky_vdec_init()

}vdec_th_info_t;


typedef struct _vdec_svread_buf{
	double cur_pts;
	int qlen[4];
	int ft_ridx, ft_widx;
	int decode_done;
	audio_buf_info  abinfo;
	unsigned int timeout_current, timeout_total;
}vdec_svread_buf_t;

typedef struct _vdec_init2_info 
{
	unsigned int workaround_flag;		// workaround for video decode timeout or bitstream error
}vdec_init2_t;


typedef struct{
       unsigned int luma_disp_addr;
       unsigned int chroma_disp_addr;
	unsigned int luma_disp_addr2;
       unsigned int chroma_disp_addr2;
       unsigned int line_size;
       unsigned char  top_field_first;
       unsigned char  progressive;
       unsigned char  top_field_disp_count;
       unsigned char  bottom_field_disp_count;
       unsigned char  frame_repeat_count;
       unsigned int buf_id;
       unsigned int width;
       unsigned int height;
       unsigned short left_offset;
       unsigned short right_offset;
       unsigned short top_offset;
       unsigned short bottom_offset;
	unsigned char do_scaling; /*display need to do scaling*/  
	unsigned char do_pd32; // 3:2 pulldown
}sky_display_params_t;


#endif /* __SKY_VDEC_H__ */

