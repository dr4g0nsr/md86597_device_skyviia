#ifndef HW_LIMIT_H
#define HW_LIMIT_H

#define FUSE_AUDIO_AX	0x1
#define FUSE_AUDIO_AB	0x2
#define FUSE_AUDIO_AD	0x4
#define FUSE_AUDIO_AA	0x8

#define FUSE_VIDEO_VX	0x1
#define FUSE_VIDEO_VD	0x2
#define FUSE_VIDEO_VR	0x4
#define FUSE_VIDEO_VV	0x8

int get_decfb_size(unsigned int w, unsigned int h, unsigned int frame_size, int num_ref_frames);
int get_fuse(unsigned char *v_flag, unsigned char *a_flag);

#endif // HW_LIMIT_H
