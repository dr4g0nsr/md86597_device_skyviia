#include <stdio.h>
#include "dts_hdr.h"
#include "read_data.h"
#include "common.h"

const static int dts_channe_table[] = {1, 2, 2, 2, 2,  3, 3, 4, 4, 5,  6, 6, 6, 7, 8,  8};
const static int dts_sample_table[] = {0, 8000, 16000, 32000, 0,
	0, 11025, 22050, 44100, 0,  0, 12000, 24000, 48000, 0,  0};
const static int dts_rate_table[] = {32, 56, 64, 96, 112,  128, 192, 224, 256, 320,
	384, 448, 512, 576, 640,  768, 960, 1024, 1152, 1280,
	1344, 1408, 1411, 1472, 1536,  1920, 2048, 3072, 3804};

static int get_dts_channel(int amode)
{
	int nRet = 0;

	if ((amode >= 0) && (amode <= 15))
	{
		nRet = dts_channe_table[amode];
	}
	
	return nRet;
}

static int get_dts_samblerate(int sfreq)
{
	int nRet = 0;

	if ((sfreq>= 0) && (sfreq<= 15))
	{
		nRet = dts_sample_table[sfreq];
	}
	
	return nRet;
}

static int get_dts_bitrate(int rate)
{
	int nRet = 0;

	if ((rate >= 0) && (rate <= 28))
	{
		nRet = dts_rate_table[rate] * 1000;
	}
	
	return nRet;
}
/*
 * buf need 11 bytes
 */
int mp_get_dts_header(unsigned char* buf, int buflen, int* chans, int* srate, int* bitrate)
{
	int nRet = -1;
	int ii;
	//const static unsigned char dts_hdr[] = {0x7f, 0xfe, 0x80, 0x01};
	int amode, sfreq, rate, ext_audio_id, ext_audio, lff;

	if ( (buf == NULL) || (chans == NULL) || (srate == NULL) || (bitrate == NULL) ||
			(buflen < 11) )
		goto dts_header_faile;

	for (ii = 0; ii < buflen - 11; ii++)
	{
		if ( (buf[ii] == 0x7f) && (buf[ii+1] == 0xfe) && (buf[ii+2] == 0x80) && (buf[ii+3] == 0x1) )
			break;
	}
	if (ii == buflen - 11)
		goto dts_header_faile;

	amode = ((buf[7] & 0x0f) << 2) | (buf[8] >> 6);
	sfreq = (buf[8] & 0x3c) >> 2;
	rate = (buf[8] & 0x03 << 3) | (buf[9] >> 5);
	ext_audio_id = buf[10] >> 5;
	ext_audio = (buf[10] >> 4) & 0x1;
	lff = (buf[10] >> 1) & 0x3;
	//printf("amode: %d, sfreq: %d, srate: %d, ext_audio_id %d, ext_audio %d, lff %d\n",
	//		amode, sfreq, rate, ext_audio_id, ext_audio, lff);

	*chans = get_dts_channel(amode);	
	if ( (ext_audio == 1) && ((ext_audio_id == 0) || (ext_audio_id == 3)) )
			(*chans)++;
	if ((lff == 1) || (lff == 2))
			(*chans)++;
	*srate = get_dts_samblerate(sfreq);
	*bitrate = get_dts_bitrate(rate);

	nRet = 1;

dts_header_faile:
	return nRet;
}

