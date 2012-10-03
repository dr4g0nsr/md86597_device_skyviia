#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include "demux_ts.h"
#include "common.h"
#include "read_data.h"
#include "mp3_hdr.h"
#include "aac_hdr.h"
#include "dts_hdr.h"
#include "mpeg_hdr.h"
#include "hw_limit.h"

#define mp_msg(msg, ...)
//#define mp_msg(msg, ...) printf(msg, ## __VA_ARGS__)
// fix for file "Kenneth Copeland.ts"
#define PTS_MAX_NUMBER    ((double)((unsigned long long )1<<34 - 1) / 90000.0 )

#define MAX_EXTRADATA_SIZE 64*1024

#define H264_STREAM_PROGRAM_STREAM_MAP			0xbc
#define H264_STREAM_PADDING_STREAM				0xbe
#define H264_STREAM_PRIVATE_STREAM_2			0xbf
#define H264_STREAM_ECM							0xf0
#define H264_STREAM_EMM							0xf1
#define H264_STREAM_PROGRAM_STREAM_DIRECTORY	0xff
#define H264_STREAM_DSMCC_STREAM				0xf2
#define H264_STREAM_H222_1_E					0xf8

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define TS_PH_PACKET_SIZE 192
#define TS_FEC_PACKET_SIZE 204
#define TS_PACKET_SIZE 188
#define TS2_PACKET_SIZE 192
#define NB_PID_MAX 8192
//#define NUM_CONSECUTIVE_TS_PACKETS 5
#define NUM_CONSECUTIVE_TS_PACKETS 250	//Barry 2011-08-22 fix some pvr files can't playback
#define NUM_CHECK_TS_PACKETS_SIZE 32   // Carlos add for get_ts_packet_size used

#define IS_AUDIO(x) (((x) == AUDIO_MP2) || ((x) == AUDIO_A52) || ((x) == AUDIO_LPCM_BE) || ((x) == AUDIO_AAC) || ((x) == AUDIO_DTS) || ((x) == AUDIO_TRUEHD) || ((x) == AUDIO_BPCM) || ((x) == AUDIO_EAC3))
#define IS_VIDEO(x) (((x) == VIDEO_MPEG1) || ((x) == VIDEO_MPEG2) || ((x) == VIDEO_MPEG4) || ((x) == VIDEO_H264) || ((x) == VIDEO_AVC)  || ((x) == VIDEO_VC1))
#define IS_SUB(x) (((x) == SPU_DVD) || ((x) == SPU_DVB) || ((x) == SPU_TELETEXT) || ((x) == SPU_PGS))

static inline void *realloc_struct(void *ptr, size_t nmemb, size_t size) {
  if (nmemb > SIZE_MAX / size) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nmemb * size);
}

typedef struct {
        uint8_t *buffer;
        uint16_t buffer_len;
} ts_section_t;

typedef struct {
	int size;
	unsigned char *start;
	uint16_t payload_size;
	es_stream_type_t type, subtype;
	double pts, last_pts;
	int pid;
	char lang[4];
	int last_cc;                            // last cc code (-1 if first packet)
	int is_synced;
	ts_section_t section;
	uint8_t *extradata;
	void * mediainfo;
	int extradata_alloc, extradata_len;
	int scrameled;
	int hw_notsupport;
	struct {
		uint8_t au_start, au_end, last_au_end;
	} sl;
} ES_stream_t;

typedef struct {
        void *sh;
        int id;
        int type;
} sh_av_t;

typedef struct MpegTSContext {
        int packet_size;                // raw packet size, including FEC if present e.g. 188 bytes
        ES_stream_t *pids[NB_PID_MAX];
        sh_av_t streams[NB_PID_MAX];
} MpegTSContext;

/*
typedef struct {
        demux_stream_t *ds;
        demux_packet_t *pack;
        int offset, buffer_size;
#ifdef HW_TS_DEMUX
        int pfd;
#endif // end of HW_TS_DEMUX
} av_fifo_t;
*/

typedef struct {
        int32_t object_type;    //aka codec used
        int32_t stream_type;    //video, audio etc.
        uint8_t buf[MAX_EXTRADATA_SIZE];
        uint16_t buf_size;
        uint8_t szm1;
} mp4_decoder_config_t;

typedef struct {
        //flags
        uint8_t flags;
        uint8_t au_start;
        uint8_t au_end;
        uint8_t random_accesspoint;
        uint8_t random_accesspoint_only;
        uint8_t padding;
        uint8_t use_ts;
        uint8_t idle;
        uint8_t duration;

        uint32_t ts_resolution, ocr_resolution;
        uint8_t ts_len, ocr_len, au_len, instant_bitrate_len, degr_len, au_seqnum_len, packet_seqnum_len;
        uint32_t timescale;
        uint16_t au_duration, cts_duration;
        uint64_t ocr, dts, cts;
} mp4_sl_config_t;

typedef struct {
        uint16_t id;
        uint8_t flags;
        mp4_decoder_config_t decoder;
        mp4_sl_config_t sl;
} mp4_es_descr_t;

typedef struct {
        uint16_t id;
        uint8_t flags;
        mp4_es_descr_t *es;
        uint16_t es_cnt;
} mp4_od_t;

typedef struct {
	uint16_t id;
	uint16_t pmt_pid;
} pat_progs_t;

typedef struct {
        uint8_t skip;
        uint8_t table_id;
        uint8_t ssi;
        uint16_t section_length;
        uint16_t ts_id;
        uint8_t version_number;
        uint8_t curr_next;
        uint8_t section_number;
        uint8_t last_section_number;
        pat_progs_t *progs;
        uint16_t progs_cnt;
        ts_section_t section;
} pat_t;

struct pmt_es_t {
	uint16_t pid;
	uint32_t type;  //it's 8 bit long, but cast to the right type as FOURCC
	uint16_t descr_length;
	uint8_t format_descriptor[5];
	uint8_t lang[4];
	uint16_t mp4_es_id;
	uint8_t encrypt;
};

typedef struct {
        uint16_t progid;
        uint8_t skip;
        uint8_t table_id;
        uint8_t ssi;
        uint16_t section_length;
        uint8_t version_number;
        uint8_t curr_next;
        uint8_t section_number;
        uint8_t last_section_number;
        uint16_t PCR_PID;
        uint16_t prog_descr_length;
        ts_section_t section;
        uint16_t es_cnt;
        struct pmt_es_t *es;
        mp4_od_t iod, *od;
        mp4_es_descr_t *mp4es;
        int od_cnt, mp4es_cnt;
        uint8_t encrypt;
} pmt_t;

typedef struct {
        uint64_t size;
        float duration;
        double first_pts;
        double last_pts;
} TS_stream_info;

typedef struct
{
        MpegTSContext ts;
        int last_pid;
        //av_fifo_t fifo[3];      //0 for audio, 1 for video, 2 for subs
        pat_t pat;
        pmt_t *pmt;
        uint16_t pmt_cnt;
        uint32_t prog;
        uint32_t vbitrate;
        int keep_broken;
        int last_aid;
        int last_vid;
        char packet[TS_FEC_PACKET_SIZE];
        TS_stream_info vstr, astr;
} ts_priv_t;

static int ts_search_header(const unsigned char *buf, int pos, int size) {
	int found = 0;
	while(pos < size)
	{
		if (buf[pos] == 0x47){
			found = 1;
			break;
		}
		pos++;
	}
	if (found == 1)
		return pos;
	return -1;
}

static uint8_t get_ts_packet_size(const unsigned char *buf, int size)
{
        int i, try_count = 0;
		int pos = 0;

        if (size < (TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS * 2))
                return 0;

try_search:
		pos = ts_search_header(buf, pos, size);
		if (pos == -1)
			return 0;
		try_count++;

        for(i=0; i<NUM_CHECK_TS_PACKETS_SIZE; i++)
        {
				if (pos + TS_PACKET_SIZE >= size)
					return 0;
                if (buf[pos + TS_PACKET_SIZE] != 0x47)
                {
                        mp_msg("GET_PACKET_SIZE1, pos %d, char: %02X\n", pos, buf[pos + TS_PACKET_SIZE]);
                        goto try_fec;
                }
				pos += TS_PACKET_SIZE;
        }
        return TS_PACKET_SIZE;

try_fec:
        for(i=0; i<NUM_CHECK_TS_PACKETS_SIZE; i++)
        {
				if (pos + TS_FEC_PACKET_SIZE >= size)
					return 0;
                if (buf[pos + TS_FEC_PACKET_SIZE] != 0x47){
                        mp_msg("GET_PACKET_SIZE2, pos %d, char: %2x\n", pos, buf[pos + TS_FEC_PACKET_SIZE]);
                        goto try_philips;
                }
				pos += TS_FEC_PACKET_SIZE;
        }
        return TS_FEC_PACKET_SIZE;

try_philips:
        for(i=0; i<NUM_CHECK_TS_PACKETS_SIZE; i++)
        {
				if (pos + TS_PH_PACKET_SIZE >= size)
					return 0;
                if (buf[pos + TS_PH_PACKET_SIZE] != 0x47)
				{
					//if (try_count <= 5)
					if (try_count <= 200)	//need < TS_FEC_PACKET_SIZE
					{
                        mp_msg("GET_PACKET_SIZE3, pos %d, char: %2x\n", pos, buf[pos + TS_PH_PACKET_SIZE]);
						pos++;
						goto try_search;
					}
					else
						return 0;
				}
				pos += TS_PH_PACKET_SIZE;
        }
        return TS_PH_PACKET_SIZE;
}

int ts_check_file(FILE *fp, FileInfo *finfo)
{
	int nRet = 0, ii;
	const int buf_size = (TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS * 2);
	unsigned char buf[TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS * 2];
	int size;
	unsigned char *ptr;
	int pid;
	ts_priv_t *priv_data;

	mp_msg("Checking for MPEG-TS...\n");

	finfo->priv = (void *)calloc(1, sizeof(ts_priv_t));
	if (finfo->priv == NULL) {
		mp_msg("COULDN'T MALLOC DATA\n");
		goto ts_check_faile;
	}
	priv_data = (ts_priv_t *)finfo->priv;

	if (finfo->FileSize < buf_size) {
		mp_msg("COULDN'T READ ENOUGH DATA %"PRIu64", EXITING TS_CHECK\n", finfo->FileSize);
		goto ts_check_faile;
	}

	if (read_nbytes(buf, buf_size, 1, fp) <= 0) {
		mp_msg("COULDN'T READ DATA, EXITING TS_CHECK\n");
		goto ts_check_faile;
	}

	for (ii = 0; ii < buf_size; ii++) {
		if (buf[ii] == 0x47)
			break;
	}

	if (ii == buf_size) {
		mp_msg("THIS DOESN'T LOOK LIKE AN MPEG-TS FILE!\n");
		goto ts_check_faile;
	}

	if (ii != 0) {
//		mp_msg("seek data: %d\n", ii);
		fseek(fp, ii - buf_size, SEEK_CUR);
		if (read_nbytes(buf, buf_size, 1, fp) <= 0) {
			mp_msg("COULDN'T READ DATA, EXITING TS_CHECK\n");
			goto ts_check_faile;
		}
	}

	size = get_ts_packet_size(buf, buf_size);
	if (size == 0)
		goto ts_check_faile;

	priv_data->ts.packet_size = size;
	mp_msg("ts_packet_size: %d\n", size);
	for(ii = 0; ii < NUM_CONSECUTIVE_TS_PACKETS; ii++) {
		ptr = &(buf[ii * size]);
		pid = ((ptr[1] & 0x1f) << 8) | ptr[2];
		mp_msg("BUF: %02x %02x %02x %02x, PID %d, SIZE: %d \n",
				ptr[0], ptr[1], ptr[2], ptr[3], pid, size);
	}
	
	nRet = 1;
	return nRet;

ts_check_faile:
	if (finfo->priv)
		free(finfo->priv);
	finfo->priv = NULL;
	return nRet;
}

static pmt_t* pmt_of_pid(ts_priv_t *priv, int pid, mp4_decoder_config_t **mp4_dec)
{
	int32_t i, j, k;

	if(priv->pmt)
	{
		for(i = 0; i < priv->pmt_cnt; i++)
		{
			if(priv->pmt[i].es && priv->pmt[i].es_cnt)
			{
				for(j = 0; j < priv->pmt[i].es_cnt; j++)
				{
					if(priv->pmt[i].es[j].pid == pid)
					{
						//search mp4_es_id
						if(priv->pmt[i].es[j].mp4_es_id)
						{
							for(k = 0; k < priv->pmt[i].mp4es_cnt; k++)
							{
								if(priv->pmt[i].mp4es[k].id == priv->pmt[i].es[j].mp4_es_id)
								{
									*mp4_dec = &(priv->pmt[i].mp4es[k].decoder);
									break;
								}
							}
						}

						return &(priv->pmt[i]);
					}
				}
			}
		}
	}

	return NULL;
}

static int fill_extradata(mp4_decoder_config_t * mp4_dec, ES_stream_t *tss)
{
        uint8_t *tmp;

        mp_msg("MP4_dec: %p, pid: %d\n", mp4_dec, tss->pid);

        if(mp4_dec->buf_size > tss->extradata_alloc)
        {
                tmp = (uint8_t *) realloc(tss->extradata, mp4_dec->buf_size);
                if(!tmp)
                        return 0;
                tss->extradata = tmp;
                tss->extradata_alloc = mp4_dec->buf_size;
        }
        memcpy(tss->extradata, mp4_dec->buf, mp4_dec->buf_size);
        tss->extradata_len = mp4_dec->buf_size;
        mp_msg("EXTRADATA: %p, alloc=%d, len=%d\n", tss->extradata, tss->extradata_alloc, tss->extradata_len);

        return tss->extradata_len;
}

static inline int32_t progid_idx_in_pmt(ts_priv_t *priv, uint16_t progid)
{
        int x;

        if(priv->pmt == NULL)
                return -1;

        for(x = 0; x < priv->pmt_cnt; x++)
        {
                if(priv->pmt[x].progid == progid)
                        return x;
        }

        return -1;
}

static inline int32_t es_pid_in_pmt(pmt_t * pmt, uint16_t pid)
{
        uint16_t i;

        if(pmt == NULL)
                return -1;

        if(pmt->es == NULL)
                return -1;

        for(i = 0; i < pmt->es_cnt; i++)
        {
                if(pmt->es[i].pid == pid)
                        return (int32_t) i;
        }

        return -1;
}

static uint16_t get_mp4_desc_len(uint8_t *buf, int *len)
{
	//uint16_t i = 0, size = 0;
	int i = 0, j, size = 0;

	mp_msg("PARSE_MP4_DESC_LEN(%d), bytes: ", *len);
	j = FFMIN(*len, 4);
	while(i < j)
	{
		mp_msg(" %x ", buf[i]);
		size |= (buf[i] & 0x7f);
		if(!(buf[i] & 0x80))
			break;
		size <<= 7;
		i++;
	}
	mp_msg(", SIZE=%d\n", size);

	*len = i+1;
	return size;
}

static uint16_t parse_mp4_slconfig_descriptor(uint8_t *buf, int len, void *elem)
{
	int i = 0;
	mp4_es_descr_t *es;
	mp4_sl_config_t *sl;

	mp_msg("PARSE_MP4_SLCONFIG_DESCRIPTOR(%d)\n", len);
	es = (mp4_es_descr_t *) elem;
	if(!es)
	{
		mp_msg("argh! NULL elem passed, skip\n");
		return len;
	}
	sl = &(es->sl);

	sl->ts_len = sl->ocr_len = sl->au_len = sl->instant_bitrate_len = sl->degr_len = sl->au_seqnum_len = sl->packet_seqnum_len = 0;
	sl->ocr = sl->dts = sl->cts = 0;

	if(buf[0] == 0)
	{
		i++;
		sl->flags = buf[i];
		i++;
		sl->ts_resolution = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->ocr_resolution = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->ts_len = buf[i];
		i++;
		sl->ocr_len = buf[i];
		i++;
		sl->au_len = buf[i];
		i++;
		sl->instant_bitrate_len = buf[i];
		i++;
		sl->degr_len = (buf[i] >> 4) & 0x0f;
		sl->au_seqnum_len = ((buf[i] & 0x0f) << 1) | ((buf[i+1] >> 7) & 0x01);
		i++;
		sl->packet_seqnum_len = ((buf[i] >> 2) & 0x1f);
		i++;

	}
	else if(buf[0] == 1)
	{
		sl->flags = 0;
		sl->ts_resolution = 1000;
		sl->ts_len = 32;
		i++;
	}
	else if(buf[0] == 2)
	{
		sl->flags = 4;
		i++;
	}
	else
	{
		sl->flags = 0;
		i++;
	}

	sl->au_start = (sl->flags >> 7) & 0x1;
	sl->au_end = (sl->flags >> 6) & 0x1;
	sl->random_accesspoint = (sl->flags >> 5) & 0x1;
	sl->random_accesspoint_only = (sl->flags >> 4) & 0x1;
	sl->padding = (sl->flags >> 3) & 0x1;
	sl->use_ts = (sl->flags >> 2) & 0x1;
	sl->idle = (sl->flags >> 1) & 0x1;
	sl->duration = sl->flags & 0x1;

	if(sl->duration)
	{
		sl->timescale = (buf[i] << 24) | (buf[i+1] << 16) | (buf[i+2] << 8) | buf[i+3];
		i += 4;
		sl->au_duration = (buf[i] << 8) | buf[i+1];
		i += 2;
		sl->cts_duration = (buf[i] << 8) | buf[i+1];
		i += 2;
	}
	else	//no support for fixed durations atm
		sl->timescale = sl->au_duration = sl->cts_duration = 0;

	mp_msg("MP4SLCONFIG(len=0x%x), predef: %d, flags: %x, use_ts: %d, tslen: %d, timescale: %d, dts: %"PRIu64", cts: %"PRIu64"\n",
		len, buf[0], sl->flags, sl->use_ts, sl->ts_len, sl->timescale, (uint64_t) sl->dts, (uint64_t) sl->cts);

	return len;
}

static int parse_mp4_descriptors(pmt_t *pmt, uint8_t *buf, int len, void *elem);

static uint16_t parse_mp4_decoder_config_descriptor(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i = 0, j;
	mp4_es_descr_t *es;
	mp4_decoder_config_t *dec;

	mp_msg("PARSE_MP4_DECODER_CONFIG_DESCRIPTOR(%d)\n", len);
	es = (mp4_es_descr_t *) elem;
	if(!es)
	{
		mp_msg("argh! NULL elem passed, skip\n");
		return len;
	}
	dec = (mp4_decoder_config_t*) &(es->decoder);

	dec->object_type = buf[i];
	dec->stream_type =  (buf[i+1]>>2) & 0x3f;

	if(dec->object_type == 1 && dec->stream_type == 1)
	{
		 dec->object_type = MP4_OD;
		 dec->stream_type = MP4_OD;
	}
	else if(dec->stream_type == 4)
	{
		if(dec->object_type == 0x6a)
			dec->object_type = VIDEO_MPEG1;
		if(dec->object_type >= 0x60 && dec->object_type <= 0x65)
			dec->object_type = VIDEO_MPEG2;
		else if(dec->object_type == 0x20)
			dec->object_type = VIDEO_MPEG4;
		else if(dec->object_type == 0x21)
			dec->object_type = VIDEO_AVC;
		/*else if(dec->object_type == 0x22)
			fmp_msg(stderr, "TYPE 0x22\n");*/
		else dec->object_type = UNKNOWN;
	}
	else if(dec->stream_type == 5)
	{
		if(dec->object_type == 0x40)
			dec->object_type = AUDIO_AAC;
		else if(dec->object_type == 0x6b)
			dec->object_type = AUDIO_MP2;
		else if(dec->object_type >= 0x66 && dec->object_type <= 0x69)
			dec->object_type = AUDIO_MP2;
		else
			dec->object_type = UNKNOWN;
	}
	else
		dec->object_type = dec->stream_type = UNKNOWN;

	if(dec->object_type != UNKNOWN)
	{
		//update the type of the current stream
		for(j = 0; j < pmt->es_cnt; j++)
		{
			if(pmt->es[j].mp4_es_id == es->id)
			{
				pmt->es[j].type = SL_PES_STREAM;
			}
		}
	}

	if(len > 13)
		parse_mp4_descriptors(pmt, &buf[13], len-13, dec);

	mp_msg("MP4DECODER(0x%x), object_type: 0x%x, stream_type: 0x%x\n", len, dec->object_type, dec->stream_type);

	return len;
}

static uint16_t parse_mp4_decoder_specific_descriptor(uint8_t *buf, int len, void *elem)
{
	int i;
	mp4_decoder_config_t *dec;

	mp_msg("PARSE_MP4_DECODER_SPECIFIC_DESCRIPTOR(%d)\n", len);
	dec = (mp4_decoder_config_t *) elem;
	if(!dec)
	{
		mp_msg("argh! NULL elem passed, skip\n");
		return len;
	}

	mp_msg("MP4 SPECIFIC INFO BYTES: \n");
	for(i=0; i<len; i++) {
		mp_msg("%02x ", buf[i]);
	}
	mp_msg("\n");

	if(len > MAX_EXTRADATA_SIZE)
	{
		mp_msg("DEMUX_TS, EXTRADATA SUSPICIOUSLY BIG: %d, REFUSED\r\n", len);
		return len;
	}
	memcpy(dec->buf, buf, len);
	dec->buf_size = len;

	return len;
}

static uint16_t parse_mp4_es_descriptor(pmt_t *pmt, uint8_t *buf, int len)
{
	int i = 0, j = 0, k, found;
	uint8_t flag;
	mp4_es_descr_t es, *target_es = NULL, *tmp;

	mp_msg("PARSE_MP4ES: len=%d\n", len);
	memset(&es, 0, sizeof(mp4_es_descr_t));
	while(i < len)
	{
		es.id = (buf[i] << 8) | buf[i+1];
		mp_msg("MP4ES_ID: %d\n", es.id);
		i += 2;
		flag = buf[i];
		i++;
		if(flag & 0x80)
			i += 2;
		if(flag & 0x40)
			i += buf[i]+1;
		if(flag & 0x20)		//OCR, maybe we need it
			i += 2;

		j = parse_mp4_descriptors(pmt, &buf[i], len-i, &es);
		mp_msg("PARSE_MP4ES, types after parse_mp4_descriptors: 0x%x, 0x%x\n", es.decoder.object_type, es.decoder.stream_type);
		if(es.decoder.object_type != UNKNOWN && es.decoder.stream_type != UNKNOWN)
		{
			found = 0;
			//search this ES_ID if we already have it
			for(k=0; k < pmt->mp4es_cnt; k++)
			{
				if(pmt->mp4es[k].id == es.id)
				{
					target_es = &(pmt->mp4es[k]);
					found = 1;
				}
			}

			if(! found)
			{
				tmp = (mp4_es_descr_t *)realloc_struct(pmt->mp4es, pmt->mp4es_cnt+1, sizeof(mp4_es_descr_t));
				if(tmp == NULL)
				{
					pmt->mp4es = NULL;
					pmt->mp4es_cnt = 0;
					mp_msg("CAN'T REALLOC MP4_ES_DESCR\n");
					continue;
				}
				pmt->mp4es = tmp;
				target_es = &(pmt->mp4es[pmt->mp4es_cnt]);
				pmt->mp4es_cnt++;
			}
			memcpy(target_es, &es, sizeof(mp4_es_descr_t));
			mp_msg("MP4ES_CNT: %d, ID=%d\n", pmt->mp4es_cnt, target_es->id);
		}

		i += j;
	}

	return len;
}

static void parse_mp4_object_descriptor(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i, j = 0, id;

	i=0;
	id = (buf[0] << 2) | ((buf[1] & 0xc0) >> 6);
	mp_msg("PARSE_MP4_OBJECT_DESCRIPTOR: len=%d, OD_ID=%d\n", len, id);
	if(buf[1] & 0x20)
	{
		i += buf[2] + 1;	//url
		mp_msg("URL\n");
	}
	else
	{
		i = 2;

		while(i < len)
		{
			j = parse_mp4_descriptors(pmt, &(buf[i]), len-i, elem);
			mp_msg("OBJD, NOW i = %d, j=%d, LEN=%d\n", i, j, len);
			i += j;
		}
	}
}

static void parse_mp4_iod(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int i, j = 0;
	mp4_od_t *iod = &(pmt->iod);

	iod->id = (buf[0] << 2) | ((buf[1] & 0xc0) >> 6);
	mp_msg("PARSE_MP4_IOD: len=%d, IOD_ID=%d\n", len, iod->id);
	i = 2;
	if(buf[1] & 0x20)
	{
		i += buf[2] + 1;	//url
		mp_msg("URL\n");
	}
	else
	{
		i = 7;
		while(i < len)
		{
			j = parse_mp4_descriptors(pmt, &(buf[i]), len-i, elem);
			mp_msg("IOD, NOW i = %d, j=%d, LEN=%d\n", i, j, len);
			i += j;
		}
	}
}


static int parse_mp4_descriptors(pmt_t *pmt, uint8_t *buf, int len, void *elem)
{
	int tag, descr_len, i = 0, j = 0;

	mp_msg("PARSE_MP4_DESCRIPTORS, len=%d\n", len);
	if(! len)
		return len;

	while(i < len)
	{
		tag = buf[i];
		j = len - i -1;
		descr_len = get_mp4_desc_len(&(buf[i+1]), &j);
		mp_msg("TAG=%d (0x%x), DESCR_len=%d, len=%d, j=%d\n", tag, tag, descr_len, len, j);
		if(descr_len > len - j+1)
		{
			mp_msg("descriptor is too long, exit\n");
			return len;
		}
		i += j+1;

		switch(tag)
		{
			case 0x1:
				parse_mp4_object_descriptor(pmt, &(buf[i]), descr_len, elem);
				break;
			case 0x2:
				parse_mp4_iod(pmt, &(buf[i]), descr_len, elem);
				break;
			case 0x3:
				parse_mp4_es_descriptor(pmt, &(buf[i]), descr_len);
				break;
			case 0x4:
				parse_mp4_decoder_config_descriptor(pmt, &buf[i], descr_len, elem);
				break;
			case 0x05:
				parse_mp4_decoder_specific_descriptor(&buf[i], descr_len, elem);
				break;
			case 0x6:
				parse_mp4_slconfig_descriptor(&buf[i], descr_len, elem);
				break;
			default:
				mp_msg("Unsupported mp4 descriptor 0x%x\n", tag);
				break;
		}
		i += descr_len;
	}

	return len;
}

static inline int32_t pid_type_from_pmt(ts_priv_t *priv, int pid)
{
        int32_t pmt_idx, pid_idx, i, j;

        pmt_idx = progid_idx_in_pmt(priv, priv->prog);

        if(pmt_idx != -1)
        {
                pid_idx = es_pid_in_pmt(&(priv->pmt[pmt_idx]), pid);
                if(pid_idx != -1)
                        return priv->pmt[pmt_idx].es[pid_idx].type;
        }
        //else
        //{
                for(i = 0; i < priv->pmt_cnt; i++)
                {
                        pmt_t *pmt = &(priv->pmt[i]);
                        for(j = 0; j < pmt->es_cnt; j++)
                                if(pmt->es[j].pid == pid)
                                        return pmt->es[j].type;
                }
        //}

        return UNKNOWN;
}

static void reset_es(ES_stream_t * tss)
{
	if (tss == NULL)
		return;
	memset(tss, 0, sizeof(ES_stream_t));
	tss->last_cc = -1;
	tss->type = UNKNOWN;
	tss->subtype = UNKNOWN;
	tss->is_synced = 0;
	tss->extradata = NULL;
	tss->mediainfo = NULL;
	tss->extradata_alloc = tss->extradata_len = 0;
}

static ES_stream_t *new_pid(ts_priv_t *priv, int pid)
{
        ES_stream_t *tss;

        tss = (ES_stream_t *)malloc(sizeof(ES_stream_t));
        if(! tss)
                return NULL;
		reset_es(tss);
        tss->pid = pid;
        priv->ts.pids[pid] = tss;

        return tss;
}

static int collect_section(ts_section_t *section, int is_start, unsigned char *buff, int size)
{
        uint8_t *ptr;
        uint16_t tlen;
        int skip, tid;

        mp_msg("COLLECT_SECTION, start: %d, size: %d, collected: %d\n", is_start, size, section->buffer_len);
        if(! is_start && !section->buffer_len)
                return 0;

        if(is_start)
        {
                if(! section->buffer)
                {
                        section->buffer = (uint8_t*) malloc(4096+256);
                        if(section->buffer == NULL)
                                return 0;
                }
                section->buffer_len = 0;
        }

        if(size + section->buffer_len > 4096+256)
        {
                mp_msg("COLLECT_SECTION, excessive len: %d + %d\n", section->buffer_len, size);
                return 0;
        }

        memcpy(&(section->buffer[section->buffer_len]), buff, size);
        section->buffer_len += size;

        if(section->buffer_len < 3)
                return 0;

        skip = section->buffer[0];
        if(skip + 4 > section->buffer_len)
                return 0;

        ptr = &(section->buffer[skip + 1]);
        tid = ptr[0];
        tlen = ((ptr[1] & 0x0f) << 8) | ptr[2];
        mp_msg("SKIP: %d+1, TID: %d, TLEN: %d, COLLECTED: %d\n", skip, tid, tlen, section->buffer_len);
        if(section->buffer_len < (skip+1+3+tlen))
        {
                mp_msg("DATA IS NOT ENOUGH, NEXT TIME\n");
                return 0;
        }

        return skip+1;
}

static inline int32_t prog_idx_in_pat(ts_priv_t *priv, uint16_t progid)
{
        int x;

        if(priv->pat.progs == NULL)
                        return -1;

        for(x = 0; x < priv->pat.progs_cnt; x++)
        {
                if(priv->pat.progs[x].id == progid)
                        return x;
        }

        return -1;
}

static int parse_pat(ts_priv_t * priv, int is_start, unsigned char *buff, int size)
{
        int skip;
        unsigned char *ptr;
        unsigned char *base;
        int entries, i;
        uint16_t progid;
        pat_progs_t *tmp;
        ts_section_t *section;

        section = &(priv->pat.section);
        skip = collect_section(section, is_start, buff, size);
        if(! skip)
                return 0;

        ptr = &(section->buffer[skip]);
        //PARSING
        priv->pat.table_id = ptr[0];
        if(priv->pat.table_id != 0)
                return 0;
        priv->pat.ssi = (ptr[1] >> 7) & 0x1;
        priv->pat.curr_next = ptr[5] & 0x01;
        priv->pat.ts_id = (ptr[3]  << 8 ) | ptr[4];
        priv->pat.version_number = (ptr[5] >> 1) & 0x1F;
        priv->pat.section_length = ((ptr[1] & 0x03) << 8 ) | ptr[2];
        priv->pat.section_number = ptr[6];
        priv->pat.last_section_number = ptr[7];

        //check_crc32(0xFFFFFFFFL, ptr, priv->pat.buffer_len - 4, &ptr[priv->pat.buffer_len - 4]);
        mp_msg("PARSE_PAT: section_len: %d, section %d/%d\n", priv->pat.section_length, priv->pat.section_number, priv->pat.last_section_number);

        entries = (int) (priv->pat.section_length - 9) / 4;     //entries per section

        for(i=0; i < entries; i++)
        {
                int32_t idx;
                base = &ptr[8 + i*4];
                progid = (base[0] << 8) | base[1];

                if((idx = prog_idx_in_pat(priv, progid)) == -1)
                {
                        int sz = sizeof(pat_progs_t) * (priv->pat.progs_cnt+1);
                        tmp = (pat_progs_t *)realloc_struct(priv->pat.progs, priv->pat.progs_cnt+1, sizeof(pat_progs_t));
                        if(tmp == NULL)
                        {
				priv->pat.progs = NULL;
				priv->pat.progs_cnt = 0;
                                mp_msg("PARSE_PAT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
                                break;
                        }
                        priv->pat.progs = tmp;
                        idx = priv->pat.progs_cnt;
                        priv->pat.progs_cnt++;
                }

                priv->pat.progs[idx].id = progid;
                priv->pat.progs[idx].pmt_pid = ((base[2]  & 0x1F) << 8) | base[3];
                mp_msg("PROG: %d (%d-th of %d), PMT: %d\n", priv->pat.progs[idx].id, i+1, entries, priv->pat.progs[idx].pmt_pid);
                mp_msg("PROGRAM_ID=%d (0x%02X), PMT_PID: %d(0x%02X)\n",
                        progid, progid, priv->pat.progs[idx].pmt_pid, priv->pat.progs[idx].pmt_pid);
        }

        return 1;
}

static int parse_program_descriptors(pmt_t *pmt, uint8_t *buf, uint16_t len)
{
        uint16_t i = 0, k, olen = len;

        while(len > 0)
        {
                mp_msg("PROG DESCR, TAG=%x, LEN=%d(%x)\n", buf[i], buf[i+1], buf[i+1]);
                if(buf[i+1] > len-2)
                {
                        mp_msg("ERROR, descriptor len is too long, skipping\n");
                        return olen;
                }

                if(buf[i] == 0x1d)
                {
                        if(buf[i+3] == 2)       //buggy versions of vlc muxer make this non-standard mess (missing iod_scope)
                                k = 3;
                        else
                                k = 4;          //this is standard compliant
                        parse_mp4_descriptors(pmt, &buf[i+k], (int) buf[i+1]-(k-2), NULL);
                }
				else if (buf[i] == 0x9)
				{
					pmt->encrypt = 1;
					mp_msg("Program CA Descriptor, CA_system_ID: %02X%02X\n", buf[i+2], buf[i+3]);
				}

                len -= 2 + buf[i+1];
				i += 2 + buf[i+1];
        }

        return olen;
}

/*
 * document iso13818-1 page 81
 */
static int parse_descriptors(struct pmt_es_t *es, uint8_t *ptr, int es_type)
{
	int j, descr_len, len;

	j = 0;
	len = es->descr_length;
	while(len > 2)
	{
		descr_len = ptr[j+1];
		mp_msg("...descr id: 0x%x, len=%d\n", ptr[j], descr_len);
		if(descr_len > len)
		{
			mp_msg("INVALID DESCR LEN for tag %02x: %d vs %d max, EXIT LOOP\n", ptr[j], descr_len, len);
			return -1;
		}


		if(ptr[j] == 0x6a || ptr[j] == 0x7a)	//A52 Descriptor
		{
			if(es_type == 0x6)
			{
				es->type = AUDIO_A52;
				mp_msg("DVB A52 Descriptor\n");
			}
		}
		else if(ptr[j] == 0x7b)	//DVB DTS Descriptor
		{
			if(es_type == 0x6)
			{
				es->type = AUDIO_DTS;
				mp_msg("DVB DTS Descriptor\n");
			}
		}
		else if(ptr[j] == 0x56) // Teletext
		{
			if(descr_len >= 5) {
				memcpy(es->lang, ptr+2, 3);
				es->lang[3] = 0;
			}
			//printf("LANG: %02x %02x %02x\n", es->lang[0], es->lang[1], es->lang[2]);
			es->type = SPU_TELETEXT;
		}
		else if(ptr[j] == 0x59)	//Subtitling Descriptor
		{
			uint8_t subtype;

			mp_msg("Subtitling Descriptor\n");
			if(descr_len < 8)
			{
				mp_msg("Descriptor length too short for DVB Subtitle Descriptor: %d, SKIPPING\n", descr_len);
			}
			else
			{
				memcpy(es->lang, &ptr[j+2], 3);
				es->lang[3] = 0;
				subtype = ptr[j+5];
				if(
					(subtype >= 0x10 && subtype <= 0x13) ||
					(subtype >= 0x20 && subtype <= 0x23)
				)
				{
					es->type = SPU_DVB;
					//page parameters: compo page 2 bytes, ancillary page 2 bytes
				}
				else
					es->type = UNKNOWN;
			}
		}
		else if(ptr[j] == 0x50)	//Component Descriptor
		{
			mp_msg("Component Descriptor\n");
			memcpy(es->lang, &ptr[j+5], 3);
			es->lang[3] = 0;
		}
		else if(ptr[j] == 0xa)	//Language Descriptor
		{
			memcpy(es->lang, &ptr[j+2], 3);
			es->lang[3] = 0;
			mp_msg("Language Descriptor: %s\n", es->lang);
		}
		else if(ptr[j] == 0x5)	//Registration Descriptor (looks like e fourCC :) )
		{
			mp_msg("Registration Descriptor\n");
			if(descr_len < 4)
			{
				mp_msg("Registration Descriptor length too short: %d, SKIPPING\n", descr_len);
			}
			else
			{
				char *d;
				memcpy(es->format_descriptor, &ptr[j+2], 4);
				es->format_descriptor[4] = 0;

				d = (char *)&ptr[j+2];
				if(d[0] == 'A' && d[1] == 'C' && d[2] == '-' && d[3] == '3')
				{
					es->type = AUDIO_A52;
				}
				else if(d[0] == 'D' && d[1] == 'T' && d[2] == 'S' && d[3] == '1')
				{
					es->type = AUDIO_DTS;
				}
				else if(d[0] == 'D' && d[1] == 'T' && d[2] == 'S' && d[3] == '2')
				{
					es->type = AUDIO_DTS;
				}
				else if(d[0] == 'D' && d[1] == 'T' && d[2] == 'S' && d[3] == '3')
				{
					es->type = AUDIO_DTS;
				}
				else if(d[0] == 'V' && d[1] == 'C' && d[2] == '-' && d[3] == '1')
				{
					es->type = VIDEO_VC1;
				}
				else if(d[0] == 'H' && d[1] == 'D' && d[2] == 'M' && d[3] == 'V')
				{
					//es->type = AUDIO_BPCM;
					es->type = UNKNOWN;
				}
				else
					es->type = UNKNOWN;
				mp_msg("FORMAT %s\n", es->format_descriptor);
			}
		}
		else if(ptr[j] == 0x1e)
		{
			es->mp4_es_id = (ptr[j+2] << 8) | ptr[j+3];
			mp_msg("SL Descriptor: ES_ID: %d(%x), pid: %d\n", es->mp4_es_id, es->mp4_es_id, es->pid);
		}
		else if(ptr[j] == 0x2)
		{
			int multiple_frame_rate_flag, frame_rate_code, MPEG_1_only_flag;
			mp_msg("video stream Descriptor\n");
			multiple_frame_rate_flag = (ptr[j+2] >> 7);
			frame_rate_code = (ptr[j+2] & 0x7f) >> 3;
			MPEG_1_only_flag = (ptr[j+2] & 0x04) >> 2;
			mp_msg("multiple_frame_rate_flag: %d %f %d\n", multiple_frame_rate_flag, 
					(float)frame_rate_code, MPEG_1_only_flag);
			if (multiple_frame_rate_flag == 0) {
				//get_mp2_framerate(frame_rate_code);
			}
			if (MPEG_1_only_flag == 0) {
				//int profile_and_level_indication = ptr[j+3];
				//memcpy(es->format_descriptor, &ptr[j+3], 1);
			}
		}
		else if(ptr[j] == 0x3)
		{
			mp_msg("audio stream Descriptor\n");
		}
		else if(ptr[j] == 0x9)
		{
			es->encrypt = 1;
			mp_msg("CA Descriptor, CA_system_ID: %02X%02X\n", ptr[j+2], ptr[j+3]);
		}
		else if(ptr[j] == 0x1b) // 27
		{
			mp_msg("MPEG-4 video Descriptor\n");
		}
		else if(ptr[j] == 0x1c) // 28
		{
			mp_msg("MPEG-4 audio Descriptor\n");
		}
		else if(ptr[j] == 0x28) // 40
		{
			mp_msg("AVC video descriptor\n");
		}
		else if(ptr[j] == 0x81)
		{
			mp_msg("AC-3 audio descriptor\n");
		}
		else
		{
			mp_msg("Unknown descriptor 0x%x, SKIPPING\n", ptr[j]);
		}

		len -= 2 + descr_len;
		j += 2 + descr_len;
	}

	return 1;
}

static int parse_sl_section(pmt_t *pmt, ts_section_t *section, int is_start, unsigned char *buff, int size)
{
        int tid, len, skip;
        uint8_t *ptr;
        skip = collect_section(section, is_start, buff, size);
        if(! skip)
                return 0;

        ptr = &(section->buffer[skip]);
        tid = ptr[0];
        len = ((ptr[1] & 0x0f) << 8) | ptr[2];
        mp_msg("TABLEID: %d (av. %d), skip=%d, LEN: %d\n", tid, section->buffer_len, skip, len);
        if(len > 4093 || section->buffer_len < len || tid != 5)
        {
                mp_msg("SECTION TOO LARGE or wrong section type, EXIT\n");
                return 0;
        }

        if(! (ptr[5] & 1))
                return 0;

        //8 is the current position, len - 9 is the amount of data available
        parse_mp4_descriptors(pmt, &ptr[8], len - 9, NULL);

        return 1;
}

static int parse_pmt(ts_priv_t * priv, uint16_t progid, uint16_t pid, int is_start, unsigned char *buff, int size)
{
	unsigned char *base, *es_base;
	pmt_t *pmt;
	int32_t idx, es_count, section_bytes;
	int skip;
	pmt_t *tmp;
	struct pmt_es_t *tmp_es;
	ts_section_t *section;
	ES_stream_t *tss;

	idx = progid_idx_in_pmt(priv, progid);

	if(idx == -1)
	{
		int sz = (priv->pmt_cnt + 1) * sizeof(pmt_t);
		tmp = (pmt_t *)realloc_struct(priv->pmt, priv->pmt_cnt + 1, sizeof(pmt_t));
		if(tmp == NULL)
		{
			priv->pmt = 0;
			priv->pmt_cnt = 0;
			mp_msg("PARSE_PMT: COULDN'T REALLOC %d bytes, NEXT\n", sz);
			return 0;
		}
		priv->pmt = tmp;
		idx = priv->pmt_cnt;
		memset(&(priv->pmt[idx]), 0, sizeof(pmt_t));
		priv->pmt_cnt++;
		priv->pmt[idx].progid = progid;
	}

	pmt = &(priv->pmt[idx]);

	section = &(pmt->section);
	skip = collect_section(section, is_start, buff, size);
	if(! skip)
		return 0;

	base = &(section->buffer[skip]);

	mp_msg("FILL_PMT(prog=%d), PMT_len: %d, IS_START: %d, TS_PID: %d, SIZE=%d, ES_CNT=%d, IDX=%d, PMT_PTR=%p\n",
		progid, pmt->section.buffer_len, is_start, pid, size, pmt->es_cnt, idx, pmt);

	pmt->table_id = base[0];
	if(pmt->table_id != 2)
		return 1;
	pmt->ssi = base[1] & 0x80;
	pmt->section_length = (((base[1] & 0xf) << 8 ) | base[2]);
	pmt->version_number = (base[5] >> 1) & 0x1f;
	pmt->curr_next = (base[5] & 1);
	pmt->section_number = base[6];
	pmt->last_section_number = base[7];
	pmt->PCR_PID = ((base[8] & 0x1f) << 8 ) | base[9];
	pmt->prog_descr_length = ((base[10] & 0xf) << 8 ) | base[11];
	if(pmt->prog_descr_length > pmt->section_length - 9)
	{
		mp_msg("PARSE_PMT, INVALID PROG_DESCR LENGTH (%d vs %d)\n", pmt->prog_descr_length, pmt->section_length - 9);
		return -1;
	}

	if(pmt->prog_descr_length)
		parse_program_descriptors(pmt, &base[12], pmt->prog_descr_length);

	es_base = &base[12 + pmt->prog_descr_length];	//the beginning of th ES loop

	section_bytes= pmt->section_length - 13 - pmt->prog_descr_length;
	es_count  = 0;

	while(section_bytes >= 5)
	{
		int es_pid, es_type;

		es_type = es_base[0];
		es_pid = ((es_base[1] & 0x1f) << 8) | es_base[2];

		idx = es_pid_in_pmt(pmt, es_pid);
		if(idx == -1)
		{
			int sz = sizeof(struct pmt_es_t) * (pmt->es_cnt + 1);
			tmp_es = (struct pmt_es_t *)realloc_struct(pmt->es, pmt->es_cnt + 1, sizeof(struct pmt_es_t));
			if(tmp_es == NULL)
			{
				pmt->es = NULL;
				pmt->es_cnt = 0;
				mp_msg("PARSE_PMT, COULDN'T ALLOCATE %d bytes for PMT_ES\n", sz);
				continue;
			}
			pmt->es = tmp_es;
			idx = pmt->es_cnt;
			memset(&(pmt->es[idx]), 0, sizeof(struct pmt_es_t));
			pmt->es[idx].type = UNKNOWN;
			pmt->es_cnt++;
		}

		pmt->es[idx].descr_length = ((es_base[3] & 0xf) << 8) | es_base[4];


		if(pmt->es[idx].descr_length > section_bytes - 5)
		{
			mp_msg("PARSE_PMT, ES_DESCR_LENGTH TOO LARGE %d > %d, EXIT\n",
				pmt->es[idx].descr_length, section_bytes - 5);
			return -1;
		}

		pmt->es[idx].pid = es_pid;

		parse_descriptors(&pmt->es[idx], &es_base[5], es_type);

		if (pmt->es[idx].type != UNKNOWN)
		{
			mp_msg("Get type from descriptor %08x\n", pmt->es[idx].type);
		} else {
			switch(es_type)
			{
				case 1:
					pmt->es[idx].type = VIDEO_MPEG1;
					break;
				case 2:
					pmt->es[idx].type = VIDEO_MPEG2;
					break;
				case 3:
				case 4:
					pmt->es[idx].type = AUDIO_MP2;
					break;
				case 6:
					if(pmt->es[idx].type == 0x6)	//this could have been ovrwritten by parse_descriptors
						pmt->es[idx].type = UNKNOWN;
					break;
				case 0x10:
					pmt->es[idx].type = VIDEO_MPEG4;
					break;
				case 0x0f:
				case 0x11:
					pmt->es[idx].type = AUDIO_AAC;
					break;
				case 0x1b:
					pmt->es[idx].type = VIDEO_H264;
					break;
				case 0x12:
					pmt->es[idx].type = SL_PES_STREAM;
					break;
				case 0x13:
					pmt->es[idx].type = SL_SECTION;
					break;
				case 0x80:
					pmt->es[idx].type = AUDIO_BPCM;
					break;
				case 0x81: // AC3
					pmt->es[idx].type = AUDIO_A52;
					break;
				case 0x83:
					//pmt->es[idx].type = AUDIO_TRUEHD;
					pmt->es[idx].type = UNKNOWN;
					break;
				case 0x87: // E-AC3
					pmt->es[idx].type = AUDIO_EAC3;
					break;
				case 0x82: // DTS 
				case 0x85: // DTS-HD High Resolution 
				case 0x86: // DTS-HD Master Audio
				case 0x8A:
					pmt->es[idx].type = AUDIO_DTS;
					break;
				case 0x90:
					pmt->es[idx].type = SPU_PGS;
					break;
				case 0xA1: // secondary E-AC3 
					//pmt->es[idx].type = AUDIO_EAC3;
					pmt->es[idx].type = UNKNOWN;
					break;
				case 0xA2: // secondary DTS
					//pmt->es[idx].type = AUDIO_DTS;
					pmt->es[idx].type = UNKNOWN;
					break;
				case 0xEA:
					pmt->es[idx].type = VIDEO_VC1;
					break;
				default:
					mp_msg("UNKNOWN ES TYPE=0x%x\n", es_type);
					pmt->es[idx].type = UNKNOWN;
			}
			mp_msg("Get type from es_type '%02x' '%08x'\n", es_type, pmt->es[idx].type);
		}

		tss = priv->ts.pids[es_pid];			//an ES stream
		if(tss == NULL)
		{
			tss = new_pid(priv, es_pid);
			if(tss)
				tss->type = (es_stream_type_t)pmt->es[idx].type;
		}

		section_bytes -= 5 + pmt->es[idx].descr_length;
		mp_msg("PARSE_PMT(%d INDEX %d), STREAM: %d, FOUND pid=0x%x (%d), type=0x%x, ES_DESCR_LENGTH: %d, bytes left: %d\n",
			progid, idx, es_count, pmt->es[idx].pid, pmt->es[idx].pid, pmt->es[idx].type, pmt->es[idx].descr_length, section_bytes);


		es_base += 5 + pmt->es[idx].descr_length;

		es_count++;
	}

	return 1;
}


static inline int32_t prog_id_in_pat(ts_priv_t *priv, uint16_t pid)
{
	int x;

	if(priv->pat.progs == NULL)
		return -1;

	for(x = 0; x < priv->pat.progs_cnt; x++)
	{
		if(priv->pat.progs[x].pmt_pid == pid)
			return priv->pat.progs[x].id;
	}

	return -1;
}

unsigned char getbits(unsigned char *buffer, unsigned int from, unsigned char len)
{
    unsigned int n;
    unsigned char m, u, l, y;

    n = from / 8;
    m = from % 8;
    u = 8 - m;
    l = (len > u ? len - u : 0);

    y = (buffer[n] << m);
    if(8 > len)
        y  >>= (8-len);
    if(l)
        y |= (buffer[n+1] >> (8-l));

	/*
    fmp_msg(stdout, "GETBITS(%d -> %d): bytes=0x%02X 0x%02X 0x%02X 0x%02X 0x%02X, n=%d, m=%d, l=%d, u=%d, Y=%d\n",
			from, (int) len, buffer[n], buffer[n+1], buffer[n+2], buffer[n+3], buffer[n+4], n, (int) m, (int) l, (int) u, (int) y);
			*/
    return  y;
}

unsigned int getbits32(unsigned char *buffer, unsigned int from)
{
	unsigned int ret = 0;
	ret |= getbits(buffer, from, 8) << 24;
	from += 8;
	ret |= getbits(buffer, from, 8) << 16;
	from += 8;
	ret |= getbits(buffer, from, 8) << 8;
	from += 8;
	ret |= getbits(buffer, from, 8);
	from += 8;
	return ret;
}

static int mp4_parse_sl_packet(pmt_t *pmt, uint8_t *buf, uint16_t packet_len, int pid, ES_stream_t *pes_es)
{
	int i, n, m, mp4_es_id = -1;
	uint64_t v = 0;
	uint32_t pl_size = 0;
	int deg_flag = 0;
	mp4_es_descr_t *es = NULL;
	mp4_sl_config_t *sl = NULL;
	uint8_t au_start = 0, au_end = 0, rap_flag = 0, ocr_flag = 0, padding = 0,  padding_bits = 0, idle = 0;

	pes_es->is_synced = 0;
	mp_msg("mp4_parse_sl_packet, pid: %d, pmt: %pm, packet_len: %d\n", pid, pmt, packet_len);
	if(! pmt || !packet_len)
		return 0;

	for(i = 0; i < pmt->es_cnt; i++)
	{
		if(pmt->es[i].pid == pid)
			mp4_es_id = pmt->es[i].mp4_es_id;
	}
	if(mp4_es_id < 0)
		return -1;

	for(i = 0; i < pmt->mp4es_cnt; i++)
	{
		if(pmt->mp4es[i].id == mp4_es_id)
			es = &(pmt->mp4es[i]);
	}
	if(! es)
		return -1;

	pes_es->subtype = (es_stream_type_t)es->decoder.object_type;

	sl = &(es->sl);
	if(!sl)
		return -1;

	//now es is the complete es_descriptor of out mp4 ES stream
	mp_msg("ID: %d, FLAGS: 0x%x, subtype: %x\n", es->id, sl->flags, pes_es->subtype);

	n = 0;
	if(sl->au_start)
		pes_es->sl.au_start = au_start = getbits(buf, n++, 1);
	else
		pes_es->sl.au_start = (pes_es->sl.last_au_end ? 1 : 0);
	if(sl->au_end)
		pes_es->sl.au_end = au_end = getbits(buf, n++, 1);

	if(!sl->au_start && !sl->au_end)
	{
		pes_es->sl.au_start = pes_es->sl.au_end = au_start = au_end = 1;
	}
	pes_es->sl.last_au_end = pes_es->sl.au_end;


	if(sl->ocr_len > 0)
		ocr_flag = getbits(buf, n++, 1);
	if(sl->idle)
		idle = getbits(buf, n++, 1);
	if(sl->padding)
		padding = getbits(buf, n++, 1);
	if(padding)
	{
		padding_bits = getbits(buf, n, 3);
		n += 3;
	}

	if(idle || (padding && !padding_bits))
	{
		pes_es->payload_size = 0;
		return -1;
	}

	//(! idle && (!padding || padding_bits != 0)) is true
	n += sl->packet_seqnum_len;
	if(sl->degr_len)
		deg_flag = getbits(buf, n++, 1);
	if(deg_flag)
		n += sl->degr_len;

	if(ocr_flag)
	{
		n += sl->ocr_len;
		mp_msg("OCR: %d bits\n", sl->ocr_len);
	}

	if(packet_len * 8 <= n)
		return -1;

	mp_msg("\nAU_START: %d, AU_END: %d\n", au_start, au_end);
	if(au_start)
	{
		int dts_flag = 0, cts_flag = 0, ib_flag = 0;

		if(sl->random_accesspoint)
			rap_flag = getbits(buf, n++, 1);

		//check commented because it seems it's rarely used, and we need this flag set in case of au_start
		//the decoder will eventually discard the payload if it can't decode it
		//if(rap_flag || sl->random_accesspoint_only)
			pes_es->is_synced = 1;

		n += sl->au_seqnum_len;
		if(packet_len * 8 <= n+8)
			return -1;
		if(sl->use_ts)
		{
			dts_flag = getbits(buf, n++, 1);
			cts_flag = getbits(buf, n++, 1);
		}
		if(sl->instant_bitrate_len)
			ib_flag = getbits(buf, n++, 1);
		if(packet_len * 8 <= n+8)
			return -1;
		if(dts_flag && (sl->ts_len > 0))
		{
			n += sl->ts_len;
			mp_msg("DTS: %d bits\n", sl->ts_len);
		}
		if(packet_len * 8 <= n+8)
			return -1;
		if(cts_flag && (sl->ts_len > 0))
		{
			int i = 0, m;

			while(i < sl->ts_len)
			{
				m = FFMIN(8, sl->ts_len - i);
				v |= getbits(buf, n, m);
				if(sl->ts_len - i > 8)
					v <<= 8;
				i += m;
				n += m;
				if(packet_len * 8 <= n+8)
					return -1;
			}

			pes_es->pts = (double) v / (double) sl->ts_resolution;
			mp_msg("CTS: %d bits, value: %"PRIu64"/%d = %.3f\n", sl->ts_len, v, sl->ts_resolution, pes_es->pts);
		}


		i = 0;
		pl_size = 0;
		while(i < sl->au_len)
		{
			m = FFMIN(8, sl->au_len - i);
			pl_size |= getbits(buf, n, m);
			if(sl->au_len - i > 8)
				pl_size <<= 8;
			i += m;
			n += m;
			if(packet_len * 8 <= n+8)
				return -1;
		}
		mp_msg("AU_LEN: %u (%d bits)\n", pl_size, sl->au_len);
		if(ib_flag)
			n += sl->instant_bitrate_len;
	}

	m = (n+7)/8;
	if(0 < pl_size && pl_size < pes_es->payload_size)
		pes_es->payload_size = pl_size;

	mp_msg("mp4_parse_sl_packet, n=%d, m=%d, size from pes hdr: %u, sl hdr size: %u, RAP FLAGS: %d/%d\n",
		n, m, pes_es->payload_size, pl_size, (int) rap_flag, (int) sl->random_accesspoint_only);

	return m;
}


static int parse_pes_extension_fields(unsigned char *p, int pkt_len)
{
	int skip;
	unsigned char flags;

	if(!(p[7] & 0x1))	//no extension_field
		return -1;
	skip = 9;
	if(p[7] & 0x80)
	{
		skip += 5;
		if(p[7] & 0x40)
			skip += 5;
	}
	if(p[7] & 0x20)	//escr_flag
		skip += 6;
	if(p[7] & 0x10)	//es_rate_flag
		skip += 3;
	if(p[7] & 0x08)//dsm_trick_mode is unsupported, skip
	{
		skip = 0;//don't let's parse the extension fields
	}
	if(p[7] & 0x04)	//additional_copy_info
		skip += 1;
	if(p[7] & 0x02)	//pes_crc_flag
		skip += 2;
	if(skip >= pkt_len)	//too few bytes
		return -1;
	flags = p[skip];
	skip++;
	if(flags & 0x80)	//pes_private_data_flag
		skip += 16;
	if(skip >= pkt_len)
		return -1;
	if(flags & 0x40)	//pack_header_field_flag
	{
		unsigned char l = p[skip];
		skip += l;
	}
	if(flags & 0x20)	//program_packet_sequence_counter
		skip += 2;
	if(flags & 0x10)	//p_std
		skip += 2;
	if(skip >= pkt_len)
		return -1;
	if(flags & 0x01)	//finally the long desired pes_extension2
	{
		unsigned char l = p[skip];	//ext2 flag+len
		skip++;
		if((l == 0x81) && (skip < pkt_len))
		{
			int ssid = p[skip];
			mp_msg("SUBSTREAM_ID=%d (0x%02X)\n", ssid, ssid);
			return ssid;
		}
	}

	return -1;
}

static int get_ac3_samplerate(int fscod) {
	unsigned int bitrate_table[3] = {
		48000, 44100, 32000
	};
	if (fscod >= 3 || fscod < 0)
		return 0;
	return bitrate_table[fscod];
}

static int get_ac3_bitrate(int frmsizcod) {
	unsigned int bitrate_table[] = {
		32, 40, 48, 56, 64,  		80, 96, 112, 128, 160,
		192, 224, 256, 320, 384,  	448, 512, 576, 640
	};
	if (frmsizcod >= 37 || frmsizcod < 0)
		return 0;
	frmsizcod /= 2;
	return bitrate_table[frmsizcod] * 1000;
}

static int get_ac3_channel(int acmod) {
	unsigned int channel_table[8] = {
		2, 1, 2, 3, 3,  4, 4, 5
	};
	if (acmod < 0 || acmod > 7)
		return 0;
	return channel_table[acmod];
}

static int h264_parse_hrd(TSSTREAMINFO *info, unsigned char *p, unsigned int nPos)
{
	int cpb_cnt_minus1, bit_rate_value_minus1 = 0, cpb_size_value_minus1;
	int bit_rate_scale, cbr_flag;
	int ii;

	ii = nPos / 8;
	cpb_cnt_minus1 = read_golomb(p, &nPos);
	bit_rate_scale = getbits(p, nPos, 4);
	nPos += 8;				// bit_rate_scale, cpb_size_scale
	for (ii = 0; ii <= cpb_cnt_minus1; ii++) 
	{
		bit_rate_value_minus1 = read_golomb(p, &nPos);
		cpb_size_value_minus1 = read_golomb(p, &nPos);
		cbr_flag = getbits(p, nPos++, 1);
		//mp_msg("bit_rate_scale: %d, bit_rate_value_minus1: %d, cpb_size_value_minus1: %d, cbr_flag: %d\n", 
		//		bit_rate_scale, bit_rate_value_minus1, cpb_size_value_minus1, cbr_flag);
	}
	info->bitrate = (bit_rate_value_minus1 + 1) << (6 + bit_rate_scale);

	return nPos;
}

static int h264_parse_vui(TSSTREAMINFO *info, unsigned char *p, unsigned int nPos)
{
	float fps = 0;
	int aspect_ratio_info_present_flag = getbits(p, nPos++, 1);
	if (aspect_ratio_info_present_flag)
	{
		int aspect_ratio_idc = getbits(p, nPos, 8);
		nPos += 8;
		//mp_msg("aspect_ratio_idc: %d, %d\n", aspect_ratio_idc, nPos);
		if (aspect_ratio_idc == 255) {
			nPos += 32;
		}
	}

	if (getbits(p, nPos++, 1))			// overscan_info_present_flag
		getbits(p, nPos++, 1);			// overscan_appropriate_flag
	if (getbits(p, nPos++, 1)) {		// video_signal_type_present_flag
		getbits(p, nPos, 3);			// video_format
		nPos += 3;
		getbits(p, nPos++, 1);			// video_full_range_flag
		if (getbits(p, nPos++, 1)) {	// colour_description_present_flag
			nPos += 24;
		}
	}
	if (getbits(p, nPos++, 1)) {		// chroma_loc_info_present_flag
		read_golomb(p, &nPos);			// chroma_sample_loc_type_top_field
		read_golomb(p, &nPos);			// chroma_sample_loc_type_bottom_field
	}
	if (getbits(p, nPos++, 1)) {		// timing_info_present_flag
		unsigned int num_units_in_tick = 0, time_scale, fixed_frame_rate_flag;
		num_units_in_tick = getbits32(p, nPos);
		nPos += 32;
		time_scale = getbits32(p, nPos);
		nPos += 32;
		fixed_frame_rate_flag = getbits(p, nPos++, 1);
		if(num_units_in_tick> 0 && time_scale > 0)
			fps = (float)time_scale / (float)num_units_in_tick;
		if(fixed_frame_rate_flag)
			fps /= 2;
		mp_msg("num_units_in_tick: %u, time_scale: %u, fixed_frame_rate_flag: %u\n",
				num_units_in_tick, time_scale, fixed_frame_rate_flag);
	} else {
		fps = 0;
	}

	if (getbits(p, nPos++, 1)) {		// nal_hrd_parameters_present_flag
		nPos = h264_parse_hrd(info, p, nPos);
	} else {
		info->bitrate = 0;
	}

	// fill data
	info->fps = fps;

	return nPos;
}

int h264_parse_sps(TSSTREAMINFO *info, unsigned char *p)
{
	int frame_cropping, vui_parameters_present;
	int num_ref_frames, display_picture_width, mbh, frame_mbs_only, display_picture_height;
	unsigned int nPos = 24;
	int ii, kk, vv;
	unsigned int golomb_tmp;
	unsigned int frame_size, crop_l = 0, crop_r = 0, crop_t = 0, crop_b = 0;
	int nRet = 0;

	golomb_tmp = read_golomb(p, &nPos);
	//mp_msg("profile_idc: %d\n", p[0]);
	//mp_msg("seq_parameter_set_id: %u, %d\n", golomb_tmp, nPos);
	if (p[0] >= 100) {
		if(read_golomb(p, &nPos) == 3)
			nPos++;
		read_golomb(p, &nPos);
		read_golomb(p, &nPos);
		nPos++;
		if(getbits(p, nPos++, 1)){
			for(ii = 0; ii < 8; ii++)
			{  // scaling list is skipped for now
				if(getbits(p, nPos++, 1))
				{
					vv = 8;
					for(kk = (ii < 6 ? 16 : 64); kk && vv; kk--)
						vv = (vv + read_golomb_s(p, &nPos)) & 255;
				}
			}
		}
	}
	golomb_tmp = read_golomb(p, &nPos);
	//mp_msg("log2_max_frame_num_minus4: %d\n", golomb_tmp);
	vv = read_golomb(p, &nPos);
	//mp_msg("pic_order_cnt_type: %d\n", vv);
	if(vv == 0) 
	{
		golomb_tmp = read_golomb(p, &nPos);
		//mp_msg("log2_max_pic_order_cnt_lsb_minus4: %d\n", golomb_tmp);
	}
	else if(vv == 1)
	{
		getbits(p, nPos++, 1);
		read_golomb(p, &nPos);
		read_golomb(p, &nPos);
		vv = read_golomb(p, &nPos);
		for(ii = 0; ii < vv; ii++)
			read_golomb(p, &nPos);
	}
	num_ref_frames = read_golomb(p, &nPos);
	golomb_tmp = getbits(p, nPos++, 1);
	//mp_msg("gaps_in_frame_num_value_allowed_flag: %d\n", golomb_tmp);
	display_picture_width = 16 *(read_golomb(p, &nPos)+1);
	mbh = read_golomb(p, &nPos)+1;
	frame_mbs_only = getbits(p, nPos++, 1);
	display_picture_height = 16 * (2 - frame_mbs_only) * mbh;
	if (!frame_mbs_only)
		getbits(p, nPos++, 1);
	golomb_tmp = getbits(p, nPos++, 1);			// direct_8x8_interface
	//mp_msg("direct_8x8_interface: %d\n", golomb_tmp);
	frame_cropping = getbits(p, nPos++, 1);
	//mp_msg("frame_cropping: %d, pos: %d\n", frame_cropping, nPos);
	if (frame_cropping) {
		crop_l = read_golomb(p, &nPos);
		crop_r = read_golomb(p, &nPos);
		crop_t = read_golomb(p, &nPos);
		crop_b = read_golomb(p, &nPos);
	}
	vui_parameters_present = getbits(p, nPos++, 1);
	//mp_msg("frame_cropping: %d, vui_parameters_present: %d\n", frame_cropping, vui_parameters_present);
	if (vui_parameters_present) {
		nPos = h264_parse_vui(info, p, nPos);
	} else {
		info->fps = 0;
	}

	info->width = display_picture_width - 2*crop_l - 2*crop_r;
	info->height = display_picture_height - 2*crop_t - 2*crop_b;

	frame_size = ( (((display_picture_width+15)>>4)<<4 ) * ( ((display_picture_height+15)>>4)<<4) / (16*16) * 448 );
	if (get_decfb_size(display_picture_width, display_picture_height, frame_size, num_ref_frames) == -1)
		nRet = 0;
	else 
		nRet = 1;

	return nRet;
}

static int mp_a52_header_parse_ac3(WAVEFORMATEX * wf, unsigned char *p, int packet_len)
{
	int nRet = 0;
	int fscod = p[4] >> 6;
	int bsid = p[5] >> 3;
	int frmsizcod = p[4] & 0x3f;
	int acmod;
	int lfeon;
	int bitcnt = 8 * 6;
	static const uint8_t pi_halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
	const unsigned i_rate_shift = pi_halfrate[bsid]; // from vlc player

	acmod = getbits(p, bitcnt, 3);
	bitcnt += 3;
	if ((acmod & 0x1) && (acmod != 0x1))
		bitcnt += 2;
	if (acmod & 0x4)
		bitcnt += 2;
	if (acmod == 0x2)
		bitcnt += 2;
	lfeon = getbits(p, bitcnt++, 1);
#if 0
	int tmp;
	tmp = getbits(p, bitcnt, 5); // dialnorm
	bitcnt += 5;
	tmp = getbits(p, bitcnt++, 1); // compre
	if (tmp)
	{
		tmp = getbits(p, bitcnt, 8); // compr
		bitcnt += 8;
	}
	tmp = getbits(p, bitcnt++, 1); // lancode
	if (tmp)
	{
		tmp = getbits(p, bitcnt, 8); // lancod
		bitcnt += 8;
	}
	tmp = getbits(p, bitcnt++, 1); // audioprodie
	if (tmp)
	{
		tmp = getbits(p, bitcnt, 5); // mixlevel
		bitcnt += 5;
		tmp = getbits(p, bitcnt, 2); // roomtyp
		bitcnt += 2;
	}
	if (acmod == 0)
	{
		tmp = getbits(p, bitcnt, 5); // dialnorm2
		bitcnt += 5;
		tmp = getbits(p, bitcnt++, 1); // compr2e
		if (tmp)
		{
			tmp = getbits(p, bitcnt, 8); // compr2
			bitcnt += 8;
		}
		tmp = getbits(p, bitcnt++, 1); // lancod2e
		if (tmp)
		{
			tmp = getbits(p, bitcnt, 8); // lancod2
			bitcnt += 8;
		}
		tmp = getbits(p, bitcnt++, 1); // audioprodi2e
		if (tmp)
		{
			tmp = getbits(p, bitcnt, 5); // mixleve2
			bitcnt += 5;
			tmp = getbits(p, bitcnt, 2); // roomtyp2
			bitcnt += 2;
		}
	}
	tmp = getbits(p, bitcnt++, 1); // copyrightb
	tmp = getbits(p, bitcnt++, 1); // origbs
	tmp = getbits(p, bitcnt++, 1); // timecod1e
	if (tmp)
	{
		puts("i got timecode1!!!");
	}
	tmp = getbits(p, bitcnt++, 1); // timecod2e
	if (tmp)
	{
		puts("i got timecode2!!!");
	}
#endif

	wf->wFormatTag = AUDIO_A52;
	if ((p[6] & 0xf8) == 0x50) {
		// Dolby surround = stereo + Dolby
		wf->nChannels = 2;
	} else {
		wf->nChannels = get_ac3_channel(acmod) + lfeon;
	}
	wf->nSamplesPerSec = (get_ac3_samplerate(fscod) >> i_rate_shift);
	wf->nAvgBytesPerSec = (get_ac3_bitrate(frmsizcod) >> i_rate_shift) / 8;
	if ( (wf->nChannels == 0) || (wf->nSamplesPerSec == 0) || (wf->nAvgBytesPerSec == 0) )
	{
		return nRet;
	}
	mp_msg("AC-3: channel: %d , samplerate: %d, bitrate: %d, lfe: %d\n",
			wf->nChannels, wf->nSamplesPerSec, wf->nAvgBytesPerSec * 8, lfeon);
	nRet = 1;

	return nRet;
}

static int mp_a52_header_parse_eac3(WAVEFORMATEX * wf, unsigned char *p, int packet_len)
{
	int nRet = 0;
	int bitcnt;
	int lfeon;
	int frmsiz, fscod, acmod;
	int size;
	int numblkscod, bsid;

	bitcnt = 16 +	// syncword
		2 +			// bsi - strmtyp
		3;			// bsi - substreamid
	frmsiz = getbits(p, bitcnt, 3) << 8;
	bitcnt += 3;
	frmsiz |= getbits(p, bitcnt, 8);
	bitcnt += 8;
	if (frmsiz < 2)
		return nRet;
	size = 2 * (frmsiz + 1);
	fscod = getbits(p, bitcnt, 2);
	bitcnt += 2;
	if (fscod == 0x03)
	{
		const unsigned fscod2 = getbits(p, bitcnt, 2 );
		bitcnt += 2;
		if( fscod2 == 0X03 )
			return nRet;
		wf->nSamplesPerSec = get_ac3_samplerate(fscod2) / 2;
		numblkscod = 6;
	}
	else
	{
		numblkscod = getbits(p, bitcnt, 2 );
		bitcnt += 2;
		static const int pi_blocks[4] = { 1, 2, 3, 6 };
		wf->nSamplesPerSec = get_ac3_samplerate(fscod);
		numblkscod = pi_blocks[numblkscod];
	}
	acmod = getbits(p, bitcnt, 3);
	bitcnt += 3;
	lfeon = getbits(p, bitcnt++, 1);
	bsid = getbits(p, bitcnt, 5);

	wf->wFormatTag = AUDIO_A52;
	wf->nChannels = get_ac3_channel(acmod) + lfeon;
	wf->nAvgBytesPerSec = size * wf->nSamplesPerSec / (numblkscod * 256);

	if ( (wf->nChannels == 0) || (wf->nSamplesPerSec == 0) || (wf->nAvgBytesPerSec == 0) )
	{
		return nRet;
	}

	mp_msg("EAC-3: channel: %d , samplerate: %d, bitrate: %d, lfe: %d\n",
			wf->nChannels, wf->nSamplesPerSec, wf->nAvgBytesPerSec * 8, lfeon);
	nRet = 1;
	return nRet;
}

int mp_a52_header(WAVEFORMATEX * wf, unsigned char *p, int packet_len)
{
	int cnt = 0;
	int bsid;
	for (cnt = 0; cnt < packet_len - 7; cnt++)
	{
		if(p[cnt] == 0x0B && p[cnt+1] == 0x77)
		{
			bsid = p[cnt+5] >> 3;
			if (bsid > 16)
				continue;

			if (bsid <= 10)
			{
				if (mp_a52_header_parse_ac3(wf, p + cnt, packet_len - cnt) == 1)
					return 1;
			}
			else
			{
				if (mp_a52_header_parse_eac3(wf, p + cnt, packet_len - cnt) == 1)
					return 1;
			}
		}
	}
	return 0;
}

int probe_audio_type(unsigned char *p, int packet_len, int * offset, es_stream_type_t * type)
{
	int nRet = 0;
	int bitrate = 0, samplerate = 0, channel = 0;
	*offset = 0;
	AAC_INFO aacinfo;

	mp_msg("probe audio type buf = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

	if (mp_get_mp3_header(p, packet_len, &channel, &samplerate, &bitrate) != -1) 
	{
		*type = AUDIO_MP2;
		nRet = 1;
	}
	else if (mp_get_aac_header(p, packet_len, &aacinfo) >= 0)
	{
		*type = AUDIO_AAC;
		nRet = 1;
	}
	if (nRet == 1)
	{
		mp_msg("Get probe audio type %08x\n", *type);
	}

	return nRet;
}

int probe_video_type(unsigned char *p, int packet_len, int * offset, es_stream_type_t * type)
{
	int nRet = 0, ii;
	*offset = 0;
	int check_size = 30;
	unsigned int mp2_frame_type, mp2_f_code;
	mp_msg("probe video len: %d type buf = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			packet_len, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	if (packet_len < 20)
		return nRet;
	if (check_size > packet_len - 4)
		check_size = packet_len - 4;
	/* need improve here */
	for (ii = 0; ii < check_size; ii++)
	{
		if (p[ii] != 0)
			break;
		if ((p[ii] == 0) && (p[ii+1] == 0) && (p[ii+2] == 0x01) && (p[ii+3] == 0xb3))
		{
			if (p[ii+10] & 0x20)
			{
				*type = VIDEO_MPEG2;
				nRet = 1;
			}
		}
		if ((p[ii] == 0) && (p[ii+1] == 0) && (p[ii+2] == 0x01) && (p[ii+3] == 0))
		{
			// picture header
			mp2_frame_type = (p[ii+5] >> 3) & 0x7;
			//mp_msg("mp2_frame_type: %x\n", mp2_frame_type);
			if ((mp2_frame_type == 2) || (mp2_frame_type == 3))
			{
				mp2_f_code = ((p[ii+7] & 0x7) << 1) | (p[ii+8] >> 7);
				//mp_msg("mp2_f_code: %x\n", mp2_f_code);
				if (mp2_f_code == 0x7)
				{
					*type = VIDEO_MPEG2;
					nRet = 1;
				}
			}
		}
		if((p[ii] == 0) && (p[ii+1] == 0) && (p[ii+2] == 0) && (p[ii+3] ==0x01) && ((p[ii+4] >> 7) == 0) && (((p[4] & 0x1f) == 7) || ((p[4] & 0x1f) == 9)))
		{
			*type = VIDEO_H264;
			nRet = 1;
		}
		if((p[ii] == 0x0) && (p[ii+1] == 0x0) && (p[ii+2] == 0x01) && (p[ii+3] ==0x0f))
		{
			*type = VIDEO_VC1;
			nRet = 1;
		}
		if (nRet == 1)
		{
			mp_msg("Get probe video type %08x\n", *type);
			*offset = ii;
			break;
		}
	}
	return nRet;
}

/*
 * ret : -2 : not get pts time info
 *       -1 : not get pts info, is skip type
 *        0 : get pts info, but type not support
 *        1 : support media type
 */
static int pes_parse2(unsigned char *buf, uint16_t packet_len, ES_stream_t *es, int32_t type_from_pmt, pmt_t *pmt, int pid, ES_stream_t *tss)
{
	unsigned char  *p;
	uint32_t       header_len;
	int64_t        pts;
	uint32_t       stream_id;
	uint32_t       pkt_len, pes_is_aligned;

	//Here we are always at the start of a PES packet
	mp_msg("pes_parse2(%p, %d): \n", buf, (uint32_t) packet_len);

	if(packet_len == 0 || packet_len > TS_FEC_PACKET_SIZE - 4)
	{
		mp_msg("pes_parse2, BUFFER LEN IS TOO SMALL OR TOO BIG: %d EXIT\n", packet_len);
		return -2;
	}

	p = buf;
	pkt_len = packet_len;


	mp_msg("pes_parse2: HEADER %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
	if (p[0] || p[1] || (p[2] != 1))
	{
		mp_msg("pes_parse2: error HEADER %02x %02x %02x (should be 0x000001) \n", p[0], p[1], p[2]);
		return -2;
	}

	packet_len -= 6;
	if(packet_len==0)
	{
		mp_msg("pes_parse2: packet too short: %d, exit\n", packet_len);
		return -2;
	}

	es->payload_size = (p[4] << 8 | p[5]);
	pes_is_aligned = (p[6] & 4);

	stream_id  = p[3];

	if ((stream_id == H264_STREAM_PROGRAM_STREAM_MAP) || 
			(stream_id == H264_STREAM_PADDING_STREAM) || 
			(stream_id == H264_STREAM_PRIVATE_STREAM_2) || 
			(stream_id == H264_STREAM_ECM) || 
			(stream_id == H264_STREAM_EMM) || 
			(stream_id == H264_STREAM_PROGRAM_STREAM_DIRECTORY) || 
			(stream_id == H264_STREAM_DSMCC_STREAM) || 
			(stream_id == H264_STREAM_H222_1_E)
			)
	{
		return -1;
	}

	if (p[7] & 0x80)
	{ 	/* pts available */
		pts  = (int64_t)(p[9] & 0x0E) << 29 ;
		pts |=  p[10]         << 22 ;
		pts |= (p[11] & 0xFE) << 14 ;
		pts |=  p[12]         <<  7 ;
		pts |= (p[13] & 0xFE) >>  1 ;

		es->pts = pts / 90000.0f;
	}
	else
		es->pts = 0.0f;

	header_len = p[8];


	if (header_len + 9 > pkt_len) //9 are the bytes read up to the header_length field
	{
		mp_msg("demux_ts: illegal value for PES_header_data_length (0x%02x)\n", header_len);
		return -2;
	}

	// reserved data stream
	if(stream_id==0xfd)
	{
		int ssid = parse_pes_extension_fields(p, pkt_len);
		//if((audio_substream_id!=-1) && (ssid != audio_substream_id))
		//	return 0;
		if(ssid == 0x72 && (type_from_pmt != AUDIO_DTS) && (type_from_pmt != AUDIO_EAC3) &&
			(type_from_pmt != SPU_PGS))
		{
			es->type  = AUDIO_TRUEHD;
			type_from_pmt = AUDIO_TRUEHD;
		}
	}

	p += header_len + 9;
	packet_len -= header_len + 3;

	if(es->payload_size)
		es->payload_size -= header_len + 3;

	es->is_synced = 1;	//only for SL streams we have to make sure it's really true, see below
	if (stream_id == 0xbd)
	{
		mp_msg("pes_parse2: audio buf = %02X %02X %02X %02X %02X %02X %02X %02X, 80: %d\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[0] & 0x80);
		/*
		* we check the descriptor tag first because some stations
		* do not include any of the A52 header info in their audio tracks
		* these "raw" streams may begin with a byte that looks like a stream type.
		*/

		if ( (type_from_pmt == AUDIO_DTS) ||
				((p[0] == 0x7f) && (p[1] == 0xfe) && (p[2] == 0x80) && (p[3] == 0x01)) )
		{
			mp_msg("DTS\n");
			es->start = p;
			es->size  = packet_len;
			es->type  = AUDIO_DTS;
			es->payload_size -= packet_len;
		}
		else if(
			(type_from_pmt == AUDIO_A52) ||		 /* A52 - raw */
			(p[0] == 0x0B && p[1] == 0x77)		/* A52 - syncword */
		)
		{
			mp_msg("A52 RAW OR SYNCWORD\n");
			es->start = p;
			es->size  = packet_len;
			es->type  = AUDIO_A52;
			es->payload_size -= packet_len;
		}
		/* SPU SUBS */
		else if(type_from_pmt == SPU_DVB ||
		((p[0] == 0x20) && pes_is_aligned)) // && p[1] == 0x00))
		{
			es->start = p;
			es->size  = packet_len;
			es->type  = SPU_DVB;
			es->payload_size -= packet_len;
		}
		else if (pes_is_aligned && ((p[0] & 0xE0) == 0x20))	//SPU_DVD
		{
			//DVD SUBS
			es->start   = p+1;
			es->size    = packet_len-1;
			es->type    = SPU_DVD;
			es->payload_size -= packet_len;
		}
		else if (pes_is_aligned && (p[0] & 0xF8) == 0x80)
		{
			mp_msg("A52 WITH HEADER\n");
			es->start   = p+4;
			es->size    = packet_len - 4;
			es->type    = AUDIO_A52;
			es->payload_size -= packet_len;
		}
		else if (pes_is_aligned && ((p[0]&0xf0) == 0xa0))
		{
			int pcm_offset;

			for (pcm_offset=0; ++pcm_offset < packet_len-1 ; )
			{
				if (p[pcm_offset] == 0x01 && p[pcm_offset+1] == 0x80)
				{ 	/* START */
					pcm_offset += 2;
					break;
				}
			}

			es->start   = p + pcm_offset;
			es->size    = packet_len - pcm_offset;
			es->type    = AUDIO_LPCM_BE;
			es->payload_size -= packet_len;
		}
		else
		{
			mp_msg("PES_PRIVATE1\n");
			es->start   = p;
			es->size    = packet_len;
			es->type    = (es_stream_type_t)(type_from_pmt == UNKNOWN ? PES_PRIVATE1 : type_from_pmt);
			es->payload_size -= packet_len;
		}
	}
	else if(((stream_id >= 0xe0) && (stream_id <= 0xef)) || (stream_id == 0xfd && type_from_pmt != UNKNOWN))
	{
		if(type_from_pmt != UNKNOWN)
		    es->type    = (es_stream_type_t)type_from_pmt;
		else if (tss->type != UNKNOWN)
		{
		    es->type    = tss->type;
		}
		else
		{
			int offset;
			if (probe_video_type(p, packet_len, &offset, &es->type) == 1)
			{
				p += offset;
				packet_len -= offset;
			}
		}
		es->start   = p;
		es->size    = packet_len;

		if(es->payload_size)
			es->payload_size -= packet_len;

		mp_msg("pes_parse2: M2V size %d\n", es->size);
	}
	else if ((stream_id == 0xfa))
	{
		int l;

		es->is_synced = 0;
		if(type_from_pmt != UNKNOWN)	//MP4 A/V or SL
		{
			es->start   = p;
			es->size    = packet_len;
			es->type    = (es_stream_type_t)type_from_pmt;

			if(type_from_pmt == SL_PES_STREAM)
			{
				//if(pes_is_aligned)
				//{
					l = mp4_parse_sl_packet(pmt, p, packet_len, pid, es);
					mp_msg("L=%d, TYPE=%x\n", l, type_from_pmt);
					if(l < 0)
					{
						mp_msg("pes_parse2: couldn't parse SL header, passing along full PES payload\n");
						l = 0;
					}
				//}

				es->start   += l;
				es->size    -= l;
			}

			if(es->payload_size)
				es->payload_size -= packet_len;
			return 0;
		}
	}
	else if ((stream_id & 0xe0) == 0xc0)
	{
		es->start   = p;
		es->size    = packet_len;

		if(type_from_pmt != UNKNOWN)
		{
			es->type = (es_stream_type_t)type_from_pmt;
		}
		else if (tss->type != UNKNOWN)
		{
		    es->type    = tss->type;
		}
		else
		{
			int offset;
			if (probe_audio_type(p, packet_len, &offset, &es->type) == 1)
			{
				p += offset;
				packet_len -= offset;
			}
		}
		es->payload_size -= packet_len;
	}
	else if (type_from_pmt != -1)	//as a last resort here we trust the PMT, if present
	{
		es->start   = p;
		es->size    = packet_len;
		es->type    = (es_stream_type_t)type_from_pmt;
		es->payload_size -= packet_len;
	}
	else
	{
		mp_msg("pes_parse2: unknown packet, id: %x\n", stream_id);
	}

	if ((es->type != UNKNOWN) && (tss->mediainfo != NULL))
	{
		return 1;
	}

	es->is_synced = 0;
	return 0;
}

int mp_lpcm_header(WAVEFORMATEX * wf, unsigned char *p, int packet_len)
{
	int nRet = 0;
	int tmp, frm_cnt, first_aac_unit, frm_num, quantization, sample_idx, chan_num;
	BitData bf;

	if ((packet_len < 6) && (wf == NULL))
		return nRet;

	InitGetBits(&bf, p, packet_len);
	frm_cnt = GetBits(&bf, 8);
	first_aac_unit = GetBits(&bf, 16);
	tmp = GetBits(&bf, 3);
	frm_num = GetBits(&bf, 5);
	quantization = GetBits(&bf, 2);
	sample_idx = GetBits(&bf, 2);
	tmp = GetBits(&bf, 1);
	chan_num = GetBits(&bf, 3);

	// check value range
	if (quantization == 3)
	{
		//printf("LPCM: illegal quantization value\n");
		return nRet;
	}

	wf->nChannels = chan_num + 1;
	wf->nSamplesPerSec = (quantization == 0)? 48000: 96000;
	wf->wBitsPerSample = 16 + (quantization * 4);
	wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->wBitsPerSample * wf->nChannels / 8;

	//printf("LPCM channel: %d, sample rate: %d, bitrate: %d\n", wf->nChannels,
	//		wf->nSamplesPerSec, wf->wBitsPerSample);

	nRet = 1;
	return nRet;
}

static int seek_header(unsigned char *p, uint32_t packet_len, int * offset)
{
	int nRet = 0;
	int ii;
	char header[4] = {0x0, 0x0, 0x0, 0x1};

	if ((p == NULL) || (packet_len < 4) || (offset == NULL))
		return nRet;
	for (ii = 0; ii <= (packet_len-4); ii++)
	{
		//if (*((int *)(p+ii)) == *((int *)header))
		if (p[ii]==header[0] && p[ii+1]==header[1] && p[ii+2]==header[2] && p[ii+3]==header[3])
		{
			nRet = 1;
			*offset = ii;
			break;
		}
	}
	return nRet;
}

static unsigned char * skip_zero(unsigned char *p, uint32_t *packet_len)
{
	int ii;

	if ((p == NULL) || (packet_len == NULL) || (*packet_len <= 4))
		return p;
	for (ii = 0; ii < (*packet_len-4); ii++)
	{
		if (ii == 5)
			break;
		if (*((int *)p) == 0)
		{
			p++;
			*packet_len -= 1;
		}
		else
			break;
	}
	return p;
}

static int info_parse(ES_stream_t *es, int pid, ES_stream_t *tss)
{
	unsigned char  *p;
	int nRet = 0;
	uint32_t       packet_len;
	unsigned char buf[256];
	WAVEFORMATEX wf, *pwf = NULL;
	TSSTREAMINFO *video_info;

	if((es == NULL) || (tss == NULL))
	{
		return -1;
	}

	p = es->start;
	packet_len = es->size;

	//mp_msg("data buf = %02X %02X %02X %02X %02X %02X %02X %02X\n",
	//		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

	switch(es->type)
	{
		case AUDIO_EAC3:
		case AUDIO_A52:
			{
				if (mp_a52_header(&wf, p, packet_len) == 1)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
					}
					memcpy(tss->mediainfo, (const void *)(&wf), sizeof(WAVEFORMATEX));
					nRet = 1;
				}
				break;
			}
		case AUDIO_DTS:
			{
				int bitrate = 0, samplerate = 0, channel = 0;
				if (mp_get_dts_header(p, packet_len, &channel, &samplerate, &bitrate) == 1)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
					}
					pwf = (WAVEFORMATEX *)tss->mediainfo;
					pwf->wFormatTag = AUDIO_DTS;
					pwf->nChannels = channel;
					pwf->nSamplesPerSec = samplerate;
					pwf->nAvgBytesPerSec = bitrate / 8;
					nRet = 1;
				}
				break;
			}
		case AUDIO_BPCM:
			{
				static const uint32_t channels[16] = {
					0, 1, 0, 2, 3, 3, 4, 4, 5, 6, 7, 8, 0, 0, 0, 0
				};
				static const uint32_t bits_per_samples[4] = { 0, 16, 20, 24 };
				int samplerate = 0, channel;

				mp_msg("pid: %d, PCM audio buf = %02X %02X %02X %02X %02X %02X %02X %02X %02X, len: %d\n", pid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], packet_len);

				uint32_t channel_layout = p[2] >> 4;
				uint32_t sample_depth = bits_per_samples[p[3] >> 6];
				channel = (int)channels[channel_layout];

				switch (p[2] & 0x0f)
				{
					case 1:
						samplerate = 48000;
						break;
					case 4:
						samplerate = 96000;
						break;
					case 5:
						samplerate = 192000;
						break;
					default:
						break;
				}

				if ((sample_depth == 0) || (samplerate == 0) || (channel == 0))
					break;

				if(tss->mediainfo == NULL) {
					tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
				}
				pwf = (WAVEFORMATEX *)tss->mediainfo;
				//pwf->wFormatTag = AUDIO_BPCM;
				pwf->nChannels = channel;
				pwf->nSamplesPerSec = samplerate;
				pwf->nAvgBytesPerSec = channel * samplerate * sample_depth / 8;

				mp_msg("PCM audio: bitrate: %d(kbps), " \
						"sample rate: %d(Hz), channel: %d\n", 
						pwf->nAvgBytesPerSec * 8 / 1024, samplerate, channel
					  );

				nRet = 1;
				break;
			}
		case AUDIO_MP2:
			{
				mp_msg("pid: %d, MP2 audio buf = %02X %02X %02X %02X %02X %02X %02X %02X %02X, len: %d\n", pid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], packet_len);
				int bitrate = 0, samplerate = 0, channel = 0;
				if (mp_get_mp3_header(p, packet_len, &channel, &samplerate, &bitrate) != -1) 
				{
					mp_msg("MPEG audio: bitrate: %d(kbps), " \
							"sample rate: %d(Hz), channel: %d\n", 
							bitrate, samplerate, channel
						  );

					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
					}
					pwf = (WAVEFORMATEX *)tss->mediainfo;
					pwf->wFormatTag = AUDIO_MP2;
					pwf->nChannels = channel;
					pwf->nSamplesPerSec = samplerate;
					pwf->nAvgBytesPerSec = bitrate / 8;

					nRet = 1;
				}
				break;
			}
		case AUDIO_AAC:
			{
				mp_msg("AAC audio buf = %02X %02X %02X %02X %02X %02X %02X %02X\n",
						p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
				AAC_INFO aacinfo;
				if (mp_get_aac_header(p, packet_len, &aacinfo) >= 0)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
					}
					pwf = (WAVEFORMATEX *)tss->mediainfo;
					pwf->nSamplesPerSec = aacinfo.nSamplingFreq;
					pwf->nChannels = aacinfo.nChannels;
					mp_msg("AAC sample rate: %d, channel: %d\n", pwf->nSamplesPerSec, pwf->nChannels);

					nRet = 1;
				}
				break;
			}
		case AUDIO_LPCM_BE:
			{
				if (mp_lpcm_header(&wf, p, packet_len) == 1)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(WAVEFORMATEX));
					}
					memcpy(tss->mediainfo, (const void *)(&wf), sizeof(WAVEFORMATEX));
					nRet = 1;
				}
				break;
			}
		case VIDEO_MPEG2:
			{
				mp_mpeg_header_t picture;
				memset((void *)&picture, 0, sizeof(mp_mpeg_header_t));
				//mp_msg("type: %02X video buf = %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", es->type, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
				if (mp_get_mp2_header(p, packet_len, &picture) == 1)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(TSSTREAMINFO));
					}
					// need to free later
					video_info = (TSSTREAMINFO*)tss->mediainfo;
					video_info->width = picture.display_picture_width;
					video_info->height = picture.display_picture_height;
					video_info->fps = picture.fps;
					video_info->bitrate = picture.bitrate;
					tss->type = VIDEO_MPEG2;

					mp_msg("MPEG2 width: %d height: %d, frame_rate: %2.2f, bit_rate: %u\n",
							video_info->width, video_info->height, video_info->fps, video_info->bitrate/1000);
					nRet = 1;
				}
				break;
			}
		case VIDEO_H264:
			{
				if (packet_len <= 10)
					break;

				if((p[0] == 0) && (p[1] == 0) && (p[2] == 0))
				{
					int nal_type = 0;
					int off_size;
					int ii = 0;
					while (seek_header(p+ii, packet_len-ii, &off_size) == 1)
					{
						ii += off_size;
						if ((p[ii + 4] >> 7) == 0)
						{
							nal_type = (p[ii + 4] & 0x1f);
							//printf("h264 nal_type: %d\n", nal_type);
							if (nal_type == 7)
							{
								p += ii;
								packet_len -= ii;
								break;
							}
						}
						ii += 4;
					}
					// get seq_parameter_set_rbsp()
					if (nal_type == 7)
					{
						//mp_msg("H264 video buf = %02X %02X %02X %02X %02X %02X %02X %02X\n",
						//		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
						p += 5;
						packet_len -= 5;
						memcpy(buf, p, packet_len);
						packet_len = mp_unescape03(buf, packet_len);
						if(tss->mediainfo == NULL) {
							tss->mediainfo = calloc(1, sizeof(TSSTREAMINFO));
						}

						// need to free later
						video_info = (TSSTREAMINFO *)tss->mediainfo;
						if (h264_parse_sps(video_info, buf) == 0)
						{
							tss->hw_notsupport = 1;
						}
						mp_msg("H264 width: %d height: %d, frame_rate: %2.2f, bit_rate: %u\n",
								video_info->width, video_info->height, video_info->fps, video_info->bitrate/1000);

						tss->type = VIDEO_H264;
						nRet = 1;
					}
				}
				break;
			}
		case VIDEO_VC1:
			{
				mp_mpeg_header_t picture;
				memset(&picture, 0, sizeof(mp_mpeg_header_t));
				//mp_msg("type: %02X video buf = %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", es->type, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
				if (mp_vc1_decode_sequence_header(&picture, p, packet_len) == 1)
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(TSSTREAMINFO));
					}
					// need to free later
					video_info = (TSSTREAMINFO*)tss->mediainfo;
					video_info->width = picture.display_picture_width;
					video_info->height = picture.display_picture_height;
					video_info->fps = picture.fps;
					video_info->bitrate = picture.bitrate;
					tss->type = VIDEO_VC1;

					mp_msg("VC1 width: %d height: %d, frame_rate: %2.2f, bit_rate: %u\n",
							video_info->width, video_info->height, video_info->fps, video_info->bitrate/1000);
					nRet = 1;
				}
				break;
			}
		case VIDEO_MPEG4:
			{
				mp_mpeg_header_t picture;
				memset((void *)&picture, 0, sizeof(mp_mpeg_header_t));
				if (mp_get_mp4_header(p, packet_len, &picture))
				{
					if(tss->mediainfo == NULL) {
						tss->mediainfo = calloc(1, sizeof(TSSTREAMINFO));
					}
					// need to free later
					video_info = (TSSTREAMINFO*)tss->mediainfo;
					video_info->width = picture.display_picture_width;
					video_info->height = picture.display_picture_height;
					video_info->fps = picture.fps;
					video_info->bitrate = picture.bitrate;
					tss->type = VIDEO_MPEG4;

					mp_msg("MPEG4 width: %d height: %d, frame_rate: %2.2f, bit_rate: %u\n",
							video_info->width, video_info->height, video_info->fps, video_info->bitrate/1000);
					nRet = 1;
				}
				break;
			}	
		case SPU_DVD:
		case SPU_DVB:
		case SPU_TELETEXT:
		case SPU_PGS:
			nRet = 1;
			tss->is_synced = 1;
			break;
		default:
			{
				mp_msg("info parse unknown pid %d type: %08x\n", pid, es->type);
				break;
			}
	}

	return nRet;
}

static inline uint8_t *pid_lang_from_pmt(ts_priv_t *priv, int pid)
{
	int32_t pmt_idx, pid_idx, i, j;

	pmt_idx = progid_idx_in_pmt(priv, priv->prog);

	if(pmt_idx != -1)
	{
		pid_idx = es_pid_in_pmt(&(priv->pmt[pmt_idx]), pid);
		if(pid_idx != -1)
			return priv->pmt[pmt_idx].es[pid_idx].lang;
	}
	else
	{
		for(i = 0; i < priv->pmt_cnt; i++)
		{
			pmt_t *pmt = &(priv->pmt[i]);
			for(j = 0; j < pmt->es_cnt; j++)
				if(pmt->es[j].pid == pid)
					return pmt->es[j].lang;
		}
	}

	return NULL;
}

static void ts_add_stream(FileInfo *finfo, ES_stream_t *es)
{
	ts_priv_t *priv = (ts_priv_t*) finfo->priv;

	//if(priv->ts.streams[es->pid].sh)
	//	return;

	if((IS_AUDIO(es->type) || IS_AUDIO(es->subtype)) && priv->last_aid+1 < MAX_A_STREAMS)
	{
/*
		sh_audio_t *sh = new_sh_audio_aid(demuxer, priv->last_aid, es->pid);
		if(sh)
		{
*/
			const char *lang = (const char *)pid_lang_from_pmt(priv, es->pid);
			//sh->needs_parsing = 1;
			//sh->format = IS_AUDIO(es->type) ? es->type : es->subtype;
			//sh->ds = demuxer->audio;

			priv->ts.streams[es->pid].id = priv->last_aid;
			//priv->ts.streams[es->pid].sh = sh;
			//priv->ts.streams[es->pid].type = TYPE_AUDIO;
			priv->ts.streams[es->pid].type = es->type;
			mp_msg("ADDED AUDIO PID %d, type: %x , subtype: %x, stream n. %d\n", es->pid, es->type, es->subtype, priv->last_aid);
			if (lang && lang[0])
			{
				mp_msg("ID_AID_%d_LANG=%s\n", es->pid, lang);
			}
			priv->last_aid++;
//		}

/*
		if(es->extradata && es->extradata_len)
		{
			sh->wf = malloc(sizeof (WAVEFORMATEX) + es->extradata_len);
			sh->wf->cbSize = es->extradata_len;
			memcpy(sh->wf + 1, es->extradata, es->extradata_len);
		}
*/
	}

	if((IS_VIDEO(es->type) || IS_VIDEO(es->subtype)) && priv->last_vid+1 < MAX_V_STREAMS)
	{
/*
		sh_video_t *sh = new_sh_video_vid(demuxer, priv->last_vid, es->pid);
		if(sh)
		{
			sh->format = IS_VIDEO(es->type) ? es->type : es->subtype;
			sh->ds = demuxer->video;
*/

			priv->ts.streams[es->pid].id = priv->last_vid;
			//priv->ts.streams[es->pid].sh = sh;
			priv->ts.streams[es->pid].type = es->type;
			mp_msg("ADDED VIDEO PID %d, type: %x stream n. %d\n", es->pid, es->type, priv->last_vid);
			priv->last_vid++;


/*
			if(sh->format == VIDEO_AVC && es->extradata && es->extradata_len)
			{
				int w = 0, h = 0;
				sh->bih = calloc(1, sizeof(BITMAPINFOHEADER) + es->extradata_len);
				sh->bih->biSize= sizeof(BITMAPINFOHEADER) + es->extradata_len;
				sh->bih->biCompression = sh->format;
				memcpy(sh->bih + 1, es->extradata, es->extradata_len);
				mp_msg(MSGT_DEMUXER,MSGL_DBG2, "EXTRADATA(%d BYTES): \n", es->extradata_len);
				for(i = 0;i < es->extradata_len; i++)
					mp_msg(MSGT_DEMUXER,MSGL_DBG2, "%02x ", (int) es->extradata[i]);
				mp_msg(MSGT_DEMUXER,MSGL_DBG2,"\n");
				if(parse_avc_sps(es->extradata, es->extradata_len, &w, &h))
				{
					sh->bih->biWidth = w;
					sh->bih->biHeight = h;
				}
			}
		}
*/
	}
}

static int get_pe_synced_num(ts_priv_t *priv, int **datary)
{
	int nRet = 0;
	int ii = 0, jj = 0, cnt = 0;
	int *ppidary = NULL;
	int malsize = 10;
	int skip = 0;

	ES_stream_t *tss;

	if (priv == NULL)
		return nRet;
	ppidary = (int *)malloc(sizeof(int) * malsize);
	for (ii = 0; ii < NB_PID_MAX; ii++)
	{
		skip = 0;
		tss = priv->ts.pids[ii];
		if ((tss == NULL) || (tss->pid == 0))
			continue;
		for (jj = 0; jj < priv->pat.progs_cnt; jj++)
		{
			if (priv->pat.progs[jj].pmt_pid == tss->pid)
			{
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;
		if (tss->is_synced == 1)
		{
			if (cnt == malsize - 1)
			{
				malsize += 10;
				ppidary = (int *)realloc(ppidary, sizeof(int) * malsize);
			}
			ppidary[cnt++] = tss->pid;
		}
	}

	if (cnt > 0)
	{
		nRet = cnt;
		*datary = ppidary;
	} else {
		if (ppidary)
			free(ppidary);
		*datary = NULL;
	}

	return nRet;
}

/*
 * return value
 * 0 : not all pid is synced
 * 1 : all pid is synced
 * 2 : all audio/video is encrypted, no need to check
 */
static int check_all_synced(ts_priv_t *priv)
{
	int nRet = 0;
	int ii, jj;
	int pes_pid, type_from_pmt;
	int all_encrypt = 0;
	int is_audio, is_video;
	pmt_t *pmt;
	ES_stream_t *tss;

	if(priv->pat.progs == NULL || priv->pmt == NULL || priv->pmt_cnt == 0 ||
			priv->pat.progs_cnt == 0)
	{
		return nRet;
	}

	/*
	// check pmt synced
	int pmt_pid
	for(ii = 0; ii < priv->pat.progs_cnt; ii++)
	{
		pmt_pid = priv->pat.progs[ii].pmt_pid;
		tss = priv->ts.pids[pmt_pid];
		if ((tss == NULL) || (tss->is_synced == 0))
		{
			//mp_msg("pmt id %d not synced\n", pmt_pid);
			return nRet;
		}
	}
	mp_msg("all pmt is synced\n");
	*/

	for(ii = 0; ii < priv->pmt_cnt; ii++)
	{
		pmt = &(priv->pmt[ii]);

		if (pmt->encrypt == 0)
		{
			for(jj = 0; jj < pmt->es_cnt; jj++)
			{
				if (pmt->es[jj].encrypt == 0)
				{
					pes_pid = pmt->es[jj].pid;
					type_from_pmt = pmt->es[jj].type;
					if (IS_SUB(type_from_pmt) == 1)
						continue;
					tss = priv->ts.pids[pes_pid];
					if ((tss == NULL) || ((type_from_pmt != UNKNOWN) && (tss->is_synced == 0)))
					{
						//mp_msg("pid %d is not synced\n", pes_pid);
						return nRet;
					}
				}
			}
		}
	}
	//mp_msg("all pid is synced\n");

	// if all program is encrypted, then no need to parse
	if ((priv->last_aid == 0) && (priv->last_vid == 0))
	{
		for(ii = 0; ii < priv->pmt_cnt; ii++)
		{
			pmt = &(priv->pmt[ii]);
			// check if all program is encrypted
			if (pmt->encrypt == 0)
			{
				//mp_msg("PMT pid: %d not encrypted\n", pmt->progid);
				for(jj = 0; jj < pmt->es_cnt; jj++)
				{
					if (pmt->es[jj].encrypt == 0)
					{
						pes_pid = pmt->es[jj].pid;
						type_from_pmt = pmt->es[jj].type;
						is_video = IS_VIDEO(type_from_pmt);
						is_audio = IS_AUDIO(type_from_pmt);
						if (is_video || is_audio)
						{
							//mp_msg("ES pid: %d , type: %d not encrypted\n", pes_pid, type_from_pmt);
							all_encrypt = 0;
							goto sync_out;
						}
					}
					else
					{
						all_encrypt++;
					}
				}
			}
			else
			{
				all_encrypt++;
			}
		}
		if((priv->pmt_cnt > 0) && (all_encrypt >= 1))
		{
			return 2;
		}
	}

	// required at least one video or audio
	if((priv->pmt_cnt > 0) && (all_encrypt == 0))
		return nRet;

sync_out:
	nRet = 1;
	return nRet;
}

static int ts_pts_parse(FILE *fp, FileInfo *finfo)
{
	int nRet = 0, ii;
	ts_priv_t *priv;
	static int bufsize = 256;
	unsigned char buf[bufsize], syncflag = 0;
	int ts_error, payload_start, pid, afc;
	int *pidary = NULL, synced_num, left_num;
	ES_stream_t *tss = NULL;
	LOFF_T pos = 0, next_pos, adap_end_pos, max_pos;
	LOFF_T max_search_size = 2*1024*1024;
	int syncnum = 0;
	int use_fread = 1;
	int fd = fileno(fp);
	double max_pts = 0, tmp_pts;
#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
		use_fread = 0;
#endif

	if ((fp == NULL) || (finfo == NULL))
		return nRet;

	priv = (ts_priv_t*)finfo->priv;
	if ((priv == NULL) || (finfo->FileSize < bufsize))
		return nRet;

	//fseek(fp, 0, SEEK_END);

	left_num = synced_num = get_pe_synced_num(priv, &pidary);
	pos = finfo->FileSize;
	if (max_search_size > finfo->FileSize)
		max_search_size = finfo->FileSize / 2;
	max_pos = pos - max_search_size;
	//mp_msg("left_num: %d, max_pos: %llu, file size: %llu\n", left_num, max_pos, pos);
	for(ii=0; ii<left_num; ii++)
	{
		mp_msg("pidary: %d\n", pidary[ii]);
	}

try_prv_sync:
	syncnum++;
	// find sync bit 0x47
	pos -= priv->ts.packet_size;
	//pos -= bufsize;
	if (pos <=0)
		goto ts_pts_failed;
	if (max_pos > pos)
	{
		mp_msg("reach max search size %dM\n", (int)max_search_size/1024/1024);
		goto ts_pts_failed;
	}
	if (use_fread == 0) {
		if (lseek64(fd, pos, SEEK_SET) == -1)
		{
			mp_msg("seek error!!!");
			goto ts_pts_failed;
		}
	} else {
		if (fseeko(fp, pos, SEEK_SET) == -1)
		{
			mp_msg("seek error!!!");
			goto ts_pts_failed;
		}
	}
	
	/*
	read_data(fp, (void *)&buf, bufsize);

	syncflag = 0;
	for (ii = 0; ii < bufsize - 14; ii++)
	{
		if (buf[ii]== 0x47)
		{
			syncflag = buf[ii];
			pos += ii+1;
			mp_msg("synced at %llu %llu %llu\n", pos, ppos, ppos-pos);
			ppos = pos;
			fseeko(fp, pos, SEEK_SET);
			break;
		}
	}
	if (syncflag != 0x47) {
		//mp_msg("TS_PARSE: END COULDN'T SYNC at %llu\n", pos);
		goto try_prv_sync;
	}

	next_pos = pos + priv->ts.packet_size - 1;
	if (next_pos > finfo->FileSize)
	{
		mp_msg("at end\n");
		goto try_prv_sync;
	}
	*/

	// check es header
	if (use_fread == 0) {
		read(fd, buf, 4);
	} else {
		read_nbytes(buf, 4, 1, fp);
		if (file_error == 1)
			goto ts_pts_failed;
	}
	if (buf[0] != 0x47) {
		mp_msg("TS_PARSE: COULDN'T SYNC at %llu(%08x %08x)\n", pos, (unsigned int)(pos>>32), (unsigned int)(pos&0xffffffff));
		//fseeko(fp, -4, SEEK_CUR);

		syncflag = 0;
		while (syncflag != 0x47)
		{
			if (use_fread == 0) {
				lseek64(fd, pos, SEEK_SET);
				read(fd, buf, priv->ts.packet_size);
			} else {
				fseeko(fp, pos, SEEK_SET);
				read_data(fp, (void *)&buf, priv->ts.packet_size);
			}
			for (ii = 0; ii < priv->ts.packet_size; ii++)
			{
				if (buf[ii]== 0x47)
				{
					syncflag = buf[ii];
					pos += ii;
					mp_msg("synced at %llu, %d\n", pos, ii);
					if (use_fread == 0) {
						lseek64(fd, pos, SEEK_SET);
					} else {
						fseeko(fp, pos, SEEK_SET);
					}
					break;
				}
			}
			if (syncflag != 0x47) {
				//mp_msg("TS_PARSE: COULDN'T FIND SYNC at %llu\n", pos);
				pos -= priv->ts.packet_size;
				if (max_pos > pos)
				{
					mp_msg("aa reach max search size %dM\n", (int)max_search_size/1024/1024);
					goto ts_pts_failed;
				}
			}
		}
	}
	next_pos = pos + priv->ts.packet_size;

	ts_error = 0;
	if ((buf[1] >> 7) & 0x01)
		ts_error = 1;
	payload_start = (buf[1] >> 6) & 0x01;
	pid = ((buf[1] & 0x1f) << 8) | (buf[2] & 0xff);
	//printf ("pid: %d %d %d\n", pid, ts_error, payload_start);

	tss = priv->ts.pids[pid];                       //an ES stream
	if((ts_error != 1) && (payload_start == 1) && (tss != NULL) && (tss->is_synced == 1))
	//if((payload_start == 1) && (tss != NULL) && (tss->is_synced == 1))
	{
		for(ii = 0; ii < synced_num; ii++)
		{
			if (pid == pidary[ii])
			{
				int64_t pts;
				//---------------------
				afc = (buf[3] >> 4) & 0x03;
				// handle adaptation field
				if (afc > 1) {
					int adapt_field_size;
					if (use_fread == 0) {
						adapt_field_size = read_char_a(fd);
					} else {
						adapt_field_size = read_char(fp);
					}
					if (adapt_field_size < 0 || adapt_field_size > priv->ts.packet_size - 5) {
						mp_msg("wrong adaptation field size %d!!!\n", adapt_field_size);
						goto try_prv_sync;
					}
					if (use_fread == 0) {
						adap_end_pos = lseek64(fd, 0, SEEK_CUR) + adapt_field_size;
						lseek64(fd, adap_end_pos, SEEK_SET);
					} else {
						adap_end_pos = ftello(fp) + adapt_field_size;
						fseeko(fp, adap_end_pos, SEEK_SET);
					}
				}
				if (use_fread == 0) {
					read(fd, buf, 14);
				} else {
					read_nbytes(buf, 1, 14, fp);
					if (file_error == 1)
						goto ts_pts_failed;
				}

				if(payload_start)
				{
					mp_msg("pts_parse: HEADER %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
					if (buf[0] || buf[1] || (buf[2] != 1))
					{
						mp_msg("pts_parse: error HEADER %02x %02x %02x (should be 0x000001) \n", buf[0], buf[1], buf[2]);
						break;
					}
					if (buf[7] & 0x80)
					{ 	/* pts available */
						pts  = (int64_t)(buf[9] & 0x0E) << 29 ;
						pts |=  buf[10]         << 22 ;
						pts |= (buf[11] & 0xFE) << 14 ;
						pts |=  buf[12]         <<  7 ;
						pts |= (buf[13] & 0xFE) >>  1 ;

						tss->last_pts = pts / 90000.0f;
						if (tss->pts != 0.0)	//2011-08-30 Carlos add, get the real duration
						{
							if ((tss->last_pts < tss->pts) && (tss->pts < (double)90000))
							{
								tss->last_pts = tss->pts;
								mp_msg("pid: %d, pts time error!!!\n", pid);
								break;
							}
							pidary[ii] = -1;
							left_num--;
							mp_msg("pid: %d synced %d\n", pid, afc);
							if (tss->last_pts >= tss->pts) {
								tmp_pts = tss->last_pts - tss->pts;
							} else {
								tmp_pts = tss->last_pts + (PTS_MAX_NUMBER - tss->pts);
							}
							if (tmp_pts > max_pts)
							{
								max_pts = tmp_pts;
								finfo->FileDuration = max_pts;
							}
						}
						else
						{
							//printf("### [%s - %d] pid = %d   tss->last_pts= %f, pts=%f\n", __func__, __LINE__, pid, tss->last_pts, tss->pts);
						}
					}
				}
				//---------------------
				break;
			}
		}
		if (left_num != 0)
		{
			if (synced_num - left_num >= 2)
			{
			} else
				goto try_prv_sync;
		}
	}
	else
	{
		goto try_prv_sync;
	}
	nRet = 1;

ts_pts_failed:
	if (pidary != NULL)
		free(pidary);

	mp_msg("syncnum: %d, search size: %llu\n", syncnum, finfo->FileSize - pos);
	return nRet;
}

static int ts_parse(FILE *fp, FileInfo *finfo, ES_stream_t *es)
{
	ES_stream_t *tss = NULL;
	int nRet =0, is_video, is_audio, is_sub;
	int ii, temp, cc, cc_ok, len, scramble;
	LOFF_T pos, next_pos, end_pos, last_update_pos = 0;
	static int buf_size = 256;
	int rem_buf_size;
	unsigned char *buf = NULL;
	int ts_error, payload_start, pid, afc;
	int rap_flag, synced_num = 0;
	int32_t progid, pid_type;
	// two check size set by (Hokkaido-2008_1920x1080_H264.avi)
	int max_check_size = 12*1024*1024;//6*1024*1024;    //Polun 2011-08-30 for mantis 5975&5976
	int max_last_check_size = 3*1024*1024;
	int check_rest = 0;
	ts_priv_t *priv;
	mp4_decoder_config_t *mp4_dec;
	pmt_t *pmt;
	unsigned char * map_buf = (unsigned char *)MAP_FAILED;

	priv = (ts_priv_t*)finfo->priv;
	
	end_pos = finfo->FileSize;
	if (max_check_size > end_pos)
		max_check_size = end_pos;
	map_buf = (unsigned char *)mmap(NULL, max_check_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
	if (map_buf == MAP_FAILED)
	{
		//printf("Can't mmap file!!!\n");
		buf = (unsigned char *)malloc(buf_size);
		if (buf == NULL)
		{
			printf("Can't malloc memory!!!\n");
			return nRet;
		}
		fseek(fp, 0, SEEK_SET);
	}
	pos = 0;
	while (pos < max_check_size - 20) {
		ts_error = 0;
		mp4_dec = NULL;

		check_rest = check_all_synced(priv);
		if (check_rest != 0)
		{
			if (check_rest == 1)
			{
				mp_msg("get all synced num-------------\n");
				break;
			}
			else if (check_rest == 2)
			{
				// check at least 256k (CDCAS200709测试码流1p_Entitle.ts)
				// check at least 700k (2006_08_03.Ts)
				// check at least 1.200M (76.5E_12528v_300000.ts)
				if (pos > 1.2*1024*1024)
				{
					mp_msg("All audio/video are encrypted!!!\n");
					finfo->bEncrypted = 1;
					break;
				}
			}
		} else {
			if (pos > last_update_pos + max_last_check_size)
			{
				mp_msg("Reach max last update size: %llu !!!\n", pos);
				break;
			}
		}

		// if no pat/pmt and no synced item 
		if(pos > max_check_size)
		{
			if (synced_num == 0) 
			{
				mp_msg("max size ----------------------\n");
				return nRet;
			}
			else if (synced_num >= 1)
			{
				mp_msg("maybe no pat or pmt data or have loarge collect data -------\n");
				break;
			}
		}

		// get first header 0x47
		//for (ii = 0; ii < (TS_FEC_PACKET_SIZE * NUM_CONSECUTIVE_TS_PACKETS); ii++) {
		for (ii = 0; ii < (TS_FEC_PACKET_SIZE * 400); ii++) {	//fix mantis: 4628
			if (map_buf == MAP_FAILED) {
				temp = read_char(fp);
				if (temp == 0x47)
					break;
			} else if (map_buf[pos] == 0x47)
				break;
			pos++;
		}
		if (map_buf == MAP_FAILED) {
			if (temp != 0x47) {
				mp_msg("synced_num: %d\n", synced_num);
				mp_msg("TS_PARSE: COULDN'T SYNC at pos %llu\n", pos);
				break;
			}
		} else if (map_buf[pos] != 0x47) {
			mp_msg("synced_num: %d\n", synced_num);
			mp_msg("TS_PARSE: COULDN'T SYNC at pos %llu\n", pos);
			break;
		}
		pos++;

		rap_flag = 0;
		next_pos = pos + priv->ts.packet_size - 1;
		if (next_pos > end_pos)
		{
			break;
		}

		if (map_buf == MAP_FAILED) {
			read_nbytes(buf, 3, 1, fp);
			if (file_error == 1)
				goto next_ts;
			if ((buf[0] >> 7) & 0x01)
				ts_error = 1;
			payload_start = (buf[0] >> 6) & 0x01;
			pid = ((buf[0] & 0x1f) << 8) | (buf[1] & 0xff);
		} else {
			if ((map_buf[pos] >> 7) & 0x01)
				ts_error = 1;
			payload_start = (map_buf[pos] >> 6) & 0x01;
			pid = ((map_buf[pos] & 0x1f) << 8) | (map_buf[pos+1] & 0xff);
		}

		tss = priv->ts.pids[pid];                       //an ES stream
		if(tss == NULL)
		{
			tss = new_pid(priv, pid);
			if(tss == NULL)
			{
				mp_msg("Can't get new tss!!!");
				return 0;
			}
		}

		if (map_buf == MAP_FAILED) {
			cc = (buf[2] & 0xf);
			afc = (buf[2] >> 4) & 0x03;
			scramble = (buf[2] & 0xc0) >> 6;
		} else {
			cc = (map_buf[pos + 2] & 0xf);
			afc = (map_buf[pos + 2] >> 4) & 0x03;
			scramble = (map_buf[pos + 2] & 0xc0) >> 6;
		}
		cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
		tss->last_cc = cc;
		if (ts_error) 
		{
			payload_start = 0;
		}

		// skip synced and invalid pid
		if((tss->is_synced == 1) || ((pid > 1) && (pid < 16)) || (pid == 8191) ||
				(tss->scrameled == 1))
		{
			goto next_ts;
		}

		if (0) {
			mp_msg("transport_error: %d, payload_unit_start: %d, PID: %d, adaptation_field: %d, pos: %llu\n", ts_error, payload_start, pid, afc, pos);
		}

		// when data is scramble, mark it as encrypt
		if ((scramble >= 2) && (tss->scrameled == 0))
		{
			//printf("scrambled pid: %d\n", pid);
			tss->scrameled = 1;
		}

		// no payload
		if (!(afc % 2)) {
			goto next_ts;
		}

		pmt = pmt_of_pid(priv, pid, &mp4_dec);
		if(mp4_dec)
		{
			fill_extradata(mp4_dec, tss);
			if(IS_VIDEO(mp4_dec->object_type) || IS_AUDIO(mp4_dec->object_type))
			{
				tss->type = SL_PES_STREAM;
				tss->subtype = (es_stream_type_t)mp4_dec->object_type;
			}
		}
		pid_type = pid_type_from_pmt(priv, pid);
		// skip unknow type
		if ((pmt != NULL) && (pid_type == UNKNOWN))
		{
			goto next_ts;
		}

		if (payload_start) {
			mp_msg("\n==transport_error1: %d, payload_unit_start: %d, PID: %d, adaptation_field: %d==\n", ts_error, payload_start, pid, afc);
			mp_msg("position: %08x %u\n", (unsigned int)pos, (unsigned int)pos);
			last_update_pos = pos;
		}
		pos += 3;

		// handle adaptation field
		if (afc > 1) {
			int adapt_field_size;
			int flags;
			if (map_buf == MAP_FAILED) {
				adapt_field_size = read_char(fp);
				flags = read_char(fp);
				pos += 2;
			} else {
				adapt_field_size = map_buf[pos++];
				flags = map_buf[pos++];
			}
			if (adapt_field_size < 0 || adapt_field_size > priv->ts.packet_size - 5) {
				mp_msg("wrong adaptation field size %d!!!\n", adapt_field_size);
				goto next_ts;
			}
			//mp_msg("\tadaptation_field_size: %d ", adapt_field_size);
			rap_flag = (flags & 0x40) >> 6;
			//mp_msg("is arp flag: %d\n", rap_flag);
			pos += adapt_field_size-1;
			if (map_buf == MAP_FAILED) {
				fseeko(fp, pos, SEEK_SET);
			}
		}

		priv->last_pid = pid;
		is_video = IS_VIDEO(tss->type) || (tss->type==SL_PES_STREAM && IS_VIDEO(tss->subtype));
		is_audio = IS_AUDIO(tss->type) || (tss->type==SL_PES_STREAM && IS_AUDIO(tss->subtype)) || (tss->type == PES_PRIVATE1);
		is_sub  = IS_SUB(tss->type);

		rem_buf_size = next_pos - pos;
		if (rem_buf_size > 184)
			rem_buf_size = 184;
		//printf("pid: %d, pos: %llX, size: %d\n", pid, pos, rem_buf_size);
		if (map_buf == MAP_FAILED) {
			len = read_nbytes(buf, 1, rem_buf_size, fp);
			if(len < rem_buf_size)
			{
				mp_msg("\r\nts_parse() couldn't read enough data: %d < %d\r\n", len, rem_buf_size);
				goto next_ts;
			}
		} else {
			buf = &map_buf[pos];
		}
		pos += rem_buf_size;

		// handle payload
		if(pid == 0) {
			if (parse_pat(priv, payload_start, buf, rem_buf_size) == 1)
			{
				tss->is_synced = 1;
				synced_num++;
			}
			goto next_ts;
		}
		else if((tss->type == SL_SECTION) && pmt)
		{
			int k, mp4_es_id = -1;
			ts_section_t *section;
			for(k = 0; k < pmt->mp4es_cnt; k++)
			{
				if(pmt->mp4es[k].decoder.object_type == MP4_OD && pmt->mp4es[k].decoder.stream_type == MP4_OD)
					mp4_es_id = pmt->mp4es[k].id;
			}
			mp_msg("MP4ESID: %d\n", mp4_es_id);
			for(k = 0; k < pmt->es_cnt; k++)
			{
				if(pmt->es[k].mp4_es_id == mp4_es_id)
				{
					section = &(tss->section);
					parse_sl_section(pmt, section, payload_start, buf, rem_buf_size);
				}
			}
			goto next_ts;
		}
		else
		{
			progid = prog_id_in_pat(priv, pid);
			if(progid != -1)
			{
				if (parse_pmt(priv, progid, pid, payload_start, buf, rem_buf_size) == 1)
				{
					tss->is_synced = 1;
					synced_num++;
				}
				goto next_ts;
			}
		}

		reset_es(es);
		if(payload_start)
		{
			uint8_t *lang = NULL;

			len = pes_parse2(buf, rem_buf_size, es, pid_type, pmt, pid, tss);

			// update timestamp
			if((len > 0) && (es->pts != 0.0f)) {
				if (tss->pts == 0.0f) {
					tss->pts = tss->last_pts = es->pts;
					mp_msg("set duration: %f\n", tss->pts);
				} else {
					if (es->pts < tss->last_pts)
					{
						if (tss->last_pts - es->pts > 2)
						{
							// maybe new vidoe. so reset and save old time.
							mp_msg("reset duration: %f, %f\n", es->pts, tss->last_pts);
							tss->pts = es->pts - (tss->last_pts - tss->pts);
							tss->last_pts = tss->pts;
						}
					}
					else if (tss->last_pts < es->pts)
					{
						tss->last_pts = es->pts;
						mp_msg("get duration: %f, %f, %f\n", tss->last_pts - tss->pts, tss->last_pts, tss->pts);
					}
				}
			}

			if(len == -2)
			{
				tss->is_synced = 0;
				goto next_ts;
			} else if (len == -1)
			{
				tss->is_synced = 1;
				mp_msg("pid: %d is skip synced %d\n", tss->pid, tss->is_synced);
				goto next_ts;
			}

			es->pid = tss->pid;
			if ((tss->type != UNKNOWN) && (es->type == AUDIO_TRUEHD))
			{
				tss->subtype = es->type;
			} else {
				tss->type = es->type;
				tss->subtype = es->subtype;
			}

			if ((len == 1) && (tss->pts != 0))
			{
				tss->is_synced |= es->is_synced || rap_flag;
				if (tss->is_synced == 1)
					synced_num++;
				mp_msg("pid: %d is synced %d\n", tss->pid, tss->is_synced);

				if((is_sub || is_audio) && (lang = pid_lang_from_pmt(priv, es->pid)))
				{
					memcpy(es->lang, lang, 3);
					es->lang[3] = 0;
				}
				else
					es->lang[0] = 0;

				ts_add_stream(finfo, tss);
			}
		}
		else
		{
			if ((es_stream_type_t)tss->type != UNKNOWN)
			{
				es->type = tss->type;
			}
			else if ((es_stream_type_t)pid_type != UNKNOWN)
			{
				es->type = (es_stream_type_t)pid_type;
			}
			else
				es->type = UNKNOWN;
            es->pid = tss->pid;
            //es->type = tss->type;
			es->subtype = tss->subtype;
            //es->pts = tss->pts = tss->last_pts;
            es->start = buf;
			es->size = rem_buf_size;

			// maybe probe unknow type here
		}

		if ((tss->is_synced != 1) && (tss->mediainfo == NULL))
		{
			if (info_parse(es, pid, tss) == 1)
			{
				if (tss->pts != 0) 
				{
					tss->is_synced = 1;
					synced_num++;
					ts_add_stream(finfo, tss);
				}
			}
		}

next_ts:
		if (map_buf == MAP_FAILED) {
			fseeko(fp, next_pos, SEEK_SET);
		}
		pos = next_pos;
              //Polun 2011-08-30 for mantis 5975&5976 
              if(pos > max_check_size)
                    printf("!!!!!!![%s - %d]   pos > max_check_size   pos = %lld \n", __func__, __LINE__,pos);
	}
	if (es->mediainfo != NULL) {
		free(es->mediainfo);
		es->mediainfo = NULL;
	}
	mp_msg("synced_num: %d\n", synced_num);
	if (map_buf == MAP_FAILED) {
		if (buf != NULL)
			free(buf);
	} else {
		munmap((void *)map_buf, max_check_size);
	}
	ts_pts_parse(fp, finfo);
	nRet = 1;
	return nRet;
}

static void demux_close_ts(ts_priv_t *priv)
{
    uint16_t i;

    if(priv)
    {
        if(priv->pat.section.buffer)
            free(priv->pat.section.buffer);
        if(priv->pat.progs)
            free(priv->pat.progs);

        if(priv->pmt)
        {
            for(i = 0; i < priv->pmt_cnt; i++)
            {
                if(priv->pmt[i].section.buffer)
                    free(priv->pmt[i].section.buffer);
                if(priv->pmt[i].es)
                    free(priv->pmt[i].es);
            }
			if (priv->pmt->mp4es)
				free(priv->pmt->mp4es);
            free(priv->pmt);
        }
		for(i = 0; i < NB_PID_MAX;i ++)
		{
			if(priv->ts.pids[i] != NULL)
			{
				if (priv->ts.pids[i]->mediainfo != NULL)
					free(priv->ts.pids[i]->mediainfo);
				if (priv->ts.pids[i]->extradata != NULL)
					free(priv->ts.pids[i]->extradata);
				if (priv->ts.pids[i]->section.buffer != NULL)
					free(priv->ts.pids[i]->section.buffer);
				free(priv->ts.pids[i]);
			}
		}
        free(priv);
    }
}

int ts_detect_streams(FILE *fp, FileInfo *finfo)
{
	int nRet = 0, ii, jj;
	ES_stream_t es, *temp_es, *video_es = NULL, *audio_es = NULL, *nosup_audio_es = NULL;
	ts_priv_t *priv = NULL;
	int is_audio, is_video;
	int is_scrameled = 0;
	int is_hw_notsupport = 0;
	WAVEFORMATEX *wf;

	priv = (ts_priv_t*)finfo->priv;
	if (priv == NULL)
	{
		goto tsreturn;
	}

	bzero(&es, sizeof(ES_stream_t));
	ts_parse(fp, finfo, &es);

	// check if have pat/pmt table
	if ((priv->pmt != NULL) && priv->pmt_cnt > 0 && priv->pat.progs_cnt > 0) {
		// use first pat program
		// todo use for loop to check
		int es_pid;
		pmt_t *pmt;
		for (jj = 0; jj < priv->pmt_cnt; jj ++)
		{
			pmt = &(priv->pmt[jj]);
			for (ii = 0; ii < pmt->es_cnt; ii++)
			{
				es_pid = pmt->es[ii].pid;
				temp_es = priv->ts.pids[es_pid];
				if ((temp_es != NULL) && (temp_es->mediainfo != NULL) && (temp_es->is_synced == 1))
				{
					if (temp_es->scrameled == 1)
					{
						is_scrameled = 1;
						break;
					}
					is_video = IS_VIDEO(temp_es->type) || (temp_es->type==SL_PES_STREAM && IS_VIDEO(temp_es->subtype));
					is_audio = IS_AUDIO(temp_es->type) || (temp_es->type==SL_PES_STREAM && IS_AUDIO(temp_es->subtype)) || (temp_es->type == PES_PRIVATE1);
					// chose first es stream
					if ((audio_es == NULL) && (is_audio == 1)) {
						wf = (WAVEFORMATEX *)temp_es->mediainfo;
						if (check_audio_type(temp_es->type, wf->nChannels, finfo->hw_a_flag) != 0) {
							audio_es = temp_es;
						} else {
							nosup_audio_es = temp_es;
						}
					}
					if ((video_es == NULL) && (is_video == 1)) {
						if (temp_es->hw_notsupport == 1)
						{
							is_hw_notsupport = 1;
						} else {
							video_es = temp_es;
						}
					}
					if ((audio_es != NULL) && (video_es != NULL))
						break;
				}
			}
			if ((audio_es != NULL) && (video_es != NULL))
				break;
		}
		// if the only audio is not suport. set it to not support audio_es
		if ((audio_es == NULL) && (nosup_audio_es != NULL))
			audio_es = nosup_audio_es;
	}
	else
	{
		mp_msg("no pmt or pat!!!\n");
	}

	if ((audio_es == NULL) && (video_es == NULL))
	{
		// check all ts stream to find out video/audio data
		for (ii = 0; ii < NB_PID_MAX; ii++) {
			temp_es = priv->ts.pids[ii];
			if ((temp_es != NULL) && (temp_es->mediainfo != NULL) && (temp_es->is_synced == 1))
			{
				if (temp_es->scrameled == 1)
				{
					is_scrameled = 1;
					break;
				}
				is_video = IS_VIDEO(temp_es->type) || (temp_es->type==SL_PES_STREAM && IS_VIDEO(temp_es->subtype));
				is_audio = IS_AUDIO(temp_es->type) || (temp_es->type==SL_PES_STREAM && IS_AUDIO(temp_es->subtype)) || (temp_es->type == PES_PRIVATE1);
				if ((audio_es == NULL) && is_audio == 1) {
					wf = (WAVEFORMATEX *)temp_es->mediainfo;
					if (check_audio_type(temp_es->type, wf->nChannels, finfo->hw_a_flag) != 0) {
						audio_es = temp_es;
					} else {
						nosup_audio_es = temp_es;
					}
				}
				if ((video_es == NULL) && is_video == 1) {
					if (temp_es->hw_notsupport == 1)
					{
						is_hw_notsupport = 1;
					} else {
						video_es = temp_es;
					}
				}
				if ((audio_es != NULL) && (video_es != NULL))
					break;
			}
		}
		// if the only audio is not suport. set it to not support audio_es
		if ((audio_es == NULL) && (nosup_audio_es != NULL))
			audio_es = nosup_audio_es;
	}

	if ( (video_es == NULL) && (is_hw_notsupport == 1) )
	{
		// the only video is not support because of hardware limit, return not support this file.
	} else {
		if (audio_es != NULL) {
			finfo->AudioType = audio_es->type;
			finfo->bAudio = 1;
			WAVEFORMATEX *wf = (WAVEFORMATEX *)audio_es->mediainfo;
			memcpy(&finfo->wf, (void *)wf, sizeof(WAVEFORMATEX));
			finfo->aBitrate = wf->nAvgBytesPerSec * 8;
			if (audio_es->last_pts >= audio_es->pts)
			{
				finfo->AudioDuration = (int)(audio_es->last_pts - audio_es->pts);
			} else {
				finfo->AudioDuration = (int)(audio_es->last_pts + (PTS_MAX_NUMBER - audio_es->pts));
			}
			mp_msg("PID: %d, audio duration: %d %f %f\n", audio_es->pid, finfo->AudioDuration, audio_es->last_pts, audio_es->pts);
			nRet = 1;
		}
		if ((video_es != NULL) && (video_es->mediainfo != NULL)) {
			finfo->bVideo = 1;
			TSSTREAMINFO *video_info = (TSSTREAMINFO *)video_es->mediainfo;
			finfo->bih.biWidth = video_info->width;
			finfo->bih.biHeight = video_info->height;
			set_fourcc((char *)&finfo->bih.biCompression, video_es->type);
			finfo->FPS = video_info->fps;
			finfo->vBitrate = video_info->bitrate;
			/*
			if (video_es->last_pts >= video_es->pts)
			{
				finfo->FileDuration = (int)(video_es->last_pts - video_es->pts);
			} else {
				finfo->FileDuration = (int)(video_es->last_pts + (PTS_MAX_NUMBER - video_es->pts));
			}
			*/
			mp_msg("PID: %d, video duration: %d %f %f\n", video_es->pid, finfo->FileDuration, video_es->last_pts, video_es->pts);
			nRet = 1;
		}
	}
	
	if (nRet == 1)
	{
		finfo->FileDuration = (finfo->FileDuration > finfo->AudioDuration)? finfo->FileDuration : finfo->AudioDuration;
	}
	else 
	{
		if (is_scrameled == 1)
		{
			mp_msg("All audio/video are scrameled!!!\n");
			finfo->bEncrypted = 1;
		}
	}
	if (!finfo->FileDuration && nRet == 1)
	{
		if (finfo->vBitrate + finfo->aBitrate > 0)
			finfo->FileDuration = (int)((finfo->FileSize * 8) / (int)(finfo->vBitrate + finfo->aBitrate));
	}

tsreturn:
	// free all priv data
	demux_close_ts(priv);
	finfo->priv = NULL;

	return nRet;
}

