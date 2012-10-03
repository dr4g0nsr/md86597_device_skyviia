#include <sys/types.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include "Parser.h"
#include "read_data.h"
#include "common.h"
#include "demux_mkv.h"
#include "demux_ts.h"
#include "demux_aac.h"
#include "demux_iso.h"
#include "demux_bd.h"
#include "demux_ac3.h"
#include "demux_ogg.h"
#include "mp3_hdr.h"
#include "dts_hdr.h"
#include "mpeg_hdr.h"
#include "demux_flac.h"
#include "demux_amr.h"
#include "demux_ape.h"
#include "hw_limit.h"

#define LOG_TAG "MediaParser_jni"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// AVI 

typedef struct
{
    uint32_t		dwMicroSecPerFrame;		// frame display rate (or 0L)
    uint32_t		dwMaxBytesPerSec;		// max. transfer rate
    uint32_t		dwPaddingGranularity;	// pad to multiples of this size; normally 2K.
    uint32_t		dwFlags;				// the ever-present flags
    uint32_t		dwTotalFrames;			// # frames in file
    uint32_t		dwInitialFrames;
    uint32_t		dwStreams;
    uint32_t		dwSuggestedBufferSize;   
    uint32_t		dwWidth;
    uint32_t		dwHeight;   
    uint32_t		dwReserved[4];
} MainAVIHeader;

typedef struct 
{
    uint32_t		fccType;
    uint32_t		fccHandler;
    uint32_t		dwFlags;				/* Contains AVITF_* flags */
    uint16_t		wPriority;
    uint16_t		wLanguage;
    uint32_t		dwInitialFrames;
    uint32_t		dwScale;	
    uint32_t		dwRate;					/* dwRate / dwScale == samples/second */
    uint32_t		dwStart;
    uint32_t		dwLength;				/* In units above... */
    uint32_t		dwSuggestedBufferSize;
    uint32_t		dwQuality;
    uint32_t		dwSampleSize;
	// rectangle_t		rcFrame;
    uint16_t		left;	
    uint16_t		top;
    uint16_t		right;
    uint16_t		bottom;
} AVIStreamHeader;

typedef struct mpeglayer3waveformat_tag 
{
	WAVEFORMATEX wf;
	unsigned short wID;
	unsigned int   fdwFlags;
	unsigned short nBlockSize;
	unsigned short nFramesPerBlock;
	unsigned short nCodecDelay;
} MPEGLAYER3WAVEFORMAT;

/*
void print_wave_header(WAVEFORMATEX *h)
{
	printf("======= WAVE Format =======\n");
	printf("Format Tag: %d (0x%X)\n",h->wFormatTag,h->wFormatTag);
	printf("Channels: %d\n",h->nChannels);
	printf("Samplerate: %ld\n",h->nSamplesPerSec);
	printf("avg byte/sec: %ld\n",h->nAvgBytesPerSec);
	printf("Block align: %d\n",h->nBlockAlign);
	printf("bits/sample: %d\n",h->wBitsPerSample);
	printf("cbSize: %d\n",h->cbSize);

	if( h->wFormatTag == 0x55 && h->cbSize >= 12 )
	{
		MPEGLAYER3WAVEFORMAT* h2 = (MPEGLAYER3WAVEFORMAT *)h;
		printf("mp3.wID=%d\n",h2->wID);
		printf("mp3.fdwFlags=0x%lX\n",h2->fdwFlags);
		printf("mp3.nBlockSize=%d\n",h2->nBlockSize);
		printf("mp3.nFramesPerBlock=%d\n",h2->nFramesPerBlock);
		printf("mp3.nCodecDelay=%d\n",h2->nCodecDelay);
	}
	else if ( h->cbSize > 0 )
	{
		int i;
		uint8_t* p = ((uint8_t*)h) + sizeof(WAVEFORMATEX);
		printf("Unknown extra header dump: ");
		for (i = 0; i < h->cbSize; i++)
			printf("[%x] ", p[i]);
		printf("\n");
	}
	printf("===========================\n");
}

void print_video_header(BITMAPINFOHEADER *h)
{
	uint32_t size = sizeof(BITMAPINFOHEADER);
	
	printf("======= VIDEO Format ======\n");
	printf("biSize %d\n", h->biSize);
	printf("biWidth %d\n", h->biWidth);
	printf("biHeight %d\n", h->biHeight);
	printf("biPlanes %d\n", h->biPlanes);
	printf("biBitCount %d\n", h->biBitCount);
	printf("biCompression %d ='%.4s'\n", h->biCompression, (char *)&h->biCompression);
	printf("biSizeImage %d\n", h->biSizeImage);

	if ( h->biSize > size )
	{
		uint32_t i;
		uint8_t* p = ((uint8_t*)h) + size;
		printf("Unknown extra header dump: ");
		for ( i = 0 ; i < h->biSize - size ; i++ )
			printf("[%x] ", *(p+i));
		printf("\n");
	}
	printf("===========================\n");
}
*/
/* form types, list types, and chunk types */
#define formtypeAVI             mmioFOURCC('A', 'V', 'I', ' ')
#define listtypeAVIHEADER       mmioFOURCC('h', 'd', 'r', 'l')
#define ckidAVIMAINHDR          mmioFOURCC('a', 'v', 'i', 'h')
#define listtypeSTREAMHEADER    mmioFOURCC('s', 't', 'r', 'l')
#define ckidSTREAMHEADER        mmioFOURCC('s', 't', 'r', 'h')
#define ckidSTREAMFORMAT        mmioFOURCC('s', 't', 'r', 'f')
#define ckidSTREAMHANDLERDATA   mmioFOURCC('s', 't', 'r', 'd')
#define listtypeAVIMOVIE        mmioFOURCC('m', 'o', 'v', 'i')

// Stream types for the <fccType> field of the stream header.
#define streamtypeVIDEO         mmioFOURCC('v', 'i', 'd', 's')
#define streamtypeAUDIO         mmioFOURCC('a', 'u', 'd', 's')

/* Chunk id to use for extra chunks for padding. */
#define ckidAVIPADDING          mmioFOURCC('J', 'U', 'N', 'K')
#define CHECK_DRM_TYPE  		mmioFOURCC('d', 'i', 'v', 'x')

#define MIN(a,b) (((a)<(b))?(a):(b))

int avi_check_file(FILE	*fp)
{
	int id;	
	unsigned int size2;

	read_nbytes(&id, 1, 4, fp);
	if(id == mmioFOURCC('R','I','F','F'))
	{
		int riff_type;			
		
		read_nbytes(&size2, 1, 4, fp);
		read_nbytes((char *)&riff_type, 1, 4, fp);	// "AVI "

		if( riff_type == formtypeAVI )
			return 1;
//			printf("RIFF type : %.4s\n", (char *)&riff_type);
		else	// for DAT = CDXA
			return 0;
//			printf("Unknown RIFF type %.4s\n", (char *)&riff_type);
	}
	else
	{
//		printf("Not RIFF header\n");
		return 0;
	}

	return 1;
}

int AVI_Parser(FILE	*fp, FileInfo *finfo)
{
	int id;	
	int last_fccType = 0;
	unsigned int chunksize, size2;
	LOFF_T asize = 0, vsize = 0, pos;
	int ii, jj;
	int gmc_flag = 0, hwlim_flag = 0, drm_flag = 0, file_flag = 0;
	int use_fread = 1;
	int fd = 0;
	int divx_drm_type = 0;
	ssize_t rret = 0;

//	try
	{
		//---- AVI header:

		if(!avi_check_file(fp))
			return 0;
		
#if defined(HAVE_ANDROID_OS)
		if( finfo->FileSize > (LOFF_T)INT_MAX)
			use_fread = 0;
#endif
		if (use_fread == 0) {
			fd = fileno(fp);
			pos = (LOFF_T)ftell(fp);
			lseek64(fd, pos, SEEK_SET);
		}
		while(1)
		{
			if (use_fread == 0) {
				rret = read(fd, (void *)&id, 4);
				if (rret == 0)
					break;
			} else {
				read_nbytes(&id, 1, 4, fp);
				if( feof(fp) || (ftello(fp) > finfo->FileSize) || file_error == 1 )
					break;
			}
			
			if( id == mmioFOURCC('L','I','S','T') )
			{
				unsigned int len;
				if (use_fread == 0) {
					read(fd, (void *)&len, 4);
					read(fd, (void *)&id, 4);
				} else {
					read_nbytes(&len, 1, 4, fp);	// list size
					read_nbytes(&id, 1, 4, fp);	// list type
					if( feof(fp) || (ftello(fp) > finfo->FileSize) || file_error == 1 )
						break;
				}
				//			printf("LIST : %.4s  len = %u\n",(char *) &id,len);
				
				if( len >= 4 ) 
				{
					len -= 4;
				}
				else 
				{
					printf("** empty list?!\n");
				}
				
				if( id == listtypeAVIMOVIE )
				{
					// found MOVI header
					unsigned int m_type, m_len;
					char * c_type, * c_len;
					int check_video_data = 0;
					int check_video = 0;
					int check_block_size = 1024;
					unsigned char *buff = NULL;
					LOFF_T tmp_pos = 0;

					switch (finfo->bih.biCompression)
					{
						case mmioFOURCC('X', 'V', 'I', 'D'):
						case mmioFOURCC('x', 'v', 'i', 'd'):
						case mmioFOURCC('D', 'I', 'V', 'X'):
						case mmioFOURCC('d', 'i', 'v', 'x'):
						case mmioFOURCC('D', 'X', '5', '0'):
						case mmioFOURCC('H', '2', '6', '4'):
						case mmioFOURCC('F', 'M', 'P', '4'):
							buff = (unsigned char *)malloc(check_block_size);
							check_video = 1;
							break;
						default:
							break;
					}
					if ((buff != NULL) && (check_video == 1))
					{
						while (len > 0)
						{
							if (use_fread == 0) {
								tmp_pos = lseek64(fd, 0, SEEK_CUR);
								read(fd, (void *)&m_type, 4);
								read(fd, (void *)&m_len, 4);
							} else {
								read_nbytes(&m_type, 1, 4, fp);
								read_nbytes(&m_len, 1, 4, fp);
								if( feof(fp) || (ftello(fp) > finfo->FileSize) || file_error == 1 )
									break;
							}
							c_type = (char *)&m_type;
							c_len = (char *)&m_len;
							/*
							printf ("m_type: %04X (%c%c%c%c), m_len: %u\n", m_type,
									c_type[0], c_type[1], c_type[2], c_type[3],
									m_len);
									*/
							len -= 8;
							if (m_type == mmioFOURCC('L','I','S','T'))
							{
								continue;
							} else if (m_type == mmioFOURCC('r','e','c',' '))
							{
									if (use_fread == 0) {
										lseek64(fd, (LOFF_T)(tmp_pos - 4), SEEK_CUR);
									} else {
										fseek(fp, -4, SEEK_CUR);
									}
									len += 4;
									continue;
							} else if ((c_type[2] == 'd') && (c_type[3] == 'b'))
							{
								// Uncompressed video frame
								check_video_data = 1;
							} else if ((c_type[2] == 'd') && (c_type[3] == 'c'))
							{
								// Compressed video frame
								check_video_data = 1;
							} else if ((c_type[2] == 'p') && (c_type[3] == 'c'))
							{
								// Palette change
							} else if ((c_type[2] == 'w') && (c_type[3] == 'b'))
							{
								// Audio data
							} else if ((c_type[0] == 'i') && (c_type[1] == 'x'))
							{
								//
							} else {
								if ( ((c_type[3] == 'd') && (c_len[0] == 'c')) ||
										((c_type[3] == 'p') && (c_len[0] == 'c')) ||
										((c_type[3] == 'w') && (c_len[0] == 'b')) )
								{
									if (use_fread == 0) {
										lseek64(fd, (LOFF_T)(tmp_pos - 7), SEEK_CUR);
									} else {
										fseek(fp, -7, SEEK_CUR);
									}
									len += 7;
									//printf("fix it and seek one bit!!\n");
									continue;
								}
								//printf("AVI type error!!!\n");
								break;
							}
							if (m_len > len)
							{
								//printf("AVI error frame len!!!");
								break;
							}
							if (check_video_data)
							{
								int gmc_search_loop = 25;
								int search_size;
								while (m_len > 0)
								{
									search_size = MIN(m_len, (unsigned int)check_block_size);
									if (use_fread == 0) {
										read(fd, (void *)buff, search_size);
									} else {
										read_nbytes(buff, 1, search_size, fp);	// list size
										if( feof(fp) || (ftello(fp) > finfo->FileSize) || file_error == 1 )
											break;
									}
									m_len -= search_size;
									len -= search_size;
									switch (finfo->bih.biCompression)
									{
										case mmioFOURCC('X', 'V', 'I', 'D'):
										case mmioFOURCC('x', 'v', 'i', 'd'):
										case mmioFOURCC('D', 'I', 'V', 'X'):
										case mmioFOURCC('d', 'i', 'v', 'x'):
										case mmioFOURCC('D', 'X', '5', '0'):
										case mmioFOURCC('F', 'M', 'P', '4'):
											if(check_mp4_header_vol(buff, search_size) == 0)
										{
											printf("AVI: GMC AND STATIC SPRITE CODING not supported\n");
											gmc_flag = 1;
										}
											break;
										case mmioFOURCC('H', '2', '6', '4'):
											for(jj=0; jj<search_size-5; jj++)
											{
												if (buff[jj]==0 &&
														buff[jj+1]==0 &&
														buff[jj+2]==0 &&
														buff[jj+3]==1 &&
														((buff[jj+4]&0x1F)==7) )
												{
													if (check_avc1_sps_bank0(buff + jj + 5,
																search_size - jj - 5) == 0)
													{
														printf("AVI: H.264 Not supported, Profile=%d, Level=%d.%d\n", buff[jj+5], buff[jj+7]/10, buff[jj+7]%10);
														hwlim_flag = 1;
													}
												}
											}
											break;
									}
									if (gmc_flag)
										break;
									gmc_search_loop--;
									if (gmc_search_loop == 0)
										break;
								}
								break;
							}
							if (use_fread == 0) {
								lseek64(fd, (LOFF_T)m_len, SEEK_CUR);
							} else {
								fseek(fp, m_len, SEEK_CUR);
							}
							len -= m_len;
						}
					}
					if (buff)
						free(buff);
					if (use_fread == 0) {
						lseek64(fd, (LOFF_T)len, SEEK_CUR);	// skip movi
					} else {
						fseeko(fp, len, SEEK_CUR);	// skip movi
					}
				}
				continue;
			}
			
			if (use_fread == 0) {
				read(fd, (void *)&size2, 4);
			} else {
				read_nbytes(&size2, 1, 4, fp);
				if( feof(fp) || (ftello(fp) > finfo->FileSize) || file_error == 1 )
					break;
			}
			//		printf("CHUNK %.4s  len=%u\n",(char *) &id,size2);
			
			chunksize = ( size2 + 1 ) & (~1);
			
			switch(id)
			{				
			case ckidAVIMAINHDR:          // read 'avih'
				{
					MainAVIHeader avih;
					int size = MIN(size2,sizeof(avih));
					if (use_fread == 0) {
						read(fd, (void *)&avih, size);
					} else {
						read_nbytes((char*)&avih,1, size, fp);
					}
					finfo->dwWidth  = avih.dwWidth;
					finfo->dwHeight = avih.dwHeight;
					break;
				}
			case ckidSTREAMHEADER:       // read 'strh'
				{
					AVIStreamHeader h;
					int size = MIN(size2,sizeof(h));			
					if (use_fread == 0) {
						read(fd, (void *)&h, size);
					} else {
						read_nbytes((char*) &h,1, size, fp);
					}
					
					if( size2 > sizeof(h) )
					{
						if (use_fread == 0) {
							lseek64(fd, (LOFF_T)(size2 - sizeof(h)), SEEK_CUR);
						} else {
							fseek(fp, size2 - sizeof(h), SEEK_CUR);
						}
					}
					
					if( h.fccType == streamtypeVIDEO )
					{
						if (finfo->FPS == 0)
						{
							finfo->FPS = (float)h.dwRate / h.dwScale;
							finfo->vFrame = h.dwLength;
						}
						if (h.fccHandler == CHECK_DRM_TYPE)
						{
							divx_drm_type = 1;
						}
					}
					else if( h.fccType == streamtypeAUDIO )
					{
						asize = h.dwLength;
					}
					
					last_fccType = h.fccType;
					break; 
				}
			case ckidSTREAMFORMAT:      // read 'strf'
				{ 	
//					if( last_fccType == streamtypeVIDEO )
					if( last_fccType == streamtypeVIDEO && finfo->bih.biCompression == 0 )	// Raymond 2008/09/04
					{								
						BITMAP_INFO_HEADER *bih = &(finfo->bih);
						int size = sizeof(BITMAP_INFO_HEADER);
						if (use_fread == 0) {
							read(fd, (void *)bih, size);
						} else {
							read_nbytes((char*) bih,1, size, fp);	
						}
						
						finfo->bVideo = 1;
						
						if( chunksize > (unsigned int)size )
						{
							if (use_fread == 0) {
								lseek64(fd, (LOFF_T)(chunksize - size), SEEK_CUR);
							} else {
								fseek(fp, chunksize - size, SEEK_CUR);
							}
						}
					} 
					else if( last_fccType == streamtypeAUDIO )
					{
						WAVEFORMATEX *wf = &(finfo->wf);
						int size = sizeof(WAVEFORMATEX);
						if (use_fread == 0) {
							read(fd, (void *)wf, size);
						} else {
							read_nbytes((char*)wf,1, size, fp);
						}
						
						finfo->bAudio = 1;
						finfo->aBitrate = (wf->nAvgBytesPerSec * 8 + 500) / 1000; // * 8 / 1000
						finfo->AudioType = wf->wFormatTag;
						
						if( chunksize > (unsigned int)size )
						{
							if (use_fread == 0) {
								lseek64(fd, (LOFF_T)(chunksize - size), SEEK_CUR);
							} else {
								fseek(fp, chunksize - size, SEEK_CUR);
							}
						}
					}
					else
					{
						if (use_fread == 0) {
							lseek64(fd, (LOFF_T)chunksize, SEEK_CUR);
						} else {
							fseek(fp, chunksize, SEEK_CUR);
						}
					}
					break;
				}		
			case ckidSTREAMHANDLERDATA: // strd
				{
					if (divx_drm_type == 1)
					{
						unsigned int drm_version;
						if (use_fread == 0) {
							read(fd, (void *)&drm_version, 4);
							lseek64(fd, (LOFF_T)chunksize - 4, SEEK_CUR);
						} else {
							read_nbytes((char*)&drm_version,1, 4, fp);
							fseek(fp, chunksize - 4, SEEK_CUR);
						}
						//printf("drm_version: %u\n", drm_version);
						if (drm_version > 3)
						{
							drm_flag = 1;
						}
						break;
					}
				}
			case ckidAVIPADDING:	// JUNK
			default:				// others skip
				if (use_fread == 0) {
					if(((LOFF_T)(lseek64(fd, 0, SEEK_CUR) + chunksize) > finfo->FileSize) ||
							(id == 0)) {
						//printf("CHUNK size error : %d\n", chunksize);
						file_flag = 1;
						break;
					}
					lseek64(fd, (LOFF_T)chunksize, SEEK_CUR);
				} else {
					if(((LOFF_T)(ftello(fp) + chunksize) > finfo->FileSize) ||
							(id == 0)) {
						//printf("CHUNK size error : %d\n", chunksize);
						file_flag = 1;
						break;
					}
					fseek(fp, chunksize, SEEK_CUR);
				}
				chunksize = 0;
				break;
			}
			if (file_flag == 1)
				break;
		}
		
		// estimate video bitrate
		if( (LOFF_T)asize > finfo->FileSize )
			asize = 0;
		
		vsize = finfo->FileSize - asize - 8 * finfo->vFrame;
		
		finfo->vBitrate = 0.008f * ( (float)vsize / (float)finfo->vFrame ) * finfo->FPS * 1000;

		// Raymond 2007/10/24
		if( finfo->FPS != 0 )
			finfo->FileDuration = (unsigned long)(finfo->vFrame / finfo->FPS);

		if ((gmc_flag == 1) || (hwlim_flag == 1))
			return 0;
		else if (drm_flag == 1)
			return -1 * PARSER_VIDEO_DIVX_VERSION_NOT_SUPPORTED;
		else if ( finfo->bAudio || finfo->bVideo )
			return 1;
		else
			return 0;
	}
//	catch (...) 
//	{
//		return 0;
//	}
}

// ASF

// ASF Object Header 
typedef struct 
{
	uint8_t guid[16];
	uint64_t size;
} ASF_obj_header_t;

// ASF Header 
typedef struct
{
	ASF_obj_header_t objh;
	uint32_t cno;		// number of subchunks
	uint8_t v1;			// unknown (0x01)
	uint8_t v2;			// unknown (0x02)
} ASF_header_t;

// ASF File Header 
typedef struct 
{
	uint8_t stream_id[16];		// stream GUID
	uint64_t file_size;
	uint64_t creation_time;		// File creation time FILETIME 8
	uint64_t num_packets;		// Number of packets UINT64 8
	uint64_t play_duration;		// Timestamp of the end position UINT64 8
	uint64_t send_duration;		// Duration of the playback UINT64 8
	uint64_t preroll;			// Time to bufferize before playing UINT32 4
	uint32_t flags;				// Unknown, maybe flags ( usually contains 2 ) UINT32 4
	uint32_t min_packet_size;	// Min size of the packet, in bytes UINT32 4
	uint32_t max_packet_size;	// Max size of the packet  UINT32 4
	uint32_t max_bitrate;		// Maximum bitrate of the media (sum of all the stream)
} ASF_file_header_t;

// ASF Stream Header
typedef struct 
{
	uint8_t type[16];			// Stream type (audio/video) GUID 16
	uint8_t concealment[16];	// Audio error concealment type GUID 16
	uint64_t unk1;				// Unknown, maybe reserved ( usually contains 0 ) UINT64 8
	uint32_t type_size;			// Total size of type-specific data UINT32 4
	uint32_t stream_size;		// Size of stream-specific data UINT32 4
	uint16_t stream_no;			// Stream number UINT16 2
	uint32_t unk2;				// Unknown UINT32 4
} ASF_stream_header_t;

unsigned char asfhdrguid[16] = {0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};

unsigned char _asf_stream_header_guid[16] = 
	{0x91, 0x07, 0xdc, 0xb7,  0xb7, 0xa9, 0xcf, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
unsigned char asf_file_header_guid[16] = {0xa1, 0xdc, 0xab, 0x8c,
  0x47, 0xa9, 0xcf, 0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};

// Raymond 2007/10/26
unsigned char asf_ext_stream_header[16] = {0xCB, 0xA5, 0xE6, 0x14,
  0x72, 0xC6, 0x32, 0x43, 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A};

//#ifdef ARCH_X86
//#define	ASF_LOAD_GUID_PREFIX(guid)	(*(uint32_t *)(guid))
//#else
#define	ASF_LOAD_GUID_PREFIX(guid)	((guid)[3] << 24 | (guid)[2] << 16 | (guid)[1] << 8 | (guid)[0])
//#endif

#define ASF_GUID_PREFIX_audio_stream				0xF8699E40
#define ASF_GUID_PREFIX_video_stream				0xBC19EFC0
#define ASF_GUID_PREFIX_audio_conceal_none			0x49f1a440
#define ASF_GUID_PREFIX_audio_conceal_interleave	0xbfc3cd50

ASF_header_t asfh;

int asf_check_file(FILE	*fp)
{
	// asf_check_header
	if (read_nbytes((char*) &asfh,1, 30, fp) != 30) // header obj : sizeof(asfh)
		return 0;

	if( memcmp(asfhdrguid, asfh.objh.guid,16) )
	{
//		printf("ASF_check : not ASF guid!\n");
		return 0; // not ASF guid
	}
	if( asfh.cno > 256 )
	{
//		printf("ASF_check : invalid subchunks_no %d\n",(int)asfh.cno);
		return 0; // invalid header???
	}

	return 1;
}

int find_asf_guid(unsigned char *buf, const unsigned char *guid, int cur_pos, int buf_len)
{
	int i;
	for ( i = cur_pos ; i < buf_len - 19; i++ ) 
	{
		if (memcmp(&buf[i], guid, 16) == 0)
			return i + 16 + 8; // point after guid + length
	}
	return -1;
}

// Raymond 2007/10/26
int get_ext_stream_properties(unsigned char *buf, int buf_len, int stream_num, float* fps)
{
	// this function currently only gets the average frame time if available

	int pos=0;
	unsigned char *buffer = &buf[0];
	uint32_t avg_ft;
	
	while ((pos = find_asf_guid(buf, asf_ext_stream_header, pos, buf_len)) >= 0) 
	{
		int this_stream_num;
		buffer = &buf[pos];
		
		buffer +=48;
		this_stream_num = buffer[0] + (buffer[1]<<8);
		
		//	printf("buffer = %d %d\n", buffer[0], buffer[1]);
		//	printf("this_stream_num = %d\n", this_stream_num);
		
		if (this_stream_num == stream_num) 
		{
			buffer += 4; //skip stream-language-id-index
			avg_ft = buffer[0] + (buffer[1]<<8) + (buffer[2]<<16) + (buffer[3]<<24);	// provided in 100ns units
			*fps = 10000000.0f / avg_ft;
			
			return 1;
		} 
	}
	return 0;
}

int ASF_Parser(FILE	*fp, FileInfo *finfo)
{
	int hdr_len;
	int pos = 0;
	unsigned char *hdr = NULL;
	unsigned char *buffer = NULL;

//	try
	{
		if(!asf_check_file(fp))
			return 0;
		
		fseek(fp, 0, SEEK_SET);		// reset to the begin
		
		hdr_len = (int)(asfh.objh.size); 
		if (hdr_len > 64 * 1024) 
		{
			//		printf("ASF_check : header size bigger than 64 kB (%d)!\n", hdr_len);
			return 0;
		}
		
		hdr = (unsigned char *)malloc(hdr_len);
		if ( hdr == NULL )
		{
			//		printf("ASF_check : hdr NULL\n");
			return 0;
		}

		if (read_nbytes(hdr, 1, hdr_len, fp) != hdr_len) // header obj
			return 0;
		
		// find stream headers
		pos = 30;	// sizeof(asfh)
		
		while ( ( pos = find_asf_guid(hdr, _asf_stream_header_guid, pos, hdr_len) ) >= 0 )
		{
			ASF_stream_header_t *streamh = (ASF_stream_header_t *)&hdr[pos];
			uint32_t type_size, stream_num;

			// Get type_size
			buffer = &hdr[pos+40];
			type_size = (buffer[3] << 24) + (buffer[2] << 16) + (buffer[1] << 8) + buffer[0];

			// Get stream video number
			buffer = &hdr[pos+48];
			stream_num = (buffer[1] << 8) + buffer[0];
			
			pos += 54; // sizeof(ASF_stream_header_t)
			if (pos > hdr_len) 
				goto len_err_out;
			
			// type-specific data:
			buffer = &hdr[pos];
			pos += type_size;
			
			if (pos > hdr_len) 
				goto len_err_out;
			
			switch(ASF_LOAD_GUID_PREFIX(streamh->type))
			{
			case ASF_GUID_PREFIX_audio_stream: 
				{
//			int len = ( streamh->type_size < sizeof(WAVEFORMATEX) ) ? sizeof(WAVEFORMATEX) : streamh->type_size;
					
					int len = sizeof(WAVEFORMATEX);
					WAVEFORMATEX *wf = &(finfo->wf);
					memcpy((char *)wf, buffer, len);
					finfo->bAudio = 1;

					finfo->aBitrate = (wf->nAvgBytesPerSec * 8 + 500) / 1000; // * 8 / 1000
					finfo->AudioType = wf->wFormatTag;
					break;
				}
			case ASF_GUID_PREFIX_video_stream: 
				{
					//			int len = streamh->type_size - (4+4+1+2);
					//			len ( len < sizeof(BITMAPINFOHEADER) ) ? sizeof(BITMAPINFOHEADER) : len;
					
					int len = sizeof(BITMAP_INFO_HEADER);
					BITMAP_INFO_HEADER *bih = &(finfo->bih);
					memcpy((char *)bih, &buffer[4+4+1+2], len);
#if 1
					if ( (bih->biCompression == mmioFOURCC('W','M','V','3')) || (bih->biCompression == mmioFOURCC('W','V','C','1')) )
					{
						//20110719 charleslin fixed mantis 5443, filter complex profile
						if((buffer[4+4+1+2+len] & 0xC0) == 0x80)
						{
							finfo->bVideo = 0;
							goto len_err_out;
						}
						else
							finfo->bVideo = 1;
					}
					else
#endif
						finfo->bVideo = 1;
					
					finfo->dwWidth  = bih->biWidth;
					finfo->dwHeight = bih->biHeight;
					
					// Raymond 2007/10/26
					get_ext_stream_properties(hdr, hdr_len, stream_num, &finfo->FPS);
					
					break;
				}
			default:
				break;
			}
		}
		
		// find file header
		pos = find_asf_guid(hdr, asf_file_header_guid, 0, hdr_len);
		if ( pos >= 0 ) 
		{
			uint32_t max_bitrate;
			uint64_t play_duration;
			buffer = &hdr[pos+40];
			
			play_duration = buffer[7];
			play_duration = (play_duration<<8) | buffer[6];
			play_duration = (play_duration<<8) | buffer[5];
			play_duration = (play_duration<<8) | buffer[4];
			play_duration = (play_duration<<8) | buffer[3];
			play_duration = (play_duration<<8) | buffer[2];
			play_duration = (play_duration<<8) | buffer[1];
			play_duration = (play_duration<<8) | buffer[0];
			
			finfo->FileDuration = (int)(play_duration/10000000);

			// Raymond 2007/10/26
			buffer = &hdr[pos+76];
			max_bitrate = (buffer[3] << 24) + (buffer[2] << 16) + (buffer[1] << 8) + buffer[0];
			finfo->vBitrate = (float)(max_bitrate);
		}

		free(hdr);
		return 1;
		
len_err_out:
		free(hdr);
		return 0;
	}
//	catch (...) 
//	{
//		free(hdr);
//		return 0;
//	}
}

// MPG

#define MAX_PS_PACKETSIZE (224*1024)

int bElementary = 0;	// Raymond 2007/11/13

static int frameratecode2framerate[16] =
{
	0,
	// Official mpeg1/2 framerates: (1-8)
	24000*10000/1001, 24*10000,25*10000,
	30000*10000/1001, 30*10000,50*10000,
	60000*10000/1001, 60*10000,
	// Xing's 15fps: (9)
	15*10000,
	// libmpeg3's "Unofficial economy rates": (10-13)
	5*10000,10*10000,12*10000,15*10000,
	// some invalid ones: (14-15)
	0,0
};

static unsigned int read_pack_timestamp(unsigned char * tbuf)
{
	int c, d, e, f, g;
	uint64_t pts;
	c = tbuf[0];
	d = tbuf[1];
	e = tbuf[2];
	f = tbuf[3];
	g = tbuf[4];
	if( ((c&0xc4)!=0x44) || ((e&0x4)!=0x4) || ((g&0x4)!=0x4) )
	{
		return 0; // invalid pts
	}
	pts = (((uint64_t)((c&0x38) >> 3)) << 30) | ((c&0x3) << 28) | (d << 20) | ((e >> 3) << 15) |
		((e & 0x3) << 13) | (f << 5) | (g >> 3);
	
	return (unsigned int)(pts/90000);
}

static unsigned int mpeg_timestamp(int time, int c)
{
	int d,e;
	unsigned int pts;
	d = time >> 16;
	e = time & 0x0000ffff;
	if( ((c&1)!=1) || ((d&1)!=1) || ((e&1)!=1) )
	{
//		++mpeg_pts_error;
		return 0; // invalid pts
	}
	pts = ( ( ( c >> 1 ) & 7 ) << 30 ) | ( ( d >> 1 ) << 15 ) | ( e >> 1 ) ;
	
	return pts;
}

// Raymond 2007/10/26
static unsigned int read_mpeg_timestamp(FILE *fp, int c)
{
	int d,e;
	unsigned int pts;
	d = read_word(fp);
	e = read_word(fp);
	if( ((c&1)!=1) || ((d&1)!=1) || ((e&1)!=1) )
	{
//		++mpeg_pts_error;
		return 0; // invalid pts
	}
	pts = ( ( ( c >> 1 ) & 7 ) << 30 ) | ( ( d >> 1 ) << 15 ) | ( e >> 1 ) ;
	
	return pts;
}

static int parse_psm(FILE *fp, int len, FileInfo *finfo) 
{
//	try
	{
		unsigned char c, id, type;
		unsigned int plen, prog_len, es_map_len;
		
		if((! len) || (len > finfo->FileSize))
			return 0;
		
		c = read_char(fp);
		if(! (c & 0x80)) 
		{
			//		stream_skip(demux->stream, len - 1);  //not yet valid, discard
			fseek(fp, len - 1, SEEK_CUR);
			return 0;
		}
		
		fseek(fp, 1, SEEK_CUR);
		
		prog_len = read_word(fp);			// length of program descriptors
		//	stream_skip(demux->stream, prog_len);				// .. that we ignore
		fseek(fp, prog_len, SEEK_CUR);
		
		es_map_len = read_word(fp);		// length of elementary streams map
		es_map_len = min(es_map_len, len - prog_len - 8);	// sanity check
		
		while(es_map_len > 0) 
		{
			type = read_char(fp);
			id = read_char(fp);
			if( id >= 0xB0 && id <= 0xEF ) 
			{
				switch(type) 
				{
				case 0x1:
					finfo->VideoType = VIDEO_MPEG1;
					finfo->bih.biCompression = mmioFOURCC('M','P','G','1');
					break;
				case 0x2:
					finfo->VideoType = VIDEO_MPEG2;
					finfo->bih.biCompression = mmioFOURCC('M','P','G','2');
					break;
				case 0x3:
				case 0x4:
					finfo->wf.wFormatTag = AUDIO_MP2;
					break;
				case 0x0f:
				case 0x11:
					finfo->AudioType = AUDIO_AAC;
					break;
				case 0x10:
					finfo->VideoType = VIDEO_MPEG4;
					break;
				case 0x1b:
					finfo->VideoType = VIDEO_H264;
					break;
				case 0x81:
					finfo->wf.wFormatTag = AUDIO_A52;
					break;
				}
				
				// mp_dbg(MSGT_DEMUX,MSGL_V, "PSM ES, id=0x%x, type=%x, stype: %x\n", id, type, priv->es_map[idoffset]);
			}
			plen = read_word(fp);		//length of elementary stream descriptors
			plen = min(plen, es_map_len);			//sanity check
			//skip descriptors for now
			if ((fseek(fp, plen, SEEK_CUR) == -1) || (es_map_len < 4 + plen))
				return -1;
			es_map_len -= 4 + plen;
		}

		//	stream_skip(demux->stream, 4);			//skip crc32
		fseek(fp, 4, SEEK_CUR);					//skip crc32
		return 1;
	}
//	catch (...) 
//	{
//		return 0;
//	}
}

// Raymond 2008/12/11

/* Check m4v VO/VOL header */
int CheckMPEG4Header( unsigned char *bs, int buffer_len, FileInfo *finfo )
{
	unsigned int tmpvar = 0;

	int i = 0, x;
	int Verid = 1;
	unsigned int TimeIncrementResolution = 0, TimeIncrement = 0;
	int is_object_layer_identifier = 0;
	int sprite_en;
	unsigned char *bptr = NULL;
	BitData bf;
	
	for( i = 0 ; i < buffer_len-4 ; i++ )
		if( bs[i] == 0 && bs[i+1] == 0 && bs[i+2] == 1 )
			if( bs[i+3] < 0x20 )
				break;

	if( i == buffer_len  - 4 )
		return -1;

#if 1
	for (x=i;x<buffer_len-4;x++)
	{
		if (bs[x] == 0 && bs[x+1] == 0 && bs[x+2] == 1)
		{
			if ((bs[x+3]>>4) == 2)
			{
				bptr = bs + x;
				break;
			}
		}
	}
	if( x == buffer_len  - 4 )
		return -1;
#else
	bptr = bs + i;

	// check VO_START_CODE
	if( !(bptr[0] == 0 && bptr[1] == 0 && bptr[2] == 1 && bptr[3] < 0x20) )
		return -2;
	
	bptr += 4;

	// check VOL_START_CODE
	if( !(bptr[0] == 0 && bptr[1] == 0 && bptr[2] == 1 ) )	
		return -3;
#endif

	if( bptr[3] < 0x20 || bptr[3] > 0x2f)
		return -3;

	bptr += 4;

	InitGetBits(&bf, bptr, buffer_len - i);

	GetBits(&bf, 9);
												
	is_object_layer_identifier = GetBits(&bf, 1);	// is_object_layer_identifier ( 1 bit )
	if( is_object_layer_identifier == 1 )
	{
		Verid = GetBits(&bf, 4);						
		GetBits(&bf, 3);
	}
		
	tmpvar = GetBits(&bf, 4);	// insert aspect_ratio_info ( 4 bits ) 
	if( tmpvar == 0xF )
		GetBits(&bf, 16);

	tmpvar = GetBits(&bf, 1);	// vol_control_parameters ( 1 bit )
	if( tmpvar == 1 )
	{
		GetBits(&bf, 3);
		tmpvar = GetBits(&bf, 1);	
		if( tmpvar == 1 )
			GetBits(&bf, 79);
	}

	tmpvar = GetBits(&bf, 2);	// vol_shape (2 bits) 
	if( tmpvar != 0 )
		return -4;			// No support Shape
	
	GetBits(&bf, 1);

	TimeIncrementResolution = GetBits(&bf, 16);
	
	GetBits(&bf, 1);
	
	tmpvar = GetBits(&bf, 1);	// fixed_vop_rate ( 1 bit )
	if( tmpvar == 1 )
	{
		int j;
		unsigned int p2;		
		int bits = 1;
											
		for( j = 1 ; j <= 16 ; j++ )
		{
			p2 = 1 << j;
			if( TimeIncrementResolution > p2 )
				bits ++;
		}			
		
		//printf("T bits = %d\n", bits );
		TimeIncrement = GetBits(&bf, bits);
		//printf("TI: %d\n", TimeIncrement);
	}
	
	GetBits(&bf, 1);

	finfo->bih.biWidth = GetBits(&bf, 13);	// vol_width (13 bits) 

	GetBits(&bf, 1);

	finfo->bih.biHeight = GetBits(&bf, 13);	// vol_height (13 bits) 
	
	GetBits(&bf, 3);
	if (Verid == 1)
		sprite_en = GetBits(&bf, 1);
	else 
	{
		sprite_en = GetBits(&bf, 2);
		if (sprite_en == 0x2)
		{
			//printf("MPEG4: GMC AND STATIC SPRITE CODING not supported\n");
			return 0;
		}
	}

	finfo->bih.biCompression = mmioFOURCC('M','P','G','4');
	if (TimeIncrement != 0)
		finfo->FPS = (float)TimeIncrementResolution/TimeIncrement;
	else
		finfo->FPS = (float)TimeIncrementResolution;

	return 1;
}

/* Check MPEG-1/2 header */
int CheckMPEGHeader( unsigned char *bs, int buffer_len, FileInfo *finfo )
{
	unsigned int tmpvar;	
	unsigned int width, height;
	unsigned int frame_rate_code;
	unsigned char *bptr = NULL;
	int i = 0;
					
	// check sequence_header
	for( i = 0 ; i < buffer_len-4 ; i++ )
		if( bs[i] == 0 && bs[i+1] == 0 && bs[i+2] == 1 && bs[i+3] == 0xB3 )
				break;

	if( i == buffer_len  - 4 )
		return -1;

	bptr = bs + i + 4;

	width = (bptr[0] << 4) + (bptr[1] >> 4);
	height = ( (bptr[1] & 0xf) << 8 ) + bptr[2];
				
	finfo->bih.biWidth  = width;
	finfo->bih.biHeight = height;
				
	frame_rate_code = bptr[3] & 15;
	finfo->FPS = (float)frameratecode2framerate[frame_rate_code]/10000;
	
	bptr += 4;
	tmpvar = (bptr[0] << 10) + (bptr[1] << 2 ) + (bptr[2] >> 6);

	finfo->vBitrate = tmpvar * 400;	// 18 bit

	finfo->bih.biCompression = mmioFOURCC('M','P','G','1');

	// check sequence extension header
	for( i = 0 ; i < buffer_len-4 ; i++ )
		if( bs[i] == 0 && bs[i+1] == 0 && bs[i+2] == 1 && bs[i+3] == 0xB5 )
				break;

	if( i == buffer_len  - 4 )
	{
		return 1;
	}else
		finfo->bih.biCompression = mmioFOURCC('M','P','G','2');

	return 1;
}

static int mpg_read_packet_dur( FILE *fp, int id, int *plen, FileInfo *finfo, int use_fread )
{
	int len;
	int time;
	unsigned int pts = 0;
	unsigned int dts = 0;
	unsigned char c = 0;
	int fd = fileno(fp);

	if ( id == 0x1BA)
	{
		unsigned int pack_time;
		unsigned char timestamp[5];
		if (use_fread == 0) {
			read(fd, (void *)timestamp, 5);
		} else {
			if (read_nbytes(timestamp, 1, 5, fp) != 5)
				return -2;
		}
		pack_time = read_pack_timestamp(timestamp);
		if (pack_time != 0)
		{
			if (pack_time > (unsigned int)finfo->FileDuration)
			{
				finfo->FileDuration = pack_time;
			}
		}
	}

	if( id < 0x1BC || id >= 0x1F0 )		return -1;
	if( id == 0x1BE )					return -1; // padding stream
	if( id == 0x1BF )					return -1; // private2

	if (use_fread == 0) {
		len = read_word_a(fd);
	} else {
		len = read_word(fp);
	}
	if( len == 0 || len > MAX_PS_PACKETSIZE )
	{
		return -2;
	}

	while( len > 0 )
	{   // Skip stuFFing bytes
		if (use_fread == 0) {
			c = read_char_a(fd);
		} else {
			c = read_char(fp);
		}
		--len;
		if( c != 0xFF )
			break;
	}
	if((c>>6)==1)  // Read (skip) STD scale & size value
	{	
		//    printf("  STD_scale=%d",(c>>5)&1);
		if (use_fread == 0) {
			lseek64(fd, 1, SEEK_CUR);
			c = read_char_a(fd); 
		} else {
			fseek(fp, 1, SEEK_CUR);
			c = read_char(fp); 
		}
		len -= 2;
	}
	// Read System-1 stream timestamps:
	if((c>>4)==2)
	{
		if (use_fread == 0) {
			time = read_dword_a(fd);
		} else {
			time = read_dword(fp);
		}
		pts = mpeg_timestamp(time, c);
		len-=4;
	} 
	else if((c>>4)==3)
	{
		if (use_fread == 0) {
			time = read_dword_a(fd);
			pts = mpeg_timestamp(time, c);
			c = read_char_a(fd); 
		} else {
			time = read_dword(fp);
			pts = mpeg_timestamp(time, c);
			c = read_char(fp); 
		}
		if((c>>4)!=1) pts=0; //printf("{ERROR4}");
		if (use_fread == 0) {
			time = read_dword_a(fd);
		} else {
			time = read_dword(fp);
		}
		dts = mpeg_timestamp(time, c);
		len-=4+1+4;
	} 
	else if( ( c >> 6 ) == 2 )
	{
		// System-2 (.VOB) stream:
		int pts_flags;
		int hdrlen;

		if (use_fread == 0) {
			c = read_char_a(fd); 
			pts_flags = c >> 6;
			c = read_char_a(fd); 
		} else {
			c = read_char(fp); 
			pts_flags = c >> 6;
			c = read_char(fp); 
		}

		hdrlen=c;
		len -= 2;

		if( hdrlen > len )
		{ 
			return -1;
		}
		if( pts_flags == 2 && hdrlen >= 5 )
		{
			if (use_fread == 0) {
				c = read_char_a(fd); 
				time = read_dword_a(fd);
			} else {
				c = read_char(fp); 
				time = read_dword(fp);
			}
			pts = mpeg_timestamp(time, c);
			len -= 5;
			hdrlen -= 5;
		}
		else if( pts_flags == 3 && hdrlen >= 10 )
		{
			if (use_fread == 0) {
				c = read_char_a(fd); 
				time = read_dword_a(fd);
				pts = mpeg_timestamp(time, c);
				c = read_char_a(fd); 
				time = read_dword_a(fd);
			} else {
				c = read_char(fp); 
				time = read_dword(fp);
				pts = mpeg_timestamp(time, c);
				c = read_char(fp); 
				time = read_dword(fp);
			}
			dts = mpeg_timestamp(time, c);
			len -= 10; 
			hdrlen -= 10;
		}

		len -= hdrlen;

		if( hdrlen > 0 )  
		{
			if (use_fread == 0) {
				lseek64(fd, (LOFF_T)hdrlen, SEEK_CUR);
			} else {
				fseek(fp, hdrlen, SEEK_CUR);	// skip header bytes
			}
		}
	}

	if( id >= 0x1E0 && id <= 0x1EF )
	{
		if( pts != 0 )
		{
			finfo->FileDuration = pts / 90000;
		}
	}

	*plen = len;
	return 0;
}

static int mpg_read_packet( FILE *fp, int id, int *plen, FileInfo *finfo )
{
//	try
	{
		int len;
		unsigned char c = 0;
		unsigned int pts = 0;
		unsigned int dts = 0;
		static int max_len = 1000;
		int read_len;
		unsigned char p[max_len];
		unsigned int pack_time;
		
		if ( id == 0x1BA)
		{
			unsigned char timestamp[5];
			if (read_nbytes(timestamp, 1, 5, fp) != 5)
				return -2;
			pack_time = read_pack_timestamp(timestamp);
			if ((pack_time != 0) && (finfo->FileDuration == 0))
			{
				finfo->FileDuration = pack_time;
			}
		}
		if( id < 0x1BC || id >= 0x1F0 )		return -1;
		if( id == 0x1BE )					return -1; // padding stream
		if( id == 0x1BF )					return -1; // private2
		
		len = read_word(fp);
		//	mp_dbg(MSGT_DEMUX,MSGL_DBG3,"PACKET len=%d",len);
		//  if(len==62480){ demux->synced=0;return -1;} 
		if( len == 0 || len > MAX_PS_PACKETSIZE )
		{
			// mp_dbg(MSGT_DEMUX,MSGL_DBG2,"Invalid PS packet len: %d\n",len);
			return -2;  // invalid packet !!!!!!
		}
		
		//	mpeg_pts_error=0;
		
		if( id == 0x1BC ) 
		{
			parse_psm(fp, len, finfo);
			return 0;
		}
		
		while( len > 0 )
		{   // Skip stuFFing bytes
			c = read_char(fp);
			--len;
			if( c != 0xFF )
				break;
		}
#if 1	// Raymond 2007/10/26
		if((c>>6)==1)  // Read (skip) STD scale & size value
		{	
			//    printf("  STD_scale=%d",(c>>5)&1);
			read_char(fp);
			len -= 2;
			//    printf("  STD_size=%d",d);
			c = read_char(fp); 
		}
		// Read System-1 stream timestamps:
		if((c>>4)==2)
		{
			pts = read_mpeg_timestamp(fp, c);
			len-=4;
		} 
		else if((c>>4)==3)
		{
			pts = read_mpeg_timestamp(fp, c);
			c = read_char(fp); 
			if((c>>4)!=1) pts=0; //printf("{ERROR4}");
			dts = read_mpeg_timestamp(fp, c);
			len-=4+1+4;
		} 
		else
#endif
		if( ( c >> 6 ) == 2 )
		{
			// System-2 (.VOB) stream:
			int pts_flags;
			int hdrlen;
			
			c = read_char(fp); 
			pts_flags = c >> 6;
			c = read_char(fp); 
			hdrlen=c;
			len -= 2;
			
			if( hdrlen > len )
			{ 
				return -1;
			}
			if( pts_flags == 2 && hdrlen >= 5 )
			{
//				fseek(fp, 5, SEEK_CUR);
				c = read_char(fp); 
				pts = read_mpeg_timestamp(fp, c);
				len -= 5;
				hdrlen -= 5;
			}
			else if( pts_flags == 3 && hdrlen >= 10 )
			{
//				fseek(fp, 10, SEEK_CUR);
				c = read_char(fp); 
				pts = read_mpeg_timestamp(fp, c);
				c = read_char(fp); 
				dts = read_mpeg_timestamp(fp, c);
				len -= 10; 
				hdrlen -= 10;
			}
			
			len -= hdrlen;
			
			if( hdrlen > 0 )  
				fseek(fp, hdrlen, SEEK_CUR);	// skip header bytes
			
			//============== DVD Audio sub-stream ======================
			if( id == 0x1BD )
			{
				int aid = read_char(fp);
				--len;
				if( len < 3 ) 
					return -1; // invalid audio packet

				if (len > max_len)
				{
					read_len = max_len;
				} else {
					read_len = len;
				}
				if (read_nbytes(p, read_len, 1, fp) != read_len)
					return -1;
				len -= read_len;

				// AID:
				// 0x20..0x3F  subtitle
				// 0x80..0x9F  AC3 audio
				// 0xA0..0xBF  PCM audio
								
				if (finfo->wf.wFormatTag == 0)
				{
				if( aid >= 0x80 && aid <= 0x9F )
				{
					if((aid & 0xF8) == 0x88)	
					{
						// dts
						int bitrate = 0, samplerate = 0, channel = 0;
						if (mp_get_dts_header(p, read_len, &channel, &samplerate, &bitrate) == 1)
						{
							finfo->bAudio = 1;
							finfo->wf.wFormatTag = AUDIO_DTS;
							finfo->wf.nChannels = channel;
							finfo->wf.nSamplesPerSec = samplerate;
							finfo->wf.nAvgBytesPerSec = bitrate / 8;
							finfo->aBitrate = bitrate;
							finfo->AudioType = finfo->wf.wFormatTag;
						}
					}
					else	
					{
						// ac3
						if (mp_a52_header(&finfo->wf, p, read_len) == 1)
						{
							finfo->bAudio = 1;
							finfo->AudioType = finfo->wf.wFormatTag;
							finfo->aBitrate = finfo->wf.nAvgBytesPerSec * 8;
						}
					}
				}
				else if( ( aid & 0xC0 ) == 0x80 || (aid & 0xE0) == 0x00 ) 
				{					
					// aid = 128 + ( aid & 0x7F );
					// aid = 0x80..0xBF
					if( ( aid & 0xE0 ) == 0xA0 && len >= 3 )
					{
						// dvd pcm
						if (mp_lpcm_header(&finfo->wf, p, read_len) == 1)
						{
							finfo->bAudio = 1;
							finfo->AudioType = AUDIO_LPCM_BE;
							finfo->aBitrate = finfo->wf.nAvgBytesPerSec * 8;
						}
					}
				} 				
				}
			} //if(id==0x1BD)			
		}

#if 0
		else 
		{
			if( c != 0x0f )
			{
				return -1;  // invalid packet !!!!!!
			}
		}
#endif				
		if( len <= 0 || len > MAX_PS_PACKETSIZE )
		{
			// Invalid PS data len: %d\n"
			return -1;  // invalid packet !!!!!!
		}
		
		if( id >= 0x1C0 && id <= 0x1DF )
		{
			// mpeg audio
			int aid = id - 0x1C0;
			unsigned int format = 0;

			if( finfo->wf.wFormatTag == 0 )
			{				
				if (len > max_len)
				{
					read_len = max_len;
				} else {
					read_len = len;
				}

				//read_nbytes(p, read_len, 1, fp);
				if (read_nbytes(p, read_len, 1, fp) != read_len)	//fix mantis: 5658
					return -1;
				len -= read_len;

				switch(aid & 0xE0)  // 1110 0000 b  (high 3 bit: type  low 5: id)
				{
					case 0x00:
						{
							int bitrate = 0, samplerate = 0, channel = 0;
							if (mp_get_mp3_header(p, read_len, &channel, &samplerate, &bitrate) != -1) 
							{
								format = AUDIO_MP2;
								finfo->wf.nSamplesPerSec	= samplerate;
								finfo->wf.nChannels			= channel;
								finfo->aBitrate				= bitrate; 
							}
						}
						break;				
					case 0xA0:
						if (mp_lpcm_header(&finfo->wf, p, read_len) == 1)
						{
							// dvd pcm
							finfo->bAudio = 1;
							finfo->AudioType = AUDIO_LPCM_BE;
							finfo->aBitrate = finfo->wf.nAvgBytesPerSec * 8;
							format = AUDIO_LPCM_BE;
						}
						break;
					case 0x80:	
								if((aid & 0xF8) == 0x88)	
								{
									// dts
									int bitrate = 0, samplerate = 0, channel = 0;
									if (mp_get_dts_header(p, read_len, &channel, &samplerate, &bitrate) == 1)
									{
										finfo->bAudio = 1;
										finfo->wf.wFormatTag = AUDIO_DTS;
										finfo->wf.nChannels = channel;
										finfo->wf.nSamplesPerSec = samplerate;
										finfo->wf.nAvgBytesPerSec = bitrate / 8;
										finfo->AudioType = finfo->wf.wFormatTag;
										finfo->aBitrate = bitrate;
										format = finfo->wf.wFormatTag;
									}
								}
								else	
								{
									// ac3
									if (mp_a52_header(&finfo->wf, p, read_len) == 1)
									{
										finfo->bAudio = 1;
										finfo->AudioType = finfo->wf.wFormatTag;
										finfo->aBitrate = finfo->wf.nAvgBytesPerSec * 8;
										format = finfo->wf.wFormatTag;
									}
								}
								break;
				}
				
				if (format != 0)
				{
					finfo->bAudio = 1;
					finfo->wf.wFormatTag = format;
					finfo->AudioType = finfo->wf.wFormatTag;
				} else {
					return -1;
				}
			}
		} 
		else if( id >= 0x1E0 && id <= 0x1EF )
		{
			// mpeg video
//			int aid = id - 0x1E0;
			
			// Raymond 2008/12/11
			//if( finfo->bih.biCompression == 0 && len < 4096 )	// video codec not found
			if( finfo->bih.biCompression == 0 )	// video codec not found
			{
				int ret = 0;
				if (len > 4096)
					len = 4096;
				unsigned char * phdr = (unsigned char *)malloc(len);

				if (read_nbytes(phdr, 1, len, fp) != len)
					return -1;

				ret = CheckMPEG4Header( phdr, 200, finfo );	// check MPEG-4
				if( ret != 1 )	// check MPEG-1/2
				{
					ret = CheckMPEGHeader( phdr, len, finfo );
				}

				if (ret == 1)
					finfo->bVideo = 1;

				free(phdr);
				len = 0;
			}
			
			// Raymond 2007/10/26
			if (( pts != 0 ) && (finfo->FileDuration == 0))
			{
				finfo->FileDuration = pts / 90000;
			}
		}
		
		*plen = len;
		return 0;
	}
//	catch (...) 
//	{
//		return -1;
//	}
}

// Raymond 2007/10/26
int GetMPGDuration(FILE	*fp, FileInfo *finfo)
{
	unsigned int head = 0;
	int skipped = 0;
	int ret = 0;	
	int packet_len = 0;
	int scan_size = 0;
	int pre_duration;
	int use_fread = 1;
	int fd = fileno(fp);
	char tmp_c;
	LOFF_T pos;

	// Raymond 2008/11/12
	if( finfo->FileSize > 300*1024*1024 )
	{
		// for AVSEQ01.DAT
		scan_size = 1*1024*1024;
	}else if( finfo->FileSize > 600000 )
		scan_size = 250000;
	else if (finfo->FileSize > 100000)
		scan_size = 100000;
	else
		scan_size = finfo->FileSize;

#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
		use_fread = 0;
#endif

	pre_duration = finfo->FileDuration;

	if (use_fread == 0) {
		lseek64(fd, finfo->FileSize - scan_size, SEEK_SET);
	} else {
		fseek(fp, -1*scan_size, SEEK_END);
	}

	// System stream
	do
	{			
		if (use_fread == 0) {
			head = read_dword_a(fd);
			pos = lseek64(fd, 0, SEEK_CUR);
			if (pos >= finfo->FileSize)
				break;
		} else {
			head = read_dword(fp);
		}

		if( ( head & 0xFFFFFF00 ) != 0x100 )
		{
			unsigned int c;
			// sync...
			while( 1 )
			{
				if (use_fread == 0) {
					c = read_char_a(fd);
					pos = lseek64(fd, 0, SEEK_CUR);
					if (pos >= finfo->FileSize)
						break;
				} else {
					c = read_char(fp);
					if( feof(fp) || file_error == 1 ) 
						break;
				}

				head <<= 8;
				if( head != 0x100 )
				{
					head |= c;
					++skipped; 
					continue;
				}
				head |= c;
				break;
			}
		}

		if (use_fread == 1) {
			if( feof(fp) || file_error == 1 ) // Raymond 2008/11/12
				break;
		}

		packet_len = 0;
		ret = mpg_read_packet_dur(fp, head, &packet_len, finfo, use_fread);
		if(!ret)
		{
			if ( (finfo->FileDuration > pre_duration) &&
					(scan_size != finfo->FileSize) )
			{
				break;
			}
			// 20100224_爆料大師\VIDEO_TS\VTS_01_5.VOB
			// timestamp maybe reset.
			if (finfo->FileDuration < pre_duration)
			{
				return 0;
			}
			if( (finfo->bih.biCompression != 0) && (packet_len > 0) )
			{
				if (use_fread == 0) {
					lseek64(fd, (LOFF_T)packet_len, SEEK_CUR);
				} else {
					fseek(fp, packet_len, SEEK_CUR);
				}
			}
		}

		if (use_fread == 1) {
			if( feof(fp) || file_error == 1 ) 
			{
				break;
			}
		}
	} while( 1 );

	return 1;
}

#define mmio_FOURCC( ch0, ch1, ch2, ch3 )				\
		( (uint32_t)(uint8_t)(ch3) | ( (uint32_t)(uint8_t)(ch2) << 8 ) |	\
		( (uint32_t)(uint8_t)(ch1) << 16 ) | ( (uint32_t)(uint8_t)(ch0) << 24 ) )

#define formtypeCDXA             mmio_FOURCC('C', 'D', 'X', 'A')

int mpg_check_file(FILE	*fp, FileInfo *finfo)
{
	unsigned int size2;

	unsigned int head = read_dword(fp);
	unsigned int temp;

	if(head == mmio_FOURCC('R','I','F','F'))
	{
		unsigned int riff_type;			

		if (read_nbytes(&size2, 1, 4, fp) != 4)
			return 0;
		riff_type = read_dword(fp);

		if( riff_type == formtypeCDXA )			// "CDXA"	
			return 1;
//			printf("RIFF type : %.4s\n", (char *)&riff_type);
		else	// for DAT = CDXA
			return 0;
//			printf("Unknown RIFF type %.4s\n", (char *)&riff_type);
	}
	// Raymond 2007/08/31
	else if ( head == mmio_FOURCC('C','o','n','t') )	// Content-type
	{
		fseek(fp, 50, SEEK_SET);
		head = read_dword(fp);
		if( head == 0x1BA )
			return 1;
		else 
			return 0;
	}
	//
	else
	{
		int skipped = 0, c = 0;
		int scan_size = 600 * 1024; // fix for ed24p_09.m2v
		if ((LOFF_T)scan_size > finfo->FileSize)
			scan_size = (int)(finfo->FileSize / 2);

		// check if this is MPEG-TS 
		if (head >> 24 == 0x47)
		{
			return 0;
		}
		temp = read_dword(fp);
		if (temp >> 24 == 0x47)
		{
			return 0;
		}
		// check if this is mov
		if (temp == mmio_FOURCC('f','t','y','p'))
		{
			return 0;
		}
		fseek(fp, -4, SEEK_CUR);

		while( skipped < scan_size)
		{
			if( head == 0x1B5 )
			{
				// for m2v
				// sequenct_extension
				fseek(fp, 6, SEEK_CUR);
				head = read_dword(fp);
				if ((head >> 8) == 0x1)
					return 1;
			}
			if( head == 0x1B3 )
			{
				// for m2v
				// sequence_header
				fseek(fp, 7, SEEK_CUR);
				temp = read_char(fp);
				if (((temp >> 1) & 0x1) == 1)
				{
					fseek(fp, 63, SEEK_CUR);
					temp = read_char(fp);
					if ((temp & 0x1) == 1)
					{
						fseek(fp, 64, SEEK_CUR);
					}
				} else {
					if ((temp & 0x1) == 1)
					{
						fseek(fp, 64, SEEK_CUR);
					}
				}

				head = read_dword(fp);
				if ((head >> 8) == 0x1)
					return 1;

				while( head == 0 )
				{
					if( skipped > scan_size/2)
						break;
					c = read_char(fp);
					head <<= 8;
					head |= c;
					if (head == 0x1)
						return 1;
					++skipped; 
				}
			}
			else if( head == 0x1BA )
			{
				// MPEG-PS pack_start_code
				c = read_char(fp);
				if ((c >> 4) == 0x2)
				{
					// MPEG1-PS
					fseek(fp, 7, SEEK_CUR);
				}
				else if ((c >> 6) == 0x1)
				{
					// MPEG2-PS
					fseek(fp, 8, SEEK_CUR);
					temp = read_char(fp) & 0x7;
					if (temp != 0)
					{
						fseek(fp, temp, SEEK_CUR);
					}

				}
				else
				{
					return 0;
				}

				head = read_dword(fp);
				if ((head >> 8) == 0x1)
					return 1;
			}
			else if( head == 0x1B0 )
			{
				// MPEG-4 visual sequenct start
				temp = read_char(fp);
				head = read_dword(fp);
				// user data
				if (head == 0x1b2)
				{
					head = read_dword(fp);
					while ((head >> 8) != 0x1)
					{
						if( skipped > scan_size/2)
							break;
						c = read_char(fp);
						head <<= 8;
						head |= c;
						++skipped; 
					}
				}
				if (head == 0x1b5)
					return 1;
			}
			else if( (head & 0xffffffe0) == 0x100)
			{
				// MPEG-4 video object start
				head = read_dword(fp);
				// MPEG-4 video object layer start
				if( (head & 0xffffffe0) == 0x120)
					return 1;
			}
#if 1	//fix mantis:4675
			else if( head >> 24 == 0x47 )
			{
				c = 1;
				fseek(fp, 184, SEEK_CUR);
				head = read_dword(fp);
				if( head >> 24 == 0x47 )	//MPEG-TS 188 case
				{
					c++;
					while (1)
					{
						fseek(fp, 184, SEEK_CUR);
						head = read_dword(fp);
						if( head >> 24 == 0x47 )
							c++;
						else
							break;
						if (c > 5)
							return 0;	// check if this is MPEG-TS
					}
				}
				else
				{
					head = read_dword(fp);
					if( head >> 24 == 0x47 )	//MPEG-TS 192 case
					{
						c++;
						while (1)
						{
							fseek(fp, 188, SEEK_CUR);
							head = read_dword(fp);
							if( head >> 24 == 0x47 )
								c++;
							else
								break;
							if (c > 5)
								return 0;	// check if this is MPEG-TS
						}
					}
				}
				++skipped;
			}
#endif

			else
			{
				c = read_char(fp);
				head <<= 8;
				head |= c;
				++skipped; 
			}
		}
	}

	return 0;
}

int MPG_Parser(FILE	*fp, FileInfo *finfo)
{
	unsigned int head = 0;
	int pes = 1;
	int skipped = 0;
	int skip_mp2 = 0;
	int max_packs = 1024;
	int skip_mp2_audio = 0;
	int ret = 0;	
	int eof = 0;
	int pos = 0;
	int synced = 0;
	int packet_len = 0;
	// Raymond 2007/06/15 - fix bug if file too small
	int packet_num = finfo->FileSize / 2048;
	if( packet_num < max_packs )
		max_packs = packet_num;
		
	bElementary = 0;	// Raymond 2007/11/13
		
//	try
	{
		// try to pre-detect PES:
		head = read_dword(fp);
		
		if( head == 0x1E0 || head == 0x1C0 )
		{
			head = read_word(fp);
			if( head > 1 && head <= 2048 )
				pes = 0; // synced = 3; // PES...
		}
		
		if( !pes ) 
			synced = 3; // hack!
				
		if( head == mmio_FOURCC('R','I','F','F') )	// for DAT = CDXA
		{
			unsigned int riff_type, size2;			

			if (read_nbytes(&size2, 1, 4, fp) != 4)
				return 0;
			riff_type = read_dword(fp);// Raymond 2007/12/11	
			
			if( riff_type == formtypeCDXA )		// "CDXA"	
				fseek(fp, 60000, SEEK_SET);
		}
		// Raymond 2007/08/31
		else if ( head == mmio_FOURCC('C','o','n','t') )	// Content-type
		{
			fseek(fp, 50, SEEK_SET);
		}
		// Raymond 2008/12/12
		else if( head == 0x100 ) 	// MPEG-4 ES
		{
			int ret = 0;
			unsigned char * phdr = (unsigned char *)malloc(200);
			
			fseek(fp, 0, SEEK_SET);

			if (read_nbytes(phdr, 1, 200, fp) != 200)
				return 0;
			
			ret = CheckMPEG4Header( phdr, 200, finfo );	// check MPEG-4
			if( ret == 1 )	
			{
				finfo->bVideo = 1;
				bElementary = 1;
				free(phdr);
				return 1;
			}
			
			free(phdr);			
			fseek(fp, 0, SEEK_SET);
		}
		else 
			fseek(fp, 0, SEEK_SET);		// reset to the begin
				
		// System stream
		do
		{
//			skipped = 0;
			
			head = read_dword(fp);
			
			if( ( head & 0xFFFFFF00 ) != 0x100 )
			{
				// sync...
				while( 1 )
				{
					int c = read_char(fp);
					if( feof(fp) || file_error == 1 ) 
						break;
					
					head <<= 8;
					if( head != 0x100 )
					{
						head |= c;
						++skipped; 
						continue;
					}
					head |= c;
					break;
				}
			}
			
			if( feof(fp) || file_error == 1 ) 
				break;

			pos = ftell(fp);
			//printf("head: %04x, pos: %d(%x)\n", head, pos, pos);
			
			// Raymond 2008/12/11
			if( pes == 1 )
			{
				// VIDEO_MPEG12
				if( head == 0x1B3 )		// sequence_header	
				{
					int temp, width, height;
					int frame_rate_code;
					
					temp = read_dword(fp);
					height = temp >> 8;
					
					width = (height >> 12);
					height = (height & 0xfff);
					
					finfo->bih.biWidth  = width;
					finfo->bih.biHeight = height;
					
					frame_rate_code = temp & 15;
					finfo->FPS = (float)frameratecode2framerate[frame_rate_code]/10000;
					
					temp = (read_dword(fp) >> 14) & 0x3ffff;
					if (temp == 0x3ffff) // variable bitrate
						finfo->vBitrate = 0;
					else
						finfo->vBitrate = (float)( temp * 400 );	// 18 bit
					
					finfo->bih.biCompression = mmioFOURCC('M','P','G','1');
					
					if( skipped == 0 || synced == 0 )
					{
						// MPEG Elementary Video
						finfo->bVideo = 1;
						bElementary = 1;
					}
					skip_mp2 = pos + 256;
				}
				
				if( head == 0x1B5 )		// sequence_extension	
				{
					int temp;
					temp = read_char(fp);
					if (temp >> 4 == 0x1)
					{
						read_skip(fp, 1);
						temp = ((read_word(fp) & 0x1fff) >> 1) << 18;
						finfo->bih.biCompression = mmioFOURCC('M','P','G','2');
						finfo->vBitrate += temp * 400;
						if( bElementary == 1 )
							return 1;
					}
				}
				
				if( head == 0x100 )
				{
					unsigned char * phdr = (unsigned char *)malloc(200);
					fseek(fp, -4, SEEK_CUR);
					if (read_nbytes(phdr, 1, 200, fp) != 200)
						return 0;
					ret = CheckMPEG4Header( phdr, 200, finfo );	// check MPEG-4
					if( ret == 1 )	
					{
						finfo->bVideo = 1;
						bElementary = 1;
						free(phdr);
						return 1;
					}
					free(phdr);
				}				/*
				// VIDEO_H264
				if( ( head  & ~0x60 ) == 0x107 && head != 0x107 ) 
				{
					finfo->bih.biCompression = mmioFOURCC('H','2','6','4');
					finfo->bVideo = 1;
					
					// width / height ?
				}
				*/
			}	
			// end Raymond 2008/12/11

			// Raymond 2007/01/30
			if( bElementary == 1 && skipped > skip_mp2)
				return 1;	// if not find 0xB5

			if( synced == 0 )
			{
				if( head == 0x1BA ) 
					synced = 1; //else
				if( head == 0x1BD || ( head >= 0x1C0 && head <= 0x1EF ) ) 
					synced = 3; // PES?
			}
			else
			{
				if( synced == 1 )
				{
					if( head == 0x1BB || head == 0x1BD || ( head >= 0x1C0 && head <= 0x1EF ) )
					{
						synced = 2;
					}
					else 
						synced = 0;
				} 
				
				if( synced >= 2 )
				{
					packet_len = 0;
					ret = mpg_read_packet(fp, head, &packet_len, finfo);
					if(!ret)
					{
						if ((finfo->bVideo) && (finfo->bAudio))
							eof = 1;
						else if (finfo->bVideo)
						{
							if (skip_mp2_audio == 0)
							{
								// Baby.mpg
								skip_mp2_audio = skipped + 1024*20;
								//printf("get video pso: %d, end %d!!!\n", skipped, skip_mp2_audio);
							}
							if (skip_mp2_audio <= skipped)
							{
								//printf("maybe mpep video only!!!\n");
								eof = 1;
							}
						}

//						if( finfo->bih.biCompression != 0 )
							fseek(fp, packet_len, SEEK_CUR);

						if( --max_packs == 0 )
							eof = 1;
						
						if( max_packs < (skipped >> 11) )	// Raymond 2007/09/21
							eof = 1;
						
//						if( finfo->bih.biCompression != 0 && finfo->bAudio == 1 && max_packs < 230 )
						if( finfo->bih.biCompression != 0 && finfo->bAudio == 1 && max_packs < 480 )	// Raymond 2007/09/17
							eof = 1;					
					}
					
//					if( synced == 3 ) 
//						synced = (ret == 1 ) ? 2 : 0; // PES detect
				} 
			}
			
		} while( !feof(fp) && !eof );
	}
//	catch (...) 
//	{
//		return 0;
//	}
	
	return 1;
}

// MOV - MP4 , 3GP , 3G2

#define MOV_FOURCC(a,b,c,d) ( ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | (d) )

int mov_check_file(FILE	*fp, FileInfo *finfo)
{
    int flags = 0;
    int no = 0;
	int fd = 0;
	int use_fread = 1;
	LOFF_T pos;

#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
	{
		use_fread = 0;
		fd = fileno(fp);
		lseek64(fd, 0, SEEK_SET);
	}
#endif
//	try
	{
		while(1)
		{
			int skipped = 8;
			uint64_t len;
			unsigned int id;
			unsigned char * id_str;

			if (use_fread == 0) {
				len  = read_dword_a(fd);
				id  = read_dword_a(fd);
				pos = (LOFF_T)lseek64(fd, 0, SEEK_CUR);
				if (pos >= finfo->FileSize)
					break;	// EOF
				if (len == 1)
				{
					len = read_qword_a(fd);
					skipped += 8;
				}
			} else {
				len  = read_dword(fp);
				id  = read_dword(fp);
				if( feof(fp) || file_error == 1 ) 
					break;	// EOF
				if (len == 1)
				{
					len = read_qword(fp);
					skipped += 8;
				}
			}
			
			if( len < 8 ) 
				break; // invalid chunk
			
			id_str = (unsigned char *)&id;
			//printf("ID is %c%c%c%c, len is %llu\n", id_str[0], id_str[1], id_str[2], id_str[3], len);
			
			switch(id)
			{
			case MOV_FOURCC('f','t','y','p'):		
				break;
				
			case MOV_FOURCC('m','o','o','v'):			
				flags |= 1;
				break;
			case MOV_FOURCC('w','i','d','e'):	// 'WIDE' chunk
				if( flags & 2 ) 
					break;
			case MOV_FOURCC('m','d','a','t'):			
				flags |= 2;
				if( flags == 3 )
					return 1;	// if we're over the headers, then we can stop parsing here!			
				break;
			case MOV_FOURCC('f','r','e','e'):
			case MOV_FOURCC('s','k','i','p'):
			case MOV_FOURCC('j','u','n','k'):	// free space
				break;
			case MOV_FOURCC('p','n','o','t'):
			case MOV_FOURCC('P','I','C','T'):
				/* dunno what, but we shoudl ignore it */
				break;
			default:
				if( no == 0 )				
					return 0;	// first chunk is bad!
			}
			
			if (use_fread == 0) {
				if( lseek64(fd, len - skipped, SEEK_CUR) == -1)
				{
					break;
				}
			} else {
				if( fseeko(fp, len - skipped, SEEK_CUR) )
				{
					break;
				}
			}
			++no;
		}
		
		if( flags == 3 )
		{	
			return 1;	// got 'mdat' and 'moov'
		}

		return 0;
	}
//	catch (...) 
//	{
//		return 0;
//	}    
}

enum TRACK_TYPE_3GP
{
	UNKNOWN_TRACK = 0,		
	VIDEO_TRACK,		// "vide"
	AUDIO_TRACK,		// "soun"
	ODSM_TRACK,			// "odsm"
	SDSM_TRACK,			// "sdsm"
	HINT_TRACK,			// "hint"
};

enum CODEC_TYPE_3GP
{
	NO_CODEC = 0,		
	MPEG4_VIDEO,		// "mp4v"	- not support , patent issue !
	MPEG4_AUDIO,		// "mp4a"	- AAC, MP3, if 3g2, it might be QCELP !
	H263_VIDEO,			// "s263"
	AMR_SPEECH,			// "samr"
	UNKNOWN_CODEC,		// other not support codec ..
};

typedef struct  
{
	LOFF_T		AtomSize;
	char	AtomType[4];
	
} ATOM_3GP;

unsigned long BElong_32(unsigned long long32)
{
	unsigned long result,temp;

	temp = long32;
	result = (temp & 0xFF) << 24 ;
	temp = long32;
	result += (temp & 0xFF00) << 8;
	temp = long32;
	result += (temp & 0xFF0000) >> 8;
	temp = long32;
	result += (temp & 0xFF000000) >> 24;

	return result;
}

/* one byte tag identifiers */
#define MP4ODescrTag				0x01 
#define MP4IODescrTag				0x02 
#define MP4ESDescrTag				0x03 
#define MP4DecConfigDescrTag		0x04 
#define MP4DecSpecificDescrTag		0x05 
#define MP4SLConfigDescrTag			0x06 
#define MP4ContentIdDescrTag		0x07 
#define MP4SupplContentIdDescrTag	0x08 
#define MP4IPIPtrDescrTag			0x09 
#define MP4IPMPPtrDescrTag			0x0A 
#define MP4IPMPDescrTag				0x0B 
#define MP4RegistrationDescrTag		0x0D 
#define MP4ESIDIncDescrTag			0x0E 
#define MP4ESIDRefDescrTag			0x0F 
#define MP4FileIODescrTag			0x10 
#define MP4FileODescrTag			0x11 
#define MP4ExtProfileLevelDescrTag	0x13 
#define MP4ExtDescrTagsStart		0x80 
#define MP4ExtDescrTagsEnd			0xFE 

/* object type identifiers in the ESDS */
/* See http://gpac.sourceforge.net/tutorial/mediatypes.htm */
/* BIFS stream version 1 */
#define MP4OTI_MPEG4Systems1                                0x01
/* BIFS stream version 2 */
#define MP4OTI_MPEG4Systems2                                0x02
/* MPEG-4 visual stream */
#define MP4OTI_MPEG4Visual                                  0x20
/* MPEG-4 audio stream */
#define MP4OTI_MPEG4Audio                                   0x40
/* MPEG-2 visual streams with various profiles */
#define MP4OTI_MPEG2VisualSimple                            0x60
#define MP4OTI_MPEG2VisualMain                              0x61
#define MP4OTI_MPEG2VisualSNR                               0x62
#define MP4OTI_MPEG2VisualSpatial                           0x63
#define MP4OTI_MPEG2VisualHigh                              0x64
#define MP4OTI_MPEG2Visual422                               0x65
/* MPEG-2 audio stream part 7 ("AAC") with various profiles */
#define MP4OTI_MPEG2AudioMain                               0x66
#define MP4OTI_MPEG2AudioLowComplexity                      0x67
#define MP4OTI_MPEG2AudioScaleableSamplingRate              0x68
/* MPEG-2 audio part 3 ("MP3") */
#define MP4OTI_MPEG2AudioPart3                              0x69
/* MPEG-1 visual visual stream */
#define MP4OTI_MPEG1Visual                                  0x6A
/* MPEG-1 audio stream part 3 ("MP3") */
#define MP4OTI_MPEG1Audio                                   0x6B
/* JPEG visual stream */
#define MP4OTI_JPEG                                         0x6C

int mp4_read_descr_len(FILE *fp, FileInfo *finfo) 
{
	uint8_t b;
	uint8_t numBytes = 0;
	uint32_t length = 0;
	int fd;
	int use_fread = 1;

#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
	{
		use_fread = 0;
		fd = fileno(fp);
	}
#endif

	do {
		if (use_fread == 0) {
			b = read_char_a(fd);
		} else {
			b = read_char(fp);
		}
		numBytes++;
		length = (length << 7) | (b & 0x7F);
	} while ((b & 0x80) && numBytes < 4);
	
	//printf("MP4 read desc len: %d\n", length);
	return length;
}

/* parse the data part of MP4 esds atoms */
//int mp4_parse_esds(TRACK_TYPE_3GP	TrackType, FILE *fp, FileInfo *finfo) 
int mp4_parse_esds(int TrackType, FILE *fp, FileInfo *finfo) 
{
//	try
//	{
		uint8_t len;
		uint8_t  version;
		
		/* 0x03 ESDescrTag */
		uint16_t ESId;
		uint8_t  streamPriority;
		
		/* 0x04 DecConfigDescrTag */
		uint8_t  objectTypeId;
		uint8_t  streamType;
		
		uint32_t maxBitrate;
		uint32_t avgBitrate;
		
		/* 0x05 DecSpecificDescrTag */
		uint8_t  decoderConfigLen;

		int fd = 0;
		int use_fread = 1;

#if defined(HAVE_ANDROID_OS)
		if( finfo->FileSize > (LOFF_T)INT_MAX)
		{
			use_fread = 0;
			fd = fileno(fp);
		}
#endif
		
		if (use_fread == 0) {
			version = read_char_a(fd);
			lseek64(fd, 3, SEEK_CUR);

			/* get and verify ES_DescrTag */
			if ( read_char_a(fd) == MP4ESDescrTag )	// 0x03
			{
				/* read length */
				len = mp4_read_descr_len(fp, finfo);

				ESId = read_word_a(fd);
				streamPriority = read_char_a(fd);

				if (len < (5 + 15)) 
				{
					return 1;
				}
			}
			else
			{
				ESId = read_word_a(fd);
			}

			/* get and verify DecoderConfigDescrTab */
			if (read_char_a(fd) != MP4DecConfigDescrTag)	// 0x04
			{
				return 1;
			}

			/* read length */
			len = mp4_read_descr_len(fp, finfo);

			objectTypeId	= read_char_a(fd);
			streamType		= read_char_a(fd);
			//	bufferSizeDB	= read_int24(fp);
			lseek64(fd, 3, SEEK_CUR);
			maxBitrate		= read_dword_a(fd);
			avgBitrate		= read_dword_a(fd);	// esds->avgBitrate/1000.0
		} else {
			version = read_char(fp);
			fseek(fp, 3, SEEK_CUR);

			/* get and verify ES_DescrTag */
			if ( read_char(fp) == MP4ESDescrTag )	// 0x03
			{
				/* read length */
				len = mp4_read_descr_len(fp, finfo);

				ESId = read_word(fp);
				streamPriority = read_char(fp);

				if (len < (5 + 15)) 
				{
					return 1;
				}
			}
			else
			{
				ESId = read_word(fp);
			}

			/* get and verify DecoderConfigDescrTab */
			if (read_char(fp) != MP4DecConfigDescrTag)	// 0x04
			{
				return 1;
			}

			/* read length */
			len = mp4_read_descr_len(fp, finfo);

			objectTypeId	= read_char(fp);
			streamType		= read_char(fp);
			//	bufferSizeDB	= read_int24(fp);
			fseek(fp, 3, SEEK_CUR);
			maxBitrate		= read_dword(fp);
			avgBitrate		= read_dword(fp);	// esds->avgBitrate/1000.0
		}
		
		if ( TrackType == AUDIO_TRACK )
//			finfo->wf.nAvgBytesPerSec = avgBitrate / 8;
			finfo->aBitrate = (avgBitrate + 500);
		else if ( TrackType == VIDEO_TRACK )
			finfo->vBitrate = (float)avgBitrate / 1024;
		
		if( objectTypeId == MP4OTI_MPEG1Audio || objectTypeId == MP4OTI_MPEG2AudioPart3 )
			finfo->AudioType = mmioFOURCC('M', 'P', '3', ' ');
//			finfo->wf.wFormatTag = 0x55;	// MP3
		else if ( objectTypeId == 0xDD )	// Vorbis ?
			finfo->AudioType = mmioFOURCC('v', 'r', 'b', 's');
		else if ( objectTypeId == 0xE1 )	// QCELP
			finfo->AudioType = mmioFOURCC('Q', 'C', 'L', 'P');
		
		decoderConfigLen = 0;
		
		if (len < 15) 
		{
			return 0;
		}
		
		if (use_fread == 0) {
			/* get and verify DecSpecificInfoTag */
			if (read_char_a(fd) != MP4DecSpecificDescrTag)	// 0x05
			{
				return 0;
			}

			/* read length */
			decoderConfigLen = len = mp4_read_descr_len(fp, finfo); 

			if( decoderConfigLen )
			{
				uint8_t temp = read_char_a(fd);
				if( ( temp >> 3 ) == 29 )
					finfo->AudioType = 0x1d61346d; // "m4a " : request multi-channel mp3 decoder
				if( ( temp >> 3 ) == 1 && finfo->bAudio && finfo->AudioType==0x6134706D )	//AAC main profile
					finfo->bAudio = 0;
			}
		} else {
			/* get and verify DecSpecificInfoTag */
			if (read_char(fp) != MP4DecSpecificDescrTag)	// 0x05
			{
				return 0;
			}

			/* read length */
			decoderConfigLen = len = mp4_read_descr_len(fp, finfo); 

			if( decoderConfigLen )
			{
				uint8_t temp = read_char(fp);
				if( ( temp >> 3 ) == 29 )
					finfo->AudioType = 0x1d61346d; // "m4a " : request multi-channel mp3 decoder
				if( ( temp >> 3 ) == 1 && finfo->bAudio && finfo->AudioType==0x6134706D )	//AAC main profile
					finfo->bAudio = 0;
			}
		}
		
		/* will skip the remainder of the atom */	
		return 0;
//	}
//	catch (...) 
//	{
//		return 0;
//	}
}

void ParseSTSD(int TrackType, FILE *fp, FileInfo *finfo )
{
	// get (mp4a, mp4v, s263, samr, sawb, avc1) from "stsd" atom
	ATOM_3GP	tAtom;	
	char temp[8];
	unsigned char * id_str;
	int fd;
	int use_fread = 1;

#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
	{
		use_fread = 0;
		fd = fileno(fp);
	}
#endif

	if (use_fread == 0) {
		read(fd, (void *)&temp, 8);	// [4-7] : EntryCount
		tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
		read(fd, (void *)(&tAtom.AtomType), 4);
		if (tAtom.AtomSize == 1)
			tAtom.AtomSize = read_qword_a(fd);
	} else {
		if (read_nbytes(temp, 1, 8, fp) != 8)	// [4-7] : EntryCount
			return;
		tAtom.AtomSize = (LOFF_T)read_dword(fp);
		if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
			return;
		if (tAtom.AtomSize == 1)
			tAtom.AtomSize = read_qword(fp);
	}
	id_str = (unsigned char *)&tAtom.AtomType;
	//printf("\t\tSTSD type: %c%c%c%c, size: %"PRIu64"\n", id_str[0], id_str[1], id_str[2], id_str[3], tAtom.AtomSize);
				
	if ( TrackType == VIDEO_TRACK )
	{
		unsigned short width , height;
		
		if (use_fread == 0) {
			lseek64(fd, 24, SEEK_CUR);
			read(fd, (void *)&width, 2);
			read(fd, (void *)&height, 2);
		} else {
			fseeko(fp, 24, SEEK_CUR);
			if (read_nbytes(&width, 1, 2, fp) != 2)
				return;
			if (read_nbytes(&height, 1, 2, fp) != 2)
				return;
		}
		
		finfo->bih.biWidth  = ( (width  & 0xFF) << 8 ) | (width  >> 8);
		finfo->bih.biHeight = ( (height & 0xFF) << 8 ) | (height >> 8);
		
		finfo->bih.biCompression = mmioFOURCC(tAtom.AtomType[0], tAtom.AtomType[1], tAtom.AtomType[2], tAtom.AtomType[3]);
		
		if( !strncmp(tAtom.AtomType, "avc1", 4) )
		{
#if 1	// Raymond 2007/12/14
			// check if baesline profile
			LOFF_T avc1_len = tAtom.AtomSize;
			if( avc1_len > 58 )	// 50 + 8
			{
				if (use_fread == 0) {
					lseek64(fd, 50, SEEK_CUR);
				} else {
					fseeko(fp, 50, SEEK_CUR);
				}
				avc1_len -= 58;
								
				while( avc1_len > 0 )
				{
					if (use_fread == 0) {
						tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
						read(fd, (void *)(&tAtom.AtomType), 4);
						if (tAtom.AtomSize == 1)
							tAtom.AtomSize = read_qword_a(fd);
					} else {
						tAtom.AtomSize = (LOFF_T)read_dword(fp);
						if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
							return;
						if (tAtom.AtomSize == 1)
							tAtom.AtomSize = read_qword(fp);
					}
					
					// bbc-africa_m720p.mov - has additional "colr" atom
					if ( !strncmp(tAtom.AtomType, "avcC", 4) ) 
					{
						if (use_fread == 0) {
							read(fd, (void *)&temp, 2);
						} else {
							if (read_nbytes(temp, 1, 2, fp) != 2)
								return;
							if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
								break;	// EOF
						}
						finfo->H264Profile = (unsigned int)temp[1];	// 66 : baseline profile
						break;
					}
					else
					{
						avc1_len -= tAtom.AtomSize;
						if (use_fread == 0) {
							lseek64(fd, tAtom.AtomSize - 8, SEEK_CUR);
						} else {
							fseek(fp, tAtom.AtomSize - 8, SEEK_CUR);
						}
					}
				}
			}
#endif
		}
		else
		{
			// parse esds atom
			if (use_fread == 0) {
				lseek64(fd, 50, SEEK_CUR);
				tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
				read(fd, (void *)(&tAtom.AtomType), 4);
				if (tAtom.AtomSize == 1)
					tAtom.AtomSize = read_qword_a(fd);
			} else {
				fseeko(fp, 50, SEEK_CUR);
				tAtom.AtomSize = (LOFF_T)read_dword(fp);
				if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
					return;
				if (tAtom.AtomSize == 1)
					tAtom.AtomSize = read_qword(fp);
			}
			if ( !strncmp(tAtom.AtomType, "esds", 4) ) 
			{
				mp4_parse_esds(TrackType, fp, finfo);
			}
		}
	}
	else if ( TrackType == AUDIO_TRACK )
	{
		unsigned short sr, ch;
		
		if (use_fread == 0) {
			lseek64(fd, 16, SEEK_CUR);
			read(fd, &ch, 2);
			lseek64(fd, 6, SEEK_CUR);
			read(fd, &sr, 2);
		} else {
			fseek(fp, 16, SEEK_CUR);
			if (read_nbytes(&ch, 1, 2, fp) != 2)
				return;
			fseek(fp, 6, SEEK_CUR);
			if (read_nbytes(&sr, 1, 2, fp) != 2)
				return;
		}
		
		if( !strncmp(tAtom.AtomType, "samr", 4) || !strncmp(tAtom.AtomType, "sawb", 4) ) 
			finfo->wf.nChannels = 1;
		else
			finfo->wf.nChannels			= ( (ch & 0xFF) << 8 ) | (ch >> 8);

		if( !strncmp(tAtom.AtomType, "samr", 4) )
			finfo->wf.nSamplesPerSec	= 8000;
		else if(	!strncmp(tAtom.AtomType, "sawb", 4) ) 
			finfo->wf.nSamplesPerSec	= 16000;
		else
			finfo->wf.nSamplesPerSec	= ( (sr & 0xFF) << 8 ) | (sr >> 8);
		
		finfo->AudioType = mmioFOURCC(tAtom.AtomType[0], tAtom.AtomType[1], tAtom.AtomType[2], tAtom.AtomType[3]);					

		// parse esds atom
		if (use_fread == 0) {
			lseek64(fd, 2, SEEK_CUR);
			tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
			read(fd, (void *)(&tAtom.AtomType), 4);
			if (tAtom.AtomSize == 1)
				tAtom.AtomSize = read_qword_a(fd);
		} else {
			fseek(fp, 2, SEEK_CUR);
			tAtom.AtomSize = (LOFF_T)read_dword(fp);
			if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
				return;
			if (tAtom.AtomSize == 1)
				tAtom.AtomSize = read_qword(fp);
		}
		if ( !strncmp(tAtom.AtomType, "esds", 4) ) 
		{
			mp4_parse_esds(TrackType, fp, finfo);
		}
	}
}

int GetTrackType(LOFF_T nPos, LOFF_T AtomSize, FILE *fp, FileInfo *finfo)
{
	int TrackType = UNKNOWN_TRACK;  
	int fd;
	int use_fread = 1;
	unsigned char * id_str;

#if defined(HAVE_ANDROID_OS)
	if( finfo->FileSize > (LOFF_T)INT_MAX)
	{
		use_fread = 0;
		fd = fileno(fp);
	}
#endif

//	try
	{
		int nRet = 0;
		LOFF_T CurrPos = nPos + 8;
		LOFF_T StopPos = nPos + AtomSize;
		int Layer = 0;	
		const char* p3GPAtomType[2] = { "mdia","hdlr" };
		ATOM_3GP	tAtom;
				
		// trak -> mdia -> (hdlr, minf) -> stbl -> stsd -> (mp4a, mp4v, s263, samr) 
		while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
		{		
			while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
			{
				if (use_fread == 0) {
					nRet = lseek64(fd, CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
					read(fd, (void *)(&tAtom.AtomType), 4);
					if (tAtom.AtomSize == 1)
						tAtom.AtomSize = read_qword_a(fd);
				} else {
					nRet = fseeko(fp, CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword(fp);
					if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
						return UNKNOWN_TRACK;
					if (tAtom.AtomSize == 1)
						tAtom.AtomSize = read_qword(fp);
					if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
						break;	// EOF
				}
				
				id_str = (unsigned char *)&tAtom.AtomType;
				//printf("\ttrack type: %c%c%c%c, size: %"PRIu64"\n", id_str[0], id_str[1], id_str[2], id_str[3], tAtom.AtomSize);
				
				if ( ( tAtom.AtomSize <= 0 ) || ( tAtom.AtomSize + CurrPos > StopPos ) ) 
				{
					return UNKNOWN_TRACK;	// AtomSize error!	
				}

				// get track type 
				if ( !strncmp(tAtom.AtomType, "hdlr", 4) ) 
				{	
					char temp[8];
					char HandleType[4];

					if (use_fread == 0) {
						read(fd, (void *)temp, 8);
						read(fd, (void *)HandleType, 4);
					} else {
						if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
							break;	// EOF
						if (read_nbytes(temp, 1, 8, fp) != 8)
							return UNKNOWN_TRACK;
						if (read_nbytes(HandleType, 1, 4, fp) != 4)
							return UNKNOWN_TRACK;
					}

					if( strncmp(&temp[4], "dhlr", 4) )	// "mhlr" or "dhlr" in MOV , "" in MP4
					{
						if ( !strncmp(HandleType, "vide", 4) ) 
						{									
							TrackType = VIDEO_TRACK;		
							finfo->bVideo = 1;
							return TrackType;
						}
						else if ( !strncmp(HandleType, "soun", 4) ) 
						{									
							TrackType = AUDIO_TRACK;
							finfo->bAudio = 1;
							return TrackType;
						}
					}
				}

				CurrPos += tAtom.AtomSize;
				
				if ( !strncmp(tAtom.AtomType, p3GPAtomType[Layer], 4) ) 
				{		
					Layer++;
					break;				
				}
			}
			
			StopPos = CurrPos;

			CurrPos = StopPos - tAtom.AtomSize + 8;	// + next layer offset
		}		

		return TrackType;
	}
//	catch (...) 
//	{
//		return TrackType;
//	}	
}

int GetTrackInfo( int TrackType, LOFF_T nPos, LOFF_T AtomSize, FILE *fp, FileInfo *finfo )
{
//	try
	{
		int nRet = 0;
		LOFF_T CurrPos = nPos + 8;
		LOFF_T StopPos = nPos + AtomSize;
		int Layer = 0;	
		const char* p3GPAtomType[4] = { "mdia","minf","stbl","stsd" };
		ATOM_3GP	tAtom;
		unsigned char * id_str;
		int fd = 0;
		int use_fread = 1;

#if defined(HAVE_ANDROID_OS)
		if( finfo->FileSize > (LOFF_T)INT_MAX)
		{
			use_fread = 0;
			fd = fileno(fp);
		}
#endif
	
		// trak -> mdia -> (hdlr, minf) -> stbl -> stsd -> (mp4a, mp4v, s263, samr) 
		while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
		{		
			while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
			{
				if (use_fread == 0) {
					nRet = lseek64(fd, CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
					read(fd, (void *)(&tAtom.AtomType), 4);
					if (tAtom.AtomSize == 1)
						tAtom.AtomSize = read_qword_a(fd);
				} else {
					nRet = fseeko(fp, CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword(fp);
					if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
						return 0;
					if (tAtom.AtomSize == 1)
						tAtom.AtomSize = read_qword(fp);
					if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
						break;	// EOF
				}
				
				id_str = (unsigned char *)&tAtom.AtomType;
				//printf("track info type: %c%c%c%c, size: %"PRIu64"\n", id_str[0], id_str[1], id_str[2], id_str[3], tAtom.AtomSize);
				if ( ( tAtom.AtomSize <= 0 ) || ( tAtom.AtomSize + CurrPos > StopPos ) ) 
				{
					return 0;	// AtomSize error!	
				}

				CurrPos += tAtom.AtomSize;

				if ( TrackType == VIDEO_TRACK )	// get video FPS
				{
#if 1				// Raymond 2007/09/13
					if ( !strncmp(tAtom.AtomType, "mdhd", 4) ) 
					{
						// Get TimeScale
						unsigned int TimeScale = 0, dur = 0;
						if (use_fread == 0) {
							lseek64(fd, 12, SEEK_CUR);
							TimeScale = read_dword_a(fd);
							dur = read_dword_a(fd);
						} else {
							fseek(fp, 12, SEEK_CUR);
							if (read_nbytes(&TimeScale, 1, 4, fp) != 4)
								return 0;
							TimeScale = BElong_32(TimeScale);
							read_nbytes(&dur, 1, 4, fp);
							dur = BElong_32(dur);
							if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
								break;	// EOF
						}
						finfo->FPS = (float)dur / TimeScale;
					}
					
					if( !strncmp(tAtom.AtomType, "stsz", 4) )	// "stsz" atom
					{
						// get Duration from "stsz" atom
						unsigned int num = 0;
						if (use_fread == 0) {
							lseek64(fd, 8, SEEK_CUR);
							num = read_dword_a(fd);
						} else {
							fseek(fp, 8, SEEK_CUR);
							if (read_nbytes(&num, 1, 4, fp) != 4)
								return 0;
							num = BElong_32(num);
							if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
								break;	// EOF
						}

						finfo->FPS = (float)num / finfo->FPS;
					}
#else					
					if ( !strncmp(tAtom.AtomType, "mdhd", 4) ) 
					{
						// Get TimeScale
						unsigned int TimeScale = 0;
						if (use_fread == 0) {
							lseek64(fd, 12, SEEK_CUR);
							TimeScale = read_dword_a(fd);
						} else {
							fseek(fp, 12, SEEK_CUR);
							read_nbytes(&TimeScale, 1, 4, fp);
							TimeScale = BElong_32(TimeScale);
						}
						finfo->FPS = (float)TimeScale;
					}
					
					if( !strncmp(tAtom.AtomType, "stts", 4) )	// "stts" atom
					{
						// get Duration from "stts" atom
						unsigned int len = 0, num = 0, dur = 0;
						if (use_fread == 0) {
							lseek64(fd, 4, SEEK_CUR);
							len = read_dword_a(fd);
						} else {
							fseek(fp, 4, SEEK_CUR);
							read_nbytes(&len, 1, 4, fp);
							len = BElong_32(len);
						}
						
						if( len >= 1 )
						{
							if (use_fread == 0) {
								num = read_dword_a(fd);
								dur = read_dword_a(fd);
							} else {
								read_nbytes(&num, 1, 4, fp);
								read_nbytes(&dur, 1, 4, fp);
								dur = BElong_32(dur);	
								num = BElong_32(num);	
							}
							
							finfo->FPS = finfo->FPS / (float)dur;
						}									
					}
#endif					
				}
				// Raymond 2008/11/13
				else if ( TrackType == AUDIO_TRACK )	// get audio
				{
					if ( !strncmp(tAtom.AtomType, "mdhd", 4) ) 
					{
						// Get TimeScale
						unsigned int TimeScale = 0, dur = 0;
						if (use_fread == 0) {
							lseek64(fd, 12, SEEK_CUR);
							TimeScale = read_dword_a(fd);
							dur = read_dword_a(fd);
						} else {
							fseek(fp, 12, SEEK_CUR);
							if (read_nbytes(&TimeScale, 1, 4, fp) != 4)
								return 0;
							TimeScale = BElong_32(TimeScale);
							if (read_nbytes(&dur, 1, 4, fp)  != 4)
								return 0;
							dur = BElong_32(dur);
							if( feof(fp) || (ftell(fp) > finfo->FileSize) || file_error == 1 ) 
								break;	// EOF
						}
						finfo->AudioDuration = dur / TimeScale;
					}
				}
				// end Raymond 2008/11/13

				if( !strncmp(tAtom.AtomType, "stsd", 4) )	// "stsd" atom
				{
					// get (mp4a, mp4v, s263, samr) from "stsd" atom
					ParseSTSD(TrackType, fp, finfo);
				}

				if( Layer < 4 )
				{
					if ( !strncmp(tAtom.AtomType, p3GPAtomType[Layer], 4) ) 
					{	
						Layer++;
						if( Layer < 4 )
						break;				
					}
				}
			}
		
			if( Layer == 4 )
				break;

			StopPos = CurrPos;

			CurrPos = StopPos - tAtom.AtomSize + 8;	// + next layer offset
		}		
	}
//	catch (...) 
//	{
//		return false;
//	}
	
	return 1;
}

int MOV_Parser(FILE	*fp, FileInfo *finfo)
{
//	try
	{	
		int nRet = 0;
		LOFF_T CurrPos = 0;
		LOFF_T StopPos = 0;
		int Layer = 0;			
		unsigned char * id_str;
		int fd = 0;
		int use_fread = 1;
		ATOM_3GP	tAtom;
		
#if defined(HAVE_ANDROID_OS)
		if( finfo->FileSize > (LOFF_T)INT_MAX)
		{
			use_fread = 0;
			fd = fileno(fp);
			lseek64(fd, CurrPos, SEEK_SET);
		}
#endif
		StopPos = finfo->FileSize;

		// moov -> trak
		while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
		{		
			while ( ( CurrPos < StopPos ) && ( CurrPos >= 0 ) )
			{
				if (use_fread == 0) {
					lseek64(fd, CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword_a(fd);
					read(fd, (void *)(&tAtom.AtomType), 4);

					if (tAtom.AtomSize == 1)
					{
						tAtom.AtomSize = read_qword_a(fd);
					}
				} else {
					fseeko(fp, (off_t)CurrPos, SEEK_SET);
					tAtom.AtomSize = (LOFF_T)read_dword(fp);
					if (read_nbytes((void *)(&tAtom.AtomType), 1, 4, fp) != 4)
						return 0;

					if (tAtom.AtomSize == 1)
					{
						tAtom.AtomSize = read_qword(fp);
					}
				}


				id_str = (unsigned char *)&tAtom.AtomType;
				//printf("type: %c%c%c%c, size: %"PRIu64"\n", id_str[0], id_str[1], id_str[2], id_str[3], tAtom.AtomSize);
				if ( ( tAtom.AtomSize < 8 ) || ( tAtom.AtomSize + CurrPos > StopPos ) ) 
				{
					return 0;	// AtomSize error!	
				}

				if ( !strncmp(tAtom.AtomType, "mvhd", 4) ) 
				{									
					// Get TimeScale and Duration
					unsigned int TimeScale = 0, Duration = 0;
					if (use_fread == 0) {
						lseek64(fd, 12, SEEK_CUR);
						TimeScale = read_dword_a(fd);
						Duration = read_dword_a(fd);
					} else {
						fseeko(fp, (off_t)12, SEEK_CUR);

						if (read_nbytes(&TimeScale, 1, 4, fp) != 4)
							return 0;
						TimeScale = BElong_32(TimeScale);
						if (read_nbytes(&Duration, 1, 4, fp) != 4)
							return 0;
						Duration = BElong_32(Duration);
					}
					finfo->FileDuration = (int)(Duration / TimeScale);
				}

				if ( !strncmp(tAtom.AtomType, "trak", 4) ) 
				{
					// Get Track infomation
					int TrackType = GetTrackType(CurrPos, tAtom.AtomSize, fp, finfo);

					if (use_fread == 0) {
						fseeko(fp, (CurrPos + 8), SEEK_SET);
					} else {
						lseek64(fd, (CurrPos + 8), SEEK_SET);
					}

					int bRet = GetTrackInfo(TrackType, CurrPos, tAtom.AtomSize, fp, finfo);
					if( bRet == 0)
						return 0;
					nRet = 1;
				}

				if ( !strncmp(tAtom.AtomType, "cmov", 4) ) 
				{
					return 0;	// Compressed header is not supported!
				}

				CurrPos += tAtom.AtomSize;
				
				if ( !strncmp(tAtom.AtomType, "moov", 4) ) 
				{									
					break;				
				}
			}
			
			Layer++;	
			if( Layer == 2 )
				break;
			
			StopPos = CurrPos;
			
			CurrPos = StopPos - tAtom.AtomSize + 8;	// + next layer offset
		}		

		return nRet;
	}
//	catch (...) 
//	{
//		return 0;
//	}	
}

// MP3 Parser
#define MP3_PARSE_PART
int MP3_Parser(FILE	*fp, FileInfo *finfo)
{		
	uint8_t hdr[4];
	int mp3_freq = 0, mp3_chans = 0, mp3_bitrate = 0, mp3_flen = 0;

	unsigned long pos = finfo->nID3Length;

	unsigned int nFrame = 0, nVBRFrame = 0;
	int isVBR = 0;	
	unsigned int FileSize = (unsigned int)(finfo->FileSize - 128);
	unsigned int parse_size = FileSize;

#ifdef MP3_PARSE_PART
#define MP3_PARSE_SIZE	500000
#if 1	//Barry 2011-08-26 Get the real mp3 duration
	if (FileSize > (10<<20))	//10MB
		parse_size = pos + MP3_PARSE_SIZE;
#else
	if( FileSize > pos + MP3_PARSE_SIZE )
		parse_size = pos + MP3_PARSE_SIZE;
#endif
#endif

	while( pos < parse_size - 4 )
	{
		fseek(fp, pos, SEEK_SET);
		if (read_nbytes(hdr, 1, 4, fp) != 4)
			return 0;
		
		if( ( mp3_flen = mp_get_mp3_header(hdr, 4, &mp3_chans, &mp3_freq, &mp3_bitrate ) ) > 0 )
		{
			finfo->wf.nSamplesPerSec	= mp3_freq;
			finfo->wf.nChannels			= mp3_chans;
			//finfo->aBitrate				= mp3_bitrate; 

			if( finfo->aBitrate	!= (unsigned int)mp3_bitrate)
			{
				nVBRFrame++;
				isVBR = 1;
			}
			else if ( isVBR == 0 && nFrame > 30 )
				break;

			nFrame++;

			pos += mp3_flen;			
		}
		else
			pos++;
	}

	finfo->bAudio = 1;
	finfo->AudioType = mmioFOURCC('M', 'P', '3', ' ');
	
//	if( isVBR == 1 )
	if( isVBR == 1 && ( nVBRFrame > 0.2 * nFrame ) )
	{
		if( finfo->wf.nSamplesPerSec != 0 )
		{
#if 1	// Raymond 2007/09/19
			float audio_dur = ((float)(1152 * nFrame) / finfo->wf.nSamplesPerSec);

#ifdef MP3_PARSE_PART
			//if( FileSize > MP3_PARSE_SIZE )
			if( FileSize != parse_size )
				audio_dur = (int)( (audio_dur * FileSize ) / MP3_PARSE_SIZE );			
#endif
			finfo->AudioDuration = (int)audio_dur;

#else 	// end Raymond 2007/09/19			
			finfo->AudioDuration = (int)((1152 * nFrame) / finfo->wf.nSamplesPerSec);

#ifdef MP3_PARSE_PART
			if( FileSize > MP3_PARSE_SIZE )
				finfo->AudioDuration = (int)( (finfo->AudioDuration * FileSize ) / MP3_PARSE_SIZE );
#endif
#endif			
			if( finfo->AudioDuration != 0 )
				finfo->aBitrate = (uint32_t) ( (finfo->FileSize - finfo->nID3Length) * 8 / (finfo->AudioDuration) );
		}
	}
	else if( finfo->aBitrate != 0 )
		finfo->AudioDuration = (int)((finfo->FileSize - finfo->nID3Length) * 8 / (finfo->aBitrate));

	finfo->FileDuration = finfo->AudioDuration;

	return 1;
}

// WAV Parser

#define formtypeWAVE             mmioFOURCC('W', 'A', 'V', 'E')

int wav_check_file(FILE	*fp)
{
	int id;	
	
	if (read_nbytes(&id, 1, 4, fp) != 4)
		return 0;
	if(id == mmioFOURCC('R','I','F','F'))
	{
		int riff_type;	
		unsigned int size2;
		
		if (read_nbytes(&size2, 1, 4, fp) != 4)
			return 0;
		if (read_nbytes((char *)&riff_type, 1, 4, fp) != 4)
			return 0;

		if( riff_type == formtypeWAVE )	// "AVI "
			return 1;
	}
		
	return 0;
}

int WAV_Parser(FILE	*fp, FileInfo *finfo)
{
	unsigned int len = 0;
	WAVEFORMATEX *wf = &(finfo->wf);

	fseek(fp, 16, SEEK_CUR);
	if (read_nbytes(&len, 1, 4, fp) != 4)
		return 0;
		
	if (read_nbytes((char*)wf,1, len, fp) != len)
		return 0;
						
	finfo->bAudio = 1;

	finfo->aBitrate = wf->nAvgBytesPerSec * 8;

	if( wf->nAvgBytesPerSec != 0 )
		finfo->AudioDuration = (int)(finfo->FileSize / wf->nAvgBytesPerSec);

	finfo->FileDuration = finfo->AudioDuration;

	return 1;
}

// FLV - Raymond 2007/11/05
#define AMF_END_OF_OBJECT         0x09
enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};

typedef enum {
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_OBJECT_END  = 0x09,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;

int flv_video_pts = 0;
unsigned int flv_video_size = 0;

int flv_check_file(FILE	*fp)
{
	char dbuf[4];	
	
	if (read_nbytes(dbuf, 1, 3, fp) != 3)
		return 0;

	if (dbuf[0] == 'F' && dbuf[1] == 'L' && dbuf[2] == 'V') 
	{
        return 1;
    }
		
	return 0;
}

inline static unsigned int read_be24(FILE *fp)
{
    unsigned int val;
    val = read_char(fp) << 16;
    val |= read_char(fp) << 8;
    val |= read_char(fp);
    return val;
}

static int amf_get_string(FILE *fp, char *buffer, int buffsize) {
    int length = read_word(fp);
    if(length >= buffsize) {
        return -1;
    }
    if (read_nbytes(buffer, 1,length, fp) != length)
	return -1;
    buffer[length] = '\0';

    return length;
}

static int amf_parse_object(FILE *fp, const char *key, LOFF_T max_pos, FileInfo *finfo, int depth) {
    AMFDataType amf_type;
    char str_val[256];
    double num_val;

    num_val = 0;
    amf_type = (AMFDataType)read_char(fp);

    switch(amf_type) {
        case AMF_DATA_TYPE_NUMBER:
            num_val = av_int2dbl(read_qword(fp)); break;
        case AMF_DATA_TYPE_BOOL:
            num_val = read_char(fp); break;
        case AMF_DATA_TYPE_STRING:
            if(amf_get_string(fp, str_val, sizeof(str_val)) < 0)
                return -1;
            break;
        case AMF_DATA_TYPE_OBJECT: {
            unsigned int keylen;

            while(ftello(fp) < max_pos - 2 && (keylen = read_word(fp))) {
				fseeko(fp, keylen, SEEK_CUR); //skip key string
                if(amf_parse_object(fp, NULL, max_pos, finfo, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
            if(read_char(fp) != AMF_END_OF_OBJECT)
                return -1;
        }
            break;
        case AMF_DATA_TYPE_NULL:
        case AMF_DATA_TYPE_UNDEFINED:
        case AMF_DATA_TYPE_UNSUPPORTED:
            break; //these take up no additional space
        case AMF_DATA_TYPE_MIXEDARRAY:
            fseeko(fp, 4, SEEK_CUR); //skip 32-bit max array index
            while(ftello(fp) < max_pos - 2 && amf_get_string(fp, str_val, sizeof(str_val)) > 0) {
                //this is the only case in which we would want a nested parse to not skip over the object
                if(amf_parse_object(fp, str_val, max_pos, finfo, depth + 1) < 0)
                    return -1;
            }
            if(read_char(fp) != AMF_END_OF_OBJECT)
                return -1;
            break;
        case AMF_DATA_TYPE_ARRAY: {
            unsigned int arraylen, i;

            arraylen = read_dword(fp);
            for(i = 0; i < arraylen && ftello(fp) < max_pos - 1; i++) {
                if(amf_parse_object(fp, NULL, max_pos, finfo, depth + 1) < 0)
                    return -1; //if we couldn't skip, bomb out.
            }
        }
            break;
        case AMF_DATA_TYPE_DATE:
            fseeko(fp, 8 + 2, SEEK_CUR); //timestamp (double) and UTC offset (int16)
            break;
        default: //unsupported type, we couldn't skip
            return -1;
    }

    if(depth == 1 && key) { //only look for metadata values when we are not nested and key != NULL
        if(amf_type == AMF_DATA_TYPE_NUMBER) {
			//printf("key: %s : %u\n", key, (uint32_t)num_val);
			if (strcmp(key, "width") == 0) {
				finfo->bih.biWidth = (uint32_t)num_val;
			} else if (strcmp(key, "height") == 0) {
				finfo->bih.biHeight = (uint32_t)num_val;
			} else if (strcmp(key, "framerate") == 0) {
				finfo->FPS = (int)num_val;
			} else if (strcmp(key, "videodatarate") == 0) {
				finfo->vBitrate = (float)(num_val * 1000);
			} else if (strcmp(key, "audiodatarate") == 0) {
				finfo->aBitrate = (uint32_t)(num_val * 1000);
			} else if (strcmp(key, "duration") == 0) {
				finfo->FileDuration = (int)num_val;
			} 
		}
    }

    return 0;
}

static void flv_read_metabody(FILE *fp, LOFF_T next_pos, FileInfo *finfo)
{
	int pos = 0;
	AMFDataType type = (AMFDataType)read_char(fp);
    char buffer[11];

	if ((type != AMF_DATA_TYPE_STRING) || amf_get_string(fp, buffer, sizeof(buffer)) < 0 || strcmp(buffer, "onMetaData"))
		return;

	amf_parse_object(fp, buffer, next_pos, finfo, 0);
}

static int flv_read_packet(FILE	*fp, FileInfo *finfo)
{
    int type, size, pts, flags, is_audio = 0;
    int h263_hdr = 0, w = 0, h = 0, read_tmp = 0, format = 0;
	int ii;
	int hw_notsupport = 0;

	for(;;)
	{
		fseek(fp, 4, SEEK_CUR); // size of previous packet 
		type = read_char(fp);
		size = read_be24(fp);
		pts = read_be24(fp);

		if( feof(fp) || file_error == 1 ) 
			return 1;

		fseek(fp, 4, SEEK_CUR); /* reserved */
		flags = 0;
		
		if( size == 0 )
			continue;
		
		if (type == FLV_TAG_TYPE_AUDIO) 
		{
			//printf("tag audio pos: %llu, size: %d!!!\n", ftello(fp), size);
			is_audio = 1;
			flags = read_char(fp);
			size--;
		}
		else if (type == FLV_TAG_TYPE_VIDEO) 
		{
			//printf("tag video pos: %llu, size: %d!!!\n", ftello(fp), size);
			is_audio = 0;
			flags = read_char(fp);
			size--;
			finfo->vFrame++;
			flv_video_size += size;
		}
		else if (type == FLV_TAG_TYPE_META) 
		{
			LOFF_T pos;
			//printf("tag metadata pos: %llu, size: %d!!!\n", ftello(fp), size);
			is_audio = -1;
			pos = ftello(fp) + size;
			flv_read_metabody(fp, pos, finfo);
			fseek(fp, pos, SEEK_SET);
		}
		else 
		{
			// skip packet 
			fseek(fp, size, SEEK_CUR);
			continue;
		}

		break;
	}
	
    if( is_audio == 1)
	{
        if(finfo->wf.nSamplesPerSec == 0)
		{
            finfo->bAudio = 1;

            finfo->wf.nChannels = ( flags & 1 ) + 1;

            if((flags >> 4) == 5)
                finfo->wf.nSamplesPerSec = 8000;
            else
                finfo->wf.nSamplesPerSec = (44100<<((flags>>2)&3))>>3;

            switch(flags >> 4)	// 0: uncompressed 1: ADPCM 2: mp3 5: Nellymoser 8kHz mono 6: Nellymoser
			{				
			case 0: 
				finfo->wf.wFormatTag = 0x1;	// PCM
				break;
			case 1: 
				finfo->wf.wFormatTag = 0x5346;	// ADPCM SWF - 'SF', pseudo id
				break;
            case 2: 
				finfo->wf.wFormatTag = 0x55;	// MP3
				break;
				// this is not listed at FLV but at SWF, strange...
			case 3: 
				finfo->wf.wFormatTag = 0x1;	// PCM
				break;
			case 10:
				finfo->AudioType = AUDIO_AAC;
				break;
            default:	// 5: Nellymoser 8kHz mono 6: Nellymoser
				finfo->wf.wFormatTag = (flags >> 4);
				break;
            }

			finfo->wf.wBitsPerSample = (flags & 2) ? 16 : 8;
			if (finfo->AudioType == UNKNOWN)
				finfo->AudioType = finfo->wf.wFormatTag;
		}
		
		fseek(fp, size, SEEK_CUR);	// skip packet data
    }
	else if (is_audio == 0)
	{
		finfo->bVideo = 1;
		//flv_video_pts = pts;
		
		switch( flags & 0xF )
		{
		case 2: 
			// H263VIDEOPACKET
			finfo->bih.biCompression = mmioFOURCC('F','L','V','1'); 
			flv_video_pts = pts;

			// Get WxH
			if( finfo->bih.biWidth == 0 )
			{
				h263_hdr = read_be24(fp);
				size -= 3;
				
				if ( h263_hdr == 0x84 )
				{
					read_tmp = read_word(fp);
					size -= 2;
					format = (read_tmp>>7) & 0x7;
					
					switch (format) 
					{
					case 0:
						read_tmp = (read_tmp<<8) | read_char(fp);
						w = (read_tmp >> 7) & 0xFF;
//						read_tmp = (read_tmp & 0x7F) | read_char(fp);
						read_tmp = ( (read_tmp & 0x7F) << 8 ) | read_char(fp);	// Raymond 2007/11/15
						h = (read_tmp >> 7) & 0xFF;
						size -= 2;
						break;
					case 1:
						read_tmp = (read_tmp<<16) | read_word(fp);
						w = (read_tmp >> 7) & 0xFFFF;
//						read_tmp = (read_tmp & 0x7F) | read_word(fp);
						read_tmp = ( (read_tmp & 0x7F) << 16 ) | read_word(fp);	// Raymond 2007/11/15
						h = (read_tmp >> 7) & 0xFFFF;
						size -= 4;
						break;
					case 2:
						w = 352;
						h = 288;
						break;
					case 3:
						w = 176;
						h = 144;
						break;
					case 4:
						w = 128;
						h = 96;
						break;
					case 5:
						w = 320;
						h = 240;
						break;
					case 6:
						w = 160;
						h = 120;
						break;
					default:
						w = h = 0;
						break;
					}
					finfo->bih.biWidth = w;
					finfo->bih.biHeight = h;
				}
			}
			break;

		case 3: 
			// SCREENVIDEOPACKET
			finfo->bih.biCompression = mmioFOURCC('F','S','V','1');		// Flash/Screen Video	
			return 0;
			//break;

		case 4:		// without alpha
			// VP6FLVVIDEOPACKET
			finfo->bih.biCompression = mmioFOURCC('v','p','6','f');
			break;

		case 5:		// with alpha
			// VP6FLVALPHAVIDEOPACKET
			finfo->bih.biCompression = mmioFOURCC('v','p','6','a');
			break;

		case 6:
			// SCREENV2VIDEOPACKET
			return 0;

		case 7:
			// AVCVIDEOPACKET
			{
				unsigned int avc_packet_type = read_char(fp);
				int compression_time = ((read_char(fp) << 16)) | (read_char(fp) << 8) | read_char(fp);
				size -= 4;
				if (avc_packet_type == 0)
				{
					// AVC sequence header
					unsigned int conf_ver = read_char(fp);
					unsigned int pro_dic  = read_char(fp);
					unsigned int pro_com  = read_char(fp);
					unsigned int level_dic  = read_char(fp);
					unsigned int leng_size  = (read_char(fp) & 0x3);
					unsigned int seq_set = (read_char(fp) & 0x1f);
					size -= 6;
					if (conf_ver == 1)
					{
						unsigned int seq_len;
						unsigned int pic_set;
						unsigned int pic_len;
						for (ii = 0; ii < (int)seq_set; ii++)
						{
							seq_len = read_word(fp);
							unsigned char *seq_buf = (unsigned char *)malloc(seq_len);
							if (read_nbytes(seq_buf, 1, seq_len, fp) != seq_len)
								return 0;
							size -= (2 + seq_len);
							seq_len = mp_unescape03(seq_buf, seq_len);
							//printf("seq %d: len: %u\n", ii, seq_len);
							if ((seq_buf[0] & 0x1f) == 7)
							{
								TSSTREAMINFO video_info;
								if (h264_parse_sps(&video_info, &seq_buf[1]) == 0)
								{
									hw_notsupport = 1;
									//printf("hw not support!!!\n");
								} else {
									finfo->bih.biCompression = mmioFOURCC('a','v','c','1');
									finfo->bih.biWidth = video_info.width;
									finfo->bih.biHeight = video_info.height;
									finfo->vBitrate = video_info.bitrate;
									finfo->FPS = video_info.fps;
									//printf("w: %d, h: %d\n", video_info.width, video_info.height);
								}
							}
							free(seq_buf);
						}
						pic_set = read_char(fp);
						size --;
						for (ii = 0; ii < (int)pic_set; ii++)
						{
							pic_len = read_word(fp);
							fseek(fp, pic_len, SEEK_CUR);
							size -= (2 + pic_len);
							//printf("pic %d: len: %u\n", ii, pic_len);
						}
					}
					if (hw_notsupport == 1)
						return 0;
					break;
				}
				else if (avc_packet_type == 1)
				{
					// AVC NALU
				}
				else if (avc_packet_type == 2)
				{
					// AVC end of sequence
				}
				return 0;
			}

		default:
			return 0;
		}

		fseek(fp, size, SEEK_CUR);	// skip packet data
    }

	return 1;
}

int FLV_Parser(FILE	*fp, FileInfo *finfo)
{
	unsigned int offset, size;
	int flags, ret;
	int is_video = 0, is_audio = 0;
	
    fseek(fp, 4, SEEK_SET);		// skip "FLV" prefix and version

    flags = read_char(fp);
    offset = read_dword(fp);

	if(flags & 0x1)	is_video = 1;
	if(flags & 0x4)	is_audio = 1;

	// Reset
	flv_video_pts = 0;
	flv_video_size = 0;

	while(!feof(fp))
	{
		if( file_error == 1 ) 
			break;
		
		ret = flv_read_packet(fp, finfo);
		if( ret == 0 )	
			break;		// if video not support
		if( (is_video == finfo->bVideo) && (is_audio == finfo->bAudio) )
			break;
	}
	
	// Get info
	if (finfo->FileDuration == 0)
	{
		if( flv_video_pts != 0 )
		{
			finfo->FPS = (float)( finfo->vFrame * 1000 ) / (float)flv_video_pts;
			finfo->FileDuration = flv_video_pts / 1000;
			finfo->vBitrate = 0.008f * ( (float)flv_video_size / (float)finfo->vFrame ) * finfo->FPS;
		}
		else
		{
			// Get duration
			fseek(fp, (long)(finfo->FileSize - 4), SEEK_SET);	// last packet size
			size = read_dword(fp);
			fseek(fp, (long)(finfo->FileSize - 3 - size), SEEK_SET);
			if( size == read_be24(fp) + 11)
				finfo->FileDuration = read_be24(fp) / 1000;
		}
	}

	return 1;
}

// SWF	- Raymond 2008/11/27

#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

#define SWF_VIDEO_CODEC_FLV1	0x02
#define SWF_VIDEO_CODEC_VP6f	0x04

#define TAG_DEFINESOUND   14
#define TAG_STREAMHEAD    18
#define TAG_JPEG2         21
#define TAG_STREAMHEAD2   45
#define TAG_VIDEOSTREAM	  60
#define TAG_FILEATTR	  69

/*
static const AVCodecTag swf_audio_codec_tags[] = {
    {CODEC_ID_PCM_S16LE,  0x00},
    {CODEC_ID_ADPCM_SWF,  0x01},
    {CODEC_ID_MP3,        0x02},
    {CODEC_ID_PCM_S16LE,  0x03},
  //{CODEC_ID_NELLYMOSER, 0x06},
    {0, 0},
};
*/

inline static unsigned int read_le16(FILE	*fp)
{
    unsigned int val;
    val = read_char(fp);
    val |= read_char(fp) << 8;
    return val;
}

inline static unsigned int read_be32(FILE	*fp)
{
    unsigned int val;
    val = read_char(fp) << 24;
    val |= read_char(fp) << 16;
    val |= read_char(fp) << 8;
    val |= read_char(fp);
    return val;
}

static int get_swf_tag(FILE	*fp, int *len_ptr)
{
    int tag, len;
    
    if (feof(fp))
        return -1;

    tag = read_le16(fp);
    len = tag & 0x3f;
    tag = tag >> 6;
    if (len == 0x3f) 
	{
        len = read_dword_le(fp);
    }
    *len_ptr = len;
    return tag;
}

static int swf_check_file(FILE	*fp)
{
    /* check file header */
	int tag = read_be32(fp) & 0xffffff00;

	if (tag == MKBETAG('F', 'W', 'S', 0))
	{
		return 1;
	}
	else if (tag == MKBETAG('C', 'W', 'S', 0)) 
	{
        //av_log(s, AV_LOG_ERROR, "Compressed SWF format not supported\n");
        return 0;
    }
	else
		return 0;
}

int SWF_Parser(FILE	*fp, FileInfo *finfo)
{
	int nbits = 0, len = 0, frame_rate = 0, tag = 0, v = 0;
	int ms_per_frame = 0;
	int codec_id = 0, sample_rate_code = 0, tag_num = 0;
	
    tag = read_be32(fp) & 0xffffff00;
    
	if (tag != MKBETAG('F', 'W', 'S', 0))
        return 0;

    read_be32(fp);

    /* skip rectangle size */
    nbits = read_char(fp) >> 3;
    len = (4 * nbits - 3 + 7) / 8;

	fseek(fp, len, SEEK_CUR);

    frame_rate = read_le16(fp);
    finfo->FPS = (float)(frame_rate >> 8);

    read_le16(fp); /* frame count */
    
    /// The Flash Player converts 8.8 frame rates to milliseconds internally. Do the same to get a correct framerate 
	ms_per_frame = (int)( ( 1000 * 256 ) / frame_rate );

    while( !feof(fp) && tag_num < 100 ) 
	{
		if ( finfo->bVideo == 1 && finfo->bAudio == 1 ) 
			break;

        tag = get_swf_tag(fp, &len);
        if (tag < 0) 
			break;
		
		tag_num++;

		// Video
        if ( tag == TAG_VIDEOSTREAM && finfo->bVideo == 0 ) 
		{	
			finfo->bVideo = 1;

            read_le16(fp);
            read_le16(fp);

            finfo->bih.biWidth = read_le16(fp);
            finfo->bih.biHeight = read_le16(fp);

            read_char(fp);

			codec_id = read_char(fp);

			len -= 10;

            if ( codec_id == SWF_VIDEO_CODEC_FLV1 ) 
			{
				finfo->bih.biCompression = mmioFOURCC('F','L','V','1'); 
            }
			else if ( codec_id == SWF_VIDEO_CODEC_VP6f )
			{
				finfo->bih.biCompression = mmioFOURCC('V','P','6','F');
			}
        } 
		else if ( tag == TAG_JPEG2 && finfo->bVideo == 0 ) 
		{
			finfo->bVideo = 1;
			finfo->bih.biCompression = mmioFOURCC('M','J','P','G');          
        }
		// Audio
		else if ( tag == TAG_STREAMHEAD || tag == TAG_STREAMHEAD2 || tag == TAG_DEFINESOUND )	
		{
			if ( tag == TAG_DEFINESOUND )	// TAG_DEFINESOUND = 14
			{
				read_le16(fp);	// ID
				len -= 2;
			}
			else	// TAG_STREAMHEAD = 18 , TAG_STREAMHEAD2 = 45
			{
				read_char(fp);	// playback property
				len -= 1;
			}

			v = read_char(fp);
			len -= 1;

			if( finfo->bAudio == 0 )
			{
				// codec id
				codec_id = (v>>4) & 15;
				if ( codec_id == 0x0 ) 
					finfo->wf.wFormatTag = 0x1;		// PCM
				else if ( codec_id == 0x1 )
					finfo->wf.wFormatTag = 0x5346;	// ADPCM - 'SF', pseudo id
				else if ( codec_id == 0x2 )
					finfo->wf.wFormatTag = 0x55;	// MP3
				else if ( codec_id == 0x3 )
					finfo->wf.wFormatTag = 0x1;		// PCM

				// sample rate
				sample_rate_code= (v>>2) & 3;

				if( sample_rate_code == 0 )
					finfo->wf.nSamplesPerSec = 5500;
				else	
					finfo->wf.nSamplesPerSec = 11025 << (sample_rate_code-1);

				finfo->wf.nChannels = 1 + (v&1);
				finfo->bAudio = 1;
			}
		}
		else if ( tag == 12 )	// DoAction
		{
			unsigned short len1;
			int code, pos = 0;
			unsigned char *tbuf = (unsigned char *)malloc(len);

			if (read_nbytes(tbuf, 1, len, fp) != len)
				return 0;
			len = 0;

			while( pos < len)
			{
				code = tbuf[pos];
				pos++;
				if( code > 0x80 )
				{
					len1 = tbuf[pos] + (tbuf[pos+1]<< 8);
					pos += 2;

					if(len > 0)
					{
						pos += len1;
					}
				}
			}

			free(tbuf);
		}
		else if ( tag == 2 )	// DefineShape
		{
			int id, nbits, pos = 0;
			int Xmin, Ymin, Xmax, Ymax;
			BitData bf;
			unsigned char *tbuf = (unsigned char *)malloc(len);

			if (read_nbytes(tbuf, 1, len, fp) != len)
				return 0;
			len = 0;

			id = tbuf[pos] + (tbuf[pos+1]<< 8);

			InitGetBits(&bf, tbuf + 2, len - 2);

			nbits = GetBits(&bf, 5);
			Xmin = (signed)GetSignedBits(&bf, nbits);
			Xmax = (signed)GetSignedBits(&bf, nbits);
			Ymin = (signed)GetSignedBits(&bf, nbits);			
			Ymax = (signed)GetSignedBits(&bf, nbits);

			free(tbuf);
		}
		else if ( tag == 32 )	// DefineShape3
		{
			int id, nbits, pos = 0;
			int Xmin, Ymin, Xmax, Ymax;
			BitData bf;
			unsigned char *tbuf = (unsigned char *)malloc(len);

			if (read_nbytes(tbuf, 1, len, fp) != len)
				return 0;
			len = 0;

			id = tbuf[pos] + (tbuf[pos+1]<< 8);

			InitGetBits(&bf, tbuf + 2, len - 2);

			nbits = GetBits(&bf, 5);
			Xmin = GetBits(&bf, nbits);
			Xmax = GetBits(&bf, nbits);
			Ymin = GetBits(&bf, nbits);			
			Ymax = GetBits(&bf, nbits);

			free(tbuf);
		}
		else if ( tag == 26 )	// PlaceObject2
		{

		}
		// 
/*		else if ( tag == TAG_FILEATTR )
		{
			v = read_char(fp);
			len -= 1;
		}
*/
		if(len > 0)
			fseek(fp, len, SEEK_CUR);
    }

	if ( finfo->bVideo == 1 || finfo->bAudio == 1 ) 
		return 1;
	else
		return 0;
}

// RM

#define MKTAG(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

#define MAX_STREAMS 32

/* originally from FFmpeg */
static void get_str(int isbyte, FILE	*fp, char *buf, int buf_size)
{
    int len;
    
    if (isbyte)
		len = read_char(fp);
    else
		len = read_word(fp);

    read_nbytes(buf, 1, (len > buf_size) ? buf_size : len, fp);
    if (len > buf_size)
		fseek(fp, len-buf_size, SEEK_CUR);
//		stream_skip(demuxer->stream, len-buf_size);
}

static void skip_str(int isbyte, FILE	*fp)
{
    int len;

    if (isbyte)
		len = read_char(fp);
    else
		len = read_word(fp);

	fseek(fp, len, SEEK_CUR);
//    stream_skip(demuxer->stream, len);    
}

int rm_check_file(FILE	*fp)
{
    int c = read_dword_le(fp);
    if (c == -256)
		return 0; /* EOF */

    if (c == MKTAG('.', 'R', 'M', 'F'))
	    return 1; 

    return 0;	/* bad magic */
}

int ra_check_file(FILE	*fp)
{
    int c = read_dword_le(fp);
    if (c == -256)
		return 0; /* EOF */

    if (c == MKTAG('.', 'r', 'a', 0xfd))
	    return 1; 

    return 0;	/* bad magic */
}

static int RAv3_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	int audio_bytes, bytes_per_minute, sample_rate, bits_per_sample, channels;
	int temp;
	WAVEFORMATEX *wf = &finfo->wf;

	sample_rate = 8000;
	bits_per_sample = 16;

	read_skip(fp, 2); // header_size
	channels = read_word(fp);
	read_skip(fp, 6); // unknow
	bytes_per_minute = read_word(fp);
	audio_bytes = read_dword(fp);
	// title
	if ((temp = read_char(fp)) > 0)
		read_skip(fp, temp);
	// author
	if ((temp = read_char(fp)) > 0)
		read_skip(fp, temp);
	// copyright
	if ((temp = read_char(fp)) > 0)
		read_skip(fp, temp);
	// comment
	if ((temp = read_char(fp)) > 0)
		read_skip(fp, temp);
	read_skip(fp, 1);
	if ((temp = read_char(fp)) == 4)
	{
		finfo->AudioType = read_dword(fp);
	} else {
		return nRet;
	}
	
	if (bytes_per_minute != 0) {
		finfo->AudioDuration = 60 * audio_bytes / bytes_per_minute;
		finfo->aBitrate = bytes_per_minute * 8 / 60 / 1000; // kbps
	}

	wf->nChannels = channels;
	wf->wBitsPerSample = bits_per_sample;
	wf->nSamplesPerSec = bits_per_sample/ 8;

	nRet = 1;
	return nRet;
}

static int RAv4_Parser(FILE *fp, int nVersion, FileInfo *finfo)
{
	int nRet = 0;
	int data_size, audio_bytes, bytes_per_minute, frame_size, sample_rate, bits_per_sample, channels;
	int temp;
	WAVEFORMATEX *wf = &finfo->wf;

	read_skip(fp, 6);
	data_size = read_dword(fp);
	read_skip(fp, 12);
	audio_bytes = read_dword(fp);
	bytes_per_minute = read_dword(fp);
	read_skip(fp, 6);
	frame_size = read_word(fp);
	read_skip(fp, 4);
	if (nVersion == 4) {
		sample_rate = read_word(fp);
		read_skip(fp, 2);
		bits_per_sample = read_word(fp);
		channels = read_word(fp);
	} else {
		sample_rate = read_dword(fp);
		read_skip(fp, 4);
		bits_per_sample = read_dword(fp);
		channels = read_word(fp);
	}
	// Interleaver ID
	if ((temp = read_char(fp)) > 0)
		read_skip(fp, temp);
	if ((temp = read_char(fp)) == 4)
	{
		finfo->AudioType = read_dword(fp);
	} else {
		return nRet;
	}
	
	if (bytes_per_minute != 0) {
		finfo->AudioDuration = 60 * audio_bytes / bytes_per_minute;
		finfo->aBitrate = bytes_per_minute * 8 / 60 / 1000; // kbps
	}
	wf->nChannels = channels;
	wf->wBitsPerSample = bits_per_sample;
	wf->nSamplesPerSec = bits_per_sample/ 8;
	wf->nBlockAlign = frame_size;

/*
	printf("data_size: %d, audio_bytes: %d, bytes_per_minute: %d, frame size: %d, " \
			"sample rate: %d, sample size: %d, channels: %d\n", 
			data_size, audio_bytes, bytes_per_minute, frame_size, sample_rate,
			bits_per_sample, channels );
*/

	nRet = 1;
	return nRet;
}

int RA_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet;
	int nVersion;

	// check header
	nRet = ra_check_file(fp);
	if( nRet == 0 ) {
		return nRet;
	}
	// check version(3 or 4)
	nVersion = read_word(fp);
	if (nVersion == 3)
		nRet = RAv3_Parser(fp, finfo);
	else if ((nVersion == 4) || (nVersion == 5))
		nRet = RAv4_Parser(fp, nVersion, finfo);
	else 
		nRet = 0;

	if (nRet != 0)
	{
		finfo->bAudio = 1;
	}
	return nRet;
}

#if 0
#define h264_PROFILE_SIMPLE		0x50
#define h264_PROFILE_MAIN		0x40
#define h264_PROFILE_SNR		0x30
#define h264_PROFILE_SPATIAL	0x20
#define h264_PROFILE_HIGH		0x10
#define h264_LEVEL_HIGH			0x4
#define h264_LEVEL_HIGH1440		0x6
#define h264_LEVEL_MAIN			0x8
#define h264_LEVEL_LOW			0xa
int set_finfo_profile_level(uint8_t prolevel, FileInfo *finfo)
{
	int nRet = 0;
	int width = 0, height = 0, fps = 0;
	if (h264_LEVEL_HIGH & prolevel)
	{
		if ((h264_PROFILE_MAIN & prolevel) ||
				(h264_PROFILE_HIGH & prolevel))
		{
			width = 1920;
			height = 1152;
			fps = 60;
		}
	}
	else if (h264_LEVEL_HIGH1440 & prolevel)
	{
		if ((h264_PROFILE_MAIN & prolevel) ||
				(h264_PROFILE_SPATIAL & prolevel) ||
				(h264_PROFILE_HIGH & prolevel))
		{
			width = 1440;
			height = 1152;
			fps = 60;
		}
	}
	else if (h264_LEVEL_MAIN & prolevel)
	{
		if ((h264_PROFILE_MAIN & prolevel) ||
				(h264_PROFILE_SNR & prolevel) ||
				(h264_PROFILE_SPATIAL & prolevel) ||
				(h264_PROFILE_HIGH & prolevel))
		{
			width = 720;
			height = 576;
			fps = 30;
		}
	}
	else if (h264_LEVEL_LOW & prolevel)
	{
		if ((h264_PROFILE_MAIN & prolevel) ||
				(h264_PROFILE_SNR & prolevel))
		{
			width = 352;
			height = 288;
			fps = 30;
		}
	}

	if (width != 0){
		finfo->bVideo = 1;
		finfo->bih.biWidth = width;
		finfo->bih.biHeight = height;
		finfo->FPS = fps;
		nRet = 1;
	}
	return nRet;
}
#endif

int TS_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	
	nRet = ts_detect_streams(fp, finfo);
	return nRet;
}

int RM_Parser(FILE	*fp, FileInfo *finfo)
{
	int nRet = rm_check_file(fp);
	if( nRet == 0 ) {
		return nRet;
	}
//
	int num_of_headers;
    int a_streams=0;
    int v_streams=0;
    int i, tmp;
    int header_size;
	int is_multirate = 0;
	int samplerate;
	int samplesize;
	int channels;
	unsigned int index_chunk_offset, data_chunk_offset;

    header_size = read_dword(fp);	/* header size */
    
    i = read_word(fp);				/* Header object version */
    
	// File version
    if ( header_size == 0x10 )
    	i = read_word(fp);
    else // we should test header_size here too. 
    	i = read_dword(fp);

    num_of_headers = read_dword(fp);	/* number of headers */

    /* parse chunks */
    for ( i = 1 ; i <= num_of_headers ; i++ )
    {
		int chunk_id, chunk_pos, chunk_size;
		
		chunk_pos = ftell(fp);
		chunk_id = read_dword_le(fp);
		chunk_size = read_dword(fp);
		
		fseek(fp, 2, SEEK_CUR);			/* version */
		
		if (chunk_size < 10)
		{
			break; // invalid chunksize
		}

		if( file_error == 1 )	
			return 0;
		
		switch(chunk_id)
		{
		case MKTAG('P', 'R', 'O', 'P'):		/* Properties header */
			
			tmp = read_dword(fp); /* max bitrate */
			tmp = read_dword(fp); /* avg bitrate */
			tmp = read_dword(fp); /* max packet size */
			tmp = read_dword(fp); /* avg packet size */
			tmp = read_dword(fp); /* nb packets */
			
			finfo->FileDuration = read_dword(fp) / 1000; /* duration */
			
			tmp = read_dword(fp);	 /* preroll */

			index_chunk_offset	= read_dword(fp);
			data_chunk_offset	= read_dword(fp) + 10;

			fseek(fp, 2, SEEK_CUR);	 /* nb streams */

			fseek(fp, 2, SEEK_CUR);	 /* flags */

			break;
		case MKTAG('C', 'O', 'N', 'T'):		/* Content description header */
			{				
				char *buf;
				int len;
				
				len = read_word(fp);
				if (len > 0)
				{
					buf = (char *)malloc(len+1);
					if (read_nbytes(buf, 1, len, fp) != len)
					{
						free(buf);
						return 0;
					}
					buf[len] = 0;
//					demux_info_add(demuxer, "name", buf);
					free(buf);
				}
				
				len = read_word(fp);
				if (len > 0)
				{
					buf = (char *)malloc(len+1);
					if (read_nbytes(buf, 1, len, fp) != len)
					{
						free(buf);
						return 0;
					}
					buf[len] = 0;
//					demux_info_add(demuxer, "author", buf);
					free(buf);
				}
				
				len = read_word(fp);
				if (len > 0)
				{
					buf = (char *)malloc(len+1);
					if (read_nbytes(buf, 1, len, fp) != len)
					{
						free(buf);
						return 0;
					}
					buf[len] = 0;
//					demux_info_add(demuxer, "copyright", buf);
					free(buf);
				}
				
				len = read_word(fp);
				if (len > 0)
				{
					buf = (char *)malloc(len+1);
					if (read_nbytes(buf, 1, len, fp) != len)
					{
						free(buf);
						return 0;
					}
					buf[len] = 0;
//					demux_info_add(demuxer, "comment", buf);
					free(buf);
				}
				break;
			}
		case MKTAG('M', 'D', 'P', 'R'):		/* Media properties header */
			{
				
				int stream_id;
				int bitrate;
				int codec_data_size;
				int codec_pos;
				int tmp;
				int len;
				char *descr, *mimet = NULL;
				
				stream_id = read_word(fp);
				
				fseek(fp, 4, SEEK_CUR); /* max bitrate */

				bitrate = read_dword(fp); /* avg bitrate */

				tmp = read_dword(fp); /* max packet size */
				tmp = read_dword(fp); /* avg packet size */
				tmp = read_dword(fp); /* start time */
				tmp = read_dword(fp); /* preroll */
				tmp = read_dword(fp); /* duration */
				
				//		skip_str(1, demuxer);	/* stream description (name) */
				if ((len = read_char(fp)) > 0) 
				{
					descr = (char *)malloc(len+1);
					if (read_nbytes(descr, 1, len, fp) != len)
					{
						free(descr);
						return 0;
					}
					descr[len] = 0;
//					printf("Stream description: %s\n", descr);
					free(descr);
				}
				//		skip_str(1, demuxer);	/* mimetype */
				if ((len = read_char(fp)) > 0) 
				{
					mimet = (char *)malloc(len+1);
					if (read_nbytes(mimet, 1, len, fp) != len)
					{
						free(mimet);
						return 0;
					}
					mimet[len] = 0;
//					printf("Stream mimetype: %s\n", mimet);
				}
				
				/* Type specific header */
				codec_data_size = read_dword(fp);
				codec_pos = ftell(fp);
								
				if (!strncmp(mimet,"audio/",6)) 
				{
					finfo->bAudio = 1;
					finfo->aBitrate = bitrate;

					if (strstr(mimet,"x-pn-realaudio") || strstr(mimet,"x-pn-multirate-realaudio")) 
					{
						// skip unknown shit - FIXME: find a better/cleaner way!
						len = codec_data_size;
						tmp = read_dword(fp);
						
						while(--len>=8)
						{
							if( tmp == MKTAG(0xfd, 'a', 'r', '.') ) 
								break; // audio
							tmp = (tmp<<8)|read_char(fp);
						}
						if (tmp != MKTAG(0xfd, 'a', 'r', '.'))
						{
//							mp_msg(MSGT_DEMUX,MSGL_V,"Audio: can't find .ra in codec data\n");
						}
						else 
						{
							/* audio header */
							char buf[128]; /* for codec name */
							int frame_size;
							int sub_packet_size;
							int sub_packet_h;
							int version;
							int flavor;
							int coded_frame_size;
							int codecdata_length;
							int i;
							char *buft;
							int hdr_size;
							
							version = read_word(fp);

							if (version == 3) 
							{
//								stream_skip(demuxer->stream, 2);
//								stream_skip(demuxer->stream, 10);
//								stream_skip(demuxer->stream, 4);

								fseek(fp, 16, SEEK_CUR);

								// Name, author, (c) are also in CONT tag
								if ((i = read_char(fp)) != 0) 
								{
									buft = (char *)malloc(i+1);
//									stream_read(demuxer->stream, buft, i);
									buft[i] = 0;
//									demux_info_add(demuxer, "Name", buft);
									free(buft);
								}
								if ((i = read_char(fp)) != 0) 
								{
									buft = (char *)malloc(i+1);
//									stream_read(demuxer->stream, buft, i);
									buft[i] = 0;
//									demux_info_add(demuxer, "Author", buft);
									free(buft);
								}
								if ((i = read_char(fp)) != 0) 
								{
									buft = (char *)malloc(i+1);
//									stream_read(demuxer->stream, buft, i);
									buft[i] = 0;
//									demux_info_add(demuxer, "Copyright", buft);
									free(buft);
								}

								i = read_char(fp);

//								if ( i != 0 )
//									mp_msg(MSGT_DEMUX,MSGL_WARN,"Last header byte is not zero!\n");
								
								read_char(fp);
								i = read_char(fp);
								finfo->AudioType = read_dword_le(fp);
								
								if (i != 4) 
								{
//									mp_msg(MSGT_DEMUX,MSGL_WARN,"Audio FourCC size is not 4 (%d), please report to ""MPlayer developers\n", i);
									fseek(fp, i - 4, SEEK_CUR);
								}
								if (finfo->AudioType != mmioFOURCC('l','p','c','J')) 
								{
//									mp_msg(MSGT_DEMUX,MSGL_WARN,"Version 3 audio with FourCC %8x, please report to "MPlayer developers\n", sh->format);
								}
								channels = 1;
								samplesize = 16;
								samplerate = 8000;
								frame_size = 240;
								strcpy(buf, "14_4");
							}
							else 
							{
								fseek(fp, 12, SEEK_CUR);

//								stream_skip(demuxer->stream, 2); // 00 00
//								stream_skip(demuxer->stream, 4); /* .ra4 or .ra5 */
//								stream_skip(demuxer->stream, 4); // ???
//								stream_skip(demuxer->stream, 2); /* version (4 or 5) */
								//		    stream_skip(demuxer->stream, 4); // header size == 0x4E
								hdr_size = read_dword(fp); // header size
//								mp_msg(MSGT_DEMUX,MSGL_V,"header size: %d\n", hdr_size);
								flavor = read_word(fp);/* codec flavor id */
								coded_frame_size = read_dword(fp);/* needed by codec */
								//stream_skip(demuxer->stream, 4); /* coded frame size */
								fseek(fp, 12, SEEK_CUR);
								//stream_skip(demuxer->stream, 4); // big number
								//stream_skip(demuxer->stream, 4); // bigger number
								//stream_skip(demuxer->stream, 4); // 2 || -''-
								//		    stream_skip(demuxer->stream, 2); // 0x10
								sub_packet_h = read_word(fp);
								
								frame_size = read_word(fp);

								sub_packet_size = read_word(fp);

								fseek(fp, 2, SEEK_CUR);
								
								if (version == 5)
									fseek(fp, 6, SEEK_CUR);
//									stream_skip(demuxer->stream, 6); //0,srate,0
								
								samplerate = read_word(fp);
								fseek(fp, 2, SEEK_CUR);
								samplesize = read_word(fp)/8;
								channels = read_word(fp);
//								mp_msg(MSGT_DEMUX,MSGL_V,"samplerate: %d, channels: %d\n",
//									sh->samplerate, sh->channels);
								
								if (version == 5)
								{
									fseek(fp, 4, SEEK_CUR); // "genr"
									if (read_nbytes(buf, 1, 4, fp) != 4)
										return 0;
									//stream_read(demuxer->stream, buf, 4); // fourcc
									buf[4] = 0;
								}
								else
								{		
									/* Desc #1 */
									skip_str(1, fp);
									/* Desc #2 */
									get_str(1, fp, buf, sizeof(buf));
								}
							}
							
							finfo->wf.nChannels = channels;
							finfo->wf.wBitsPerSample = samplesize * 8;
							finfo->wf.nSamplesPerSec = samplerate;
							finfo->wf.nAvgBytesPerSec = bitrate;
							finfo->wf.nBlockAlign = frame_size;
//							finfo->wf.cbSize = 0;
							finfo->AudioType = MKTAG(buf[0], buf[1], buf[2], buf[3]);
							
							switch (finfo->AudioType)
							{
							case MKTAG('d', 'n', 'e', 't'):	// Audio: DNET -> AC3
								break;

							case MKTAG('1', '4', '_', '4'):
								break;
								
							case MKTAG('2', '8', '_', '8'):
								break;
								
							case MKTAG('s', 'i', 'p', 'r'):	// Audio: SiproLab's ACELP.net
#if 0
								sh->format = 0x130;
								/* for buggy directshow loader */
								sh->wf->cbSize = 4;
								sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
								sh->wf->wBitsPerSample = 0;
								sh->wf->nAvgBytesPerSec = 1055;
								sh->wf->nBlockAlign = 19;
								//			    sh->wf->nBlockAlign = frame_size / 288;
								buf[0] = 30;
								buf[1] = 1;
								buf[2] = 1;
								buf[3] = 0;
								memcpy((sh->wf+18), (char *)&buf[0], 4);
								break;
#endif
							case MKTAG('a', 't', 'r', 'c'):	// Audio: Sony ATRAC3 (RealAudio 8) (unsupported)
#if 0
								sh->format = 0x270;
								/* 14 bytes extra header needed ! */
								sh->wf->cbSize = 14;
								sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
								sh->wf->nAvgBytesPerSec = 16537; // 8268
								sh->wf->nBlockAlign = 384; // 192
								sh->wf->wBitsPerSample = 0; /* from AVI created by VirtualDub */
								break;
#endif
							case MKTAG('c', 'o', 'o', 'k'):		// Audio: Real's GeneralCooker (?) (RealAudio G2?) (unsupported)
								// realaudio codec plugins - common:
								//			    sh->wf->cbSize = 4+2+24;
								fseek(fp, 3, SEEK_CUR);  // Skip 3 unknown bytes 
								if (version==5)
									fseek(fp, 1, SEEK_CUR);  // Skip 1 additional unknown byte 
								codecdata_length=read_dword(fp);
								fseek(fp, codecdata_length, SEEK_CUR);	// codecdata
								break;
							case MKTAG('r', 'a', 'a', 'c'):
							case MKTAG('r', 'a', 'c', 'p'):
								/* This is just AAC. The two or five bytes of config data needed for libfaad are stored after the audio headers. */
								fseek(fp, 3, SEEK_CUR);  // Skip 3 unknown bytes 
								if (version==5)
									fseek(fp, 1, SEEK_CUR);  // Skip 1 additional unknown byte 
								codecdata_length=read_dword(fp);
								if (codecdata_length>=1) 
								{
									codecdata_length = codecdata_length - 1;
//									sh->codecdata = calloc(sh->codecdata_len, 1);
									fseek(fp, 1, SEEK_CUR);
									fseek(fp, codecdata_length, SEEK_CUR);	// codecdata
									//stream_read(demuxer->stream, sh->codecdata, sh->codecdata_len);
								}
//								finfo->AudioType = mmioFOURCC('M', 'P', '4', 'A');
								break;
							default:
								break;
//								mp_msg(MSGT_DEMUX,MSGL_V,"Audio: Unknown (%s)\n", buf);
							}
															
							++a_streams;							
						}
					} 
					else if (strstr(mimet,"X-MP3-draft-00")) 
					{
						finfo->wf.nChannels = 0;//sh->channels;
						finfo->wf.wBitsPerSample = 16;
						finfo->wf.nSamplesPerSec = 0;//sh->samplerate;
						finfo->wf.nAvgBytesPerSec = 0;//bitrate;
						finfo->wf.nBlockAlign = 0;//frame_size;
//						finfo->wf.cbSize = 0;
//						finfo->wf.wFormatTag = 
						finfo->AudioType = mmioFOURCC('a','d','u',0x55);
						
						++a_streams;
					}
					else if (strstr(mimet,"x-ralf-mpeg4")) 
					{
						finfo->AudioType = mmioFOURCC('r','a','l','f');
//						mp_msg(MSGT_DEMUX,MSGL_ERR,"Real lossless audio not supported yet\n");
					}
					else 
					{
//						mp_msg(MSGT_DEMUX,MSGL_V,"Unknown audio stream format\n");
					}
				}
				else if (!strncmp(mimet,"video/",6)) 
				{
					finfo->bVideo = 1;
					finfo->vBitrate = (float)bitrate;

					if (strstr(mimet,"x-pn-realvideo") || strstr(mimet,"x-pn-multirate-realvideo"))
					{
						tmp = read_dword(fp);
						
						len = codec_data_size;
						while(--len>=8)
						{
							if(tmp==MKTAG('O', 'D', 'I', 'V')) 
								break;  // video
							tmp=(tmp<<8)|read_char(fp);
						}

						if(tmp != MKTAG('O', 'D', 'I', 'V'))
						{
							//mp_msg(MSGT_DEMUX,MSGL_V,"Video: can't find VIDO in codec data\n");
						}
						else 
						{
							/* video header */
							
							finfo->bih.biCompression = read_dword_le(fp); /* fourcc */
							finfo->bih.biSize = 48;
							finfo->bih.biWidth = read_word(fp);
							finfo->bih.biHeight = read_word(fp);
							finfo->bih.biPlanes = 1;
							finfo->bih.biBitCount = 24;
							finfo->bih.biSizeImage= finfo->bih.biWidth * finfo->bih.biHeight * 3;
							
							finfo->FPS = (float) read_word(fp);
							if ( finfo->FPS <= 0 ) 
								finfo->FPS = 24; // we probably won't even care about fps
							
							fseek(fp, 4, SEEK_CUR);	// unknown

							//		    if(sh->format==0x30335652 || sh->format==0x30325652 )
							if(1)
							{
								int tmp = read_word(fp);
								if( tmp > 0 )
								{
									finfo->FPS = (float)tmp; 
								}
							}
							
							fseek(fp, 2, SEEK_CUR);
							
							// read codec sub-format (to make difference between low and high rate codec)
							//((unsigned int*)(sh->bih+1))[0]= read_dword(fp);
							read_dword(fp);
							
							/* h263 hack */
							tmp = read_dword(fp);
							//((unsigned int*)(sh->bih+1))[1]=tmp;

							switch (tmp)
							{
							case 0x10000000:
								/* sub id: 0 */
								/* codec id: rv10 */
								break;
							case 0x10003000:
							case 0x10003001:
								/* sub id: 3 */
								/* codec id: rv10 */
								finfo->bih.biCompression = mmioFOURCC('R', 'V', '1', '3');
								break;
							case 0x20001000:
							case 0x20100001:
							case 0x20200002:
								/* codec id: rv20 */
								break;
							case 0x30202002:
								/* codec id: rv30 */
								break;
							case 0x40000000:
								/* codec id: rv40 */
								break;
							default:
								/* codec id: none */
								break;
//								mp_msg(MSGT_DEMUX,MSGL_V,"unknown id: %x\n", tmp);
							}
							
							if( ( finfo->bih.biCompression <= 0x30335652 ) && (tmp>=0x20200002) )
							{
								// read data for the cmsg24[] (see vd_realvid.c)
								unsigned int cnt = codec_data_size - (ftell(fp) - codec_pos);
								if (cnt < 2) 
								{
//									mp_msg(MSGT_DEMUX, MSGL_ERR,"realvid: cmsg24 data too short (size %u)\n", cnt);
								}
								else  
								{
									unsigned int ii;
									if (cnt > 6) 
									{
//										mp_msg(MSGT_DEMUX, MSGL_WARN,"realvid: cmsg24 data too big, please report (size %u)\n", cnt);
										cnt = 6;
									}
									for (ii = 0; ii < cnt; ii++)
										read_char(fp);
//										((unsigned char*)(sh->bih+1))[8+ii]=(unsigned short)read_char(fp);
								}
							} 
							
							++v_streams;							
						}
					} 
					else 
					{
//						mp_msg(MSGT_DEMUX,MSGL_V,"Unknown video stream format\n");
					}
				}
				else if (strstr(mimet,"logical-")) 
				{
					if (strstr(mimet,"fileinfo")) 
					{
//						mp_msg(MSGT_DEMUX,MSGL_V,"Got a logical-fileinfo chunk\n");
					} 
					else if (strstr(mimet,"-audio") || strstr(mimet,"-video")) 
					{
						int i, stream_cnt;
						int stream_list[MAX_STREAMS];
						
						is_multirate = 1;
						fseek(fp, 4, SEEK_CUR); // Length of codec data (repeated)
						stream_cnt = read_dword(fp); // Get number of audio or video streams
						
						if (stream_cnt >= MAX_STREAMS) 
						{
//							mp_msg(MSGT_DEMUX,MSGL_ERR,"Too many streams in %s. Big troubles ahead.\n", mimet);
							goto skip_this_chunk;
						}
						
						for (i = 0; i < stream_cnt; i++)
							stream_list[i] = read_word(fp);
						
						for (i = 0; i < stream_cnt; i++)
						{
							if (stream_list[i] >= MAX_STREAMS) 
							{
//								mp_msg(MSGT_DEMUX,MSGL_ERR,"Stream id out of range: %d. Ignored.\n", stream_list[i]);
								fseek(fp, 4, SEEK_CUR); // Skip DATA offset for broken stream
							}
							else 
							{
								read_dword(fp);
								//priv->str_data_offset[stream_list[i]] = read_dword(fp);
//								mp_msg(MSGT_DEMUX,MSGL_V,"Stream %d with DATA offset 0x%08x\n", stream_list[i], priv->str_data_offset[stream_list[i]]);
							}							
						}
						// Skip the rest of this chunk
					} 
//					else 
//						mp_msg(MSGT_DEMUX,MSGL_V,"Unknown logical stream\n");
				}
				else 
				{
//					printf("Not audio/video stream or unsupported!\n");
				}
skip_this_chunk:
				/* skip codec info */
				tmp = ftell(fp) - codec_pos;
//				mp_msg(MSGT_DEMUX,MSGL_V,"### skipping %d bytes of codec info\n", codec_data_size - tmp);

				fseek(fp, codec_data_size - tmp, SEEK_CUR);

				if (mimet)
					free (mimet);
				break;
			}
		case MKTAG('D', 'A', 'T', 'A'):
			goto header_end;
		case MKTAG('I', 'N', 'D', 'X'):
		default:	// Unknown chunk
			fseek(fp, chunk_size - 10, SEEK_CUR);
			break;
		}
    }

header_end:
/*
    if(is_multirate) 
	{
        mp_msg(MSGT_DEMUX,MSGL_V,"Selected video id %d audio id %d\n", demuxer->video->id, demuxer->audio->id);
        // Perform some sanity checks to avoid checking streams id all over the code
        if (demuxer->audio->id >= MAX_STREAMS) 
		{
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Invalid audio stream %d. No sound will be played.\n", demuxer->audio->id);
            demuxer->audio->id = -2;
        }
		else if ((demuxer->audio->id >= 0) && (priv->str_data_offset[demuxer->audio->id] == 0)) 
		{
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Audio stream %d not found. No sound will be played.\n", demuxer->audio->id);
            demuxer->audio->id = -2;
        }
        if (demuxer->video->id >= MAX_STREAMS)
		{
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Invalid video stream %d. No video will be played.\n", demuxer->video->id);
            demuxer->video->id = -2;
        }
		else if ((demuxer->video->id >= 0) && (priv->str_data_offset[demuxer->video->id] == 0))
		{
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Video stream %d not found. No video will be played.\n", demuxer->video->id);
            demuxer->video->id = -2;
        }
    }

    if(is_multirate && ((demuxer->video->id >= 0) || (demuxer->audio->id  >=0))) 
	{
        // If audio or video only, seek to right place and behave like standard file 
        if (demuxer->video->id < 0) 
		{
            // Stream is audio only, or -novideo
            stream_seek(demuxer->stream, priv->data_chunk_offset = priv->str_data_offset[demuxer->audio->id]+10);
            is_multirate = 0;
        }
        if (demuxer->audio->id < 0) 
		{
            // Stream is video only, or -nosound
            stream_seek(demuxer->stream, priv->data_chunk_offset = priv->str_data_offset[demuxer->video->id]+10);
            is_multirate = 0;
        }
    }

	if(!is_multirate) 
	{
//    printf("i=%d num_of_headers=%d   \n",i,num_of_headers);
		priv->num_of_packets = read_dword(fp);
//    stream_skip(demuxer->stream, 4); // number of packets 
		stream_skip(demuxer->stream, 4); // next data header 

//		mp_msg(MSGT_DEMUX,MSGL_V,"Packets in file: %d\n", priv->num_of_packets);

		if (priv->num_of_packets == 0)
			priv->num_of_packets = -10;
	}
	else 
	{
        priv->audio_curpos = priv->str_data_offset[demuxer->audio->id] + 18;
        stream_seek(demuxer->stream, priv->str_data_offset[demuxer->audio->id]+10);
        priv->a_num_of_packets=priv->a_num_of_packets = read_dword(fp);
        priv->video_curpos = priv->str_data_offset[demuxer->video->id] + 18;
        stream_seek(demuxer->stream, priv->str_data_offset[demuxer->video->id]+10);
        priv->v_num_of_packets = read_dword(fp);
        priv->stream_switch = 1;
        // Index required for multirate playback, force building if it's not there 
        // but respect user request to force index regeneration 
        if (index_mode == -1)
            index_mode = 1;
    }

	priv->audio_need_keyframe = 0;
	priv->video_after_seek = 0;
  
	switch (index_mode)
	{
	case -1: // untouched
		if (priv->index_chunk_offset && (priv->index_chunk_offset < demuxer->movi_end))
		{
			parse_index_chunk(demuxer);
			demuxer->seekable = 1;
		}
		break;
	case 1: // use (generate index)
		if (priv->index_chunk_offset && (priv->index_chunk_offset < demuxer->movi_end))
		{
			parse_index_chunk(demuxer);
			demuxer->seekable = 1;
		}
		else 
		{
			generate_index(demuxer);
			demuxer->seekable = 1;
		}
		break;
	case 2: // force generating index
		generate_index(demuxer);
		demuxer->seekable = 1;
		break;
	default: // do nothing
		break;
    }
	
    if(demuxer->video->sh)
	{
		sh_video_t *sh=demuxer->video->sh;
		mp_msg(MSGT_DEMUX,MSGL_V,"VIDEO:  %.4s [%08X,%08X]  %dx%d  (aspect %4.2f)  %4.2f fps\n",
			&sh->format,((unsigned int*)(sh->bih+1))[1],((unsigned int*)(sh->bih+1))[0],
			sh->disp_w,sh->disp_h,sh->aspect,sh->fps);
    }
*/	
	return 1;
}

//////////////////////////////////////////////////////////////////////////////

// detection based on the extension

#ifdef WIN32
int strcasecmp(const char* s1, const char* s2) { return stricmp(s1,s2); }
#endif

int file_type_by_filename(const char* filename)
{
	int i;
	char* extension = strrchr((char *)filename,'.');
//	printf("Filename %s ext: %s\n", filename, extension);

	if(extension)
	{
		++extension;

		// Look for the extension in the extensions table
		for( i=0 ; i<(int)(sizeof(extensions_table)/sizeof(extensions_table[0])) ; i++ ) 
		{
//			if( !stricmp(extension, extensions_table[i].extension) ) 
			if( !strcasecmp(extension, extensions_table[i].extension) ) 
			{
//				printf("File type %d based on filename extension\n",extensions_table[i].demuxer_type);
//				return extensions_table[i].demuxer_type;
				return i;
			}
		}
	}
//	return FILE_TYPE_UNKNOWN_FILE;
	return -1;
}

static int file_type_demuxer(FILE *fp, FileInfo *finfo, int filetype)
{
	int nRet = 0;

	fseek(fp, 0, SEEK_SET);

	switch(filetype)
	{
		case FILE_TYPE_AAC :
			nRet = aac_check_file(fp, finfo);
			break;
		case FILE_TYPE_MP3 :
			nRet = mp3_check_file(fp, finfo);
			break;
		case FILE_TYPE_WAV :
			nRet = wav_check_file(fp);
			break;
		case FILE_TYPE_OGG :
			nRet = ogg_check_file(fp, finfo);
			break;		
		case FILE_TYPE_AMR :
			nRet = amr_check_file(fp, finfo);
			break;		
		case FILE_TYPE_RA :
			nRet = ra_check_file(fp);
			break;		
		case FILE_TYPE_FLAC :
			nRet = flac_check_file(fp, finfo);
			break;		
		case FILE_TYPE_AC3 :
			nRet = ac3_check_file(fp, finfo);
			break;		
		case FILE_TYPE_AVI :	
			nRet = avi_check_file(fp);
			break;
		case FILE_TYPE_MPEG_PS :
			nRet = mpg_check_file(fp, finfo);
			break;
		case FILE_TYPE_FLV :
			nRet = flv_check_file(fp);
			break;
		case FILE_TYPE_SWF :
			nRet = swf_check_file(fp);
			break;
		case FILE_TYPE_RM :
			nRet = rm_check_file(fp);
			break;
		case FILE_TYPE_ASF :
			nRet = asf_check_file(fp);
			break;
		case FILE_TYPE_MOV :
			nRet = mov_check_file(fp, finfo);
			break;		
		case FILE_TYPE_TS :
			nRet = ts_check_file(fp, finfo);
			break;		
		case FILE_TYPE_MKV :
			nRet = mkv_check_file(fp, finfo);
			break;
		case FILE_TYPE_ISO :
			nRet = iso_check_file(fp, finfo);
			break;
		case FILE_TYPE_BD :
			nRet = bd_check_file(fp, finfo);
			break;
		case FILE_TYPE_APE :
			nRet = ape_check_file(fp, finfo);
			break;		
		default:
			break;
	}

	// Raymond 2007/12/21
	if( file_error == 1 )	
		return FILE_TYPE_UNKNOWN_FILE;

	if ( nRet == 1 )
		return filetype;

	return FILE_TYPE_UNKNOWN_FILE;
}

static int file_type_by_dir_demuxer(FILE *fp, FileInfo *finfo)
{
	int i;
	int nRet = 0;
	int dir_ary[] = {FILE_TYPE_ISO, FILE_TYPE_BD};
	int dir_num = sizeof(dir_ary)/sizeof(int);
	
	for ( i = 0; i < dir_num; i++ )
	{
		nRet = file_type_demuxer(fp, finfo, dir_ary[i]);
		if ( nRet != 0 )
			return nRet;
	}

	return FILE_TYPE_DIRECTORY;
}

int file_type_by_demuxer(FILE *fp, FileInfo *finfo, int skip_type)
{
	int i;
	int nRet = 0;
	
	for ( i = FILE_TYPE_AVI ; i <= FILE_TYPE_MOV; i++ )
	{
		if (i == skip_type)
			continue;
		if (i == FILE_TYPE_MPEG_PS)
			continue;
		nRet = file_type_demuxer(fp, finfo, i);
		if ( nRet != 0 )
			return nRet;
	}
	nRet = file_type_demuxer(fp, finfo, FILE_TYPE_MPEG_PS);
	if ( nRet != 0 )
		return nRet;

	return FILE_TYPE_UNKNOWN_FILE;
}

int file_type_by_audio_demuxer(FILE *fp, FileInfo *finfo, int skip_type)
{
	int i;
	int nRet = 0;
	
	for ( i = FILE_TYPE_AAC ; i < FILE_TYPE_DIRECTORY; i++ )	// Raymond 2007/11/07
	{
		if (i == skip_type)
			continue;
		nRet = file_type_demuxer(fp, finfo, i);
		if ( nRet != 0 )
			return nRet;
	}

	return FILE_TYPE_UNKNOWN_FILE;
}

static int check_audio_type_support(FileInfo *finfo)
{
	int nRet = 1;
	int ii;
	unsigned int asf_wma_support[] = {0x160, 0x161, 0x162};

	if (( finfo->bAudio == 0) || (finfo->AudioType == 0))
		return nRet;

	if ((finfo->FileType == FILE_TYPE_ASF) && (finfo->bVideo == 0))
	{
		for (ii = 0; ii < (int)(sizeof(asf_wma_support)/sizeof(unsigned int)); ii++)
		{
			if (finfo->AudioType == asf_wma_support[ii])
			{
				return nRet;
			}
		}
		return 0;
	}

	nRet = check_audio_type(finfo->AudioType, finfo->wf.nChannels, finfo->hw_a_flag); 
	return nRet;
}

int MediaParser(const char *in_filename, FileInfo *finfo)
{
	// open file
	FILE	*fp = NULL;
	int index = -1, FileType = 0;
	int DemuxerType = 0;
	int nRet = 0;
	int is_dir = 0;
	struct stat file_stat;

	// Raymond 2008/11/03
	uint32_t nProductType	= finfo->ProductType;
	uint32_t nDispWidth		= finfo->DispWidth;
    uint32_t nDispHeight	= finfo->DispHeight;
		
	// check input file
	if((fp = fopen(in_filename, "rb")) == NULL) 
	{ 
		LOGE("FileParser : Unable to open input file: %s", strerror(errno));
		goto parse_error;
	}
	// check stat 
	if (fstat(fileno(fp), &file_stat) == -1)
	{
		LOGE("FileParser : Unable to get file status: %s", strerror(errno));
		fclose(fp);
		goto parse_error;
	}
	if (S_ISDIR(file_stat.st_mode))
	{
		is_dir = 1;
	}

	memset(finfo, 0, sizeof(FileInfo));	// clear

	// Raymond 2008/11/03
	finfo->ProductType	= nProductType;
	finfo->DispWidth	= nDispWidth;
    finfo->DispHeight	= nDispHeight;
	finfo->filepath = (char *)malloc(1024);
	strncpy(finfo->filepath, in_filename, 1024);
	finfo->filepath[1023] = '\0';
	get_fuse(&finfo->hw_v_flag, &finfo->hw_a_flag);
	
	file_error = 0;	// Raymond 2007/12/21

	// get file length
    finfo->FileSize = file_stat.st_size;

	//printf("File size of %s = %"PRIu64"\n", in_filename, finfo->FileSize);

	// decide file type based on filename extension
	// Raymond 2007/06/14
	if (is_dir == 0)
	{
		if (finfo->FileSize == 0)
		{
			LOGE("%s:%d\n", __func__, __LINE__);
			fclose(fp);
			goto parse_error;
		}

		index = file_type_by_filename(in_filename);
		if( index < 0 )
			FileType = FILE_TYPE_UNKNOWN_FILE;
		else
		{
			FileType = extensions_table[index].demuxer_type;
			finfo->SubFileNameIndex = index;
			if ((FileType == FILE_TYPE_ISO) && (finfo->FileSize > (LOFF_T)7.95*1024*1024*1024))
			{
				FileType = FILE_TYPE_BD;
			}
		}
		// decide file type based on simple parsing first
		DemuxerType = file_type_demuxer(fp, finfo, FileType);
		if ((DemuxerType == FILE_TYPE_UNKNOWN_FILE) && (FileType == FILE_TYPE_RM))
		{ 
			// speed up ra parseing for file ext .ram
			DemuxerType = file_type_demuxer(fp, finfo, FILE_TYPE_RA);
		}
		if (DemuxerType == FILE_TYPE_UNKNOWN_FILE)
		{
			if( FileType >= FILE_TYPE_AAC )	// Raymond 2007/11/07
			{
				DemuxerType = file_type_by_audio_demuxer(fp, finfo, FileType);
				if (DemuxerType == FILE_TYPE_UNKNOWN_FILE)
					DemuxerType = file_type_by_demuxer(fp, finfo, FileType);
			} else {
				DemuxerType = file_type_by_demuxer(fp, finfo, FileType);
				if (DemuxerType == FILE_TYPE_UNKNOWN_FILE)
					DemuxerType = file_type_by_audio_demuxer(fp, finfo, FileType);
			}
		}
	} else {
		FileType = FILE_TYPE_DIRECTORY;
		DemuxerType = file_type_by_dir_demuxer(fp, finfo);
	}

	// Raymond 2007/12/21
	if( file_error == 1 )	
	{
		LOGE("%s:%d\n", __func__, __LINE__);
		fclose(fp);
		goto parse_error;
	}
		
	//printf("DemuxerType: %d\n", DemuxerType);
	fseek(fp, 0, SEEK_SET);
	switch(DemuxerType)
	{
	case FILE_TYPE_AVI :
		nRet = AVI_Parser(fp, finfo);
		break;

	case FILE_TYPE_MPEG_PS :

		nRet = MPG_Parser(fp, finfo);

		if( nRet )
		{
			if( bElementary == 0 )		// Raymond 2007/11/13
			{
				int beg_time = finfo->FileDuration;
				if ((GetMPGDuration(fp, finfo) == 1) && (finfo->FileDuration > beg_time))
				{
					finfo->FileDuration -= beg_time;
				} else {
					if (finfo->vBitrate + finfo->aBitrate > 0)
						finfo->FileDuration = (int)((finfo->FileSize * 8) / (int)(finfo->vBitrate + finfo->aBitrate));
				}
			} else if ((int)finfo->vBitrate > 0) {
				// m2v
				finfo->FileDuration = (int)((finfo->FileSize * 8) / (int)finfo->vBitrate);
			}
#if 1
			//printf("[%s - %d]       pos = %d   ****************\n", __func__, __LINE__, ftell(fp));
			if ((finfo->FileDuration == 0) && ((int)finfo->vBitrate == 0) && (finfo->bVideo == 1) && (finfo->bAudio == 0))
			{
				//mp4v
				if (strcasecmp((char *)&(finfo->bih.biCompression), "MPG4") == 0)
				{
					unsigned char *check_buf = NULL;
					unsigned int hdr = 0, frames = 0, y;
					unsigned char c;
					off_t start_pos=0, end_pos=0;
					start_pos = ftell(fp);
					check_buf = (unsigned char *)malloc(512<<10);

					fread(&check_buf[0], 1, 512<<10, fp);
					for (y=0;y<(512<<10)-4;y++)
					{
						if (check_buf[y]==0 && check_buf[y+1]==0 && check_buf[y+2]==1 && check_buf[y+3]==0xB6)
						{
							frames++;
							end_pos = y;
						}
					}
					if (check_buf)
						free(check_buf);
					finfo->FileDuration = (int)((float)finfo->FileSize/(((float)end_pos-start_pos)/(frames-1)*finfo->FPS));
				}
			}
#endif

		}
			
		break;
	
	case FILE_TYPE_FLV :				// Raymond 2007/11/05
		nRet = FLV_Parser(fp, finfo);
		break;

	case FILE_TYPE_SWF :				// Raymond 2008/11/27
		nRet = SWF_Parser(fp, finfo);
		break;

	case FILE_TYPE_RM :				// Raymond 2007/11/21
		nRet = RM_Parser(fp, finfo);
		break;

	case FILE_TYPE_RA :				// mingyu 2010/7/1
		nRet = RA_Parser(fp, finfo);
		break;
	
	case FILE_TYPE_FLAC :				// mingyu 2010/7/1
		nRet = FLAC_Parser(fp, finfo);
		break;
	
	case FILE_TYPE_AC3 :				// mingyu 2010/7/1
		nRet = AC3_Parser(fp, finfo);
		break;
	
	case FILE_TYPE_ASF :
		nRet = ASF_Parser(fp, finfo);
		break;
	
	case FILE_TYPE_MOV :
		nRet = MOV_Parser(fp, finfo);
		break;

	case FILE_TYPE_AAC :
		nRet = AAC_Parser(fp, finfo);
		break;

	case FILE_TYPE_MP3 :
		nRet = MP3_Parser(fp, finfo);
		break;

	case FILE_TYPE_WAV :
		nRet = WAV_Parser(fp, finfo);
		break;

	case FILE_TYPE_OGG :
		nRet = OGG_Parser(fp, finfo);
		break;

	case FILE_TYPE_AMR :
		nRet = AMR_Parser(fp, finfo);
		break;

	case FILE_TYPE_UNKNOWN_FILE :		
		break;

	case FILE_TYPE_TS :
		nRet = TS_Parser(fp, finfo);
		break;

	case FILE_TYPE_MKV :
		nRet = MKV_Parser(fp, finfo);
		break;

	case FILE_TYPE_ISO :
		nRet = ISO_Parser(fp, finfo);
		break;

	case FILE_TYPE_BD :
		nRet = BD_Parser(fp, finfo, is_dir);
		break;

	case FILE_TYPE_APE :
		nRet = APE_Parser(fp, finfo);
		break;

	default :
		break;
	}

	finfo->FileType = DemuxerType;

//	print_video_header(&(finfo->bih));
//	print_wave_header (&(finfo->wf));
	
	// Raymond 2007/12/21
	if( file_error == 1 )	
	{
		LOGE("%s:%d\n", __func__, __LINE__);
		fclose(fp);
		goto parse_error;
	}
		
	// close file
	fclose(fp);

	// determine support or not
	if( nRet <= 0 )
	{
		LOGE("%s:%d\n", __func__, __LINE__);
		nRet *= -1;
		goto parse_error;
	}
	
	// nRet = 1 
	if ( finfo->bVideo )
	{	
		if( finfo->bih.biCompression )
		{
			int i = 0;
			int n = sizeof(VideoFourCC) / sizeof(VideoFourCC[0]);
			char fcc4[5] = {0};
			char *fcc = (char *)&(finfo->bih.biCompression);			
			memcpy(fcc4, fcc, 4);
			fcc4[4] = '\0';
												
			for( i = 0 ; i < n ; i++ )
			{
				if ( finfo->FileType == VideoFourCC[i].file_type )
				{					
					if( !strcasecmp(fcc4, VideoFourCC[i].fourcc) ) 	
					{
						finfo->VideoFormat = (char *)VideoFourCC[i].video_format;	// Raymond 2007/10/26

						finfo->bVideoSupported = 1;	

						if( finfo->ProductType == SK8860 )
						{
							if ( finfo->bVideo  == 1)
							{
								switch (finfo->hw_v_flag)
								{
									case FUSE_VIDEO_VX:
										break;
									case FUSE_VIDEO_VD:
										if (finfo->FileType == FILE_TYPE_RM)
											finfo->bVideoSupported = 0;
										else if( strcasecmp(fcc4, "RV30") == 0 )
											finfo->bVideoSupported = 0;
										else if( strcasecmp(fcc4, "RV40") == 0 )
											finfo->bVideoSupported = 0;
										break;
									case FUSE_VIDEO_VV:
										if (finfo->FileType == FILE_TYPE_RM)
											finfo->bVideoSupported = 0;
										else if( strcasecmp(fcc4, "RV30") == 0 )
											finfo->bVideoSupported = 0;
										else if( strcasecmp(fcc4, "RV40") == 0 )
											finfo->bVideoSupported = 0;
										else if( strcasecmp(fcc4, "DIV3") == 0 )
											finfo->bVideoSupported = 0;
										break;
									case FUSE_VIDEO_VR:
										if( strcasecmp(fcc4, "DIVX") == 0 ) 	
										{
											//finfo->bVideoSupported = 0;	
										}
										else if( strcasecmp(fcc4, "DX50") == 0 ) 	
										{
											//finfo->bVideoSupported = 0;	
										}
										else if( strcasecmp(fcc4, "DIV3") == 0 ) 	
											finfo->bVideoSupported = 0;	
										break;
									default:
										break;
								}
								// I01_HD_1080p12_2000x1100_01.divx
								if( finfo->bih.biWidth > 1920 || finfo->bih.biHeight > 1088 )	
								{
									LOGE("%s:%d\n", __func__, __LINE__);
									nRet = PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
									goto parse_error;
								}
							}
						}
						else if( finfo->ProductType == SK8850 )
						{
							// Raymond 2007/10/26
							if( !strcasecmp(fcc4, "WMV3") )
							{
								finfo->bVideoSupported = 0;	// not support
								return PARSER_VIDEO_CODEC_NOT_SUPPORTED;
							}
							else if( !strcasecmp(fcc4, "avc1") )
							{
								if( finfo->H264Profile != 66 )
								{
									finfo->bVideoSupported = 0;	// if H.264 profile not baseline
									return PARSER_VIDEO_CODEC_NOT_SUPPORTED;
								}
								else if( finfo->bih.biWidth > 720 || finfo->bih.biHeight > 576 )	// Raymond 2007/09/17
								{
									finfo->bVideoSupported = 0;	// if width x height > 720 x 576
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
							// Raymond 2008/01/24 - limit M-JPEG resolution to 720x576
							else if( !strcasecmp(fcc4, "MJPG") ||
								 !strcasecmp(fcc4, "jpeg") ||
						  		 !strcasecmp(fcc4, "AVRn") ||
								 !strcasecmp(fcc4, "AVDJ") 
								)	
							{
								if( finfo->bih.biWidth > 720 || finfo->bih.biHeight > 576 )	
								{
									finfo->bVideoSupported = 0;	// if width x height > 720 x 576
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
							// Raymond 2008/04/28 - HW scalar limitation
							else
							{
								if( finfo->bih.biWidth > 1440 || finfo->bih.biHeight > 960 )
								{
									finfo->bVideoSupported = 0;     // if width x height > 1440 x 960
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
						}
						else	// SK8855
						{	
							// Raymond 2008/08/21 - memory size limitation
							if( finfo->bih.biWidth > finfo->DispWidth || finfo->bih.biHeight > finfo->DispHeight )
							{
								if(DemuxerType != FILE_TYPE_SWF)	// Raymond 2008/12/02 - for special case - Mechanical clock.swf
								{
									finfo->bVideoSupported = 0;
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
							// Raymond 2007/10/26
							else if( !strcasecmp(fcc4, "WMV3") )
							{
								if( finfo->bih.biWidth * finfo->bih.biHeight > 352 * 288 )      // Raymond 2008/08/19
								{
									finfo->bVideoSupported = 0;	// if width x height > 352 x 288
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
							else if( !strcasecmp(fcc4, "avc1") )
							{
								if( finfo->H264Profile != 66 )
								{
									finfo->bVideoSupported = 0;	// if H.264 profile not baseline
									return PARSER_VIDEO_CODEC_NOT_SUPPORTED;
								}
								else if( finfo->bih.biWidth > 720 || finfo->bih.biHeight > 576 )	// Raymond 2007/09/17
								{
									finfo->bVideoSupported = 0;	// if width x height > 720 x 576
									return PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED;
								}
							}
						}

						break;	
					}
				}
			}
		}

		if( !finfo->bVideoSupported )
		{
			LOGE("%s:%d\n", __func__, __LINE__);
			nRet = PARSER_VIDEO_CODEC_NOT_SUPPORTED;
			goto parse_error;
		}
		else
		{
			if ( finfo->bAudio ) 
			{
				if (check_audio_type_support(finfo) == 1)
				{
					LOGE("%s:%d\n", __func__, __LINE__);
					nRet = PARSER_VIDEO_FILE_SUPPORTED;
					goto parse_error;
				}
				else 
				{
					LOGE("%s:%d\n", __func__, __LINE__);
					nRet = PARSER_AUDIO_CODEC_NOT_SUPPORTED;
					goto parse_error;
				}
			}
			else
			{
				LOGE("%s:%d\n", __func__, __LINE__);
				nRet = PARSER_VIDEO_FILE_SUPPORTED;
				goto parse_error;
			}
		}
	}

	if ( finfo->bAudio )	// Audio only
	{
		if( finfo->AudioDuration == 0 )
			finfo->AudioDuration = finfo->FileDuration;

		if (check_audio_type_support(finfo) == 1)
		{
			nRet = PARSER_AUDIO_FILE_SUPPORTED;
		}
		else 
		{
			nRet = PARSER_AUDIO_CODEC_NOT_SUPPORTED;
		}
	}
	else	// no video and audio
	{
		nRet = PARSER_FILE_NOT_SUPPORTED;
	}

parse_error:
	if (finfo->filepath)
	{
		free(finfo->filepath);
		finfo->filepath = NULL;
	}
	LOGE("%s:%d nRet:%d\n", __func__, __LINE__, nRet);
	return nRet;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Parser::Parser()
{
	memset(&finfo, 0, sizeof(FileInfo));
}

Parser::~Parser()
{
	if ((finfo.FileType == FILE_TYPE_BD) && (finfo.priv != NULL))
	{
		BD_Parser_free(&finfo);
	}
	if (finfo.priv != NULL)
		free(finfo.priv);
}

int Parser::Open( char* strSourceName )
{
	int nRet = 0;
	if (!strSourceName || strSourceName[0] == '\0')
	{
		return nRet;
	}

	nRet = MediaParser(strSourceName, &finfo);
	if (nRet >= PARSER_VIDEO_FILE_SUPPORTED)
		nRet = 1;
	else 
	{
#if 1
		if (nRet == PARSER_VIDEO_CODEC_NOT_SUPPORTED)
		{
			char *vtype = (char *)&finfo.bih.biCompression;
			LOGE("Video codec not support: %c%c%c%c\n", vtype[0], vtype[1], vtype[2], vtype[3]);
		}
		else if (nRet == PARSER_AUDIO_CODEC_NOT_SUPPORTED)
			LOGE("Audio codec not support: %08x\n", finfo.AudioType);
		else if (nRet == PARSER_VIDEO_RESOLUTION_NOT_SUPPORTED)
			LOGE("Video resolution not support: %dx%d\n", finfo.bih.biWidth, finfo.bih.biHeight);
		else if (nRet == PARSER_VIDEO_DIVX_VERSION_NOT_SUPPORTED)
			LOGE("Video divx version not support!\n");
#endif 
		nRet = 0;
	}
	return nRet;
}

int Parser::GetFileType()
{
	return finfo.FileType;
}

BITMAP_INFO_HEADER * Parser::GetVideoInfo()
{
	return &(finfo.bih);
}

// Raymond 2007/10/26
char * Parser::GetVideoFormat()
{
	return finfo.VideoFormat;
}

float Parser::GetVideoFPS()
{
	return finfo.FPS;
}

float Parser::GetVideoBitrate()
{
	return finfo.vBitrate;
}

unsigned int Parser::GetH264Profile()
{
	return finfo.H264Profile;
}

WAVEFORMATEX * Parser::GetAudioInfo()
{
	return &(finfo.wf);
}

unsigned int Parser::GetAudioFCC()
{
	return finfo.AudioType;
}

int Parser::GetDuration()
{
	if(finfo.bVideo == 1)
		return finfo.FileDuration;
	else
		return finfo.AudioDuration;
}

int Parser::GetAudioBitrate()
{
	return finfo.aBitrate;
}

int Parser::HasVideo()
{
	return finfo.bVideo;
}

int Parser::HasAudio()
{
	return finfo.bAudio;
}

int Parser::HasFileTag()
{
	return finfo.bTag;
}

int Parser::HasEncrypted()
{
	return finfo.bEncrypted;
}

ID3_FRAME_INFO * Parser::GetTag()
{
	return (ID3_FRAME_INFO *)&(finfo.ID3Tag);
}

bd_priv_t * Parser::GetBDInfo()
{
	if (finfo.FileType == FILE_TYPE_BD)
		return (bd_priv_t *)finfo.priv;
	else 
		return NULL;
}

