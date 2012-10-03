#ifndef MPEG_HDR_H
#define MPEG_HDR_H

#include "MediaParser.h"

typedef struct {
    // video info:
    int mpeg1; // 0=mpeg2  1=mpeg1
    int display_picture_width;
    int display_picture_height;
    int aspect_ratio_information;
    int frame_rate_code;
    float fps;
    int frame_rate_extension_n;
    int frame_rate_extension_d;
    int bitrate; // 0x3FFFF==VBR
    // timing:
    int picture_structure;
    int progressive_sequence;
    int repeat_first_field;
    int progressive_frame;
    int top_field_first;
    int display_time; // secs*100
    //the following are for mpeg4
    unsigned int timeinc_resolution, timeinc_bits, timeinc_unit;
    int picture_type;
} mp_mpeg_header_t;

int mp_unescape03(unsigned char *buf, int len);
int mp_vc1_decode_sequence_header(mp_mpeg_header_t * picture, unsigned char * tmp_buf, int len);
int mp_get_mp4_header(unsigned char * data, unsigned int length, mp_mpeg_header_t * picture);
int mp_get_mp2_header(unsigned char * data, unsigned int length, mp_mpeg_header_t * picture);
int check_avc1_sps_bank0(unsigned char *in_buf, int len);
unsigned int read_golomb(unsigned char *buffer, unsigned int *init);
int read_golomb_s(unsigned char *buffer, unsigned int *init);
int check_mp4_header_vol(unsigned char * buf, int buf_size);

#endif // MPEG_HDR_H
