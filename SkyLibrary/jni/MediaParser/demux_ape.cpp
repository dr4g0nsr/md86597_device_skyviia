#include "demux_ape.h"
#include "read_data.h"
#include "common.h"
#include "mp3_hdr.h"

static const char ape_header [] = "MAC ";

#define mp_msg(msg, ...)
//#define mp_msg(msg, ...) printf(msg, ## __VA_ARGS__)

#define APE_MIN_VERSION 3950
#define APE_MAX_VERSION 3990

#define MAC_FORMAT_FLAG_8_BIT                 1 // is 8-bit [OBSOLETE]
#define MAC_FORMAT_FLAG_CRC                   2 // uses the new CRC32 error detection [OBSOLETE]
#define MAC_FORMAT_FLAG_HAS_PEAK_LEVEL        4 // uint32 nPeakLevel after the header [OBSOLETE]
#define MAC_FORMAT_FLAG_24_BIT                8 // is 24-bit [OBSOLETE]
#define MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS    16 // has the number of seek elements after the peak level
#define MAC_FORMAT_FLAG_CREATE_WAV_HEADER    32 // create the wave header on decompression (not stored)

#define MAC_SUBFRAME_SIZE 4608
#define APE_EXTRADATA_SIZE 6
#ifndef UINT_MAX
#define UINT_MAX 0xFFFFFFFFUL
#endif

#define APE_TAG_VERSION               2000
#define APE_TAG_FOOTER_BYTES          32
#define APE_TAG_FLAG_CONTAINS_HEADER  (1 << 31)
#define APE_TAG_FLAG_IS_HEADER        (1 << 29)

typedef struct {
    int64_t pos;
    int nblocks;
    int size;
    int skip;
    int64_t pts;
} APEFrame;

typedef struct {
    /* Derived fields */
    unsigned long junklength;
    unsigned long firstframe;
    unsigned long totalsamples;
    int currentframe;
    APEFrame *frames;

    /* Info from Descriptor Block */
    char magic[4];
    short fileversion;
    short padding1;
    unsigned long descriptorlength;
    unsigned long headerlength;
    unsigned long seektablelength;
    unsigned long wavheaderlength;
    unsigned long audiodatalength;
    unsigned long audiodatalength_high;
    unsigned long wavtaillength;
    unsigned char md5[16];

    /* Info from Header Block */
    unsigned short compressiontype;
    unsigned short formatflags;
    unsigned long blocksperframe;
    unsigned long finalframeblocks;
    unsigned long totalframes;
    unsigned short bps;
    unsigned short channels;
    unsigned long samplerate;

    /* Seektable */
    unsigned long *seektable;
   //time stamp
   float time;
   float last_pts;

} APECONTEXT;

int ape_check_file(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	unsigned char hdr[4];
	APECONTEXT *ape = NULL;

	mp_msg("ape_check_file\n");
	if (read_nbytes(hdr, 1, 4, fp) != 4)
		return 0;
	ape = (APECONTEXT *)calloc(1, sizeof(APECONTEXT));

	if ((memcmp(hdr, ape_header, 4) != 0) || (ape == NULL))
		goto check_fail;

	ape->fileversion = read_word_le(fp);
	if (ape->fileversion < APE_MIN_VERSION || ape->fileversion > APE_MAX_VERSION) {
		mp_msg("Unsupported file version - %d.%02d\n", ape->fileversion / 1000, (ape->fileversion % 1000) / 10);
		goto check_fail;
	}

	finfo->priv = (void *)ape;
	nRet = 1;
	return nRet;

check_fail:
	if (ape != NULL)
	{
		finfo->priv = NULL;
		free(ape);
	}
	return nRet;
}

static int ape_tag_read_field(FILE *fp, FileInfo *finfo)
{
	ID3_FRAME_INFO *pTag = &(finfo->ID3Tag);
	char key[1024], *value;
	uint32_t size, flags;
	int i, c;

	size = read_dword_le(fp);  /* field size */
	flags = read_dword_le(fp); /* field flags */
	for (i = 0; i < sizeof(key) - 1; i++) {
		c = read_char(fp);
		if (c < 0x20 || c > 0x7E)
			break;
		else
			key[i] = c;
	}
	key[i] = 0;
	if (c != 0) {
		mp_msg("Invalid APE tag key '%s'.\n", key);
		return -1;
	}
	if (size >= UINT_MAX)
		return -1;
	value = (char *)malloc(size+1);
	if (!value)
		return -1;
	if (read_nbytes(value, 1, size, fp) != size)
		return -1;
	value[size] = 0;

	if (strncasecmp(key, "Track", 5) == 0)
	{
		if (size <= 16)
			memcpy(pTag->Track, value, size);
	}
	else if (strncasecmp(key, "Year", 4) == 0)
	{
		if (size <= 16)
			memcpy(pTag->Year, value, size);
	}
	else if (strncasecmp(key, "Title", 5) == 0)
	{
		if (size <= LEN_XXL)
			memcpy(pTag->Title, value, size);
	}
	else if (strncasecmp(key, "Artist", 6) == 0)
	{
		if (size <= LEN_XXL)
			memcpy(pTag->Artist, value, size);
	}
	else if (strncasecmp(key, "Album", 5) == 0)
	{
		if (size <= LEN_XXL)
			memcpy(pTag->Album, value, size);
	}
	else if (strncasecmp(key, "Comment", 7) == 0)
	{
		if (size <= LEN_XXL)
			memcpy(pTag->Comment, value, size);
	}
	else if (strncasecmp(key, "Genre", 5) == 0)
	{
		if (size == 1)
			pTag->Genre = (int)(value[0]);
	}

	if (value)
		free(value);

	return 0;
}

static int ff_ape_parse_tag(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	uint32_t val, fields, tag_bytes;
	char buf[8];
	int i;

	fseek(fp, (long)(finfo->FileSize - APE_TAG_FOOTER_BYTES), SEEK_SET);
	if (read_nbytes(buf, 1, 8, fp) != 8)
		return nRet;
	if (strncmp(buf, "APETAGEX", 8)) {
		return nRet;
	}

    val = read_dword_le(fp);        /* APE tag version */
    if (val > APE_TAG_VERSION) {
        mp_msg("Unsupported tag version. (>=%d)\n", APE_TAG_VERSION);
		return nRet;
    }

    tag_bytes = read_dword_le(fp);  /* tag size */
    if (tag_bytes - APE_TAG_FOOTER_BYTES > (1024 * 1024 * 16)) {
        mp_msg("Tag size is way too big\n");
        return nRet;
    }

    fields = read_dword_le(fp);     /* number of fields */
    if (fields > 65536) {
        mp_msg("Too many tag fields (%d)\n", fields);
        return nRet;
    }

    val = read_dword_le(fp);        /* flags */
    if (val & APE_TAG_FLAG_IS_HEADER) {
        mp_msg("APE Tag is a header\n");
        return nRet;
    }

    fseek(fp, (long)(finfo->FileSize - tag_bytes), SEEK_SET);

    for (i=0; i<fields; i++)
        if (ape_tag_read_field(fp, finfo) < 0) break;

	if (i > 0)
		finfo->bTag = 1;
	nRet = 1;
	return nRet;
}

int APE_Parser(FILE	*fp, FileInfo *finfo)
{
	int nRet = 0;
	int total_blocks;
	APECONTEXT *ape = NULL;

	mp_msg("APE_Parser\n");
	ape = (APECONTEXT *)finfo->priv;

	if (ape == NULL)
		return nRet;

	fseek(fp, 6, SEEK_SET);
	if (ape->fileversion >= 3980) {
		ape->padding1             = read_word_le(fp);
		ape->descriptorlength     = read_dword_le(fp);
		ape->headerlength         = read_dword_le(fp);
		ape->seektablelength      = read_dword_le(fp);
		ape->wavheaderlength      = read_dword_le(fp);
		ape->audiodatalength      = read_dword_le(fp);
		ape->audiodatalength_high = read_dword_le(fp);
		ape->wavtaillength        = read_dword_le(fp);
		if (read_nbytes(ape->md5, 1, 16, fp) != 16)
			goto fail;

		/* Skip any unknown bytes at the end of the descriptor.
		   This is for future compatibility */
		if (ape->descriptorlength > 52)
			read_skip(fp, ape->descriptorlength - 52);

		/* Read header data */
		ape->compressiontype      = read_word_le(fp);
		ape->formatflags          = read_word_le(fp);
		ape->blocksperframe       = read_dword_le(fp);
		ape->finalframeblocks     = read_dword_le(fp);
		ape->totalframes          = read_dword_le(fp);
		ape->bps                  = read_word_le(fp);
		ape->channels             = read_word_le(fp);
		ape->samplerate           = read_dword_le(fp);
	} else {
		ape->descriptorlength = 0;
		ape->headerlength = 32;

		ape->compressiontype      = read_word_le(fp);
		ape->formatflags          = read_word_le(fp);
		ape->channels             = read_word_le(fp);
		ape->samplerate           = read_dword_le(fp);
		ape->wavheaderlength      = read_dword_le(fp);
		ape->wavtaillength        = read_dword_le(fp);
		ape->totalframes          = read_dword_le(fp);
		ape->finalframeblocks     = read_dword_le(fp);

		if (ape->formatflags & MAC_FORMAT_FLAG_HAS_PEAK_LEVEL) {
			read_skip(fp, 4); /* Skip the peak level */
			ape->headerlength += 4;
		}
		if (ape->formatflags & MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS) {
			ape->seektablelength = read_dword_le(fp);
			ape->headerlength += 4;
			ape->seektablelength *= sizeof(long);
		} else
			ape->seektablelength = ape->totalframes * sizeof(long);

		if (ape->formatflags & MAC_FORMAT_FLAG_8_BIT)
			ape->bps = 8;
		else if (ape->formatflags & MAC_FORMAT_FLAG_24_BIT)
			ape->bps = 24;
		else
			ape->bps = 16;

		if (ape->fileversion >= 3950)
			ape->blocksperframe = 73728 * 4;
		else if (ape->fileversion >= 3900 || (ape->fileversion >= 3800  && ape->compressiontype >= 4000))
			ape->blocksperframe = 73728;
		else
			ape->blocksperframe = 9216;

		/* Skip any stored wav header */
		if (!(ape->formatflags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER))
			read_skip(fp, ape->wavheaderlength);
	}

	//limit
	//if(ape->compressiontype > 2000)
	if(ape->compressiontype > 4000)
	{
		mp_msg("not supported APE format %d !\n", ape->compressiontype);
		goto fail;
	}
	if(ape->totalframes > UINT_MAX / sizeof(APEFrame)){
		mp_msg("Too many frames: %lu\n", ape->totalframes);
		goto fail;
	}

	finfo->bAudio = 1;
	finfo->AudioType = mmioFOURCC('A', 'P', 'E', ' ');


	finfo->wf.nSamplesPerSec	= ape->samplerate;
	finfo->wf.nChannels			= ape->channels;

	total_blocks = (ape->totalframes == 0) ? 0 : ((ape->totalframes - 1) * ape->blocksperframe) + ape->finalframeblocks;
	if (ape->samplerate > 0)
	{
		finfo->FileDuration = finfo->AudioDuration =
			ape->totalframes * ape->blocksperframe / ape->samplerate;
		finfo->aBitrate = (double)(ape->audiodatalength * 8) / ((double)total_blocks/ape->samplerate);
	}

	// get tag info
	ff_ape_parse_tag(fp, finfo);

	nRet = 1;

fail:
	if (ape)
	{
		free(ape);
		finfo->priv = NULL;
	}

	return nRet;
}

