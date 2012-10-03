#include "read_data.h"
#include "common.h"
#include "demux_ac3.h"
#include "demux_ts.h"
#include "bswap.h"

int ac3_check_file(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	int buf_size = 256;
	int bsid;
	unsigned char buf[256];
	int ac3_order = 0;

	if (fread(buf, buf_size, 1, fp) == 1)
	{
		if(buf[0] == 0x0B && buf[1] == 0x77)
		{
			bsid = buf[5] >> 3;
			if (bsid > 16)
			{
				// not support
			}
			else if (bsid <= 10)
			{
				// ac3
				nRet = 1;
			}
			else
			{
				// eac3 not support
			}
		}
		else if(buf[1] == 0x0B && buf[0] == 0x77)
		{
			bsid = buf[4] >> 3;
			if (bsid > 16)
			{
				// not support
			}
			else if (bsid <= 10)
			{
				// ac3
				nRet = 1;
				ac3_order = 1;
			}
			else
			{
				// eac3 not support
			}
		}
	}

	if (nRet == 1)
	{
		finfo->priv = (void *)malloc(sizeof(int));
		*(int *)finfo->priv = ac3_order;
	}

	return nRet;
}

int AC3_Parser(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	int buf_size = 256;
	unsigned char buf[256];
	int ac3_order = 0;
	unsigned int * int_bswap;
	int ii;

	if (fread(buf, buf_size, 1, fp) == 1)
	{
		if (finfo->priv)
			ac3_order = *(int *)finfo->priv;

		if (ac3_order == 1)
		{
			int_bswap = (unsigned int *)buf;
			for (ii = 0; ii < buf_size/sizeof(int); ii++)
			{
				*int_bswap = bswap_32(*int_bswap);
				int_bswap++;
			}
		}
		nRet = mp_a52_header(&finfo->wf, buf, buf_size);
	}
	
	if (nRet)
	{
		finfo->bAudio = 1;
		finfo->AudioType = mmioFOURCC('A', 'C', '3', ' ');

		finfo->aBitrate = finfo->wf.nAvgBytesPerSec * 8;
		// assume it is CBR
		finfo->FileDuration = finfo->AudioDuration = (int)((finfo->FileSize * 8)/finfo->aBitrate);
	}

	if (finfo->priv)
	{
		free(finfo->priv);
		finfo->priv = NULL;
	}

	return nRet;
}

