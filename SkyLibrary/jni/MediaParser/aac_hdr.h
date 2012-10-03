#ifndef AAC_HDR_H
#define AAC_HDR_H

// AAC Parser
enum{ AAC_ADIF, AAC_ADTS };

typedef struct 
{
    int num_ele;
    int ele_is_cpe[16];
    int ele_tag[16];
} EleList;

typedef struct 
{
    int present;
    int ele_tag;
    int pseudo_enab;
} MIXdown;

typedef struct 
{
    int profile;
    int sampling_rate_idx;
    int nChannels;
//	char comments[(1<<8)+1];
    long buffer_fullness;	// put this transport level info here 
	EleList front;
    EleList side;
    EleList back;
    EleList lfe;
    EleList data;
    EleList coupling;
    MIXdown mono_mix;
    MIXdown stereo_mix;
    MIXdown matrix_mix;
    
} ProgConfig;

// audio data transport stream frame format header
typedef struct
{
	int	syncword;
	int	id;
	int	layer;
	int	protection_abs;
	int profile;
	int	sampling_freq_idx;
	int	private_bit;
	int	channel_config;
	int	original_copy;
	int	home;
	int	copyright_id_bit;
	int	copyright_id_start;
	int	frame_length;
	int	adts_buffer_fullness;
	int	num_of_rdb;
	int	crc_check;
} ADTSHeader;

// audio data interchange format header
typedef struct 
{
    char    adif_id[5];
    int	    copy_id_present;
    char    copy_id[10];
    int	    original_copy;
    int	    home;
    int	    bitstream_type;
    long    bitrate;		// bps
    int	    num_pce;
//    int	    prog_tags[16];	//(1<<4)

	ProgConfig prog_config;

} ADIFHeader;

typedef struct
{
	ADTSHeader adtsHeader;
	ADIFHeader adifHeader;
	
	int nAACFormat;
	int	nADIFHeaderLength;
	int nChannels;
	int nSamplingFreq;
	int nBitRate;			// kbps
	
} AAC_INFO;

int mp_get_aac_header(unsigned char *buf, int bufsize, AAC_INFO *AacInfo);

#endif // AAC_HDR_H
