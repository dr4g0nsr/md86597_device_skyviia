#include "demux_aac.h"
#include "read_data.h"
#include "mp3_hdr.h"
#include "common.h"
#include "aac_hdr.h"

static int acc_get_info(FILE *fp, FileInfo *finfo, AAC_INFO *aacinfo)
{
	unsigned char hdr[100] = {0};
	int nID3Length = GetID3Tag(fp, finfo);
	int nRet;

	fseek(fp, nID3Length, SEEK_SET);
	if (read_nbytes(hdr, 1, 100, fp) != 100)
		return 0;

	nRet = mp_get_aac_header(hdr, 100, aacinfo);
	if (nRet >= 0) 
	{
		nRet = 1;
	} else {
		nRet = 0;
	}
	return nRet;
}

int aac_check_file(FILE	*fp, FileInfo *finfo)
{
	int nRet;
	AAC_INFO aacinfo;
	off_t pos;
	unsigned char hdr[100];
	unsigned int nAACFrameLen = 0;
	int check_max = 5;
	size_t read_num;

	pos = GetID3Tag(fp, finfo);

	fseek(fp, pos, SEEK_SET);
	read_num = read_nbytes(hdr, 1, 100, fp);
	if (read_num != 100)
		return 0;

	nRet = mp_get_aac_header(hdr, 100, &aacinfo);
	if (nRet >= 0)
	{
		if (aacinfo.nAACFormat == AAC_ADTS)
		{
			pos += nRet;
			fseek(fp, pos, SEEK_SET);
			while(1)
			{
				if (read_nbytes(hdr, 1, 6, fp) != 6)
					return 0;
				if( hdr[0] == 0xFF && ( (unsigned char)(hdr[1] & 0xF6) == (unsigned char)0xF0 ) )
				{		
					nAACFrameLen = (( ( int ) (hdr[3] & 3) ) << 11 ) +	( ( int )hdr[4] << 3 ) + ( hdr[5] >> 5 );

					if( (pos + nAACFrameLen > finfo->FileSize) || (nAACFrameLen == 0) )
					{
						nRet = -1;
						break;
					}

					pos += nAACFrameLen;
					fseek(fp, pos, SEEK_SET);
					check_max--;
					if (check_max == 0)
						break;
				} else {
					nRet = -1;
					break;
				}
			}
		}
	}

	return (nRet >= 0)? 1:0;
}

int AAC_Parser(FILE	*fp, FileInfo *finfo)
{
	int nRet;
	AAC_INFO AacInfo;

	nRet = acc_get_info(fp, finfo, &AacInfo);
	if (nRet == 0)
		return nRet;
	if ( AacInfo.nAACFormat == AAC_ADTS )
	{
		// get frame number and duration
		
		unsigned char pBuf[6];
		off_t pos = finfo->nID3Length;
		unsigned int nAACFrameLen = 0;
#if 1	//Barry 2011-06-05 get the real duration
		unsigned int nFrame = 0;
#else
		unsigned int nFrame = 0, skip_Frame = 50;
		unsigned int nPartLen = 0;
#endif
		int nAACFileSize = (int)( finfo->FileSize - finfo->nID3Length );
		
		// skip if has ID3 tag
		fseek(fp, pos, SEEK_SET);

		while ( pos < finfo->FileSize ) 
		{
			if (read_nbytes(pBuf, 1, 6, fp) != 6)
				return 0;

			if( pBuf[0] == 0xFF && ( (unsigned char)(pBuf[1] & 0xF6) == (unsigned char)0xF0 ) )
			{		
				nAACFrameLen = (( ( int ) (pBuf[3] & 3) ) << 11 ) +	( ( int )pBuf[4] << 3 ) + ( pBuf[5] >> 5 );

				if( (pos + nAACFrameLen > finfo->FileSize) || (nAACFrameLen == 0) )
				{
					return 0;
				}
				
				pos += nAACFrameLen;
				nFrame++;
				fseek(fp, pos, SEEK_SET);
#if 0	//Barry 2011-06-05 get the real duration
				if (nFrame > skip_Frame)
					nPartLen += nAACFrameLen;
				if (nFrame > 200 + skip_Frame)
					break;
#endif
			}
			else if( !strncmp((char *)pBuf, "TAG", 3) )	// Skip the final TAG data if "TAG"
			{
				break;	// Skip the final TAG data
			}
			else 
				break;
		}
				
#if 1	//Barry 2011-06-05 get the real duration
		finfo->AudioDuration = (int)((nFrame * 1024) / AacInfo.nSamplingFreq);
#else
		finfo->AudioDuration = (int)(((nAACFileSize/nPartLen) * (nFrame - skip_Frame) * 1024) / AacInfo.nSamplingFreq);
#endif
		if (finfo->AudioDuration != 0)
			AacInfo.nBitRate = (int) ( nAACFileSize / finfo->AudioDuration );
		else
			AacInfo.nBitRate = 0;

		//AacInfo.nBitRate = AacInfo.nBitRate * 8 / 1024;
		AacInfo.nBitRate = AacInfo.nBitRate * 8;
	}
	else if ( AacInfo.nAACFormat == AAC_ADIF )
	{
		finfo->AudioDuration = (int)( finfo->FileSize * 8 / (float)(AacInfo.adifHeader.bitrate) );
	}
	else
		return 0;
	
	finfo->bAudio = 1;
	finfo->AudioType = mmioFOURCC('A', 'A', 'C', ' ');

	finfo->FileDuration = finfo->AudioDuration;

	finfo->wf.nSamplesPerSec	= AacInfo.nSamplingFreq;
	finfo->wf.nChannels			= AacInfo.nChannels;

	finfo->aBitrate = AacInfo.nBitRate;

	return 1;
}

