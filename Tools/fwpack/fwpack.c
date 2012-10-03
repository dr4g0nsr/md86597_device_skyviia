/*        This software is confidential and proprietary and may be used       */
/*         only as expressly authorized by a licensing agreement from         */
/*                                                                            */
/*                    (C) COPYRIGHT 2011 Skyviia Corporation.                 */
/*                             ALL RIGHTS RESERVED                            */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define hdr_block_size	512
#define line_size	256

//support yaffs2
//#define block_size	2048
int	block_size;
unsigned int	FrameSet=0;

#define hdr_version	0x0001

typedef	unsigned char		uint8_t;
typedef	unsigned short		uint16_t;
typedef	unsigned int		uint32_t;

typedef unsigned int 		uint32;

#define byteReverse(buf, len)	/* Nothing */

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(uint32 buf[4], uint32 const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

MD5_CTX context;

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void MD5Update(struct MD5Context *ctx, unsigned char const *buf, unsigned len)
{
  unsigned int t;

    //Update bitcount 

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((unsigned int) len << 3)) < t)
	ctx->bits[1]++;		// Carry from low to high 
   ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	// Bytes already in shsInfo->data 

    //Handle any leading odd-sized chunks

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (unsigned int *) ctx->in);
	buf += t;
	len -= t;
    }
    // Process data in 64-byte chunks 

    while (len >= 64) {
	memcpy(ctx->in, buf, 64);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (unsigned int *) ctx->in);
	buf += 64;
	len -= 64;
    }

    // Handle any remaining bytes of data. 

    memcpy(ctx->in, buf, len);
}


/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void MD5Final(unsigned char digest[16], struct MD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    // Compute number of bytes mod 64 
    count = (ctx->bits[0] >> 3) & 0x3F;

    //Set the first char of padding to 0x80.  This is safe since there is
      // always at least one byte free 
    p = ctx->in + count;
    *p++ = 0x80;

    // Bytes of padding needed to make 64 bytes 
    count = 64 - 1 - count;

    // Pad out to 56 mod 64 
    if (count < 8) {
	// Two lots of padding:  Pad the first block to 64 bytes 
	memset(p, 0, count);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (unsigned int *) ctx->in);

	// Now fill the next block with 56 bytes 
	memset(ctx->in, 0, 56);
    } else {
	// Pad block to 56 bytes 
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    //Append length in bits and transform 
    ((unsigned int *) ctx->in)[14] = ctx->bits[0];
    ((unsigned int *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (unsigned int *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));	// In case it's sensitive 
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void MD5Transform(unsigned int buf[4], unsigned int const in[16])
{
    register unsigned int a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

int skip_comment(FILE *fp, char *buf, int size)
{
	char *return_buf;
	
	do {
		return_buf = fgets(buf ,size ,fp);
		
		if (return_buf==NULL)
			return 0;	/*EOF*/
			
		if (buf[0] == '/')
			continue;	/*skip this line*/
		else
			return 1;	/*ok we got it return*/			
	}while (return_buf !=NULL);
	
	return 0;	/*should not go to here.*/
}

int convert(char *ptr)
{
	if ((*ptr >='A') && (*ptr <='F'))
		return (*ptr-'A'+10);
	else
	if ((*ptr >='a') && (*ptr <='f'))
		return (*ptr-'a'+10);
	else
	if ((*ptr >='0') && (*ptr <='9'))
		return (*ptr-'0');
}

int read_hexs(char *dest_buf, char *src_str, int num)
{
	int i, temp, value;

	for (i=0; i<num; i++)
	{		
		while (*src_str != '\n')
		{
			if (*src_str == ' ')	
			{	/*skip space*/
				src_str++;
			}
			else
			{	
				if(!isxdigit(*src_str))	
					return -1;
				else
					value = convert(src_str);				
				src_str++;

				if (*src_str == ' ')
				{
					src_str++;
					*dest_buf++ = (char) value;
					break;
				}
				
				if(!isxdigit(*src_str))
					return -1;
				else
					value = (value <<4) + convert(src_str);

				*dest_buf++ = (char)value;
				
				src_str++;
			
				break;
			}
		}

		if (*src_str == '\n')
			return (i+1);
	}

	return i;
}

int gen_hdr(FILE *out_fp, char *input_file, char *buffer)
{	
	FILE  *fp;
	char  *ptr, str[line_size];
	int   ret=0, find, i;
//	unsigned int	num, ram_number_no, FrameSet;
	unsigned int	num, ram_number_no;
			
	if (out_fp==NULL)
		return -1;		/*no output target*/
						
	if (!buffer)	
		return -1;		/*buffer error*/
	
	memset(buffer, 0xFF, hdr_block_size);
	ptr = buffer;

	fp = fopen(input_file, "r");
	
	if (fp==NULL)
	{
		printf("Can not find input header file\n");
		free(buffer);
		return -1;
	}
	
	//header
	find = skip_comment(fp, str, line_size);
	
	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	
	memcpy(ptr, str, 16);
	ptr += 16;
	
	//version
	find = skip_comment(fp, str, line_size);
	
	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}	
	sscanf(str, " %x %[^\n ]", &num);
	
	if (num >hdr_version)
	{
		printf("header version is newer than this program. \n");
		goto close_file1;
	}
	
	*((unsigned short *) ptr) = (unsigned short) num;
	
	ptr +=2;
	//flash control reg
	find = skip_comment(fp, str, line_size);

	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}

	find = read_hexs( ptr, str, 28);
	if (find <= 0)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	ptr += 28;
	
	//Page Read, Page Write, Erase, CopyBack
	for (i=0; i< 4; i++)
	{	
		find = skip_comment(fp, str, line_size);
		if (!find)
		{
			printf("Invalid file data \n");
			goto close_file1;
		}
	
		find = read_hexs(ptr, str, 12);
		if (!find)
		{
			printf("Invalid file data \n");
			goto close_file1;
		}
		ptr += 12;
	}
	
	/*2010 12 13 Add for Vendor ID, Product ID*/
	ptr = buffer + 144;
	
	//flash control reg
	find = skip_comment(fp, str, line_size);

	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	//Vendor ID, Product ID
	find = read_hexs( ptr, str, 8);
	if (find <= 0)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	
	ptr = buffer + 152;
	//flash control reg
	find = skip_comment(fp, str, line_size);

	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	
	//Firmware Setting
	sscanf(str, " %x %[^\n ]", &FrameSet);
	*((unsigned int *) ptr) =  FrameSet;
	ptr +=4;
			
	/*2010 12 13 Add End*/
	
	
	ptr = buffer + 240;
	
	//read RAM setting number
	find = skip_comment(fp, str, line_size);
	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	sscanf(str, " %x %[^\n ]", &ram_number_no);	
	*((unsigned int *) ptr) =  ram_number_no;
	ptr +=4;
		
	//DDR_REG setting, 3 is CPU PLL, DDRII_PLL, and ResetCTL
	for( i=0; i< (3+ram_number_no); i++)
	{
		find = skip_comment(fp, str, line_size);
		if (!find)
		{
			printf("Invalid file data \n");
			goto close_file1;
		}
		sscanf(str, " %x %[^\n ]", &num);	
		*((unsigned int *) ptr) =  num;
		ptr +=4;
	}
	
	
	//Extend Tag.
	ptr = buffer + 480;
	
	//read RAM setting number
	find = skip_comment(fp, str, line_size);
	if (!find)
	{
		printf("Invalid file data \n");
		goto close_file1;
	}
	sscanf(str, " %x %[^\n ]", &num);	
	*((unsigned int *) ptr) =  num;
			
	fclose(fp);
		
	return 0;
	
close_file1:
close_file:
	fclose(fp);
	
	return -1;
}

int readbuffer(FILE *input_fd, FILE *output_fd, int size, char *buf)
{
	int  length, num_read;

	length = size;

	do {		
		num_read = fread(buf, 1, block_size, input_fd);
		length -= num_read;
		
		if (num_read !=block_size)
		{	/*append 0xFF to the end*/
			memset( (buf+num_read), 0xFF, (block_size-num_read));
		}
		
		/*2011 01 05 Add for MD5 check*/
		MD5Update (&context, buf, block_size);
		/*2011 01 05 Add for MD5 check*/
		
		fwrite(buf, 1, block_size, output_fd);

	} while(length >0);
	
	return 0;
}

int main(int argc, char *argv[])
{
	FILE *fp, *fp_write, *fp_read ;
	int  ub_write_offset, ub_write_size, ub_nand_offset, ub_nand_size;
	int  kernel_offset, kernel_size, rootfs_offset, rootfs_size;
	int  user_offset, user_size;
	int  i = 0, length , ret;
	char *buffer = NULL;
	
	struct stat st;
	char   hdr_buf[512];
	char  dump[128], file_name[7][128];
	
	unsigned char digest[16];
	int	yaffs2=0;

	if (argc != 2)
	{
		printf("Usage: %s input_config \n", argv[0]);
		return -1;
	}

	printf("Fwpack version: 2011/03/04 Copyright by Skyviia \n");
	printf("All right reserved.\n");
	
	MD5Init(&context);
	
//[begin]support yaffs2
//	/*allocate buffer for operation.*/
/*
	buffer = malloc(block_size);
	
	if (!buffer)
	{
		printf("Error: Can not allocate buffer \n");
		return -1;
	}
	
*/
//[end]
	fp = fopen (argv[1], "r");
	
	if (fp == NULL)
	{
		printf("Error: Can not open file config %s \n", argv[1]);
//		free(buffer);
		return -1;
	}
	
	memset(file_name, 0, sizeof(file_name));
	
	while (fgets (dump, sizeof (dump), fp)) {
		/* Skip incomplete conversions and comment strings */
		if (dump[0] == '#')
			continue;
		
		length = strlen(dump);
		
		if ((dump[length-2]==0x0D)&&(dump[length-1]==0x0A))
		{
			length =length-2;
			//printf("Windows format\n");
		}
		else
		if ((dump[length-1]==0x0A))
		{
			length =length-1;
			//printf("linux format\n");
		}
		
		dump[length] = 0;	/*end of string.*/
		
		memcpy( file_name[i], dump, length);
		
		i++;
		
		if(i==7)
			break;
	}
	
	fclose(fp);
	
	if(i<7)
	{
		printf("Invaliad file config \n");
//		free(buffer);
		return -1;
	}
	
	/*create new output file.*/
	fp_write = fopen (file_name[0], "wb");
	
	if (fp_write ==NULL)
	{
		printf("Can not create file %s \n", file_name[0]);
//		free(buffer);
		return -1;
	}
	
	ret = gen_hdr(fp_write, file_name[1], hdr_buf);
	
	if (ret)
	{
		goto close_err;
	}
	
	fseek(fp_write, 512, SEEK_SET);
	
//[begin]support yaffs2
	/*2011/03/04 for Yaffs2 */
	/*allocate buffer for operation.*/
	
	if (FrameSet & 1)
	{	/*default SLC*/
		block_size = 2048;
		printf("NAND: SLC 2K page\n");
	}
	else if (FrameSet & 4)
        {
               block_size = 4096;
               printf("NAND: MLC 4K page\n");
        }
        else
	{
		block_size = 8192;
		printf("NAND: MLC 8K page\n");
	}
	
	if (FrameSet & 2)
	{
		yaffs2 = 0;
		printf("UBIFS system\n");
	}
	else
	{
		yaffs2 = 1;
		printf("yaffs2 system\n");
	}
	
	buffer = malloc(block_size);
	
	if (!buffer)
	{
		printf("Error: Can not allocate buffer \n");
		return -1;
	}
//[begin]support yaffs2
	
	length = hdr_block_size;
	ub_write_offset = length;
	
	/*now append data  u-boot-write*/
	ret = strncmp(file_name[2] ,"none", 4);
	
	if (ret ==0)
	{ 	/*no u-boot-write.*/
		printf("no u-boot writer\n");
		
		ub_write_size = 0;		
		ub_nand_offset = ub_write_offset;
	}
	else
	{
		if ( stat (file_name[2], &st))
		{
			printf("Can not find u-boot writer %s \n", file_name[2]);
			goto close_err;
		}
					
		fp_read = fopen(file_name[2], "rb");
		
		if(fp_read==NULL)
		{
			printf("Can not find u-boot writer %s \n", file_name[2]);
			goto close_err;
		}
				
		ub_write_size = (st.st_size  + block_size-1)& ~(block_size-1);
		ub_nand_offset = ub_write_offset + ub_write_size;
		
		ret = readbuffer(fp_read, fp_write, st.st_size, buffer);
		
		fclose(fp_read);
	}
		
	/*now append data u-boot-nand_loader*/
	ret = strncmp(file_name[3] ,"none", 4);
	
	if (ret ==0)
	{ 	/*no u-boot-loader.*/
		printf("no u-boot nand\n");
		
		ub_nand_size = 0;		
		kernel_offset = ub_nand_offset;
	}
	else
	{
		if ( stat (file_name[3], &st))
		{
			printf("Can not find u-boot nand %s \n", file_name[3]);
			goto close_err;
		}
					
		fp_read = fopen(file_name[3], "rb");
		
		if(fp_read==NULL)
		{
			printf("Can not find u-boot nand %s \n", file_name[3]);
			goto close_err;
		}
				
		ub_nand_size =  (st.st_size  + block_size-1)& ~(block_size-1);
		kernel_offset = ub_nand_offset + ub_nand_size ;
		
		ret = readbuffer(fp_read, fp_write, st.st_size, buffer);
		
		fclose(fp_read);
	}
		
	/*now append data uImage*/
	ret = strncmp(file_name[4] ,"none", 4);
		
	if (ret ==0)
	{ 	/*no kernel image.*/
		printf("no uImage\n");
		
		kernel_size = 0;		
		rootfs_offset = kernel_offset;
	}
	else
	{
		if ( stat (file_name[4], &st))
		{
			printf("Can not find uImage %s \n", file_name[4]);
			goto close_err;
		}
					
		fp_read = fopen(file_name[4], "rb");
		
		if(fp_read==NULL)
		{
			printf("Can not find uImage %s \n", file_name[4]);
			goto close_err;
		}
				
		kernel_size =  (st.st_size  + block_size-1)& ~(block_size-1);
		rootfs_offset = kernel_offset + kernel_size;
		
		ret = readbuffer(fp_read, fp_write, st.st_size, buffer);
		
		fclose(fp_read);
	}
		
	/*now append data rootfs*/	
	ret = strncmp(file_name[5] ,"none", 4);
	
	if (ret ==0)
	{ 	/*no root fs.*/
		printf("no rootfs \n");
		rootfs_size = 0;
		user_offset = rootfs_offset;
	}
	else
	{
		if ( stat (file_name[5], &st))
		{
			printf("Can not find rootfs %s \n", file_name[5]);
			goto close_err;
		}
					
		fp_read = fopen(file_name[5], "rb");
		
		if(fp_read==NULL)
		{
			printf("Can not find rootfs %s \n", file_name[5]);
			goto close_err;
		}
				
		rootfs_size = (st.st_size  + block_size-1)& ~(block_size-1);	
		user_offset = rootfs_offset + rootfs_size;
		
		ret = readbuffer(fp_read, fp_write, st.st_size, buffer);
		
		fclose(fp_read);
	}

	/*now append user data  */
	ret = strncmp(file_name[6] ,"none", 4);
	
	if (ret ==0)
	{ 	/*no userdata image.*/
		printf("no userdata image \n");	
		user_size = 0;
	}
	else
	{		
		if ( stat (file_name[6], &st))
		{
			printf("Can not find userdata img %s \n", file_name[6]);
			goto close_err;
		}
		
		user_size = (st.st_size  + block_size-1)& ~(block_size-1);	
					
		fp_read = fopen(file_name[6], "rb");
		
		if(fp_read==NULL)
		{
			printf("Can not find rootfs %s \n", file_name[6]);
			goto close_err;
		}
					
		ret = readbuffer(fp_read, fp_write, st.st_size, buffer);
		
		fclose(fp_read);
	}
		 		
	/*now update header */
	fseek(fp_write, 0, SEEK_SET);
	
	/*2011 01 05 Add for MD5 check*/
	MD5Final (digest, &context);
	memcpy((hdr_buf+160), digest, 16);
	/*2011 01 05 Add for MD5 check*/
		
	*((unsigned int*)(hdr_buf+192)) = ub_write_offset;
	*((unsigned int*)(hdr_buf+196)) = ub_write_size;
	*((unsigned int*)(hdr_buf+200)) = ub_nand_offset;
	*((unsigned int*)(hdr_buf+204)) = ub_nand_size;
	*((unsigned int*)(hdr_buf+208)) = kernel_offset;
	*((unsigned int*)(hdr_buf+212)) = kernel_size;
	*((unsigned int*)(hdr_buf+216)) = rootfs_offset;
	*((unsigned int*)(hdr_buf+220)) = rootfs_size;
	*((unsigned int*)(hdr_buf+224)) = user_offset;
	*((unsigned int*)(hdr_buf+228)) = user_size;
	
	fwrite(hdr_buf, 1, hdr_block_size, fp_write);
	
	fseek(fp_write, 0, SEEK_END);

	
	fclose(fp_write);
	free(buffer);
	
	return 0;
						
close_err:
	fclose(fp_write);
	
	if(!buffer)
		free(buffer);
	
	/*delete output file.*/	
	remove(file_name[0]);
	
	return -1;
}
