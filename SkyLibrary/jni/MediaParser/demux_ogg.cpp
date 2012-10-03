#include <unistd.h>
#include "demux_ogg.h"
#include "read_data.h"
#include "common.h"
#include "intreadwrite.h"

#define PACKET_TYPE_HEADER  0x01
#define PACKET_TYPE_BITS    0x07

#define oggpack_read_dword(ptr)	( (int)( ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0] ) )

typedef struct stream_header_video {
    int32_t width;
    int32_t height;
} stream_header_video;

typedef struct stream_header_audio {
    int16_t channels;
    int16_t blockalign;
    int32_t avgbytespersec;
} stream_header_audio;

typedef struct __attribute__((__packed__)) stream_header {
    char streamtype[8];
    char subtype[4];

    int32_t size;               // size of the structure

    int64_t time_unit;          // in reference time
    int64_t samples_per_unit;
    int32_t default_len;        // in media time

    int32_t buffersize;
    int16_t bits_per_sample;

    int16_t padding;

    union {
        // Video specific
        stream_header_video video;
        // Audio specific
        stream_header_audio audio;
    } sh;
} stream_header;

struct video_data_t {
	int serial_no;
	BITMAP_INFO_HEADER bih;
	float fps;
};

struct audio_data_t {
	int serial_no;
	uint32_t bitrate;
	WAVEFORMATEX wf;
};

static uint64_t ogg_page_granulepos(unsigned char *page)
{
	uint64_t granulepos = page[13]&(0xff);
	granulepos = (granulepos<<8)|(page[12]&0xff);
	granulepos = (granulepos<<8)|(page[11]&0xff);
	granulepos = (granulepos<<8)|(page[10]&0xff);
	granulepos = (granulepos<<8)|(page[9]&0xff);
	granulepos = (granulepos<<8)|(page[8]&0xff);
	granulepos = (granulepos<<8)|(page[7]&0xff);
	granulepos = (granulepos<<8)|(page[6]&0xff);
	return(granulepos);
}

//static int vorbis_comment(unsigned char *hdr, FileInfo *finfo)
static int vorbis_comment(unsigned char *hdr, FileInfo *finfo, int now_pos, int max_pos)
{
	unsigned char *ptr = hdr;
	int num_comment, i;
	
	num_comment = oggpack_read_dword(ptr);	
	ptr += 4;
	now_pos += 4;
	
	if( num_comment < 0 )
		return 0;

	//printf("num_comment: %d    finfo->bVideo=%d, finfo->bAudio=%d\n", num_comment, finfo->bVideo, finfo->bAudio);

	for( i = 0 ; i < num_comment ; i++ )
	{
		int len = oggpack_read_dword(ptr);
		ptr += 4;
		now_pos += 4;
		if ( (now_pos+len) >= max_pos)
			break;
		
		if( len < 0 )	
			break;

		//printf("[%s - %d]  i=%d   len=%d    %.7s\n", __func__, __LINE__, i, len, ptr);

		if( !strncmp((char *)ptr, "TITLE", 5) )					// TITLE
		{
			memcpy(&(finfo->ID3Tag.Title), ptr + 6, len - 6);	
			finfo->bTag = 1;
		}
		else if( !strncmp((char *)ptr, "ARTIST", 6) )			// ARTIST
		{
			memcpy(&(finfo->ID3Tag.Artist), ptr + 7, len - 7);
			finfo->bTag = 1;
		}
		else if( !strncmp((char *)ptr, "ALBUM", 5) )			// ALBUM
		{
			memcpy(&(finfo->ID3Tag.Album), ptr + 6, len - 6);	
			finfo->bTag = 1;
		}
		else if( !strncmp((char *)ptr, "COMMENT", 7) )			// COMMENT
		{
			memcpy(&(finfo->ID3Tag.Comment), ptr + 8, len - 8);	
		}
		else if( !strncmp((char *)ptr, "DATE", 4) )				// DATE
		{
			memcpy(&(finfo->ID3Tag.Year), ptr + 5, len - 5);		
		}
		else if( !strncmp((char *)ptr, "TRACKNUMBER", 11) )		// TRACKNUMBER
		{
//			memcpy(&(finfo->ID3Tag.Track), ptr + 12, len - 12);		
		}
		else if( !strncmp((char *)ptr, "GENRE", 5) )			// GENRE
		{
//			memcpy(&(finfo->ID3Tag.Genre), ptr + 6, len - 6);		
		}
		else if( !strncmp((char *)ptr, "CHAPTER", 7) )			// GENRE
		{
			unsigned char *chapter_info = (unsigned char *)malloc(len-7);
			memcpy(&chapter_info[0], ptr + 7, len - 7);
			//printf("CHAPTER: %s\n", chapter_info);
			if ((!strncmp((char *)&chapter_info[2], "=", 1)) && (!strncmp((char *)&chapter_info[5], ":", 1)) && (!strncmp((char *)&chapter_info[8], ":", 1)) )
			{
				finfo->FileDuration = atoi((char *)&chapter_info[9])+(atoi((char *)&chapter_info[6])*60)+(atoi((char *)&chapter_info[3])*3600);
				//printf("[%s - %d]   finfo->FileDuration = %d\n", __func__, __LINE__, finfo->FileDuration);
			}
			
			if (chapter_info)
				free(chapter_info);
		}
		
		ptr += len;
	}	  
	
	return 1;
}

int ogg_check_file(FILE	*fp, FileInfo *finfo)
{
	unsigned char hdr[500] = {0};	// Raymond 2008/05/27
	int nCnt = 0, nRet = 0;

	read_nbytes(hdr, 1, 27, fp);

	while(!strncmp((char *)hdr, "OggS", 4)) 
	{
		int headerbytes = 0, bodybytes = 0, i, pos = 0;
		unsigned char *ptr;

		headerbytes = hdr[26];
		/* count up body length in the segment table */
		read_nbytes(hdr, 1, hdr[26], fp);
		for( i = 0 ; i < headerbytes ; i++ )
			bodybytes += hdr[i];

		fseeko(fp, (off_t)bodybytes, SEEK_CUR);
		read_nbytes(hdr, 1, 27, fp);
		nCnt++;
		if (nCnt >= 5)
		{
			nRet = 1;
			break;
		}
	}
	return nRet;
}

static int ogg_check_audio(int bodybytes, unsigned char *ptr, struct audio_data_t *a_data)
{
	if( bodybytes >= 7 && !strncmp((char *)(ptr+1),"vorbis", 6) ) 
	{
		int version;
		long bitrate_upper;
		long bitrate_nominal;
		long bitrate_lower;

		ptr += 7;

		version		= oggpack_read_dword(ptr);		ptr += 4;
		if( version != 0 )
			return 0;

		a_data->wf.nChannels = (uint16_t)(*ptr);
		ptr++;

		a_data->wf.nSamplesPerSec	= (uint32_t)oggpack_read_dword(ptr);	ptr += 4;

		bitrate_upper	= oggpack_read_dword(ptr);	ptr += 4;
		bitrate_nominal	= oggpack_read_dword(ptr);	ptr += 4;
		bitrate_lower	= oggpack_read_dword(ptr);	ptr += 4;

		a_data->bitrate = (uint32_t)bitrate_nominal;

		return 1;
	}
	else
		return 0;
}

static int ogg_get_duration(FILE *fp, FileInfo *finfo, int s_no)
{
	int max_size = 5120;
	unsigned char hdr[max_size];
	unsigned char *ptr = (unsigned char *)hdr;
	unsigned char *ptr2;
	unsigned int pos = (unsigned int)(finfo->FileSize - max_size);
	int i = 0;
	int64_t granulepos = 0;
	int serial_no;

	fseek(fp, pos, SEEK_SET);
	read_nbytes(hdr, 1, max_size, fp);

	for( i = 0 ; i < max_size - 16; i++, ptr++ )
	{
		if(!strncmp((char *)ptr, "OggS", 4)) 
		{
			pos += i;
			
			ptr2 = ptr+14;
			serial_no = oggpack_read_dword(ptr2);
			if (serial_no == s_no)
			{
				// find the final "OggS" packet then get granulepos
				granulepos = ogg_page_granulepos(ptr);
				break;
			}
		}
	}
	
	if( finfo->wf.nSamplesPerSec != 0 )
		finfo->AudioDuration = (int)( ( granulepos + (finfo->wf.nSamplesPerSec >> 1) ) / finfo->wf.nSamplesPerSec );
	
	finfo->FileDuration = finfo->AudioDuration;

	return 1;
}

int OGG_Parser(FILE	*fp, FileInfo *finfo)
{
	unsigned char hdr[1024] = {0};	// Raymond 2008/05/27
	int nCnt = 0, nRet = 0, ii;
	int n_audio = 0, n_video = 0;
	struct video_data_t **video_data = NULL;
	struct audio_data_t **audio_data = NULL;

	read_nbytes(hdr, 1, 27, fp);

	while(!strncmp((char *)hdr, "OggS", 4)) 
	{
		int headerbytes = 0, bodybytes = 0, i;
		unsigned char *ptr;
		off_t pos;
		int serial_no, page_seq_no;

		headerbytes = hdr[26];
		ptr = &hdr[14];
		serial_no = oggpack_read_dword(ptr);
		ptr = &hdr[18];
		page_seq_no = oggpack_read_dword(ptr);
		/* count up body length in the segment table */
		read_nbytes(hdr, 1, hdr[26], fp);
		for( i = 0 ; i < headerbytes ; i++ )
			bodybytes += hdr[i];

		//printf("stream: %d, seq_no: %d, bodybytes: %d\n", serial_no, page_seq_no, bodybytes);
		pos = ftello(fp) + bodybytes;

		/* check begin of stream */
		if (hdr[5] == 0x2)
		{
			if (bodybytes > 500)
			{
				printf("too large. skip this!!!");
				goto ogg_parse_err;
			}
			read_nbytes(hdr, 1, bodybytes, fp);

			if( bodybytes >= 7 && !strncmp((char *)(&hdr[1]),"vorbis", 6) ) 
			{
				struct audio_data_t *tmp_audio = (struct audio_data_t *)calloc(1, sizeof(struct audio_data_t));
				if (ogg_check_audio(bodybytes, hdr, tmp_audio) == 1)
				{
					n_audio++;
					tmp_audio->serial_no = serial_no;
					audio_data = (audio_data_t **)realloc((void *)audio_data, sizeof(struct audio_data_t *)*n_audio);
					audio_data[n_audio - 1] = tmp_audio;
				}
			} else if ( ((hdr[0] & PACKET_TYPE_BITS) == PACKET_TYPE_HEADER) &&
					(bodybytes >= (int)sizeof(stream_header) + 1) ){
				stream_header *st = (stream_header*)(hdr + 1);
				if (strncmp(st->streamtype, "video", 5) == 0) {
					struct video_data_t *tmp_video = (struct video_data_t *)calloc(1, sizeof(struct video_data_t));
					float frametime;
					tmp_video->serial_no = serial_no;
					tmp_video->bih.biCompression = mmioFOURCC(st->subtype[0], st->subtype[1], st->subtype[2], st->subtype[3]);
					frametime = AV_RL64(&st->time_unit) * 0.0000001;
					tmp_video->fps = 1.0/frametime;
					tmp_video->bih.biBitCount = AV_RL16(&st->bits_per_sample);
					tmp_video->bih.biWidth  = AV_RL32(&st->sh.video.width);
					tmp_video->bih.biHeight = AV_RL32(&st->sh.video.height);
					if (!tmp_video->bih.biBitCount)
						tmp_video->bih.biBitCount = 24;
					tmp_video->bih.biPlanes = 1;
					tmp_video->bih.biSizeImage = (tmp_video->bih.biBitCount >> 3) * tmp_video->bih.biWidth * tmp_video->bih.biHeight;
					n_video++;
					video_data = (struct video_data_t **)realloc((void *)video_data, sizeof(struct video_data_t *)*n_video);
					video_data[n_video - 1] = tmp_video;
				} else if (strncmp(st->streamtype, "audio", 5) == 0) {
				} else if (strncmp(st->streamtype, "text", 4) == 0) {
				} else {
					goto ogg_parse_err;
				}
			}
		} else if (page_seq_no == 1) {
			int read_data = bodybytes;
			if (bodybytes > 1024)
			{
				read_data = 1024;
			}
			read_nbytes(hdr, 1, read_data, fp);

			if( bodybytes >= 7 && !strncmp((char *)(&hdr[1]),"vorbis", 6) ) 
			{
				ptr = &hdr[7];
				int vendorlen  = oggpack_read_dword(ptr);
				if( vendorlen > 0 )
				{	
					//printf("vendorlen: %d\n", vendorlen);
					// skip vendor information
					//vorbis_comment(&hdr[7+4+vendorlen], finfo);
					vorbis_comment(&hdr[7+4+vendorlen], finfo, 7+4+vendorlen, 1024);
				}
			}
		} else {
			break;
		}

		if (n_video > 0)
		{
			memcpy((void *)&finfo->bih, (void *)&video_data[0]->bih, sizeof(BITMAP_INFO_HEADER));
			finfo->FPS = video_data[0]->fps;
			finfo->bVideo = 1;
			nRet = 1;
		}

		if (n_audio > 0)
		{
			memcpy((void *)&finfo->wf, (void *)&audio_data[0]->wf, sizeof(WAVEFORMATEX));
			finfo->aBitrate = audio_data[0]->bitrate;
			finfo->bAudio = 1;
			finfo->AudioType = finfo->wf.wFormatTag;
			if (finfo->AudioType == 0)
			{
				finfo->AudioType = AUDIO_OGG;
			}
			nRet = 1;
		}

		fseeko(fp, pos, SEEK_SET);
		read_nbytes(hdr, 1, 27, fp);
	}

	if (n_audio > 0)
	{
		ogg_get_duration(fp, finfo, audio_data[0]->serial_no);
	}

ogg_parse_err:
	if (n_video > 0)
	{
		for(ii = 0; ii < n_video; ii++)
		{
			free(video_data[ii]);
		}
		free(video_data);
	}
	if (n_audio > 0)
	{
		for(ii = 0; ii < n_audio; ii++)
		{
			free(audio_data[ii]);
		}
		free(audio_data);
	}
	return nRet;
}

