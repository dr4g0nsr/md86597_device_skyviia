#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include "hw_limit.h"

#define mp_msg(msg, ...)
//#define mp_msg(msg, ...) printf(msg, ## __VA_ARGS__)

#define HX170DEC_IOC_MAGIC  'k'
#define SKY_VDEC_IOC_GET_MEM_SIZE       _IOWR(HX170DEC_IOC_MAGIC,  8, unsigned int *)
#define MMAP_SIZE       (6 << 20)       // 6MB

#define DISP_BASE_SIZE  0x10000
#define DISP_BASE_ADDR  0xc0fc0000
#define FUSE_ADDR 0x9134

#define SK8860_AAVVF	0x7
#define SK8860_ABVXF	0x6
#define SK8860_AAVRF	0x5
#define SK8860_AAVXF	0x4
#define SK8860_AXVVF	0x3
#define SK8860_ABVDF	0x2
#define SK8860_AXVDF	0x1
#define SK8860_AXVXF	0x0

#define FUSE_ANDROID "/data/tmpfsmisc/fuse"
#define FUSE_QT "/tmp/fuse"

typedef struct {
	unsigned busAddress;
	unsigned size;
	int id;
}BufParams;

/*
 * return:
 * 	0: can not get decfb info
 *	1: get decfb info and can support
 *	-1: get decfb info but can not support
 */
int get_decfb_size(unsigned int w, unsigned int h, unsigned int frame_size, int num_ref_frames)
{
	int nRet = 0;
	unsigned int dpb_id = 0;
	BufParams dpb_parm;
	unsigned int vd_dpb_size;
	int multibuf_op_mode = 0;
	int StandaloneH264JitterBufferNum = 0;
	int fd;

#if 1
	fd = open("/dev/skyvdec", O_RDONLY);
	if (fd == -1)
		goto fb_size_err;

	dpb_parm.id = dpb_id;

	if (ioctl(fd, SKY_VDEC_IOC_GET_MEM_SIZE, &dpb_parm) == -1)
	{
		fprintf(stderr,"SKY_VDEC_IOC_GET_MEM_SIZE ioctl failed\n");
		goto fb_size_err;
	}
	close(fd);

	vd_dpb_size = dpb_parm.size - MMAP_SIZE;
#else
	vd_dpb_size = 52428800;
#endif
	if (multibuf_op_mode)
	{
		if ( (frame_size*(num_ref_frames+2)) > vd_dpb_size )
		{
			mp_msg("Video memory not enough:\n");
			mp_msg("WxH = %dx%d, num_ref_frames=%d   vd_dpb_size=%d     need_dpb_size=%d\n", w, h, num_ref_frames, vd_dpb_size, frame_size*(num_ref_frames+2));
			nRet = -1;
		}
		else
			nRet = 1;
	}
	else
	{
		if ( (((w<<4) >= 1920) || ((h<<4) >= 1080)) && (num_ref_frames == 5) )
			StandaloneH264JitterBufferNum = 3;
		else
			StandaloneH264JitterBufferNum = 4;

		if ( (frame_size*((num_ref_frames+1)*2+StandaloneH264JitterBufferNum)) > vd_dpb_size )
		{
			mp_msg("Video memory not enough:\n");
			mp_msg("WxH = %dx%d, num_ref_frames=%d   vd_dpb_size=%d     need_dpb_size=%d\n", w, h, num_ref_frames, vd_dpb_size, frame_size*((num_ref_frames+1)*2+StandaloneH264JitterBufferNum));
			nRet = -1;
		}
		else
			nRet = 1;
	}

fb_size_err:
	return nRet;
}

static int init_fuse(void)
{
	FILE *fd;
	int fuse_flag = SK8860_AXVXF;
	char fuse_buf[5];

    fd = fopen(FUSE_ANDROID, "r");
    if (fd == NULL)
    {
		fd = fopen(FUSE_QT, "r");
    }
    if (fd == NULL)
    {
		mp_msg("Can't open fuse!!!\n");
		goto fuse_fail;
    }
	if (fread(fuse_buf, 5, 1, fd) == 1)
	{
		if (strncmp(fuse_buf, "00001", 5) == 0)
			fuse_flag = SK8860_AAVVF;
		else if (strncmp(fuse_buf, "10101", 5) == 0)
			fuse_flag = SK8860_ABVDF;
		else if (strncmp(fuse_buf, "00011", 5) == 0)
			fuse_flag = SK8860_AAVRF;
		else if (strncmp(fuse_buf, "00111", 5) == 0)
			fuse_flag = SK8860_AAVXF;
		else if (strncmp(fuse_buf, "11001", 5) == 0)
			fuse_flag = SK8860_AXVVF;
		else if (strncmp(fuse_buf, "10101", 5) == 0)
			fuse_flag = SK8860_ABVDF;
		else if (strncmp(fuse_buf, "11101", 5) == 0)
			fuse_flag = SK8860_AXVDF;
	} else {
		mp_msg("fuse data not enough!!!\n");
	}
	fclose(fd);

fuse_fail:
	return fuse_flag;
}

int get_fuse(unsigned char *v_flag, unsigned char *a_flag)
{
	int nRet = -1;
	int fuse_flag;

	fuse_flag = init_fuse();
	{
		nRet = 1;
		mp_msg("fuse flag: %x\n", fuse_flag);
		switch (fuse_flag)
		{
			case SK8860_AAVVF: 
				*a_flag = FUSE_AUDIO_AA;
				*v_flag = FUSE_VIDEO_VV;
				mp_msg("SK8860_AAVVF\n");
				break;
			case SK8860_ABVXF: 
				*a_flag = FUSE_AUDIO_AB;
				*v_flag = FUSE_VIDEO_VX;
				mp_msg("SK8860_ABVXF\n");
				break;
			case SK8860_AAVRF: 
				*a_flag = FUSE_AUDIO_AA;
				*v_flag = FUSE_VIDEO_VR;
				mp_msg("SK8860_AAVRF\n");
				break;
			case SK8860_AAVXF: 
				*a_flag = FUSE_AUDIO_AA;
				*v_flag = FUSE_VIDEO_VX;
				mp_msg("SK8860_AAVXF\n");
				break;
			case SK8860_AXVVF: 
				*a_flag = FUSE_AUDIO_AX;
				*v_flag = FUSE_VIDEO_VV;
				mp_msg("SK8860_AXVVF\n");
				break;
			case SK8860_ABVDF: 
				*a_flag = FUSE_AUDIO_AB;
				*v_flag = FUSE_VIDEO_VD;
				mp_msg("SK8860_ABVDF\n");
				break;
			case SK8860_AXVDF: 
				*a_flag = FUSE_AUDIO_AX;
				*v_flag = FUSE_VIDEO_VD;
				mp_msg("SK8860_AXVDF\n");
				break;
			default:
				*a_flag = 0;
				*v_flag = 0;
				mp_msg("SK8860\n");
				break;
		}
	}
	// support all audio type
	*a_flag = FUSE_AUDIO_AX;

	return nRet;
}

