#ifndef DEMUX_TS_H
#define DEMUX_TS_H

#include "MediaParser.h"

typedef struct 
{
	unsigned int width;
	unsigned int height;
	unsigned int bitrate;
	float fps;
} TSSTREAMINFO;

int ts_check_file(FILE *fp, FileInfo *finfo);
int ts_detect_streams(FILE *fp, FileInfo *finfo);
int h264_parse_sps(TSSTREAMINFO *info, unsigned char *p);
int mp_a52_header(WAVEFORMATEX * wf, unsigned char *p, int packet_len);
int mp_lpcm_header(WAVEFORMATEX * wf, unsigned char *p, int packet_len);

#endif // DEMUX_TS_H
