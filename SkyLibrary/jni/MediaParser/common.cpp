#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "hw_limit.h"

uint32_t audio_not_support[] = 
{
	mmioFOURCC('Q', 'c', 'l', 'p'),
	AUDIO_EAC3,
	0xffff,					// I14_HD_1080p12_unsupported_audio_01.divx
	0
};

int check_audio_type(uint32_t type, uint32_t channel, unsigned char a_flag)
{
	int ii = 0;
	int nRet = 1;

	while(audio_not_support[ii] != 0)
	{
		if (type == audio_not_support[ii])
		{
			nRet = 0;
			break;
		}
		ii++;
	}

	// check hw support
	switch (a_flag)
	{
		case FUSE_AUDIO_AX:
			break;
		case FUSE_AUDIO_AB:
			if (type == AUDIO_DTS)
				nRet = 0;
			break;
		case FUSE_AUDIO_AA:
			if (type == AUDIO_DTS)
				nRet = 0;
		case FUSE_AUDIO_AD:
			if (type == AUDIO_A52)
				nRet = 0;
			break;
		default:
			break;
	}

	// check channel number
	if ((type == AUDIO_BPCM) && (channel > 2))
		nRet = 0;
	if ((type == AUDIO_OGG) && (channel > 2))
		nRet = 0;

	return nRet;
}

void set_fourcc(char *buf, es_stream_type_t type)
{
	if (buf == NULL)
		return;
	switch (type)
	{
		case VIDEO_MPEG1   :
			memcpy(buf, "MPG1", 4);
			break;
		case VIDEO_MPEG2   :
			memcpy(buf, "MPG2", 4);
			break;
		case VIDEO_MPEG4   :
			memcpy(buf, "MPG4", 4);
			break;
		case VIDEO_H264    :
			memcpy(buf, "H264", 4);
			break;
		case VIDEO_AVC     :
			memcpy(buf, "avc1", 4);
			break;
		case VIDEO_VC1     :
			memcpy(buf, "WVC1", 4);
			break;
		case AUDIO_MP2     :
			memcpy(buf, "MP2A", 4);
			break;
		case AUDIO_A52     :
			memcpy(buf, "A52 ", 4);
			break;
		case AUDIO_DTS     :
			memcpy(buf, "DTS ", 4);
			break;
		case AUDIO_LPCM_BE :
			memcpy(buf, "LPCM", 4);
			break;
		case AUDIO_BPCM :
			memcpy(buf, "BPCM", 4);
			break;
		case AUDIO_AAC     :
			memcpy(buf, "AAC ", 4);
			break;
		case AUDIO_TRUEHD  :
			memcpy(buf, "trhd", 4);
			break;
		default:
			buf[0] = '\0';
			break;
	}
}


