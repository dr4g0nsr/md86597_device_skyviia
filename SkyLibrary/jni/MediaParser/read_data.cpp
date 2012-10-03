#include <math.h>
#include "read_data.h"
#include "common.h"

int file_error;

double av_int2dbl(int64_t v){
	    if(v+v > (int64_t)(0xFFEULL<<52))
			        return 0.0/0.0;
		    return ldexp(((v&((1LL<<52)-1)) + (1LL<<52)) * (v>>63|1), (v>>52&0x7FF)-1075);
}

float av_int2flt(int32_t v){
	    if(v+v > (int64_t)0xFF000000U)
			        return 0.0/0.0;
		    return ldexp(((v&0x7FFFFF) + (1<<23)) * (v>>31|1), (v>>23&0xFF)-150);
}

static int err_cnt = 0;
size_t read_nbytes(void *ptr, size_t rsize, size_t rlen, FILE	*fp)
{	
	size_t nRet = fread(ptr, rsize, rlen, fp);
	//printf("ret: %"PRIu64" %"PRIu64"\n", nRet, rlen);
	if( nRet != rlen && !feof(fp) )	// Raymond 2007/12/24
	{
		err_cnt++;
		if (err_cnt < 10)
			printf("------ file_error = 1    nRet[%d] != rlen [%d]  --------\n", nRet, rlen);
		file_error = 1;
	}
	else
		err_cnt = 0;

	return nRet;
}

// Raymond 2008/12/11
void InitGetBits(BitData *bf, unsigned char* pSource,int N)
{
	bf->pSource=(unsigned char*)pSource;	
//	bf->nMaxNumSourceBytes=N;
	bf->nSourceCount=0;
	bf->cword=0;
	bf->nbits=0;
}

static int GetByte(BitData* bf)
{
	return (int)(bf->pSource[bf->nSourceCount++]);
}

long GetBits(BitData* bf, int n)
{
	while ( bf->nbits < n )
	{
		bf->cword = (bf->cword << 8) | GetByte(bf);
		bf->nbits += 8;
	}

	bf->nbits -= n;

	return (long)((bf->cword >> bf->nbits) & (((int64_t)1 << n) - 1));
}

void FlushBits(BitData* bf, int n)
{
	while ( bf->nbits < n )
	{
		bf->cword = bf->pSource[bf->nSourceCount++];
		bf->nbits += 8;
	}

	bf->nbits -= n;
}

long GetSignedBits(BitData* bf, int n)
{
	while ( bf->nbits < n )
	{
		bf->cword = (bf->cword << 8) | GetByte(bf);
		bf->nbits += 8;
	}

	bf->nbits -= n;

	return (signed long)((bf->cword >> bf->nbits) & (((int64_t)1 << n) - 1));
}

