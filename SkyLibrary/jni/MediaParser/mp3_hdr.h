#ifndef MP3_HDR_H
#define MP3_HDR_H

#include "MediaParser.h"

int mp3_check_file(FILE	*fp, FileInfo *finfo);
int mp_get_mp3_header(unsigned char* hbuf, int length, int* chans, int* srate, int* bitrate);
int	GetID3Tag(FILE	*fp, FileInfo *finfo);

#endif // MP3_HDR_H 
