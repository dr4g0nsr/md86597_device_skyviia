#include "demux_amr.h"
#include "read_data.h"
#include "common.h"

static const char AMR_header [] = "#!AMR\n";
static const char AMRWB_header [] = "#!AMR-WB\n";

int amr_check_file(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	unsigned char hdr[5];

	read_nbytes(hdr, 1, 5, fp);

	if (memcmp(hdr, AMR_header, 5) == 0)
		nRet = 1;

	return nRet;
}

int AMR_Parser(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	unsigned char hdr[32];
	int frame_type;
	int frame_size;
	int frame_num = 0;

	fseek(fp, 0, SEEK_SET);
	read_nbytes(hdr, 1, 9, fp);

	if (memcmp(hdr, AMR_header, 6) != 0)
	{
		static uint8_t packed_size[16] = {18, 24, 33, 37, 41, 47, 51, 59, 61, 6, 6, 0, 0, 0, 1, 1};
		if (memcmp(hdr, AMRWB_header, 9) != 0)
		{
			return nRet;
		}
		finfo->AudioType = mmioFOURCC('s', 'a', 'w', 'b');
		finfo->wf.nSamplesPerSec = 16000;
		frame_type = (hdr[9] >> 3) & 0xf;
		frame_size = packed_size[frame_type];
	}
	else
	{
		off_t pos = 6;
		static const uint8_t packed_size[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
		// get duration
		while (pos < finfo->FileSize)
		{
			fseek(fp, pos, SEEK_SET);
			read_nbytes(hdr, 1, 1, fp);
			if( file_error == 1 )
				return 0;
			frame_type = (hdr[0] >> 3) & 0xf;
			frame_size = packed_size[frame_type] + 1;
			pos += frame_size;
			frame_num ++;
		}

		finfo->AudioType = mmioFOURCC('s', 'a', 'm', 'r');
		finfo->wf.nSamplesPerSec = 8000;
		finfo->FileDuration = finfo->AudioDuration = (int)(frame_num * 0.02);
		finfo->aBitrate = (finfo->FileSize * 8 * 50)/ frame_num;
		nRet = 1;
	}
	finfo->wf.nChannels = 1;
	finfo->bAudio = 1;

	return nRet;
}


