
#ifndef _MEDIA_PARSER_H_
#define	_MEDIA_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "type.h"

#if 0
typedef unsigned long long uint64_t;
typedef long long int64_t;
#endif

#if defined(HAVE_ANDROID_OS)
#ifndef PRIu64
#define PRIu64 "llu"
#endif // PRIu64
#define LOFF_T loff_t
#else
#ifndef PRIu64
#define PRIu64 "llu"
#endif // PRIu64
#define LOFF_T off_t
#endif // HAVE_ANDROID_OS

typedef struct tagBITMAP_INFO_HEADER 
{
	uint32_t	biSize;
	uint32_t	biWidth;
	uint32_t	biHeight;
	uint16_t	biPlanes;
	uint16_t	biBitCount;
	uint32_t	biCompression;
	uint32_t	biSizeImage;
	uint32_t	biXPelsPerMeter;
	uint32_t	biYPelsPerMeter;
	uint32_t	biClrUsed;
	uint32_t	biClrImportant;
} BITMAP_INFO_HEADER;

typedef struct 
{
	uint16_t	wFormatTag;
	uint16_t	nChannels;
	uint32_t	nSamplesPerSec;
	uint32_t	nAvgBytesPerSec;
	uint16_t	nBlockAlign;
	uint16_t	wBitsPerSample;
//	uint16_t	cbSize;
} WAVEFORMATEX;

typedef struct ID3frameinfo
{
	char Title[LEN_XXL];
	unsigned char Title_encode_type;		// 0: local, 16: UTF-16, 8: UTF-8
	char Artist[LEN_XXL];
	unsigned char Artist_encode_type;
	char Album[LEN_XXL];
	unsigned char Album_encode_type;
	char Year[16];
	unsigned char Year_encode_type;
	char Comment[LEN_XXL];
	unsigned char Comment_encode_type;
	char Track[16];
	unsigned char Track_encode_type;
	unsigned int Genre;
	char GenreString[LEN_XXL];
	unsigned char GenreString_encode_type;
	unsigned char has_pic;					// 0: no picture, 1: has picture
} ID3_FRAME_INFO;

typedef struct
{
	// Raymond 2008/11/03
	char * filepath;
	uint32_t	ProductType;
	uint32_t	DispWidth;
    uint32_t	DispHeight;

	LOFF_T		FileSize;
	uint32_t	FileType;
	uint32_t	SubFileNameIndex;	// Raymond 2007/07/07
    uint32_t	VideoType;
    uint32_t	AudioType;
    uint32_t	dwWidth;
    uint32_t	dwHeight;
	uint32_t	vFrame;
	float		FPS;
	float		vBitrate;
	uint32_t	aBitrate;
	uint32_t	H264Profile;
	int		FileDuration;
	int		AudioDuration;

	int bVideo;
	int bVideoSupported;
	int bAudio;
	int bAudioSupported;

	BITMAP_INFO_HEADER bih;	// video info
	WAVEFORMATEX wf;		// audio info

	char	*VideoFormat;	// Raymond 2007/10/26
	
	int			bTag;
	int			bEncrypted;
	int			nID3Length;
	ID3_FRAME_INFO	ID3Tag;
	int			ID3_version;
	void * priv;
	unsigned char hw_v_flag;
	unsigned char hw_a_flag;

} FileInfo;

// Raymond 2008/11/03
enum PRODUCT_TYPE
{
	SK8860,
	SK8850,
	SK8855,
};

enum FILE_TYPE
{
	FILE_TYPE_UNKNOWN_FILE,
// Video
	FILE_TYPE_AVI,
	FILE_TYPE_MPEG_PS,
	FILE_TYPE_FLV,		// Raymond 2007/11/05
	FILE_TYPE_SWF,		// Raymond 2008/11/27
	FILE_TYPE_RM,		// Raymond 2007/11/21		
	FILE_TYPE_ASF,
	FILE_TYPE_TS,	
	FILE_TYPE_MKV,
	FILE_TYPE_ISO,
	FILE_TYPE_BD,
	FILE_TYPE_MOV,	
// Audio
	FILE_TYPE_AAC,
	FILE_TYPE_MP3,
	FILE_TYPE_WAV,
	FILE_TYPE_RA,	
	FILE_TYPE_FLAC,	
	FILE_TYPE_AC3,	
	FILE_TYPE_AMR,	
	FILE_TYPE_OGG,	
	FILE_TYPE_APE,	
// Directory
	FILE_TYPE_DIRECTORY
};

static struct 
{
	const char *extension;
    int demuxer_type;
} extensions_table[] = 
{
// Video
    { "mpeg", FILE_TYPE_MPEG_PS },
    { "mpg", FILE_TYPE_MPEG_PS },
    { "mpe", FILE_TYPE_MPEG_PS },
    { "vob", FILE_TYPE_MPEG_PS },
    { "m2v", FILE_TYPE_MPEG_PS },	// 5
	{ "dat", FILE_TYPE_MPEG_PS },
	{ "m4v", FILE_TYPE_MPEG_PS },
	{ "264", FILE_TYPE_MPEG_PS },

    { "avi", FILE_TYPE_AVI },
	{ "divx", FILE_TYPE_AVI },

    { "mov", FILE_TYPE_MOV },
	{ "mp4", FILE_TYPE_MOV },		// 10
    { "3gp", FILE_TYPE_MOV },
	{ "3g2", FILE_TYPE_MOV },

	{ "flv", FILE_TYPE_FLV },	// Raymond 2007/11/05
	{ "swf", FILE_TYPE_SWF },	// Raymond 2008/11/27

	{ "wmv", FILE_TYPE_ASF },		// 15

	{ "rmvb", FILE_TYPE_RM },	// Raymond 2007/11/21
	{ "rm", FILE_TYPE_RM },		// Raymond 2007/11/21
	{ "ram", FILE_TYPE_RM },	// Raymond 2007/11/21
	{ "rma", FILE_TYPE_RM },	// Raymond 2007/11/22
	{ "ts", FILE_TYPE_TS },			// 20
	{ "trp", FILE_TYPE_TS },
	{ "m2ts", FILE_TYPE_TS },
	{ "mts", FILE_TYPE_TS },
	{ "pt", FILE_TYPE_TS },
	{ "mkv", FILE_TYPE_MKV },
	{ "iso", FILE_TYPE_ISO },
	{ "iso", FILE_TYPE_BD },
	{ "ogm", FILE_TYPE_OGG },
	
//    { "asx", FILE_TYPE_ASF },
    { "asf", FILE_TYPE_ASF },		// 25

// Audio	
	{ "m4a", FILE_TYPE_MOV },
	{ "wma", FILE_TYPE_ASF },
	{ "aac", FILE_TYPE_AAC },
	{ "mp3", FILE_TYPE_MP3 },
	{ "wav", FILE_TYPE_WAV },  
	{ "ogg", FILE_TYPE_OGG },		// 31
	{ "ra", FILE_TYPE_RA },		// mingyu 2010/7/1
	{ "flac", FILE_TYPE_FLAC },
	{ "amr", FILE_TYPE_AMR },
	{ "ape", FILE_TYPE_APE },
	{ "ac3", FILE_TYPE_AC3 }
};

static const char * file_format_info[] = 
{
	"File format not support",	
// Video
    "AVI file format",
	"MPG file format",
	"FLV file format",		// Raymond 2007/11/05
	"SWF file format",		// Raymond 2008/11/27
	"RM file format",		// Raymond 2007/11/21
    "ASF/WMV/WMA file format",
	"MOV/MP4/3GP file format",    		
// Audio
	"AAC file format",
	"MP3 file format",
	"WAV file format",
	"OGG file format",
};

enum PARSER_RET
{
	PARSER_FILE_NOT_SUPPORTED,
	PARSER_VIDEO_CODEC_NOT_SUPPORTED,
	PARSER_AUDIO_CODEC_NOT_SUPPORTED,
	PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED,
	PARSER_VIDEO_DIVX_VERSION_NOT_SUPPORTED,
	PARSER_VIDEO_FILE_SUPPORTED,
	PARSER_AUDIO_FILE_SUPPORTED,
};

static struct 
{
	uint32_t file_type;
	const char *fourcc;
	const char *video_format;		// Raymond 2007/10/26
} VideoFourCC[] = 
{
	// MJPEG
	{ FILE_TYPE_AVI,		"MJPG",	"M-JPEG" },	
	{ FILE_TYPE_AVI,		"AVRn",	"M-JPEG" },
		
	{ FILE_TYPE_AVI,		"mpg2", "MPEG-2" },	

	{ FILE_TYPE_AVI,		"XVID", "MPEG-4" },	
	{ FILE_TYPE_AVI,		"xvid", "MPEG-4" },	

	{ FILE_TYPE_AVI,		"DX50", "MPEG-4" },	// DivX 5.X
	{ FILE_TYPE_AVI,		"DIVX", "MPEG-4" },	// DivX 4.X

	{ FILE_TYPE_AVI,        "M4S2", "MPEG-4" },       // Raymond 2007/09/17 - for MagicPixel's sample
	{ FILE_TYPE_AVI,        "WMV3", "VC-1" },
	{ FILE_TYPE_AVI,        "WVC1", "VC-1" },
	{ FILE_TYPE_AVI,        "avc1", "H264" },
	{ FILE_TYPE_AVI,        "H264", "H264" },
	{ FILE_TYPE_AVI,        "DIV3", "MPEG-4" },
	{ FILE_TYPE_AVI,        "MPEG", "MPEG-1" },
	{ FILE_TYPE_AVI,		"mjpa",	"M-JPEG" },
	{ FILE_TYPE_AVI,		"MP43",	"MPEG-4" },
	{ FILE_TYPE_AVI,		"MP4S",	"MPEG-4" },
	{ FILE_TYPE_AVI,		"FMP4",	"MPEG-4" },

	{ FILE_TYPE_MPEG_PS,	"MPG1", "MPEG-1" },
	{ FILE_TYPE_MPEG_PS,	"MPG2", "MPEG-2" },
	{ FILE_TYPE_MPEG_PS,	"MPG4", "MPEG-4" },	// Raymond 2008/12/11

	{ FILE_TYPE_ASF,		"WMV3", "VC-1" },
	{ FILE_TYPE_ASF,		"WVC1", "VC-1" },	
	{ FILE_TYPE_ASF,		"DIVX", "MPEG-4" },
	{ FILE_TYPE_ASF,		"DIV3", "MPEG-4" },
	{ FILE_TYPE_ASF,		"MP43",	"MPEG-4" },
	{ FILE_TYPE_ASF,		"MP4S",	"MPEG-4" },

	{ FILE_TYPE_MOV,		"mp4v", "MPEG-4" },	
	{ FILE_TYPE_MOV,		"s263",	"H.263" },	
	{ FILE_TYPE_MOV,		"avc1",	"H.264" },	
	{ FILE_TYPE_MOV,		"jpeg",	"M-JPEG" },	
	{ FILE_TYPE_MOV,		"AVDJ",	"M-JPEG" },
	{ FILE_TYPE_MOV,		"h263",	"H.263" },	// Raymond 2007/06/05 - force to 352x288
	{ FILE_TYPE_MOV,		"mjpa",	"M-JPEG" },

	{ FILE_TYPE_FLV,		"FLV1",	"H.263" },	// Raymond 2007/11/05
	{ FILE_TYPE_FLV,		"vp6a",	"VP6A" },
	{ FILE_TYPE_FLV,		"vp6f",	"VP6F" },
	{ FILE_TYPE_FLV,		"avc1",	"H.264" },
	{ FILE_TYPE_FLV,		"H264",	"VC-1" },
	{ FILE_TYPE_SWF,		"FLV1",	"H.263" },	// Raymond 2008/11/27
	{ FILE_TYPE_RM,         "RV30", "RM" },
	{ FILE_TYPE_RM,         "RV40", "RM" },
	{ FILE_TYPE_TS,         "MPG1", "MPEG-1" },
	{ FILE_TYPE_TS,         "MPG2", "MPEG-2" },
	{ FILE_TYPE_TS,         "MPG4", "MPEG-4" },
	{ FILE_TYPE_TS,         "H264", "H.264" },
	{ FILE_TYPE_TS,         "avc1", "H.264" },
	{ FILE_TYPE_TS,         "WMV3", "VC-1" },
	{ FILE_TYPE_TS,         "WVC1", "VC-1" },
	{ FILE_TYPE_MKV,        "XVID", "MPEG-4" },
	{ FILE_TYPE_MKV,        "H264", "H.264" },
	{ FILE_TYPE_MKV,        "avc1", "H.264" },
	{ FILE_TYPE_MKV,        "RV40", "RM" },
	{ FILE_TYPE_MKV,		"mp4v", "MPEG-4" },	
	{ FILE_TYPE_MKV,		"mpg2", "MPEG-2" },	
	{ FILE_TYPE_MKV,        "WMV3", "VC-1" },
	{ FILE_TYPE_MKV,        "WVC1", "VC-1" },
	{ FILE_TYPE_MKV,        "DIV3", "MPEG-4" },
	{ FILE_TYPE_MKV,        "DX50", "MPEG-4" },
	{ FILE_TYPE_MKV,        "DIVX", "MPEG-4" },
	{ FILE_TYPE_ISO,        "MPG1", "MPEG-1" },
	{ FILE_TYPE_ISO,        "MPG2", "MPEG-2" },
	{ FILE_TYPE_BD,         "MPG1", "MPEG-1" },
	{ FILE_TYPE_BD,         "MPG2", "MPEG-2" },
	{ FILE_TYPE_BD,         "MPG4", "MPEG-4" },
	{ FILE_TYPE_BD,         "H264", "H.264" },
	{ FILE_TYPE_BD,         "avc1", "H.264" },
	{ FILE_TYPE_BD,         "WVC1", "VC-1" },
	{ FILE_TYPE_OGG,        "XVID", "MPEG-4" },
	{ FILE_TYPE_OGG,        "DX50", "MPEG-4" },
};

static struct 
{
	uint32_t file_type;
	const char *fourcc;
} AudioFourCC[] = 
{	
	{ FILE_TYPE_MOV,	"samr" },	// AMR-NB	
	{ FILE_TYPE_MOV,	"sawb" },	// AMR-WB	
	{ FILE_TYPE_MOV,	"mp4a" },	// AAC 

	// not support - this is not actual FourCC
//	{ FILE_TYPE_MOV,	"MP3 " },	// MP3	
//	{ FILE_TYPE_MOV,	"QCLP" },	// QCELP	
//	{ FILE_TYPE_MOV,	"vrbs" },	// AAC Vorbis
//	{ FILE_TYPE_MOV,	"m4a " },	// Multi-channel MP3	
};

static uint16_t AudioFormatTag[] = 
{
	0x1,	// PCM
	0x2,	// WAVE_FORMAT_ADPCM
	0x3,	// WAVE_FORMAT_IEEE_FLOAT
	0x6,	// WAVE_FORMAT_ALAW
	0x7,	// WAVE_FORMAT_MULAW
	0x11,	// IMA ADPCM
	0x50,	// WAVE_FORMAT_MPEG			, MP2
	0x55,	// WAVE_FORMAT_MPEGLAYER3	, MP3	
	0x161,	// WMA
};

// Raymond 2008/05/30
#define ID3_NR_OF_V1_GENRES 148

static const char *ID3_v1_genre_description[ID3_NR_OF_V1_GENRES] =
{
  "Blues", //0
  "Classic Rock", //1
  "Country", //2
  "Dance", //3
  "Disco", //4
  "Funk", //5
  "Grunge", //6
  "Hip-Hop", //7
  "Jazz", //8
  "Metal", //9
  "New Age", //10
  "Oldies", //11
  "Other", //12
  "Pop", //13
  "R&B", //14
  "Rap", //15
  "Reggae", //16
  "Rock", //17
  "Techno", //18
  "Industrial", //19
  "Alternative", //20
  "Ska", //21
  "Death Metal", //22
  "Pranks", //23
  "Soundtrack", //24
  "Euro-Techno", //25
  "Ambient", //26
  "Trip-Hop", //27
  "Vocal", //28
  "Jazz+Funk", //29
  "Fusion", //30
  "Trance", //31
  "Classical", //32
  "Instrumental", //33
  "Acid", //34
  "House", //35
  "Game", //36
  "Sound Clip", //37
  "Gospel", //38
  "Noise", //39
  "AlternRock", //40
  "Bass", //41
  "Soul", //42
  "Punk", //43
  "Space", //44
  "Meditative", //45
  "Instrumental Pop", //46
  "Instrumental Rock", //47
  "Ethnic", //48
  "Gothic", //49
  "Darkwave", //50
  "Techno-Industrial", //51
  "Electronic", //52
  "Pop-Folk", //53
  "Eurodance", //54
  "Dream", //55
  "Southern Rock", //56
  "Comedy", //57
  "Cult", //58
  "Gangsta", //59
  "Top 40", //60
  "Christian Rap", //61
  "Pop/Funk", //62
  "Jungle", //63
  "Native American", //64
  "Cabaret", //65
  "New Wave", //66
  "Psychadelic", //67
  "Rave", //68
  "Showtunes", //69
  "Trailer", //70
  "Lo-Fi", //71
  "Tribal", //72
  "Acid Punk", //73
  "Acid Jazz", //74
  "Polka", //75
  "Retro", //76
  "Musical", //77
  "Rock & Roll", //78
  "Hard Rock", //79
// following are winamp extentions
  "Folk", //80
  "Folk-Rock", //81
  "National Folk", //82
  "Swing", //83
  "Fast Fusion", //84
  "Bebob", //85
  "Latin", //86
  "Revival", //87
  "Celtic", //88
  "Bluegrass", //89
  "Avantgarde", //90
  "Gothic Rock", //91
  "Progressive Rock", //92
  "Psychedelic Rock", //93
  "Symphonic Rock", //94
  "Slow Rock", //95
  "Big Band", //96
  "Chorus", //97
  "Easy Listening", //98
  "Acoustic", //99
  "Humour", //100
  "Speech", //101
  "Chanson", //102
  "Opera", //103
  "Chamber Music", //104
  "Sonata", //105
  "Symphony", //106
  "Booty Bass", //107
  "Primus", //108
  "Porn Groove", //109
  "Satire", //110
  "Slow Jam", //111
  "Club", //112
  "Tango", //113
  "Samba", //114
  "Folklore", //115
  "Ballad", //116
  "Power Ballad", //117
  "Rhythmic Soul", //118
  "Freestyle", //119
  "Duet", //120
  "Punk Rock", //121
  "Drum Solo", //122
  "A capella", //123
  "Euro-House", //124
  "Dance Hall", //125
  "Goa", //126
  "Drum & Bass", //127
  "Club-House", //128
  "Hardcore", //129
  "Terror", //130
  "Indie", //131
  "Britpop", //132
  "Negerpunk", //133
  "Polsk Punk", //134
  "Beat", //135
  "Christian Gangsta Rap", //136
  "Heavy Metal", //137
  "Black Metal", //138
  "Crossover", //139
  "Contemporary Christian",//140
  "Christian Rock ", //141
  "Merengue", //142
  "Salsa", //143
  "Trash Metal", //144
  "Anime", //145
  "JPop", //146
  "Synthpop" //147
};

#define ID3_V1GENRE2DESCRIPTION(x) (x < ID3_NR_OF_V1_GENRES && x >= 0) ? ID3_v1_genre_description[x] : NULL 

// end Raymond 2008/05/30

/* WAVE form wFormatTag IDs */

#define  WAVE_FORMAT_UNKNOWN                    0x0000 /* Microsoft Corporation */
#define  WAVE_FORMAT_ADPCM                      0x0002 /* Microsoft Corporation */
#define  WAVE_FORMAT_IEEE_FLOAT                 0x0003 /* Microsoft Corporation */
#define  WAVE_FORMAT_VSELP                      0x0004 /* Compaq Computer Corp. */
#define  WAVE_FORMAT_IBM_CVSD                   0x0005 /* IBM Corporation */
#define  WAVE_FORMAT_ALAW                       0x0006 /* Microsoft Corporation */
#define  WAVE_FORMAT_MULAW                      0x0007 /* Microsoft Corporation */
#define  WAVE_FORMAT_DTS                        0x0008 /* Microsoft Corporation */
#define  WAVE_FORMAT_OKI_ADPCM                  0x0010 /* OKI */
#define  WAVE_FORMAT_DVI_ADPCM                  0x0011 /* Intel Corporation */
#define  WAVE_FORMAT_IMA_ADPCM                  (WAVE_FORMAT_DVI_ADPCM) /*  Intel Corporation */
#define  WAVE_FORMAT_MEDIASPACE_ADPCM           0x0012 /* Videologic */
#define  WAVE_FORMAT_SIERRA_ADPCM               0x0013 /* Sierra Semiconductor Corp */
#define  WAVE_FORMAT_G723_ADPCM                 0x0014 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_DIGISTD                    0x0015 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIGIFIX                    0x0016 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIALOGIC_OKI_ADPCM         0x0017 /* Dialogic Corporation */
#define  WAVE_FORMAT_MEDIAVISION_ADPCM          0x0018 /* Media Vision, Inc. */
#define  WAVE_FORMAT_CU_CODEC                   0x0019 /* Hewlett-Packard Company */
#define  WAVE_FORMAT_YAMAHA_ADPCM               0x0020 /* Yamaha Corporation of America */
#define  WAVE_FORMAT_SONARC                     0x0021 /* Speech Compression */
#define  WAVE_FORMAT_DSPGROUP_TRUESPEECH        0x0022 /* DSP Group, Inc */
#define  WAVE_FORMAT_ECHOSC1                    0x0023 /* Echo Speech Corporation */
#define  WAVE_FORMAT_AUDIOFILE_AF36             0x0024 /* Virtual Music, Inc. */
#define  WAVE_FORMAT_APTX                       0x0025 /* Audio Processing Technology */
#define  WAVE_FORMAT_AUDIOFILE_AF10             0x0026 /* Virtual Music, Inc. */
#define  WAVE_FORMAT_PROSODY_1612               0x0027 /* Aculab plc */
#define  WAVE_FORMAT_LRC                        0x0028 /* Merging Technologies S.A. */
#define  WAVE_FORMAT_DOLBY_AC2                  0x0030 /* Dolby Laboratories */
#define  WAVE_FORMAT_GSM610                     0x0031 /* Microsoft Corporation */
#define  WAVE_FORMAT_MSNAUDIO                   0x0032 /* Microsoft Corporation */
#define  WAVE_FORMAT_ANTEX_ADPCME               0x0033 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_CONTROL_RES_VQLPC          0x0034 /* Control Resources Limited */
#define  WAVE_FORMAT_DIGIREAL                   0x0035 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIGIADPCM                  0x0036 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_CONTROL_RES_CR10           0x0037 /* Control Resources Limited */
#define  WAVE_FORMAT_NMS_VBXADPCM               0x0038 /* Natural MicroSystems */
#define  WAVE_FORMAT_CS_IMAADPCM                0x0039 /* Crystal Semiconductor IMA ADPCM */
#define  WAVE_FORMAT_ECHOSC3                    0x003A /* Echo Speech Corporation */
#define  WAVE_FORMAT_ROCKWELL_ADPCM             0x003B /* Rockwell International */
#define  WAVE_FORMAT_ROCKWELL_DIGITALK          0x003C /* Rockwell International */
#define  WAVE_FORMAT_XEBEC                      0x003D /* Xebec Multimedia Solutions Limited */
#define  WAVE_FORMAT_G721_ADPCM                 0x0040 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_G728_CELP                  0x0041 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_MSG723                     0x0042 /* Microsoft Corporation */
#define  WAVE_FORMAT_MPEG                       0x0050 /* Microsoft Corporation */
#define  WAVE_FORMAT_RT24                       0x0052 /* InSoft, Inc. */
#define  WAVE_FORMAT_PAC                        0x0053 /* InSoft, Inc. */
#define  WAVE_FORMAT_MPEGLAYER3                 0x0055 /* ISO/MPEG Layer3 Format Tag */
#define  WAVE_FORMAT_LUCENT_G723                0x0059 /* Lucent Technologies */
#define  WAVE_FORMAT_CIRRUS                     0x0060 /* Cirrus Logic */
#define  WAVE_FORMAT_ESPCM                      0x0061 /* ESS Technology */
#define  WAVE_FORMAT_VOXWARE                    0x0062 /* Voxware Inc */
#define  WAVE_FORMAT_CANOPUS_ATRAC              0x0063 /* Canopus, co., Ltd. */
#define  WAVE_FORMAT_G726_ADPCM                 0x0064 /* APICOM */
#define  WAVE_FORMAT_G722_ADPCM                 0x0065 /* APICOM */
#define  WAVE_FORMAT_DSAT_DISPLAY               0x0067 /* Microsoft Corporation */
#define  WAVE_FORMAT_VOXWARE_BYTE_ALIGNED       0x0069 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC8                0x0070 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC10               0x0071 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC16               0x0072 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC20               0x0073 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT24               0x0074 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT29               0x0075 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT29HW             0x0076 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_VR12               0x0077 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_VR18               0x0078 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_TQ40               0x0079 /* Voxware Inc */
#define  WAVE_FORMAT_SOFTSOUND                  0x0080 /* Softsound, Ltd. */
#define  WAVE_FORMAT_VOXWARE_TQ60               0x0081 /* Voxware Inc */
#define  WAVE_FORMAT_MSRT24                     0x0082 /* Microsoft Corporation */
#define  WAVE_FORMAT_G729A                      0x0083 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_MVI_MVI2                   0x0084 /* Motion Pixels */
#define  WAVE_FORMAT_DF_G726                    0x0085 /* DataFusion Systems (Pty) (Ltd) */
#define  WAVE_FORMAT_DF_GSM610                  0x0086 /* DataFusion Systems (Pty) (Ltd) */
#define  WAVE_FORMAT_ISIAUDIO                   0x0088 /* Iterated Systems, Inc. */
#define  WAVE_FORMAT_ONLIVE                     0x0089 /* OnLive! Technologies, Inc. */
#define  WAVE_FORMAT_SBC24                      0x0091 /* Siemens Business Communications Sys */
#define  WAVE_FORMAT_DOLBY_AC3_SPDIF            0x0092 /* Sonic Foundry */
#define  WAVE_FORMAT_MEDIASONIC_G723            0x0093 /* MediaSonic */
#define  WAVE_FORMAT_PROSODY_8KBPS              0x0094 /* Aculab plc */
#define  WAVE_FORMAT_ZYXEL_ADPCM                0x0097 /* ZyXEL Communications, Inc. */
#define  WAVE_FORMAT_PHILIPS_LPCBB              0x0098 /* Philips Speech Processing */
#define  WAVE_FORMAT_PACKED                     0x0099 /* Studer Professional Audio AG */
#define  WAVE_FORMAT_MALDEN_PHONYTALK           0x00A0 /* Malden Electronics Ltd. */
#define  WAVE_FORMAT_RHETOREX_ADPCM             0x0100 /* Rhetorex Inc. */
#define  WAVE_FORMAT_IRAT                       0x0101 /* BeCubed Software Inc. */
#define  WAVE_FORMAT_VIVO_G723                  0x0111 /* Vivo Software */
#define  WAVE_FORMAT_VIVO_SIREN                 0x0112 /* Vivo Software */
#define  WAVE_FORMAT_DIGITAL_G723               0x0123 /* Digital Equipment Corporation */
#define  WAVE_FORMAT_SANYO_LD_ADPCM             0x0125 /* Sanyo Electric Co., Ltd. */
#define  WAVE_FORMAT_SIPROLAB_ACEPLNET          0x0130 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_ACELP4800         0x0131 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_ACELP8V3          0x0132 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_G729              0x0133 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_G729A             0x0134 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_KELVIN            0x0135 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_G726ADPCM                  0x0140 /* Dictaphone Corporation */
#define  WAVE_FORMAT_QUALCOMM_PUREVOICE         0x0150 /* Qualcomm, Inc. */
#define  WAVE_FORMAT_QUALCOMM_HALFRATE          0x0151 /* Qualcomm, Inc. */
#define  WAVE_FORMAT_TUBGSM                     0x0155 /* Ring Zero Systems, Inc. */
#define  WAVE_FORMAT_MSAUDIO1                   0x0160 /* Microsoft Corporation */
#define  WAVE_FORMAT_CREATIVE_ADPCM             0x0200 /* Creative Labs, Inc */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH8       0x0202 /* Creative Labs, Inc */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH10      0x0203 /* Creative Labs, Inc */
#define  WAVE_FORMAT_UHER_ADPCM                 0x0210 /* UHER informatic GmbH */
#define  WAVE_FORMAT_QUARTERDECK                0x0220 /* Quarterdeck Corporation */
#define  WAVE_FORMAT_ILINK_VC                   0x0230 /* I-link Worldwide */
#define  WAVE_FORMAT_RAW_SPORT                  0x0240 /* Aureal Semiconductor */
#define  WAVE_FORMAT_IPI_HSX                    0x0250 /* Interactive Products, Inc. */
#define  WAVE_FORMAT_IPI_RPELP                  0x0251 /* Interactive Products, Inc. */
#define  WAVE_FORMAT_CS2                        0x0260 /* Consistent Software */
#define  WAVE_FORMAT_SONY_SCX                   0x0270 /* Sony Corp. */
#define  WAVE_FORMAT_FM_TOWNS_SND               0x0300 /* Fujitsu Corp. */
#define  WAVE_FORMAT_BTV_DIGITAL                0x0400 /* Brooktree Corporation */
#define  WAVE_FORMAT_QDESIGN_MUSIC              0x0450 /* QDesign Corporation */
#define  WAVE_FORMAT_VME_VMPCM                  0x0680 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_TPC                        0x0681 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_OLIGSM                     0x1000 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLIADPCM                   0x1001 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLICELP                    0x1002 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLISBC                     0x1003 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLIOPR                     0x1004 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_LH_CODEC                   0x1100 /* Lernout & Hauspie */
#define  WAVE_FORMAT_NORRIS                     0x1400 /* Norris Communications, Inc. */
#define  WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS    0x1500 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_DVM                        0x2000 /* FAST Multimedia AG */

int MediaParser(const char *in_filename, FileInfo *finfo);

#endif // _MEDIA_PARSER_H_
