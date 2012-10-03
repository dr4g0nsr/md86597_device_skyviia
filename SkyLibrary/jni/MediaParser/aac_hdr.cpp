#include <stdio.h>
#include "read_data.h"
#include "aac_hdr.h"
#include "string.h"

// AAC sampling rate
static int SampleRate[16] = 
{
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 
	22050, 16000, 12000, 11025, 8000, 0, 0, 0, 0
};

// program configuration element 
static int get_ele_list(BitData *bf, EleList *p, int enable_cpe)
{  
    int i, j, nChannels = 0;
    for (i = 0, j = p->num_ele; i<j; i++) 
	{
		if (enable_cpe)		
		{
			if ((p->ele_is_cpe[i] = GetBits(bf, 1)))
				nChannels++;
		}		
		else
			p->ele_is_cpe[i] = 0; // sdb 

		p->ele_tag[i] = GetBits(bf, 4);
		nChannels++;
    }

	return nChannels;
}

static int get_prog_config(BitData *bf, ProgConfig *p)
{
    int tag;
		
	p->nChannels = 0;

    tag = GetBits(bf, 4);

    p->profile				= GetBits(bf, 2);
    p->sampling_rate_idx	= GetBits(bf, 4);
    p->front.num_ele		= GetBits(bf, 4);
    p->side.num_ele			= GetBits(bf, 4);
    p->back.num_ele			= GetBits(bf, 4);
    p->lfe.num_ele			= GetBits(bf, 2);
    p->data.num_ele			= GetBits(bf, 3);
    p->coupling.num_ele		= GetBits(bf, 4);

    if ( ( p->mono_mix.present = GetBits(bf, 1) ) == 1 )
		p->mono_mix.ele_tag = GetBits(bf, 4);
    if ( ( p->stereo_mix.present = GetBits(bf, 1) ) == 1 )
		p->stereo_mix.ele_tag = GetBits(bf, 4);
	
    if ( ( p->matrix_mix.present = GetBits(bf, 1) ) == 1 ) 
	{
		p->matrix_mix.ele_tag		= GetBits(bf, 2);
		p->matrix_mix.pseudo_enab	= GetBits(bf, 1);
    }

    p->nChannels += get_ele_list(bf, &p->front, 1);
    p->nChannels += get_ele_list(bf, &p->side, 1);
    p->nChannels += get_ele_list(bf, &p->back, 1);
    p->nChannels += get_ele_list(bf, &p->lfe, 0);
    
    return 1;
}

static int get_adif_header(BitData* bf, ADIFHeader *p)
{
    int i, n;
    // adif header 
    for (i = 0; i < 4; i++)
		p->adif_id[i] = (char)GetBits(bf, 8); 
    p->adif_id[i] = 0;	    // null terminated string 
    
	// test for id 
    if (strncmp(p->adif_id, "ADIF", 4) != 0)
		return 0;	    // bad id 
    
	// copyright string 
    if ((p->copy_id_present = GetBits(bf, 1)) == 1) 
	{
		for (i = 0; i < 9; i++)
			p->copy_id[i] = (char)GetBits(bf, 8); 
		p->copy_id[i] = 0;  // null terminated string 
    }

    p->original_copy	= GetBits(bf, 1);
    p->home				= GetBits(bf, 1);
    p->bitstream_type	= GetBits(bf, 1);
    p->bitrate			= GetBits(bf, 23);

    // program config elements 
    n = GetBits(bf, 4) + 1;
    for ( i = 0 ; i < n ; i++ ) 
	{
		if( p->bitstream_type == 0 )
			p->prog_config.buffer_fullness = GetBits(bf, 20);

		if( !get_prog_config(bf, &(p->prog_config)) )
		{
			return 0;
		}
    }
 
    return 1;
}

static int get_adts_header(BitData* bf, ADTSHeader *p)
{
	p->syncword = GetBits(bf, 12);
	if ( p->syncword != 0xfff) 
	{
		return 0;
	}

	p->id					= GetBits(bf, 1);
	p->layer				= GetBits(bf, 2);
	p->protection_abs		= GetBits(bf, 1);
	p->profile				= GetBits(bf, 2);
	p->sampling_freq_idx	= GetBits(bf, 4);
	p->private_bit			= GetBits(bf, 1);
	p->channel_config		= GetBits(bf, 3);
	p->original_copy		= GetBits(bf, 1);
	p->home					= GetBits(bf, 1);
	p->copyright_id_bit		= GetBits(bf, 1);
	p->copyright_id_start	= GetBits(bf, 1);
	p->frame_length			= GetBits(bf, 13);
	p->adts_buffer_fullness = GetBits(bf, 11);

	if( p->layer != 0 )		
		return 0;	// ADTS layer data error 

	if (p->sampling_freq_idx > 11)
		return 0;
	
	/*
	if( p->profile != 1 )		
		return 0;	// AAC profile not supported!
		*/

	return 1;
}


int mp_get_aac_header(unsigned char *hdr, int bufsize, AAC_INFO *AacInfo)
{
	int nRet = -1;
	BitData bf;
	int ii;

	memset((void *)AacInfo, 0, sizeof(AAC_INFO));
	for (ii=0; ii<bufsize - 12; ii++)
	{
		// AAC Header
		if(!strncmp((char *)(hdr + ii), "ADIF", 4)) 
		{
			// ADIF 
			InitGetBits(&bf, &hdr[ii], bufsize - ii);
			nRet = get_adif_header(&bf, &(AacInfo->adifHeader));
			if ( nRet == 0 ) 
				return -1;

			AacInfo->nAACFormat = AAC_ADIF;

			AacInfo->nChannels		= AacInfo->adifHeader.prog_config.nChannels;
			AacInfo->nSamplingFreq	= SampleRate[AacInfo->adifHeader.prog_config.sampling_rate_idx];

			AacInfo->nBitRate		= (AacInfo->adifHeader.bitrate + 512) / 1024;
			nRet = 1;
		}
		else if( hdr[ii] == 0xFF && ( (unsigned char)(hdr[ii+1] & 0xF6) == (unsigned char)0xF0 ) )
		{
			// ADTS 		
			// need 4 bytes
			InitGetBits(&bf, &hdr[ii], bufsize - ii);
			nRet = get_adts_header(&bf, &(AacInfo->adtsHeader));
			if ( nRet == 0 ) 
				return -1;

			AacInfo->nAACFormat = AAC_ADTS;

			AacInfo->nChannels = AacInfo->adtsHeader.channel_config;
			AacInfo->nSamplingFreq = SampleRate[AacInfo->adtsHeader.sampling_freq_idx];		
			nRet = 1;
		}
		if (nRet == 1)
		{
			nRet = ii;
			break;
		}
	}

	return nRet;
}

