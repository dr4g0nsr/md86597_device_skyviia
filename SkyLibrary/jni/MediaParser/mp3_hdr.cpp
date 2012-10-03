#include <stdio.h>
#include "mp3_hdr.h"
#include "read_data.h"

static int freqs[9] = 
{ 
	44100, 48000, 32000,	// MPEG 1.0
	22050, 24000, 16000,	// MPEG 2.0
	11025, 12000,  8000		// MPEG 2.5
};
static int mult[3] = { 12, 144, 144 };
static int tabsel_123[2][3][16] = 
{
   { 
		{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
		{0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
		{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} 
   },

   {
		{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} 
   }
};

// return ID3 Length if "ID3"
int	GetID3Tag(FILE	*fp, FileInfo *finfo)
{
	ID3_FRAME_INFO *pTag = &(finfo->ID3Tag);
	int i;
	unsigned char Encoding_type;
	char hdr[10], default_title[LEN_XXL]={0};		//Barry 2009-07-09
	FILE *pic_fp;
	//char *img_filename = NULL;


	// check "ID3" first
	if (read_nbytes(hdr, 1, 10, fp) != 10)
	{
		fclose(fp);
		return 0;
	}

	if( hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3' && (hdr[3] >= 2 ) )	// V2.x
	{
		unsigned int ID3_len = (unsigned int)(( hdr[6] << 21 ) + ( hdr[7] << 14 ) + ( hdr[8] << 7 ) +  hdr[9]);
		finfo->ID3_version = (int)hdr[3];	//[3]: majar version,  [4]: reversion,  [5]: flag
		finfo->nID3Length = ID3_len + 10;

		if ( finfo->bTag == 0 )
		{
			unsigned int pos = 0;
			unsigned int len = 0;
			char type[5];
			char *ptr = NULL;
			char *ID3_buf = NULL;
			unsigned char *uptr = NULL;

			ID3_buf = (char *)malloc(ID3_len);
			if(ID3_buf == NULL)
			{
				//				printf("ID3_buf NULL\n");
				return 0;
			}
			//			printf("Got ID3\n");
			finfo->bTag = 1;
			if (read_nbytes(ID3_buf, 1, ID3_len, fp) != ID3_len)
			{
				fclose(fp);
				return 0;
			}

			ptr = ID3_buf;
			type[4] = '\0';
			Encoding_type = 0xFF;
			pTag->Genre = 0xFF;

			// parse ID3_buf
			while( pos < ID3_len - 4 )
			{
				memcpy(type, ptr, 4);
				if (type[0]==0x0 && type[1]==0x0 && type[2]==0x0 && type[3]==0x0)
				{
					//					printf("*******  Frame type is NULL  *******\n");
					break;
				}

				// parse frame type
				if( strncmp(type, "TALB", 4) != 0 && strncmp(type, "TAL", 3) == 0 )		// TAL : Album/Movie/Show title	V2.2 ?
				{
					uptr = (unsigned char *)(ptr + 3);
					len = (unsigned int)( ( uptr[0] << 16 ) + ( uptr[1] << 8 ) + uptr[2] );

					if (uptr[3] == 1 && uptr[4] == 0xFE && uptr[5] == 0xFF)
						printf("Can't support UTF-16 big-endian\n");
					else
					{
						if (uptr[3] == 0)
							Encoding_type = 0;
						else if (uptr[3] == 1)
							Encoding_type = 16;
						else if (uptr[3] == 3)
							Encoding_type = 8;
					}
					if (Encoding_type == 16)
						memcpy(pTag->Album,	ptr+7+2, len-1-2);
					else
						memcpy(pTag->Album,	ptr+7, len-1);
					pTag->Album_encode_type = Encoding_type;

					ptr += 6 + len;
					pos += 6 + len;
				}
				else if( strncmp(type, "PIC", 3) == 0 )	// Picture	V2.2
				{
					uptr = (unsigned char *)(ptr + 3);
					len = (unsigned int)( ( uptr[0] << 16 ) + ( uptr[1] << 8 ) + uptr[2] );

					//check image format
					memcpy(type, ptr+7, 3);
					type[3] = '\0'; type[4] = '\0';

					if( strncmp(type, "JPG", 3) == 0 )
					{
						/*
						//						printf("Has JPEG image\n");
						pic_fp = fopen(img_filename, "wb");
						if( pic_fp == NULL )
						{
							return 0;
						}
						fwrite( ptr+12, 1, len-6, pic_fp);
						fclose(pic_fp);
						*/
						pTag->has_pic = 1;
					}
					else
						printf("Not support image format: %s\n", type);

					ptr += 6 + len;
					pos += 6 + len;
				}
				else	//V2.3, V2.4
				{
					if (type[0] == 0x54)		//if frame type == "T???"
					{
						uptr = (unsigned char *)(ptr + 4);
						//len = (unsigned int)( (uptr[0] << 24) + (uptr[1] << 16) + (uptr[2] << 8) + uptr[3] - 1 );	//include 1 byte "Encoding type"
						len = (unsigned int)( (uptr[0] << 24) + (uptr[1] << 16) + (uptr[2] << 8) + uptr[3] );
						//						printf("[%s] : Frame length = %d\n", type, len);

						//Barry 2009-07-09		if TXXX frame len == 0
						if (len <= 1)
						{
							//printf("[%s] len = 0\n", type);
							ptr += 11;
							pos += 11;
							continue;
						}
						else
						{
							len -= 1;	//include 1 byte "Encoding type"
						}

						if (uptr[6] == 1 && uptr[7] == 0xFE && uptr[8] == 0xFF)
							printf("Can't support UTF-16 big-endian\n");
						else
						{
							if (uptr[6] == 0)
								Encoding_type = 0;
							else if (uptr[6] == 1)
								Encoding_type = 16;
							else if (uptr[6] == 3)
								Encoding_type = 8;
						}

						if( strncmp(type, "TIT2", 4) == 0 )			/* TIT2 : Title/songname/content description */
						{
							if (Encoding_type == 16)
								memcpy(pTag->Title,	ptr+11+2, len-2);
							else
								memcpy(pTag->Title,	ptr+11, len);
							pTag->Title_encode_type = Encoding_type;
						}
						else if( strncmp(type, "TPE1", 4) == 0 )	/* TPE1 : Lead performer(s)/Soloist(s) */
						{
							if (Encoding_type == 16)
								memcpy(pTag->Artist, ptr+11+2, len-2);
							else
								memcpy(pTag->Artist, ptr+11, len);
							pTag->Artist_encode_type = Encoding_type;
						}
						else if( strncmp(type, "TALB", 4) == 0 )	/* TALB : Album/Movie/Show title */
						{
							if (Encoding_type == 16)
								memcpy(pTag->Album, ptr+11+2, len-2);
							else
								memcpy(pTag->Album, ptr+11, len);
							pTag->Album_encode_type = Encoding_type;
						}
						else if( strncmp(type, "TYER", 4) == 0 )	/* TYER : Year */
						{
							if (Encoding_type == 16)
								memcpy(pTag->Year,	ptr+11+2, len-2);
							else
								memcpy(pTag->Year, ptr+11, len);
							pTag->Year_encode_type = Encoding_type;
						}
						else if( strncmp(type, "TRCK", 4) == 0 )	/* TRCK : Track number/Position in set */
						{
							if (Encoding_type == 16)
							{
								memcpy(pTag->Track, ptr+11+2, len-2);
								pTag->Track[len-2] = '\0';
							}
							else
							{
								memcpy(pTag->Track, ptr+11, len);
								pTag->Track[len] = '\0';
							}
							pTag->Track_encode_type = Encoding_type;
						}
						else if( strncmp(type, "TCON", 4) == 0 )	/* TCON : Content type, Genre */
						{
							if (Encoding_type == 16)
							{
								memcpy(pTag->GenreString, ptr+11+2, len-2);
								pTag->GenreString[len-2] = '\0';
							}
							else
							{
								memcpy(pTag->GenreString, ptr+11, len);
								pTag->GenreString[len] = '\0';
							}
							pTag->GenreString_encode_type = Encoding_type;
							pTag->Genre = 0xAA;
						}
						//						else
						//							printf("Skip frame: %s   len=%d\n\n", type, len);

						ptr += 11 + len;
						pos += 11 + len;
					}
					else	// Not T??? frame
					{
						uptr = (unsigned char *)(ptr + 4);
						len = (unsigned int)( (uptr[0] << 24) + (uptr[1] << 16) + (uptr[2] << 8) + uptr[3] );
						//						printf("[%s] : Frame length = %d\n", type, len);

						if( strncmp(type, "APIC", 4) == 0 )	/* APIC : Attached picture */
						{
							char media_type[11];
							media_type[10] = '\0';
							memcpy(media_type, ptr+11, 10);

							if( strncmp(media_type, "image/png", 9) == 0 )
								printf("Not support image format [image/png] !\n");
							else if( strncmp(media_type, "image/jpg", 9) == 0 || strncmp(media_type, "image/jpeg", 10) == 0 )
							{
								for (i=0;i<256;i++)
								{
									if ( ptr[i+8] == 0xFF && ptr[i+9] == 0xD8 )
										break;
								}

								if (i < 256)	//Barry 2009-08-30
								{
									/*
									pic_fp = fopen(img_filename, "wb");
									if( pic_fp == NULL )
									{
										return 0;
									}

									//Barry 2009-05-21
									if ((pos+len-i+2) > ID3_len)
										fwrite( ptr+4+4+i, 1, ID3_len - (pos+4+4+i+1), pic_fp);
									else
										fwrite( ptr+4+4+i, 1, len-i+2, pic_fp);	//header: 4 bytes, size: 4 bytes, flags: 2 bytes

									fclose(pic_fp);
									*/
									pTag->has_pic = 1;
								}
							}
						}
						//						else
						//							printf("Skip frame: %s   len=%d\n\n", type, len);

						ptr += 10 + len;
						pos += 10 + len;
					}
				}
			}
			if(ID3_buf != NULL)
				free(ID3_buf);

			//Get Genre idx from "TAG", if has "TAG"
			if (pTag->Genre == 0xFF)
			{
				char tag[4], get_genre_idx[2];
				fseek(fp, finfo->FileSize - 128, SEEK_SET);
				if (read_nbytes(tag, 1, 3, fp) != 3)
				{
					fclose(fp);
					return 0;
				}
				tag[3] = '\0';
				get_genre_idx[1] = '\0';

				if(!strcmp(tag,"TAG"))
				{
					fseek(fp, finfo->FileSize - 1, SEEK_SET);
					if (read_nbytes(get_genre_idx, 1, 1, fp) != 1)
					{
						fclose(fp);
						return 0;
					}
					pTag->Genre = (int)(get_genre_idx[0]);
					if (/*pTag->Genre >= 0 && */pTag->Genre < 148)
						strncpy(pTag->GenreString, ID3_v1_genre_description[pTag->Genre], strlen(ID3_v1_genre_description[pTag->Genre]));
				}
			}
		}	// end if ( finfo->bTag == 0 )
	}

	// check "TAG" in final 128 bytes		V1.x
	if ( finfo->FileSize > 128 && finfo->bTag == 0 )
	{
		char tag[4];
		fseek(fp, finfo->FileSize - 128, SEEK_SET);
		if (read_nbytes(tag, 1, 3, fp) != 3)
		{
			fclose(fp);
			return 0;
		}
		tag[3] = '\0';

		if(strcmp(tag,"TAG"))
		{
			//			printf("No TAG\n");
		}
		else
		{
			//			printf("Got TAG\n");
			finfo->bTag = 1;
			finfo->ID3_version = 1;

			char buf[125];
			if (read_nbytes(buf, 1, 125, fp) != 125)
			{
				fclose(fp);
				return 0;
			}

			memcpy(pTag->Title,		buf,		30);
			memcpy(pTag->Artist,	buf + 30,	30);
			memcpy(pTag->Album,		buf + 60,	30);
			memcpy(pTag->Year,		buf + 90,	4);
			memcpy(pTag->Comment,	buf + 94,	30);

			if(buf[122] == 0 && buf[123] != 0)
			{
				pTag->Track[0] = buf[123];
				pTag->Track[1] = '\0';
			}

			pTag->Genre = (unsigned int)(buf[124]);
			if (/*pTag->Genre >= 0 && */pTag->Genre < 148)
				strncpy(pTag->GenreString, ID3_v1_genre_description[pTag->Genre], strlen(ID3_v1_genre_description[pTag->Genre]));

			finfo->nID3Length = 128;
		}
	}

	// get the description of genre index
	if (pTag->GenreString_encode_type == 0)
	{
		unsigned int idx, idx1;
		char Genre_idx_str[4];
		idx = strcspn(pTag->GenreString, "(") + 1;
		idx1 = strcspn(pTag->GenreString, ")");
		//if ( !(idx1 == 0) && !(idx1 == strlen(pTag->GenreString)) )
		if ( !(idx1 == 0) && !(idx1 == strlen(pTag->GenreString)) && (idx1-idx <= 3) )	//Barry 2009-05-25
		{
			strncpy(Genre_idx_str, pTag->GenreString+idx, idx1-idx);
			Genre_idx_str[idx1-idx] = '\0';
			memset(pTag->GenreString, 0, sizeof(pTag->GenreString));
			if (atoi(Genre_idx_str) < ID3_NR_OF_V1_GENRES)    //Barry 2009-09-12
				strncpy(pTag->GenreString, ID3_v1_genre_description[atoi(Genre_idx_str)], strlen(ID3_v1_genre_description[atoi(Genre_idx_str)]));
		}
	}

	//if pTag->Title == NULL, set pTag->Title = filename
	for(i=0;i<LEN_XXL;i++)
	{
		if (pTag->Title[i] != 0x0 && pTag->Title[i] != 0x20)	//Barry 2009-09-02
			break;
	}

	if (i == LEN_XXL)
	{
		strncpy(pTag->Title, default_title, LEN_XXL);
		pTag->Title_encode_type = 8;			// filename use UTF-8
	}

	//20090911 Robert Add GetEXIF check
#if 0
	if (pTag->has_pic == 1)
	{
		ImageInfo_t_jpg *ID3_ImgInfo = (ImageInfo_t_jpg *)malloc(sizeof(ImageInfo_t_jpg));
		if (ID3_ImgInfo != 0)
		{
			memset(ID3_ImgInfo, 0, sizeof(ImageInfo_t_jpg));
			if (GetEXIF((char*)img_filename, (ImageInfo_t_jpg*)ID3_ImgInfo) == -1)
			{
				pTag->has_pic = 0;
				unlink(img_filename);
			}
			free(ID3_ImgInfo);
		}

	}
#endif

	if ( finfo->bTag == 0 )	// No ID3 tag
	{
		finfo->nID3Length = 0;
		finfo->ID3_version = 0;
		return 0;
	}

	return finfo->nID3Length;
}

int mp3_check_file(FILE	*fp, FileInfo *finfo)
{
	uint8_t hdr[4];
	int mp3_freq = 0, mp3_chans = 0, mp3_bitrate = 0, mp3_flen = 0;

	int nID3Length = GetID3Tag(fp, finfo);

	fseek(fp, nID3Length, SEEK_SET);
	if (read_nbytes(hdr, 1, 4, fp) != 4)
		return 0;

	mp3_flen = mp_get_mp3_header(hdr, 4, &mp3_chans, &mp3_freq, &mp3_bitrate );

	if( mp3_flen > 0 )
	{
		finfo->wf.nSamplesPerSec	= mp3_freq;
		finfo->wf.nChannels			= mp3_chans;		
		finfo->aBitrate				= mp3_bitrate; 

		return 1;
	}
//	else	// Raymond 2007/06/13
//	else if( nID3Length > 0 )	// Raymond 2007/08/02	
	else if( nID3Length > 0 || extensions_table[finfo->SubFileNameIndex].demuxer_type == FILE_TYPE_MP3 )	// Raymond 2007/09/20		
	{
		unsigned int pos = 0, parse_size = 3000;	// Raymond 2008/03/20
		uint8_t *buf = (uint8_t *)malloc(parse_size+3);
		int ii, get_tag = 0;
		if( buf == NULL )
			return 0;

#if 1	// Raymond 2008/08/27 - for multiple ID3 tag

		while(1)
		{
			fseek(fp, finfo->nID3Length, SEEK_SET);
			if (read_nbytes(buf, 1, 30, fp) != 30)
				return 0;

			for (ii = 0; ii < 30; ii++)
			{
				if( buf[ii] == 'I' && buf[ii+1] == 'D' && buf[ii+2] == '3' && (buf[ii+3] >= 2 ) )	// check if 2 ID3
				{				
					nID3Length = (int)(( buf[ii+6] << 21 ) + ( buf[ii+7] << 14 ) + ( buf[ii+8] << 7 ) +  buf[ii+9]);
					finfo->nID3Length += nID3Length + 10 + ii;
					get_tag = 1;
					if( (unsigned)finfo->nID3Length >= finfo->FileSize )
						return 0;
					break;
				}
			}
			if (get_tag == 0)
				break;
			else
				get_tag = 0;
		}

		fseek(fp, finfo->nID3Length, SEEK_SET);
		parse_size = read_nbytes(buf, 1, parse_size, fp);
#endif		
		while( pos < parse_size )
		{
			if( buf[pos] == 0xFF )
			{
				mp3_flen = mp_get_mp3_header(&buf[pos], 4, &mp3_chans, &mp3_freq, &mp3_bitrate );
				
				if( mp3_flen > 0 )
				{
					finfo->wf.nSamplesPerSec	= mp3_freq;
					finfo->wf.nChannels			= mp3_chans;		
					finfo->aBitrate				= mp3_bitrate; 
					
					finfo->nID3Length += pos;
					free(buf);
					return 1;
				}		
			}
			
			pos++;
		}

		free(buf);
	}
	
	return 0;
}

// return frame size or -1 (bad frame)
int mp_get_mp3_header(unsigned char* hbuf, int length, int* chans, int* srate, int* bitrate)
{
    int lsf, framesize, padding, bitrate_index, sampling_frequency, layer;
	int ii, divisor;
	unsigned long newhead = 0;
		
	if ((hbuf == NULL) || (chans == NULL) || (srate == NULL) || (bitrate == NULL))
		return -1;

    // head_check:
	for (ii = 0; ii < length-3; ii++)
	{
		newhead = hbuf[ii] << 8 | hbuf[ii+1];
		if( ( newhead & 0xffe0 ) == 0xffe0 )
			break;
	}
	if( ( newhead & 0xffe0 ) != 0xffe0 )
	{
		return -1;
    }
	
    layer = 4 - ( ( hbuf[ii+1] >> 1 ) & 3 );
    if( layer == 4 )
	{ 
//		printf("not layer-1/2/3\n"); 
		return -1;
    }
	
    bitrate_index = hbuf[ii+2] >> 4 ;  // valid: 1..14
    sampling_frequency = ( ( hbuf[ii+2] >> 2 ) & 0x3 );  // valid: 0..2
    if( sampling_frequency == 3 )
	{
//		printf("invalid sampling_frequency\n");
		return -1;
    }
	
    if( hbuf[ii+1] & 0x10 ) 
	{
		// MPEG 1.0 (lsf==0) or MPEG 2.0 (lsf==1)
		lsf = ( hbuf[ii+1] & 0x8 ) ? 0x0 : 0x1;
		sampling_frequency += ( lsf * 3 );
    }
	else 
	{
		// MPEG 2.5
		lsf = 1;
		sampling_frequency += 6;
    }
	
    padding   = ( ( hbuf[ii+2] >> 1 ) & 0x1 );
    *chans    = ( ( ( hbuf[ii+3] >> 6 ) ) == 3 ) ? 1 : 2;

	*srate = freqs[sampling_frequency];
	*bitrate = tabsel_123[lsf][layer-1][bitrate_index] * 1000;
	framesize = (*bitrate) * mult[layer-1];

    if(!framesize)
	{
//		printf("invalid framesize/bitrate_index\n");
		return -1;
    }
	
	divisor = ((layer == 3)? (*srate << lsf): *srate);
    framesize /= divisor;
    if( layer == 1 )
		framesize = ( framesize + padding ) << 2;	// * 4
    else
		framesize += padding;
	 	
	if (ii + framesize < length)
	{
		// check again
		if (hbuf[ii + framesize] == 0)
			ii++;
		if (hbuf[ii + framesize] != 0xff)
			framesize = -1;
	}

    return framesize;
}

