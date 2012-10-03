#ifndef DST_HDR_H
#define DST_HDR_H

#include "MediaParser.h"
int mp_get_dts_header(unsigned char* buf, int buflen, int* chans, int* srate, int* bitrate);

#endif // DST_HDR_H
