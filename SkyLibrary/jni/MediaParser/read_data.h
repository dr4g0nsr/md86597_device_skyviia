#ifndef READ_DATA_H
#define READ_DATA_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define read_data(fp, str, len) read_nbytes(str, 1, len, fp)
#define read_tell(fp) ftell(fp)
double av_int2dbl(int64_t v);
float av_int2flt(int32_t v);

typedef struct 
{
	unsigned char* pSource;
	int		nSourceCount;
//	int		nMaxNumSourceBytes;

	int		nbits;
	long	cword;
	
} BitData;

extern int file_error;

long GetSignedBits(BitData* bf, int n);
void FlushBits(BitData* bf, int n);
long GetBits(BitData* bf, int n);
void InitGetBits(BitData *bf, unsigned char* pSource,int N);

size_t read_nbytes(void *ptr, size_t rsize, size_t rlen, FILE	*fp);

inline static int read_char_a(int fp)
{
	int c = 0;
	read(fp, &c, 1);
	return c;
}

inline static unsigned int read_word_a(int fp)
{
	int x,y;
	x = read_char_a(fp);
	y = read_char_a(fp);
	return (x<<8)|y;
}

inline static unsigned int read_dword_a(int fp)
{
	unsigned int y;
	y = read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	return y;
}

inline static uint64_t read_qword_a(int fp)
{
	uint64_t y;
	y = read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	y = (y<<8)|read_char_a(fp);
	return y;
}

inline static int read_char(FILE *fp)
{
	int c = 0;
	read_nbytes(&c, 1, 1, fp);
	return c;
}

inline static unsigned int read_word(FILE *fp)
{
	int x,y;
	x = read_char(fp);
	y = read_char(fp);
	return (x<<8)|y;
}

inline static unsigned int read_dword(FILE *fp)
{
	unsigned int y;
	y = read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	return y;
}

inline static unsigned int read_word_le(FILE *fp)
{
	int x,y;
	x = read_char(fp);
	y = read_char(fp);
	return (y<<8)|x;
}

inline static unsigned int read_dword_le(FILE *fp)
{
	unsigned int y;
	y =  read_char(fp);
	y |= read_char(fp)<<8;
	y |= read_char(fp)<<16;
	y |= read_char(fp)<<24;
	return y;
}

inline static uint64_t read_qword(FILE *fp)
{
	uint64_t y;
	y = read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	y = (y<<8)|read_char(fp);
	return y;
}

inline static int read_skip(FILE *fp, long size)
{
	return fseek(fp, size, SEEK_CUR);
}

#endif // READ_DATA_H
