#ifndef COMMON_H
#define COMMON_H

#define MAX_A_STREAMS 256
#define MAX_V_STREAMS 256

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define AV_LZO_INPUT_PADDING 8

#ifdef __GNUC__
#    define AV_GCC_VERSION_AT_LEAST(x,y) (__GNUC__ > x || __GNUC__ == x && __GNUC_MINOR__ >= y)
#else
#    define AV_GCC_VERSION_AT_LEAST(x,y) 0
#endif

#ifndef av_always_inline
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define av_always_inline __attribute__((always_inline)) inline
#else
#    define av_always_inline inline
#endif
#endif

#ifndef av_const
#if AV_GCC_VERSION_AT_LEAST(2,6)
#    define av_const __attribute__((const))
#else
#    define av_const
#endif
#endif

#ifndef mmioFOURCC
#define mmioFOURCC( ch0, ch1, ch2, ch3 )                \
	        ( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |    \
			          ( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )
#endif

#ifndef av_alias
#if HAVE_ATTRIBUTE_MAY_ALIAS && (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(3,3)
#   define av_alias __attribute__((may_alias))
#else
#   define av_alias
#endif
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

typedef enum
{
        UNKNOWN         = 0,
        VIDEO_MPEG1     = 0x10000001,
        VIDEO_MPEG2     = 0x10000002,
        VIDEO_MPEG4     = 0x10000004,
        VIDEO_H264      = 0x10000005,
        VIDEO_AVC       = mmioFOURCC('a', 'v', 'c', '1'),
        VIDEO_VC1       = mmioFOURCC('W', 'V', 'C', '1'),
        AUDIO_MP2       = 0x50,
        AUDIO_A52       = 0x2000,
        AUDIO_DTS       = 0x2001,
        AUDIO_LPCM_BE   = 0x10001,
		AUDIO_BPCM		= mmioFOURCC('B', 'P', 'C', 'M'),
        AUDIO_AAC       = mmioFOURCC('M', 'P', '4', 'A'),
        AUDIO_TRUEHD    = mmioFOURCC('T', 'R', 'H', 'D'),
        AUDIO_EAC3		= mmioFOURCC('e', 'a', 'c', '3'),
        AUDIO_OGG		= mmioFOURCC('v', 'r', 'b', 's'),
        SPU_DVD         = 0x3000000,
        SPU_DVB         = 0x3000001,
        SPU_TELETEXT    = 0x3000002,
		SPU_PGS			= 0x3000003,
        PES_PRIVATE1    = 0xBD00000,
        SL_PES_STREAM   = 0xD000000,
        SL_SECTION      = 0xD100000,
        MP4_OD          = 0xD200000,
} es_stream_type_t;

void set_fourcc(char *buf, es_stream_type_t type);
int check_audio_type(uint32_t type, uint32_t channel, unsigned char a_flag);

#endif // COMMON_H
