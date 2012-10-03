#include "demux_flac.h"
#include "read_data.h"
#include "mp3_hdr.h"
#include "common.h"

#define FLAC_STREAMINFO_SIZE   34
#define FLAC_MIN_BLOCKSIZE     16
#define FLAC_MAX_BLOCKSIZE  65535

enum {
	FLAC_METADATA_TYPE_STREAMINFO = 0,
	FLAC_METADATA_TYPE_PADDING,
	FLAC_METADATA_TYPE_APPLICATION,
	FLAC_METADATA_TYPE_SEEKTABLE,
	FLAC_METADATA_TYPE_VORBIS_COMMENT,
	FLAC_METADATA_TYPE_CUESHEET,
	FLAC_METADATA_TYPE_PICTURE,
	FLAC_METADATA_TYPE_INVALID = 127
};

static inline int Get16bBE( unsigned char *p )
{
	    return (p[0] << 8)|(p[1]);
}

static inline int Get24bBE( unsigned char *p )
{
	    return (p[0] << 16)|(p[1] << 8)|(p[2]);
}

static void flac_parse_block_header(unsigned char *data, int *last, int *type, int *size)
{
	if (data == NULL)
		return;
	if(last)
		*last = data[0] >> 7;
	if(type)
		*type = data[0] & 0x7;
	if(size)
		*size = Get24bBE(&data[1]);
}

int flac_check_file(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	unsigned char hdr[4] = {0};
	int nID3Length = GetID3Tag(fp, finfo);

	fseek(fp, nID3Length, SEEK_SET);
	if (read_nbytes(hdr, 1, 4, fp) != 4)
		return 0;

	if (memcmp(hdr, "fLaC", 4) == 0)
		nRet = 1;

	return nRet;
}

int FLAC_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	int last = 0, type, size;
	unsigned char hdr[FLAC_STREAMINFO_SIZE+4];
	int min_block, max_block, min_frame, max_frame, sample_rate, channel, bit_per_sample;
	float duration = 0;
	uint64_t sample;
	BitData bf;

	fseek(fp, finfo->nID3Length + 4, SEEK_SET);
	while (last == 0)
	{
		if (read_nbytes(hdr, 1, 4, fp) != 4)
			return 0;
		flac_parse_block_header(hdr, &last, &type, &size);
		//printf("last: %d, type: %d, size: %d\n", last, type, size);

		if (size > finfo->FileSize)
			return 0;
		if (type != FLAC_METADATA_TYPE_STREAMINFO)
		{
			fseek(fp, size, SEEK_CUR);
		} else {
			if (size != FLAC_STREAMINFO_SIZE)
			{
				//printf("info size error!!!");
				break;
			}
			if (read_nbytes(hdr, 1, FLAC_STREAMINFO_SIZE, fp) != FLAC_STREAMINFO_SIZE)
				return 0;
			InitGetBits(&bf, hdr, FLAC_STREAMINFO_SIZE*8);
			min_block = GetBits(&bf, 16);
			max_block = GetBits(&bf, 16);
			min_frame = GetBits(&bf, 24);
			max_frame = GetBits(&bf, 24);
			sample_rate = GetBits(&bf, 20);
			channel = GetBits(&bf, 3) + 1;
			bit_per_sample = GetBits(&bf, 5) + 1;
			sample = GetBits(&bf, 32) << 4;
			sample |= GetBits(&bf, 4);

			if ((min_block < 16) || (min_block > FLAC_MAX_BLOCKSIZE) ||
					(max_block < 16) || (max_block > FLAC_MAX_BLOCKSIZE) ||
					(channel > 8)
					)
				break;

			if ((sample > 0) && (sample_rate > 0))
				duration = sample / sample_rate;

			/*
			printf("block: %d %d, frame: %d %d, sample_rate: %d, channel: %d, \n\t" \
					"bit_per_sample: %d, samples: %llu\n",
					min_block, max_block, min_frame, max_frame, sample_rate,
					channel, bit_per_sample, sample);
					*/

			finfo->bAudio = 1;
			finfo->AudioType = mmioFOURCC('f', 'L', 'a', 'C');
			finfo->FileDuration = finfo->AudioDuration = (int)duration;
			finfo->wf.nSamplesPerSec = sample_rate;
			finfo->wf.nChannels = channel;
			if (duration)
				finfo->aBitrate = (finfo->FileSize * 8) / duration;

			nRet = 1;
			break;
		}
	}

	return nRet;
}

