#include <stdio.h>
#include <stdlib.h>
#include "demux_mkv.h"
#include "ebml.h"
#include "common.h"
#include "read_data.h"
#include "matroska.h"
#include "help.h"
#include "bswap.h"
#include "intreadwrite.h"
#include "mpeg_hdr.h"
#include "common.h"

#define mp_msg(MSGT, MSGL, msg, ...)
//#define mp_msg(MSGT, MSGL, msg, ...) printf(msg, ## __VA_ARGS__) 

#define REALHEADER_SIZE    16
#define RVPROPERTIES_SIZE  34
#define RAPROPERTIES4_SIZE 56
#define RAPROPERTIES5_SIZE 70

// Map flavour to bytes per second
#define SIPR_FLAVORS 4
#define ATRC_FLAVORS 8
#define COOK_FLAVORS 34
static const int sipr_fl2bps[SIPR_FLAVORS] = {813, 1062, 625, 2000};
static const int atrc_fl2bps[ATRC_FLAVORS] =
{8269, 11714, 13092, 16538, 18260, 22050, 33075, 44100};
static const int cook_fl2bps[COOK_FLAVORS] =
{ 1000,  1378,  2024,  2584, 4005,  5513, 8010, 4005,   750, 2498,
	4048,  5513,  8010, 11973, 8010,  2584, 4005, 2067,  2584, 2584,
	4005,  4005,  5513,  5513, 8010, 12059, 1550, 8010, 12059, 5513,
	12016, 16408, 22911, 33506};

typedef struct {
	const char *id;
	int fourcc;
	int extradata;
} videocodec_info_t;

static const videocodec_info_t vinfo[] = {
	{ MKV_V_MPEG1,     mmioFOURCC('m', 'p', 'g', '1'), 0 },
	{ MKV_V_MPEG2,     mmioFOURCC('m', 'p', 'g', '2'), 0 },
	{ MKV_V_MPEG4_SP,  mmioFOURCC('m', 'p', '4', 'v'), 1 },
	{ MKV_V_MPEG4_ASP, mmioFOURCC('m', 'p', '4', 'v'), 1 },
	{ MKV_V_MPEG4_AP,  mmioFOURCC('m', 'p', '4', 'v'), 1 },
	{ MKV_V_MPEG4_AVC, mmioFOURCC('a', 'v', 'c', '1'), 1 },
	{ MKV_V_THEORA,    mmioFOURCC('t', 'h', 'e', 'o'), 1 },
	{ NULL, 0, 0 }
};

typedef struct
{
  uint32_t order, type, scope;
  uint32_t comp_algo;
  uint8_t *comp_settings;
  int comp_settings_len;
} mkv_content_encoding_t;

typedef struct mkv_track
{
  int tnum;
  char *name;

  char *codec_id;
  int ms_compat;
  char *language;
  int hw_notsupport;

  int type;

  uint32_t v_width, v_height, v_dwidth, v_dheight;
  float v_frate;

  uint32_t a_formattag;
  uint32_t a_channels, a_bps;
  float a_sfreq;

  float default_duration;

  int default_track;

  void *private_data;
  unsigned int private_size;

  /* stuff for realmedia */
  int realmedia;
  int64_t rv_kf_base;
  int rv_kf_pts;
  float rv_pts;  /* previous video timestamp */
  float ra_pts;  /* previous audio timestamp */

  /** realaudio descrambling */
  int sub_packet_size; ///< sub packet size, per stream
  int sub_packet_h; ///< number of coded frames per block
  int coded_framesize; ///< coded frame size, per stream
  int audiopk_size; ///< audio packet size
  unsigned char *audio_buf; ///< place to store reordered audio data
  float *audio_timestamp; ///< timestamp for each audio packet
  int sub_packet_cnt; ///< number of subpacket already received
  int audio_filepos; ///< file position of first audio packet in block

  /* stuff for quicktime */
  int fix_i_bps;
  float qt_last_a_pts;

  int subtitle_type;

  /* The timecodes of video frames might have to be reordered if they're
     in display order (the timecodes, not the frames themselves!). In this
     case demux packets have to be cached with the help of these variables. */
  int reorder_timecodes;
  //demux_packet_t **cached_dps;
  int num_cached_dps, num_allocated_dps;
  float max_pts;

  /* generic content encoding support */
  mkv_content_encoding_t *encodings;
  int num_encodings;

  /* For VobSubs and SSA/ASS */
  //sh_sub_t *sh_sub;
} mkv_track_t;

typedef struct mkv_index
{
  int tnum;
  uint64_t timecode, filepos;
} mkv_index_t;

typedef struct mkv_demuxer
{
  off_t segment_start;

  float duration, last_pts;
  uint64_t last_filepos;

  mkv_track_t **tracks;
  int num_tracks;

  uint64_t tc_scale, cluster_tc, first_tc;
  int has_first_tc;

  uint64_t cluster_size;
  uint64_t blockgroup_size;

  mkv_index_t *indexes;
  int num_indexes;

  off_t *parsed_cues;
  int parsed_cues_num;
  off_t *parsed_seekhead;
  int parsed_seekhead_num;

  uint64_t *cluster_positions;
  int num_cluster_pos;

  int64_t skip_to_timecode;
  int v_skip_to_keyframe, a_skip_to_keyframe;

  int64_t stop_timecode;

  int last_aid;
  int audio_tracks[MAX_A_STREAMS];
} mkv_demuxer_t;



int mkv_check_file(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	int version;
	char * str;
	str = ebml_read_header (fp, &version);
	if (str == NULL || strcmp (str, "matroska") || version > 2)
	{
		mp_msg (MSGT_DEMUX, MSGL_DBG2, "[mkv] no head found\n");
		return 0;
	}
	free (str);
	nRet = 1;
	return nRet;
}

static int
demux_mkv_read_info (FILE *s, mkv_demuxer_t *mkv_d)
{
  uint64_t length, l;
  int il;
  uint64_t tc_scale = 1000000;
  long double duration = 0.;

  length = ebml_read_length (s, NULL);
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TIMECODESCALE:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 1;
            tc_scale = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + timecode scale: %"PRIu64"\n",
                    tc_scale);
            break;
          }

        case MATROSKA_ID_DURATION:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 1;
            duration = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + duration: %.3Lfs\n",
                    duration * tc_scale / 1000000000.0);
            break;
          }

        default:
          ebml_read_skip (s, &l);
          break;
        }
      length -= l + il;
    }
  mkv_d->tc_scale = tc_scale;
  mkv_d->duration = duration * tc_scale / 1000000000.0;
  return 0;
}

static int
demux_mkv_read_trackaudio (FILE *s, mkv_track_t *track)
{
  uint64_t len, length, l;
  int il;

  track->a_sfreq = 8000.0;
  track->a_channels = 1;

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_AUDIOSAMPLINGFREQ:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 0;
            track->a_sfreq = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Sampling frequency: %f\n",
                    track->a_sfreq);
            break;
          }

        case MATROSKA_ID_AUDIOBITDEPTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->a_bps = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Bit depth: %u\n",
                    track->a_bps);
            break;
          }

        case MATROSKA_ID_AUDIOCHANNELS:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->a_channels = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Channels: %u\n",
                    track->a_channels);
            break;
          }

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return len;
}

static int
demux_mkv_read_trackvideo (FILE *s, mkv_track_t *track)
{
  uint64_t len, length, l;
  int il;

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_VIDEOFRAMERATE:
          {
            long double num = ebml_read_float (s, &l);
            if (num == EBML_FLOAT_INVALID)
              return 0;
            track->v_frate = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Frame rate: %f\n",
                    track->v_frate);
            if (track->v_frate > 0)
              track->default_duration = 1 / track->v_frate;
            break;
          }

        case MATROSKA_ID_VIDEODISPLAYWIDTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_dwidth = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Display width: %u\n",
                    track->v_dwidth);
            break;
          }

        case MATROSKA_ID_VIDEODISPLAYHEIGHT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_dheight = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Display height: %u\n",
                    track->v_dheight);
            break;
          }

        case MATROSKA_ID_VIDEOPIXELWIDTH:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_width = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel width: %u\n",
                    track->v_width);
            break;
          }

        case MATROSKA_ID_VIDEOPIXELHEIGHT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              return 0;
            track->v_height = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |   + Pixel height: %u\n",
                    track->v_height);
            break;
          }

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return len;
}

static int
handle_block (mkv_demuxer_t *mkv_d, uint8_t *block, uint64_t length,
		              uint64_t block_duration, int64_t block_bref, int64_t block_fref, uint8_t simpleblock)
{
	int ii, num, tmp;
	mkv_track_t *track = NULL;
	int nRet = 1;

	/* first byte(s): track num */
	num = ebml_read_vlen_uint (block, &tmp);

	// find track info
	for (ii=0; ii<mkv_d->num_tracks; ii++)
	{
		if ( (mkv_d->tracks[ii]->type == MATROSKA_TRACK_VIDEO)
				&& (mkv_d->tracks[ii]->tnum == num) )
		{
			track = mkv_d->tracks[ii];
			break;
		}
	}

	if ( (track != NULL) && track->ms_compat && (track->private_data) &&
			(track->hw_notsupport == 0) )
	{
		BITMAP_INFO_HEADER *src;
		uint32_t biCompression;
		src = (BITMAP_INFO_HEADER *) track->private_data;
		biCompression = le2me_32 (src->biCompression);
		if ( (biCompression == mmioFOURCC('X', 'V', 'I', 'D')) ||
				(biCompression == mmioFOURCC('x', 'v', 'i', 'd')) ||
				(biCompression == mmioFOURCC('D', 'I', 'V', 'X')) ||
				(biCompression == mmioFOURCC('d', 'i', 'v', 'x')) ||
				(biCompression == mmioFOURCC('D', 'X', '5', '0')) )
		{
			uint8_t *check_mp4_vol_buf = (uint8_t *)malloc(128);
			if (check_mp4_vol_buf != NULL)
			{
				memcpy(check_mp4_vol_buf, block, 128);
				if ( !check_mp4_header_vol(check_mp4_vol_buf, 128) )
				{
					printf("\nMKV: GMC AND STATIC SPRITE CODING not supported\n\n");
					track->hw_notsupport = 1;
					nRet = 0;
				}
				free(check_mp4_vol_buf);
			}

		}
	}

	return nRet;
}

static int
demux_mkv_read_block(FILE *s, mkv_demuxer_t *mkv_d)
{
	uint64_t length, l;
	int il, tmp;
	uint64_t block_duration = 0,  block_length = 0;
	int64_t block_bref = 0, block_fref = 0;
	uint8_t *block = NULL;

	length = ebml_read_length (s, NULL);
	while (length > 0)
	{
		switch (ebml_read_id (s, &il))
		{
			case MATROSKA_ID_BLOCKDURATION:
				//puts("MATROSKA_ID_BLOCKDURATION");
				block_duration = ebml_read_uint (s, &l);
				if (block_duration == EBML_UINT_INVALID) {
					if(block)
						free(block);
					return 0;
				}
				block_duration *= mkv_d->tc_scale / 1000000.0;
				break;
			case MATROSKA_ID_BLOCK:
				//puts("MATROSKA_ID_BLOCK");
				block_length = ebml_read_length (s, &tmp);
				if(block)
					free(block);
				if (block_length > SIZE_MAX - AV_LZO_INPUT_PADDING) return 0;
				block = (uint8_t *)malloc (block_length + AV_LZO_INPUT_PADDING);
				if (!block)
				{
					return 0;
				}
				if (fread(block, 1, (size_t)block_length, s) != (size_t) block_length)
				{
					if(block)
						free(block);
					return 0;
				}
				l = tmp + block_length;
				break;
			case MATROSKA_ID_REFERENCEBLOCK:
				//puts("MATROSKA_ID_REFERENCEBLOCK");
				{
					int64_t num = ebml_read_int (s, &l);
					if (num == EBML_INT_INVALID) {
						if(block)
							free(block);
						return 0;
					}
					if (num <= 0)
						block_bref = num;
					else
						block_fref = num;
					break;
				}
			case EBML_ID_INVALID:
				//puts("EBML_ID_INVALID");
				if(block)
					free(block);
				return 0;
			default:
				ebml_read_skip (s, &l);
				break;
		}
		length -= l + il;

		if (block)
		{
			handle_block (mkv_d, block, block_length,
					block_duration, block_bref, block_fref, 0);
			free (block);
			block = NULL;
		}
	}
	return 0;
}

static int
demux_mkv_read_cluster(FILE *s, mkv_demuxer_t *mkv_d)
{
	uint64_t length, l;
	int il;

	length = ebml_read_length (s, NULL);
	while (length > 0)
	{
		switch (ebml_read_id (s, &il))
		{
			case MATROSKA_ID_BLOCKGROUP:
				demux_mkv_read_block(s, mkv_d);
				return 1;
				break;
			case EBML_ID_INVALID:
				return 0;
			default:
				ebml_read_skip (s, &l);
				break;
		}
		length -= l + il;
	}

	return 1;
}

/**
 * \brief free array of kv_content_encoding_t
 * \param encodings pointer to array
 * \param numencodings number of encodings in array
 */
static void
demux_mkv_free_encodings(mkv_content_encoding_t *encodings, int numencodings)
{
  while (numencodings-- > 0)
    free(encodings[numencodings].comp_settings);
  free(encodings);
}

static int
demux_mkv_read_trackencodings (FILE *s, mkv_track_t *track)
{
  mkv_content_encoding_t *ce, e;
  uint64_t len, length, l;
  int il, n;

  ce = (mkv_content_encoding_t *)malloc (sizeof (*ce));
  n = 0;

  len = length = ebml_read_length (s, &il);
  if (len == EBML_UINT_INVALID)
	  goto err_out;
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_CONTENTENCODING:
          {
            uint64_t len;
            int i;

            memset (&e, 0, sizeof (e));
            e.scope = 1;

            len = ebml_read_length (s, &i);
	    if (len == EBML_UINT_INVALID)
		    goto err_out;
            l = len + i;

            while (len > 0)
              {
                uint64_t num, ll;
                int iil;

                switch (ebml_read_id (s, &iil))
                  {
                  case MATROSKA_ID_CONTENTENCODINGORDER:
                    num = ebml_read_uint (s, &ll);
                    if (num == EBML_UINT_INVALID)
                      goto err_out;
                    e.order = num;
                    break;

                  case MATROSKA_ID_CONTENTENCODINGSCOPE:
                    num = ebml_read_uint (s, &ll);
                    if (num == EBML_UINT_INVALID)
                      goto err_out;
                    e.scope = num;
                    break;

                  case MATROSKA_ID_CONTENTENCODINGTYPE:
                    num = ebml_read_uint (s, &ll);
                    if (num == EBML_UINT_INVALID)
                      goto err_out;
                    e.type = num;
                    break;

                  case MATROSKA_ID_CONTENTCOMPRESSION:
                    {
                      uint64_t le;

                      le = ebml_read_length (s, &i);
		      if (le == EBML_UINT_INVALID)
			      goto err_out;
                      ll = le + i;

                      while (le > 0)
                        {
                          uint64_t lll;
                          int iiil;

                          switch (ebml_read_id (s, &iiil))
                            {
                            case MATROSKA_ID_CONTENTCOMPALGO:
                              num = ebml_read_uint (s, &lll);
                              if (num == EBML_UINT_INVALID)
                                goto err_out;
                              e.comp_algo = num;
                              break;

                            case MATROSKA_ID_CONTENTCOMPSETTINGS:
                              lll = ebml_read_length (s, &i);
			      if (lll == EBML_UINT_INVALID)
                                goto err_out;
                              e.comp_settings = (uint8_t*)malloc (lll);
                              read_data(s, e.comp_settings, lll);
                              e.comp_settings_len = lll;
                              lll += i;
                              break;

                            default:
                              ebml_read_skip (s, &lll);
                              break;
                            }
                          le -= lll + iiil;
                        }

                      if (e.type == 1)
                        {
                          mp_msg(MSGT_DEMUX, MSGL_WARN,
                                 MSGTR_MPDEMUX_MKV_TrackEncrypted, track->tnum);
                        }
                      else if (e.type != 0)
                        {
                          mp_msg(MSGT_DEMUX, MSGL_WARN,
                                 MSGTR_MPDEMUX_MKV_UnknownContentEncoding, track->tnum);
                        }

                      if (e.comp_algo != 0 && e.comp_algo != 2)
                        {
                          mp_msg (MSGT_DEMUX, MSGL_WARN,
                                  MSGTR_MPDEMUX_MKV_UnknownCompression,
                                  track->tnum, e.comp_algo);
                        }
#if !CONFIG_ZLIB
                      else if (e.comp_algo == 0)
                        {
                          mp_msg (MSGT_DEMUX, MSGL_WARN,
                                  MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported,
                                  track->tnum);
                        }
#endif

                      break;
                    }

                  default:
                    ebml_read_skip (s, &ll);
                    break;
                  }
                len -= ll + iil;
              }
            for (i=0; i<n; i++)
              if (e.order <= ce[i].order)
                break;
            ce = (mkv_content_encoding_t*)realloc (ce, (n+1) *sizeof (*ce));
            memmove (ce+i+1, ce+i, (n-i) * sizeof (*ce));
            memcpy (ce+i, &e, sizeof (e));
            n++;
            break;
          }

        default:
          ebml_read_skip (s, &l);
          break;
        }

      length -= l + il;
    }

  track->encodings = ce;
  track->num_encodings = n;
  return len;

err_out:
  demux_mkv_free_encodings(ce, n);
  return 0;
}

/**
 * \brief free any data associated with given track
 * \param track track of which to free data
 */
static void
demux_mkv_free_trackentry(mkv_track_t *track) {
  free (track->name);
  free (track->codec_id);
  free (track->language);
  free (track->private_data);
  free (track->audio_buf);
  free (track->audio_timestamp);
  demux_mkv_free_encodings(track->encodings, track->num_encodings);
  free(track);
}

static int
demux_mkv_read_trackentry (FILE *s, mkv_demuxer_t *mkv_d)
{
  mkv_track_t *track;
  uint64_t len, length, l;
  int il;

  track = (mkv_track_t *)calloc (1, sizeof (*track));
  /* set default values */
  track->default_track = 1;
  track->name = 0;
  track->language = strdup("eng");

  len = length = ebml_read_length (s, &il);
  len += il;
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TRACKNUMBER:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              goto err_out;
            track->tnum = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Track number: %u\n",
                    track->tnum);
            break;
          }

        case MATROSKA_ID_TRACKNAME:
          {
            track->name = ebml_read_utf8 (s, &l);
            if (track->name == NULL)
              goto err_out;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Name: %s\n",
                    track->name);
            break;
          }

        case MATROSKA_ID_TRACKTYPE:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              goto err_out;
            track->type = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Track type: ");
            switch (track->type)
              {
              case MATROSKA_TRACK_AUDIO:
                mp_msg (MSGT_DEMUX, MSGL_V, "Audio\n");
                break;
              case MATROSKA_TRACK_VIDEO:
                mp_msg (MSGT_DEMUX, MSGL_V, "Video\n");
                break;
              case MATROSKA_TRACK_SUBTITLE:
                mp_msg (MSGT_DEMUX, MSGL_V, "Subtitle\n");
                break;
              default:
                mp_msg (MSGT_DEMUX, MSGL_V, "unknown\n");
                break;
            }
            break;
          }

        case MATROSKA_ID_TRACKAUDIO:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Audio track\n");
          l = demux_mkv_read_trackaudio (s, track);
          if (l == 0)
            goto err_out;
          break;

        case MATROSKA_ID_TRACKVIDEO:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Video track\n");
          l = demux_mkv_read_trackvideo (s, track);
          if (l == 0)
            goto err_out;
          break;

        case MATROSKA_ID_CODECID:
          track->codec_id = ebml_read_ascii (s, &l);
          if (track->codec_id == NULL)
            goto err_out;
          if (!strcmp (track->codec_id, MKV_V_MSCOMP) ||
              !strcmp (track->codec_id, MKV_A_ACM))
            track->ms_compat = 1;
          else if (!strcmp (track->codec_id, MKV_S_VOBSUB))
            track->subtitle_type = MATROSKA_SUBTYPE_VOBSUB;
          else if (!strcmp (track->codec_id, MKV_S_TEXTSSA)
                   || !strcmp (track->codec_id, MKV_S_TEXTASS)
                   || !strcmp (track->codec_id, MKV_S_SSA)
                   || !strcmp (track->codec_id, MKV_S_ASS))
            {
              track->subtitle_type = MATROSKA_SUBTYPE_SSA;
            }
          else if (!strcmp (track->codec_id, MKV_S_TEXTASCII))
            track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
          if (!strcmp (track->codec_id, MKV_S_TEXTUTF8))
            {
              track->subtitle_type = MATROSKA_SUBTYPE_TEXT;
            }
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Codec ID: %s\n",
                  track->codec_id);
          break;

        case MATROSKA_ID_CODECPRIVATE:
          {
            int x;
            uint64_t num = ebml_read_length (s, &x);
	    // audit: cheap guard against overflows later..
	    if (num > SIZE_MAX - 1000)
              goto err_out;
            l = x + num;
            track->private_data = malloc (num + AV_LZO_INPUT_PADDING);
            if (read_data(s, track->private_data, num) != (size_t) num)
              goto err_out;
            track->private_size = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + CodecPrivate, length "
                    "%u\n", track->private_size);
			if( track->type == MATROSKA_TRACK_VIDEO &&
					!strcmp(track->codec_id,MKV_V_MPEG4_AVC)
			  )
			{
				unsigned char*  ptr, *hdr;
				int write_len=0;
				int avc1_header_size = 0;

				ptr = (unsigned char *)track->private_data + 7; //skip 01 64 00 29 FF E1 00 and seek to first header len pos.

				unsigned char *avc1_header = (unsigned char *)malloc (track->private_size + AV_LZO_INPUT_PADDING);
				hdr = avc1_header;

				write_len = *ptr++; //get first header len

				hdr[0] = hdr[1] = hdr[2] = 0;
				hdr[3] = (unsigned char)write_len;
				hdr += 4;

				memcpy(hdr, ptr, write_len);
				hdr += write_len;
				ptr += write_len;

				avc1_header_size += 4 + write_len;

				mp_msg(MSGT_DEMUX, MSGL_V, "1write_len[%d]\n",write_len);//SkyMedi_Vincent

				ptr += 2;   //skip 01 00 and seek to second header len pos.
				write_len = *ptr++; //get second header len

				hdr[0] = hdr[1] = hdr[2] = 0;
				hdr[3] = (unsigned char)write_len;
				hdr += 4;

				memcpy(hdr, ptr, write_len);

				avc1_header_size += 4 + write_len;

				mp_msg(MSGT_DEMUX, MSGL_V, "2write_len[%d]\n",write_len);//SkyMedi_Vincent
				mp_msg(MSGT_DEMUX, MSGL_V, "avc1_header_size = %d\n", avc1_header_size);
				{
					int i = 0;
					for( i = 0 ; i< avc1_header_size; i++)
						mp_msg(MSGT_DEMUX, MSGL_V, "%02X ", avc1_header[i]);
					mp_msg(MSGT_DEMUX, MSGL_V, "\n");
				}

				if (!check_avc1_sps_bank0(avc1_header+5, avc1_header_size-5))
					goto err_out;

				free(avc1_header);
			}
            break;
          }

        case MATROSKA_ID_TRACKLANGUAGE:
          free(track->language);
          track->language = ebml_read_utf8 (s, &l);
          if (track->language == NULL)
            goto err_out;
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Language: %s\n",
                  track->language);
          break;

        case MATROSKA_ID_TRACKFLAGDEFAULT:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              goto err_out;
            track->default_track = num;
            mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default flag: %u\n",
                    track->default_track);
            break;
          }

        case MATROSKA_ID_TRACKDEFAULTDURATION:
          {
            uint64_t num = ebml_read_uint (s, &l);
            if (num == EBML_UINT_INVALID)
              goto err_out;
            if (num == 0)
              mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: 0");
            else
              {
                track->v_frate = 1000000000.0 / num;
                track->default_duration = num / 1000000000.0;
                mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |  + Default duration: "
                        "%.3fms ( = %.3f fps)\n",num/1000000.0,track->v_frate);
              }
            break;
          }

        case MATROSKA_ID_TRACKENCODINGS:
          l = demux_mkv_read_trackencodings (s, track);
          if (l == 0)
            goto err_out;
          break;

        default:
          ebml_read_skip (s, &l);
          break;
        }
      length -= l + il;
    }

  mkv_d->tracks[mkv_d->num_tracks++] = track;
  return len;

err_out:
  demux_mkv_free_trackentry(track);
  return 0;
}

static int
demux_mkv_read_tracks (FILE *s, mkv_demuxer_t *mkv_d)
{
  uint64_t length, l;
  int il;

  mkv_d->tracks = (mkv_track_t**)malloc (sizeof (*mkv_d->tracks));
  mkv_d->num_tracks = 0;

  length = ebml_read_length (s, NULL);
  while (length > 0)
    {
      switch (ebml_read_id (s, &il))
        {
        case MATROSKA_ID_TRACKENTRY:
          mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] | + a track...\n");
          mkv_d->tracks = (mkv_track_t**)realloc (mkv_d->tracks,
                                   (mkv_d->num_tracks+1)
                                   *sizeof (*mkv_d->tracks));
          l = demux_mkv_read_trackentry (s, mkv_d);
          if (l == 0)
            return 1;
          break;

        default:
            ebml_read_skip (s, &l);
            break;
        }
      length -= l + il;
    }
  return 0;
}

static void
demux_close_mkv (mkv_demuxer_t *mkv_d)
{
	if (mkv_d)
	{
		int i;
		//free_cached_dps (demuxer);
		if (mkv_d->tracks)
		{
			for (i=0; i<mkv_d->num_tracks; i++)
				demux_mkv_free_trackentry(mkv_d->tracks[i]);
			free (mkv_d->tracks);
		}
		free (mkv_d->indexes);
		free (mkv_d->cluster_positions);
		free (mkv_d->parsed_cues);
		free (mkv_d->parsed_seekhead);
		free (mkv_d);
	}
}

static int
demux_mkv_open_video (FileInfo *finfo, mkv_track_t *track)
{
  BITMAP_INFO_HEADER *bih = &finfo->bih;
#ifdef CONFIG_QTX_CODECS
  void *ImageDesc = NULL;
#endif

  if (track->ms_compat)  /* MS compatibility mode */
    {
      BITMAP_INFO_HEADER *src;

      if (track->private_data == NULL
          || track->private_size < sizeof (BITMAP_INFO_HEADER))
        return 1;

      src = (BITMAP_INFO_HEADER *) track->private_data;
      bih->biSize = le2me_32 (src->biSize);
      bih->biWidth = le2me_32 (src->biWidth);
      bih->biHeight = le2me_32 (src->biHeight);
      bih->biPlanes = le2me_16 (src->biPlanes);
      bih->biBitCount = le2me_16 (src->biBitCount);
      bih->biCompression = le2me_32 (src->biCompression);
      bih->biSizeImage = le2me_32 (src->biSizeImage);
      bih->biXPelsPerMeter = le2me_32 (src->biXPelsPerMeter);
      bih->biYPelsPerMeter = le2me_32 (src->biYPelsPerMeter);
      bih->biClrUsed = le2me_32 (src->biClrUsed);
      bih->biClrImportant = le2me_32 (src->biClrImportant);

      if (track->v_width == 0)
        track->v_width = bih->biWidth;
      if (track->v_height == 0)
        track->v_height = bih->biHeight;
    }
  else
    {
      bih->biSize = sizeof (BITMAP_INFO_HEADER);
      bih->biWidth = track->v_width;
      bih->biHeight = track->v_height;
      bih->biBitCount = 24;
      bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount/8;

      if (track->private_size >= RVPROPERTIES_SIZE
          && (!strcmp (track->codec_id, MKV_V_REALV10)
              || !strcmp (track->codec_id, MKV_V_REALV20)
              || !strcmp (track->codec_id, MKV_V_REALV30)
              || !strcmp (track->codec_id, MKV_V_REALV40)))
        {
          unsigned char *src;
          uint32_t type2;
          unsigned int cnt;

          src = (uint8_t *)track->private_data + RVPROPERTIES_SIZE;

          cnt = track->private_size - RVPROPERTIES_SIZE;
          bih->biPlanes = 1;
          type2 = AV_RB32(src - 4);
          if (type2 == 0x10003000 || type2 == 0x10003001)
            bih->biCompression=mmioFOURCC('R','V','1','3');
          else
            bih->biCompression=mmioFOURCC('R','V',track->codec_id[9],'0');
          track->realmedia = 1;

#ifdef CONFIG_QTX_CODECS
        }
      else if (track->private_size >= sizeof (ImageDescription)
               && !strcmp(track->codec_id, MKV_V_QUICKTIME))
        {
          ImageDescriptionPtr idesc;

          idesc = (ImageDescriptionPtr) track->private_data;
          idesc->idSize = be2me_32 (idesc->idSize);
          idesc->cType = be2me_32 (idesc->cType);
          idesc->version = be2me_16 (idesc->version);
          idesc->revisionLevel = be2me_16 (idesc->revisionLevel);
          idesc->vendor = be2me_32 (idesc->vendor);
          idesc->temporalQuality = be2me_32 (idesc->temporalQuality);
          idesc->spatialQuality = be2me_32 (idesc->spatialQuality);
          idesc->width = be2me_16 (idesc->width);
          idesc->height = be2me_16 (idesc->height);
          idesc->hRes = be2me_32 (idesc->hRes);
          idesc->vRes = be2me_32 (idesc->vRes);
          idesc->dataSize = be2me_32 (idesc->dataSize);
          idesc->frameCount = be2me_16 (idesc->frameCount);
          idesc->depth = be2me_16 (idesc->depth);
          idesc->clutID = be2me_16 (idesc->clutID);
          bih->biPlanes = 1;
          bih->biCompression = idesc->cType;
          ImageDesc = idesc;
#endif /* CONFIG_QTX_CODECS */

        }
      else
        {
          const videocodec_info_t *vi = vinfo;
          while (vi->id && strcmp(vi->id, track->codec_id)) vi++;
          bih->biCompression = vi->fourcc;
          track->reorder_timecodes = 1;
          if (!vi->id) {
              mp_msg (MSGT_DEMUX,MSGL_WARN, MSGTR_MPDEMUX_MKV_UnknownCodecID,
                      track->codec_id, track->tnum);
              return 1;
          }
        }
    }

  if (track->v_frate == 0.0)
    track->v_frate = 25.0;
  finfo->FPS = track->v_frate;

  return 0;
}

#define AAC_SYNC_EXTENSION_TYPE 0x02b7
static int
aac_get_sample_rate_index (uint32_t sample_rate)
{
	static const unsigned int srates[] = {92017, 75132, 55426, 46009, 37566, 27713, 23004, 18783, 13856, 11502, 9391, 0};
	int i = 0;
	while (sample_rate < srates[i]) i++;
	return i;
}

static int
demux_mkv_open_audio (FileInfo *finfo, mkv_track_t *track)
{
  WAVEFORMATEX *finfowf = &finfo->wf;

  if (track->ms_compat && (track->private_size >= sizeof(WAVEFORMATEX)))
    {
      WAVEFORMATEX *wf = (WAVEFORMATEX *)track->private_data;
      finfowf->wFormatTag = le2me_16 (wf->wFormatTag);
      finfowf->nChannels = le2me_16 (wf->nChannels);
      finfowf->nSamplesPerSec = le2me_32 (wf->nSamplesPerSec);
      finfowf->nAvgBytesPerSec = le2me_32 (wf->nAvgBytesPerSec);
      finfowf->nBlockAlign = le2me_16 (wf->nBlockAlign);
      finfowf->wBitsPerSample = le2me_16 (wf->wBitsPerSample);
      if (track->a_sfreq == 0.0)
        track->a_sfreq = finfowf->nSamplesPerSec;
      if (track->a_channels == 0)
        track->a_channels = finfowf->nChannels;
      if (track->a_bps == 0)
        track->a_bps = finfowf->wBitsPerSample;
      track->a_formattag = finfowf->wFormatTag;
    }
  else
    {
      memset(finfowf, 0, sizeof (WAVEFORMATEX));
      if (!strcmp(track->codec_id, MKV_A_MP3) ||
          !strcmp(track->codec_id, MKV_A_MP2))
        track->a_formattag = 0x0055;
      else if (!strncmp(track->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
        track->a_formattag = 0x2000;
      else if (!strncmp(track->codec_id, MKV_A_EAC3, strlen(MKV_A_EAC3)))
        track->a_formattag = AUDIO_EAC3;
      else if (!strcmp(track->codec_id, MKV_A_DTS))
	  {
        track->a_formattag = 0x2001;
        //dts_packet = 1;
	  }
      else if (!strcmp(track->codec_id, MKV_A_PCM) ||
               !strcmp(track->codec_id, MKV_A_PCM_BE))
        track->a_formattag = 0x0001;
      else if (!strcmp(track->codec_id, MKV_A_AAC_2MAIN) ||
               !strncmp(track->codec_id, MKV_A_AAC_2LC,
                        strlen(MKV_A_AAC_2LC)) ||
               !strcmp(track->codec_id, MKV_A_AAC_2SSR) ||
               !strcmp(track->codec_id, MKV_A_AAC_4MAIN) ||
               !strncmp(track->codec_id, MKV_A_AAC_4LC,
                        strlen(MKV_A_AAC_4LC)) ||
               !strcmp(track->codec_id, MKV_A_AAC_4SSR) ||
               !strcmp(track->codec_id, MKV_A_AAC_4LTP) ||
               !strcmp(track->codec_id, MKV_A_AAC))
        track->a_formattag = mmioFOURCC('M', 'P', '4', 'A');
      else if (!strcmp(track->codec_id, MKV_A_VORBIS))
        {
          if (track->private_data == NULL)
            return 1;
          track->a_formattag = mmioFOURCC('v', 'r', 'b', 's');
        }
      else if (!strcmp(track->codec_id, MKV_A_QDMC))
        track->a_formattag = mmioFOURCC('Q', 'D', 'M', 'C');
      else if (!strcmp(track->codec_id, MKV_A_QDMC2))
        track->a_formattag = mmioFOURCC('Q', 'D', 'M', '2');
      else if (!strcmp(track->codec_id, MKV_A_WAVPACK))
        track->a_formattag = mmioFOURCC('W', 'V', 'P', 'K');
      else if (!strcmp(track->codec_id, MKV_A_TRUEHD))
        track->a_formattag = mmioFOURCC('T', 'R', 'H', 'D');
      else if (!strcmp(track->codec_id, MKV_A_FLAC))
        {
          if (track->private_data == NULL || track->private_size == 0)
            {
              mp_msg (MSGT_DEMUX, MSGL_WARN,
                      MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders);
              return 1;
            }
          track->a_formattag = mmioFOURCC ('f', 'L', 'a', 'C');
        }
      else if (track->private_size >= RAPROPERTIES4_SIZE)
        {
          if (!strcmp(track->codec_id, MKV_A_REAL28))
            track->a_formattag = mmioFOURCC('2', '8', '_', '8');
          else if (!strcmp(track->codec_id, MKV_A_REALATRC))
            track->a_formattag = mmioFOURCC('a', 't', 'r', 'c');
          else if (!strcmp(track->codec_id, MKV_A_REALCOOK))
            track->a_formattag = mmioFOURCC('c', 'o', 'o', 'k');
          else if (!strcmp(track->codec_id, MKV_A_REALDNET))
            track->a_formattag = mmioFOURCC('d', 'n', 'e', 't');
          else if (!strcmp(track->codec_id, MKV_A_REALSIPR))
            track->a_formattag = mmioFOURCC('s', 'i', 'p', 'r');
        }
      else
        {
          mp_msg (MSGT_DEMUX, MSGL_WARN, MSGTR_MPDEMUX_MKV_UnknownAudioCodec,
                  track->codec_id, track->tnum);
          return 1;
        }
    }

  finfowf->wFormatTag = track->a_formattag;
  finfowf->nChannels = track->a_channels;
  finfowf->nSamplesPerSec = (uint32_t) track->a_sfreq;
  if (track->a_bps == 0)
    {
      finfowf->wBitsPerSample = 16;
    }
  else
    {
      finfowf->wBitsPerSample = track->a_bps;
    }
  if (track->a_formattag == 0x0055)  /* MP3 || MP2 */
    {
      finfowf->nAvgBytesPerSec = 16000;
      finfowf->nBlockAlign = 1152;
    }
  else if (track->a_formattag == 0x2000 )/* AC3 */
    {
    }
  else if (track->a_formattag == AUDIO_EAC3 )/* EAC3 */
    {
    }
  else if (track->a_formattag == 0x2001) /* DTS */
    {
      //dts_packet = 1;
    }
  else if (track->a_formattag == 0x0001)  /* PCM || PCM_BE */
    {
      finfowf->nAvgBytesPerSec = track->a_channels * track->a_sfreq * 2;
      finfowf->nBlockAlign = finfowf->nAvgBytesPerSec;
	  /*
      if (!strcmp(track->codec_id, MKV_A_PCM_BE))
        sh_a->format = mmioFOURCC('t', 'w', 'o', 's');
		*/
    }
  else if (!strcmp(track->codec_id, MKV_A_QDMC) ||
           !strcmp(track->codec_id, MKV_A_QDMC2))
    {
      finfowf->nAvgBytesPerSec = 16000;
      finfowf->nBlockAlign = 1486;
      track->fix_i_bps = 1;
      track->qt_last_a_pts = 0.0;
    }
  else if (track->a_formattag == mmioFOURCC('M', 'P', '4', 'A'))
    {
      int profile, srate_idx;

      finfowf->nAvgBytesPerSec = 16000;
      finfowf->nBlockAlign = 1024;

      if (!strcmp (track->codec_id, MKV_A_AAC) &&
          (NULL != track->private_data))
        {
          return 0;
        }

      /* Recreate the 'private data' */
      /* which faad2 uses in its initialization */
      srate_idx = aac_get_sample_rate_index (track->a_sfreq);
      if (!strncmp (&track->codec_id[12], "MAIN", 4))
        profile = 0;
      else if (!strncmp (&track->codec_id[12], "LC", 2))
        profile = 1;
      else if (!strncmp (&track->codec_id[12], "SSR", 3))
        profile = 2;
      else
        profile = 3;

      if (strstr(track->codec_id, "SBR") != NULL)
        {
          /* HE-AAC (aka SBR AAC) */
          track->default_duration = 1024.0 / finfowf->nSamplesPerSec;
          finfowf->nSamplesPerSec *= 2;
        }
      else
        {
          track->default_duration = 1024.0 / (float)finfowf->nSamplesPerSec;
        }
    }
  else if (track->a_formattag == mmioFOURCC('v', 'r', 'b', 's'))  /* VORBIS */
    {
      //finfowf->cbSize = track->private_size;
    }
  else if (track->private_size >= RAPROPERTIES4_SIZE
           && !strncmp (track->codec_id, MKV_A_REALATRC, 7))
    {
      /* Common initialization for all RealAudio codecs */
      unsigned char *src = (unsigned char *)track->private_data;
      int codecdata_length, version;
      int flavor;

      finfowf->nAvgBytesPerSec = 0;  /* FIXME !? */

      version = AV_RB16(src + 4);
      flavor = AV_RB16(src + 22);
      track->coded_framesize = AV_RB32(src + 24);
      track->sub_packet_h = AV_RB16(src + 40);
      finfowf->nBlockAlign =
      track->audiopk_size = AV_RB16(src + 42);
      track->sub_packet_size = AV_RB16(src + 44);
      if (version == 4)
        {
          src += RAPROPERTIES4_SIZE;
          src += src[0] + 1;
          src += src[0] + 1;
        }
      else
        src += RAPROPERTIES5_SIZE;

      src += 3;
      if (version == 5)
        src++;
      codecdata_length = AV_RB32(src);
      src += 4;

      switch (track->a_formattag) {
        case mmioFOURCC('a', 't', 'r', 'c'):
          finfowf->nAvgBytesPerSec = atrc_fl2bps[flavor];
          finfowf->nBlockAlign = track->sub_packet_size;
          break;
        case mmioFOURCC('c', 'o', 'o', 'k'):
          finfowf->nAvgBytesPerSec = cook_fl2bps[flavor];
          finfowf->nBlockAlign = track->sub_packet_size;
          break;
        case mmioFOURCC('s', 'i', 'p', 'r'):
          finfowf->nAvgBytesPerSec = sipr_fl2bps[flavor];
          finfowf->nBlockAlign = track->coded_framesize;
          break;
        case mmioFOURCC('2', '8', '_', '8'):
          finfowf->nAvgBytesPerSec = 3600;
          finfowf->nBlockAlign = track->coded_framesize;
          break;
      }

      track->realmedia = 1;
    }
  else if (!strcmp(track->codec_id, MKV_A_FLAC) ||
           (track->a_formattag == 0xf1ac))
    {
      unsigned char *ptr;
      int size;
#if 1	//Fuchun 2010.06.23
	if(track->a_formattag == mmioFOURCC('f', 'L', 'a', 'C'))
	{
		ptr = (unsigned char *)track->private_data;
		size = track->private_size;
	}
	else
	{
		//sh_a->format = mmioFOURCC('f', 'L', 'a', 'C');
		ptr = (unsigned char *) track->private_data + sizeof (WAVEFORMATEX);
		size = track->private_size - sizeof (WAVEFORMATEX);
	}
	if(size < 4 || ptr[0] != 'f' || ptr[1] != 'L' ||ptr[2] != 'a' || ptr[3] != 'C')
	{
		//finfowf->cbSize = 4;
	}
	else
	{
		//finfowf->cbSize = size;
	}
#else
      free(finfowf);
      finfowf = NULL;

      if (track->a_formattag == mmioFOURCC('f', 'L', 'a', 'C'))
        {
          ptr = (unsigned char *)track->private_data;
          size = track->private_size;
        }
      else
        {
          sh_a->format = mmioFOURCC('f', 'L', 'a', 'C');
          ptr = (unsigned char *) track->private_data
            + sizeof (WAVEFORMATEX);
          size = track->private_size - sizeof (WAVEFORMATEX);
        }
      if (size < 4 || ptr[0] != 'f' || ptr[1] != 'L' ||
          ptr[2] != 'a' || ptr[3] != 'C')
        {
          dp = new_demux_packet (4);
          memcpy (dp->buffer, "fLaC", 4);
        }
      else
        {
          dp = new_demux_packet (size);
          memcpy (dp->buffer, ptr, size);
        }
      dp->pts = 0;
      dp->flags = 0;
      ds_add_packet (demuxer->audio, dp);
#endif
    }
  else if (track->a_formattag == mmioFOURCC('W', 'V', 'P', 'K') ||
           track->a_formattag == mmioFOURCC('T', 'R', 'H', 'D'))
    {  /* do nothing, still works */  }
  else if (!track->ms_compat || (track->private_size < sizeof(WAVEFORMATEX)))
    {
      return 1;
    }

  return 0;
}

int MKV_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet = 0, cont = 0, ii, jj;
	int is_hw_notsupport = 0;
	nRet = mkv_check_file(fp, finfo);
	mkv_track_t *track;

	if (nRet == 0)
		return nRet;

	mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] Found the head...\n");

	if (ebml_read_id (fp, NULL) != MATROSKA_ID_SEGMENT)
	{
		mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] but no segment :(\n");
		return 0;
	}
	ebml_read_length (fp, NULL);  /* return bytes number until EOF */

	mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] + a segment...\n");

	mkv_demuxer_t *mkv_d = (mkv_demuxer_t *)calloc (1, sizeof (mkv_demuxer_t));
	finfo->priv = mkv_d;
	mkv_d->tc_scale = 1000000;
	mkv_d->segment_start = read_tell (fp);
	mkv_d->parsed_cues = (off_t *)malloc (sizeof (off_t));
	mkv_d->parsed_seekhead = (off_t *)malloc (sizeof (off_t));

	while (!cont)
	{
		switch (ebml_read_id (fp, NULL))
		{
			case MATROSKA_ID_INFO:
				mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ segment information...\n");
				cont = demux_mkv_read_info (fp, mkv_d);
				break;

			case MATROSKA_ID_TRACKS:
				mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ segment tracks...\n");
				cont = demux_mkv_read_tracks (fp, mkv_d);
				break;

			case MATROSKA_ID_CUES:
			case MATROSKA_ID_TAGS:
			case MATROSKA_ID_SEEKHEAD:
			case MATROSKA_ID_CHAPTERS:
			case MATROSKA_ID_ATTACHMENTS:
				ebml_read_skip (fp, NULL);
				break;
			case MATROSKA_ID_CLUSTER:
				mp_msg (MSGT_DEMUX, MSGL_V, "[mkv] |+ found cluster, headers are "
						"parsed completely :)\n");
				demux_mkv_read_cluster(fp, mkv_d);
				cont = 1;
				break;

			default:
				cont = 1;
			case EBML_ID_VOID:
				ebml_read_skip (fp, NULL);
				break;
		}
	}

	/* select video track */
	track = NULL;
	/* search for a video track that has the 'default' flag set */
	/*
	for (ii=0; ii<mkv_d->num_tracks; ii++)
		if (mkv_d->tracks[ii]->type == MATROSKA_TRACK_VIDEO
				&& mkv_d->tracks[ii]->default_track)
		{
			if (mkv_d->tracks[ii]->hw_notsupport == 1)
			{
				is_hw_notsupport = 1;
				break;
			} else {
				track = mkv_d->tracks[ii];
				break;
			}
		}
		*/

	if (track == NULL)
	{
		int comp_algo_support = 1;
		/* no track has the 'default' flag set */
		/* let's take the first video track */
		for (ii=0; ii<mkv_d->num_tracks; ii++)
		{
			if (mkv_d->tracks[ii]->type == MATROSKA_TRACK_VIDEO)
			{
				if(mkv_d->tracks[ii]->hw_notsupport == 1)
				{
					is_hw_notsupport = 1;
				} else {
					track = mkv_d->tracks[ii];

					for (jj=0; jj < track->num_encodings; jj++)
					{
						mp_msg(MSGT_DEMUX, MSGL_V, "num_encodings: %d, comp algo: %d\n",
								track->num_encodings,
								track->encodings->comp_algo);
                      if (track->encodings->comp_algo != 0 && track->encodings->comp_algo != 2 &&track->encodings->comp_algo != 3)
					  {
						  printf("MKV: Video track %d has been compressed\n", track->tnum);
						  comp_algo_support = 0;
					  }
					}
					if (comp_algo_support == 0)
						continue;
					else
					{
						comp_algo_support = 1;
						break;
					}
				}
			}
		}
		if (comp_algo_support == 0)
		{
			goto mkv_parse_err;
		}
	}
	if (track != NULL)
	{
		if (demux_mkv_open_video(finfo, track) == 0)
		{
			finfo->bVideo = 1;
			finfo->FileDuration = mkv_d->duration;
			nRet = 1;
		}
	}

	if ((track == NULL) && (is_hw_notsupport == 1))
	{
		// the only video is not support.
	}
	else 
	{

		/* select audio track */
		track = NULL;
		/* search for an audio track that has the 'default' flag set */
		/*
		for (ii=0; ii < mkv_d->num_tracks; ii++)
		{
			if (mkv_d->tracks[ii]->type == MATROSKA_TRACK_AUDIO
					&& mkv_d->tracks[ii]->default_track)
			{
				track = mkv_d->tracks[ii];
				break;
			}
		}
		*/

		if (track == NULL)
		{
			/* no track has the 'default' flag set */
			/* let's take the first audio track */
			for (ii=0; ii < mkv_d->num_tracks; ii++)
			{
				if (mkv_d->tracks[ii]->type == MATROSKA_TRACK_AUDIO)
				{
					track = mkv_d->tracks[ii];
					int comp_algo_support = 1;

					for (jj=0; jj < track->num_encodings; jj++)
					{
						mp_msg(MSGT_DEMUX, MSGL_V, "num_encodings: %d, comp algo: %d\n",
								track->num_encodings,
								track->encodings->comp_algo);
                      if (track->encodings->comp_algo != 0 && track->encodings->comp_algo != 2 &&track->encodings->comp_algo != 3)
					  {
						  printf("MKV: Audio track %d has been compressed\n", track->tnum);
						  comp_algo_support = 0;
					  }
					}
					if (demux_mkv_open_audio(finfo, track) == 0)
					{
						finfo->bAudio = 1;
						finfo->AudioDuration = mkv_d->duration;
						finfo->AudioType = track->a_formattag;
						if (comp_algo_support == 1)
						{
							nRet = 1;
							if (check_audio_type(track->a_formattag, track->a_channels, finfo->hw_a_flag) == 1)
							{
								break;
							}
						} else {
							nRet = 0;
						}
					} else {
						nRet = 0;
					}
				}
			}
		}
	}

mkv_parse_err:
	if (mkv_d != NULL)
	{
		demux_close_mkv(mkv_d);
		finfo->priv = NULL;
	}

	return nRet;
}

