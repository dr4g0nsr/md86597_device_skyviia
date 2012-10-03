
#ifndef PARSER_H
#define PARSER_H

#include "MediaParser.h"
#include "demux_bd.h"

class Parser  
{
public:
	Parser();
	virtual ~Parser();

	int	Open( char* strSourceName );

	FileInfo finfo;

	int HasVideo();
	int HasAudio();
	int HasTag();
	int HasEncrypted();

	int GetFileType();
	BITMAP_INFO_HEADER *GetVideoInfo();
	WAVEFORMATEX *GetAudioInfo();	
	char * GetVideoFormat();		// Raymond 2007/10/26
	unsigned int GetAudioFCC();
	int GetDuration();
	float GetVideoFPS();
	float GetVideoBitrate();
	int GetAudioBitrate();
	unsigned int GetH264Profile();
	bd_priv_t * GetBDInfo();

	int HasFileTag();
	ID3_FRAME_INFO * GetTag();
};

#endif

