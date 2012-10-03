/*
 * This code is based on u-boot/tool/fw_env.c.
 *  
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include <mtd/mtd-user.h>

#include "cutils/properties.h"

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

#define NOT_SUPPORT_YAFFS2	1

#define MTD_BOOT	   "/dev/mtd/mtd0"
#define MTD_DEVICE	   "/dev/mtd/mtd1"

#define MTD_KER0_DEVICE	   "/dev/mtd/mtd2" 
#define MTD_KER1_DEVICE    "/dev/mtd/mtd3"

#define MTD_ROOT0_DEVICE	"/dev/mtd/mtd4" 
#define MTD_ROOT1_DEVICE	"/dev/mtd/mtd5" 

#define MTD_USER_DEVICE		"/dev/mtd/mtd6" 


#define ENV_KERTAG	   "kernel"
#define ENV_ROOTTAG	   "rootfs"

#define USERDATA_UPDATE		"/data/refresh.sv@"

#define FIRMW_HDR	   "#SKYVIIA#SKYVIIA"

#define MTD_ENV_DEVICE_OPEN	0xFFFFFFFF
#define MTD_ENV_DEVICE_CLOSE	0x00000000

typedef	unsigned char		_uint8_t;
typedef	unsigned short		_uint16_t;
typedef	unsigned int		_uint32_t;

/*
* this is patch for writeoob --- it is should be defined in 
* <mtd/mtd-abi.h>, however, nomral mtd-abi.h dosen't define it
*/

#ifndef MEMWRITEPAGE

/*remark: this extra define should match kernel patch code!!*/
struct mtd_page_buf {
	uint32_t start;      //page start address
	uint32_t ooblength;  
	uint32_t datlength;
	unsigned char  *oobptr;
	unsigned char  *datptr;
};

#define MEMWRITEPAGE		_IOWR('M', 49, struct mtd_page_buf)

#endif

//#define DEBUG
#ifdef	DEBUG
#define	DBG_PRINTF(fmt, args...)	printf(fmt , ##args)
#else
#define DBG_PRINTF(fmt, args...)
#endif

// define the string length for system property checking!
#define MAX_PROP_LEN 90
#define S_OK         0
#define S_ERR        -1
#define ERR_FILEPATH    -1
#define ERR_MTD_INIT    -2
#define ERR_CMDLINE     -3
#define ERR_INVALID_FMT -4
#define ERR_CHK_VIDPID  -5
#define ERR_ALLOC_BUF   -6
#define ERR_READ_FILE   -7
#define ERR_ERASE_BLK   -8
#define ERR_WRITE_BLK   -9
#define ERR_READ_BLK    -10
#define ERR_CHK_BUFFER  -11
#define ERR_CHKSUM      -12
#define ERR_BLK_COUNT   -13
#define ERR_WRITE_TWICE -14
#define ERR_MTDERR	-15

#define _HAVE_ANDROID_ 1

/*
 * REMARK: The following setting is match U-boot settingm
 *        so change it must change u-boot, too!
 */
#define CONFIG_ENV_SIGNATURE	0x23594B53		/*SKY#, little-endian express*/
#define CONFIG_ENV_SIZE 	0x2000			/*8K*/
#define ENV_SIZE		(CONFIG_ENV_SIZE-12)

#define SERACH_BLCOK		2		/*CONFIG_ENV_RANGE/erasesize*/

#define ENV_BLOCK0_OFFSET	0x0		/*From mtd1, block0*/

#define RDWRSIZE_MAXSIZE	8*1024		/*Read/Write Maximum size 16K.*/

struct env_image_redundant {
	uint32_t	sig_flag;	/* sky# */
	uint32_t	crc;		/* CRC32 over data bytes*/
	unsigned int	flags;
	char		data[];
};

struct mtd_env_handle{
	int 			dev_current;	
	void			*image;	
	uint32_t		*crc;
	unsigned int		*flags;
	char			*data;
	
	uint32_t	 	offset0;
	uint32_t	 	offset1;
	
	int 			fd;		/*file handle*/
	uint32_t	 	erasesize;	
};

static unsigned int open_state = MTD_ENV_DEVICE_CLOSE;
static char g_szRetInfo[MAX_PROP_LEN + 1];

/*2011/03/15 Add Begin*/
static int	progress =0;
static int	total_size;
static int	percent =0;
static int	show_percent =0;
/*2011/03/15 Add End*/

/*2011/01/05 Add for MD5 firmware check*/
#define byteReverse(buf, len)	/* Nothing */

struct MD5Context {
	_uint32_t buf[4];
	_uint32_t bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(_uint32_t buf[4], _uint32_t const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

/*2011/01/05 Add for MD5 firmware check*/

static const uint32_t crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
static _uint32_t crc32 (_uint32_t crc, _uint8_t *buf, unsigned int len)
{
	crc = crc ^ 0xffffffffL;
	while (len >= 8)
	{
		DO8(buf);
		len -= 8;
	}
	if (len) do {
		DO1(buf);
	} while (--len);
	
	return crc ^ 0xffffffffL;
}

/*2011/01/05 Add for MD5 check for firmware*/

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
    // Compute number of bytes mod 64 
    unsigned count = (ctx->bits[0] >> 3) & 0x3F;

    //Set the first char of padding to 0x80.  This is safe since there is
      // always at least one byte free 
    unsigned char *p = ctx->in + count;
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

/*2011/01/05 Add for MD5 check for firmware*/

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static char *envmatch (char * s1, char * s2)
{
	while (*s1 == *s2++) {
		if (*s1++ == '=') {
			return s2;
		}
	}
			
	if (*s1 == '\0' && *(s2 - 1) == '=') {
		return s2;
	}
	return NULL;
}

/*
 * Test for bad block on NAND, just returns 0 on NOR, on NAND:
 * 0	- block is good
 * > 0	- block is bad
 * < 0	- failed to test
 */
static int flash_bad_block (int fd, loff_t *blockstart)
{
	int badblock = ioctl(fd, MEMGETBADBLOCK, blockstart);

	if (badblock < 0) {
		DBG_PRINTF("Cannot read bad block mark");
		return badblock;
	}

	if (badblock) {
		DBG_PRINTF ("Bad block at 0x%llx, "
			 "skipping\n", *blockstart);
		return badblock;
	}

	return S_OK;
}

/*
 * Read data from flash at an offset into a provided buffer. On NAND it skips
 * bad blocks but makes sure it stays within "SERACH_BLCOK" blocks 
 * starting from the offset block.
 */ 
static int flash_read_buf(struct mtd_env_handle *handle, void *buf,
	  int blk_id)
{
	size_t blocklen;	/* erase / write length - one block on NAND*/
	size_t readlen = CONFIG_ENV_SIZE;	/* read length */
	off_t  block_seek;
	loff_t blockstart;	/* running start of the current block -
				   MEMGETBADBLOCK needs 64 bits */
	int    i, rc, fd , is_empty=1;
	
	struct env_image_redundant *image;
	unsigned long  *tmp;
		
	fd = handle->fd;
	blocklen = handle->erasesize;
	
	if (blk_id == 0) {
		blockstart = ENV_BLOCK0_OFFSET;
	}
	else {
		blockstart = ENV_BLOCK0_OFFSET + (2*blocklen); 
	}
	
	i=0;
	
	/*get good block location*/
	while (i < SERACH_BLCOK) {
		rc = flash_bad_block (fd, &blockstart);
		
		if (rc == 0) {
			break;		/*good block*/
		}
			
		if (rc < 0) {  /* block test failed */
			return -1;	/* almost impossible*/
		}

		/* this block is bad, check next one */
		blockstart += blocklen;
		i++;
	}
	
	if (i >= SERACH_BLCOK)
	{
//[begin]support yaffs2
/*
		DBG_PRINTF ("Too few good blocks within range\n");
		return -1;
*/
		/*2011/04/28 patch for 2 bad blocks BEGIN*/
		/*search for the other blocks*/
		if (blk_id ==0)
		{
			blockstart = ENV_BLOCK0_OFFSET + (2*blocklen);
		}
		else
		if (blk_id ==1)
		{
			blockstart = ENV_BLOCK0_OFFSET;
		}
		
		if (flash_bad_block (fd, &blockstart) < 0)
			return -1;		/*bad 3 blocks*/
		
		blockstart += blocklen; 
		if (flash_bad_block (fd, &blockstart) < 0)
			return -1;		/*bad 3 blocks*/
		
		/*ok, there are 2 blocks can used within 4 blocks*/
		
		/*2011/04/28 patch for 2 bad blocks End*/
//[end]
	}
	
	if (blk_id ==0) {
		handle->offset0 = (uint32_t) (blockstart);
	}
	else {
		handle->offset1 = (uint32_t) (blockstart);
	}
			
	/*search environment setting inside the block, backward.*/
	block_seek = blocklen - CONFIG_ENV_SIZE;
	
	tmp = (unsigned long *)buf;
	while (block_seek >=0) {
		lseek (fd, blockstart + block_seek, SEEK_SET);
		rc = read(fd, buf, readlen);
		
		/*rc should be -1 (ECC_ERROR??) or "block_seek" */
		if (rc != (int)readlen) {			
			printf("rc is %d \n", rc);
			printf("Read error on %s: %s\n",
				 MTD_DEVICE , strerror (errno));
			break;
		}
				
		/*check buffer data is valid data*/
		for (i=0; i<16 ; i++) { /*we only check the first 64 bytes*/
			if(tmp[i]!=0xFFFFFFFF) {
				is_empty = 0;	/*not empty block*/
				break;
			}
		}
			
		if (i<16) { /*not empty page*/
			image = buf;
			if (image->sig_flag==CONFIG_ENV_SIGNATURE) {
				if( image->crc == crc32 (0, (uint8_t *) image->data, ENV_SIZE)) { /*ok we got it.*/
					//printf("crc ok  %08x \n",block_seek);
					/*remember page offset.*/
					if (blk_id ==0) {
						handle->offset0 = (uint32_t) (blockstart + block_seek);
					}
					else {
						handle->offset1 = (uint32_t) (blockstart + block_seek);
					}
					return S_OK;		/*if find, code will return directly.*/
				}
			}
		}
		
		/*empty page, search previous backward page*/
		block_seek -= CONFIG_ENV_SIZE;
	}
	
	/*garbege data blocks... erase the buffer in write_buf.*/
	if (blk_id == 0) {
	    /*so next update will start at blockstart, so it erase the block!*/
		handle->offset0 = (uint32_t) (blockstart + blocklen - CONFIG_ENV_SIZE);
	}
	else {
    	/*so next update will start at blockstart, so it erase the block!*/
		handle->offset1 = (uint32_t) (blockstart + blocklen - CONFIG_ENV_SIZE);
	}

	/*empty blocks?? garbege data blocks?? */
	DBG_PRINTF ("page empty or garbege data block? \n");
	return -2;
}


/*
 * Write count bytes at offset, but stay within ENVSETCORS (dev) sectors of
 * DEVOFFSET (dev). Similar to the read case above, 
 * 
 */
static int flash_write_buf (struct mtd_env_handle *handle, char *buf, 
	char *cmp_buf, int blk_id)
{
	
	struct erase_info_user erase;
	size_t    blocklen;	/* length of NAND block / NOR erase sector */
	off_t     block_seek;	/* offset inside the erase block to the start
				   of the data */
	loff_t    blockstart;	/* running start of the current block -
				   MEMGETBADBLOCK needs 64 bits */
	uint32_t  offset;
	int 	  rc, fd, i;
	
	uint32_t  *src, *cmp;
	
	fd = handle->fd;
	blocklen = handle->erasesize;
	
	if (blk_id == 0)
		offset = (handle->offset0);
	else
		offset = (handle->offset1);

	blockstart = offset & ~(blocklen - 1);	/*blockstart is current valid section*/
	block_seek = offset & (blocklen - 1);	/*the page inside blocks*/
					
	while(1) {	
		/*try to write next "section"*/
		block_seek += CONFIG_ENV_SIZE;
	
		if ((long)block_seek >= (long)blocklen) {
		    /*we must erase the block, then write the first section.*/
			printf("debug erase \n");
			erase.start =  blockstart;
			erase.length = blocklen;
		
			if (ioctl (fd, MEMERASE, &erase) != 0) {
				DBG_PRINTF ("MTD erase error on  %s\n",	
					 strerror (errno));
				return -1;
			}
			
			if (lseek (fd, blockstart, SEEK_SET) == -1) {
				DBG_PRINTF ("Seek error on %s\n",
				 	strerror (errno));
				return -1;
			}

			if (write (fd, buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				printf ("Write error: %s\n",
					 strerror (errno));
				return -1;
			}
		
			/*adjust current block section location.*/
			if (blk_id == 0)
				handle->offset0 = blockstart;
			else
				handle->offset1 = blockstart;

			return S_OK;
		}
		else {
		    /*we can write directly.*/						
			if (lseek (fd, blockstart+block_seek , SEEK_SET) == -1) {
				DBG_PRINTF ("Seek error on %s\n",
					 strerror (errno));
				return -1;
			}

			if (write (fd, buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				DBG_PRINTF ("Write error: %s\n",
					 strerror (errno));
				return -1;
			}
		
			/*read compare.*/
			if (lseek (fd, blockstart+block_seek , SEEK_SET) == -1) {
				DBG_PRINTF ("Seek error on %s\n",
					 strerror (errno));
				return -1;
			}

			if (read (fd, cmp_buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				DBG_PRINTF ("Read error: %s\n",
					 strerror (errno));
				return -1;
			}
		
			src = (uint32_t *) buf;
			cmp = (uint32_t *) cmp_buf;
		
			for (i=0; i<(CONFIG_ENV_SIZE/4); i++) {
				if (src[i] != cmp[i]) {
				    /*data write != read, error? write next */
					break;	/*try to write next section*/
				}
			}
			
			if (i!=(CONFIG_ENV_SIZE/4)) {
				continue;
			}
					
			/*adjust current block section location.*/			
			if (blk_id == 0)
				handle->offset0 = blockstart+block_seek ;
			else
				handle->offset1 = blockstart+block_seek ;
			break;
		}
	}
	return S_OK;
}
 
 

/*
 * Search the environment for a variable.
 * return S_ERR if no found.
 * return S_OK if found, buf will be the string.
 */
int fw_getenv (struct mtd_env_handle *handle, char *name, char *buf, int *length)
{
	if ((buf==NULL) || (name==NULL) || (*length==0) || (handle==NULL)) {
		return S_ERR;	/*Invalid setting*/
	}
	
	//struct mtd_env_handle *handle = (struct mtd_env_handle *) hand;
	
	char *env_end = handle->data + ENV_SIZE;
    char *nxt = NULL;
    char *env = NULL;
	for (env = handle->data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= env_end) {
				printf("## Error: "
					"environment not terminated\n");
				return -1;
			}
		}
		char *val = envmatch (name, env);
		if (!val) {
			continue;
		}
		
		/*ok we find the string.*/
		int n = 0; 
		int len = *length;
		
		while ((len > n++) && (*buf++ = *val++) != '\0')
			;
			
		if (len == n) {
			*buf = '\0';
		}
		*length = n;
		return S_OK;
	}
	return S_ERR;
}

/*
 * Deletes or sets environment variables. Returns -1 and sets errno error codes:
 * 0	  - OK
 * EROFS  - certain variables ("ethaddr", "serial#") cannot be
 *	    modified or deleted
 *
 *
 */
int fw_setenv (struct mtd_env_handle *handle, char *tag, char *buf)
{
	int  i, len;
	char *env, *nxt, *env_end;
	char *oldval = NULL;
	char *name, *val;
//	struct mtd_env_handle *handle;
	
	if ((handle == NULL) || (tag == NULL)) {
		errno = EINVAL;
		return  -1;
	}
	
//	handle = (struct mtd_env_handle *) hand;	
	name = tag;
	
	env_end = handle->data + ENV_SIZE;
	/*
	 * search if variable with this name already exists
	 */
	for (env = handle->data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= env_end) {
				printf("## Error: "
					"environment not terminated\n");
				errno = EINVAL;
				return -1;
			}
		}
		if ((oldval = envmatch (name, env)) != NULL)
			break;
	}
	
	
	/*
	 * Delete any existing definition
	 */
	if (oldval) {
		/*
		 * Ethernet Address and serial# can be set only once
		 */
		if ((strcmp (name, "ethaddr") == 0) ||
			(strcmp (name, "serial#") == 0)) {		
			errno = EROFS;
			return -1;
		}

		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
			*++env = '\0';		/*20100420 Fixed.*/
		}		
	}
	
	/* Delete only ? */
	if (buf==NULL)
		return S_OK;
	
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CONFIG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */

	len += strlen (buf) + 1;
	
	if (len > (env_end - env)) {
		DBG_PRINTF (
			"Error: environment overflow, \"%s\" deleted\n",
			name);
		return -1;
	}
	
	while ((*env = *name++) != '\0')
		env++;
			
	val = buf;

	*env = '=';
	while ((*++env = *val++) != '\0');
	
	/* end is marked with double '\0' */
	*++env = '\0';
	
	return S_OK;

}

int fw_saveenv(struct mtd_env_handle *handle)
{
//	struct mtd_env_handle *handle;
	off_t  offset;
	int    rc, dev_target, id;
	char   *verify_buf = NULL;
		
	if (handle == NULL) {
		return -1;
    }
    
	verify_buf = malloc(CONFIG_ENV_SIZE);
	
	if (!verify_buf) {
	    /*almost impossible*/
		return -1;		
	}

//	handle = (struct mtd_env_handle *) hand;
	
	*handle->crc = crc32 (0, (uint8_t *) handle->data, ENV_SIZE);	/*update crc*/
	
	dev_target = handle->dev_current;		/*which block we want to write?*/
	
	if (handle->dev_current)
		id = 0;
	else
		id = 1;
	
	(*handle->flags)++;
	
	rc = flash_write_buf(handle, handle->image, verify_buf, id);
	
	free(verify_buf);
	
	if (rc < 0)
		return rc;
		
	handle->dev_current = id;

	return S_OK;	
}

struct mtd_env_handle *env_init(void)
{
	struct mtd_env_handle *handle;
	int    crc0_ok=0, crc1_ok=0;
	int    rc, fd;
	
	void   *addr0, *addr1;
	struct env_image_redundant *redundant;	
	struct mtd_info_user mtdinfo;
	
	unsigned int flag0=0, flag1=0;
	
	if (open_state != (unsigned int)MTD_ENV_DEVICE_CLOSE)
	{	/*already init.*/
		DBG_PRINTF("Multiple env_init??\n");
		return NULL;
	}

	handle = malloc(sizeof(struct mtd_env_handle));
	if (handle == NULL)
	{
		DBG_PRINTF("Can not allocate handle\n");
		return NULL;
	}
		
	memset(handle , 0 , sizeof(struct mtd_env_handle));
	
	addr0 = calloc (1, CONFIG_ENV_SIZE);
	addr1 = calloc (1, CONFIG_ENV_SIZE);
	
	if ((addr0 == NULL) || (addr1 == NULL))
	{
		DBG_PRINTF("Can not allocate buffer for env_data\n");
		goto err_handle;
	}
	
	handle->image = addr0;
	redundant = addr0;
	
	handle->crc	= &redundant->crc;
	handle->flags	= &redundant->flags;
	handle->data	= redundant->data;
		
	fd = open (MTD_DEVICE, O_RDWR);
	
	if (fd < 0) {
		DBG_PRINTF("Can not open mtd device %s: %s\n", 
			MTD_DEVICE, strerror (errno));
		goto err_device;
	}
	
	handle->fd = fd;
	
	ioctl (fd, MEMGETINFO, &mtdinfo);
	
	handle->erasesize = mtdinfo.erasesize;
	
	rc = flash_read_buf(handle, handle->image, 0);
	
	if (rc==0) {
		crc0_ok = 1 ;
		/*Notice: handle->flags is pointer, so we must add "*" 
	  	to access the value*/
		flag0 = *handle->flags;
	}
	else {	
	    /*bad block, empty or garbege?*/
		/*rc should not be -1.*/
	}
	
	handle->image = addr1;
	redundant = addr1;
	
	rc = flash_read_buf(handle, handle->image, 1);
	
	if (rc==0)
	{
		crc1_ok = 1;
		/* redundant->flags is value !*/  
		flag1 = redundant->flags;
	}
	else
	{	/*bad block, empty or garbege?*/

	}
		
	if (crc0_ok && !crc1_ok) {
		handle->dev_current = 0;		
	} else if (!crc0_ok && crc1_ok) {
		handle->dev_current = 1;		
	} else if (!crc0_ok && !crc1_ok) {
		/*almost impossible. It should be fixed in u-boot.*/
		printf("Error: both block fail\n");
		goto err_read1;
	}
	else {	
		if ((flag0 ==0xFFFFFFFF && flag1 == 0) ||
		    flag1 > flag0)
		{			
			handle->dev_current = 1;
		}
		else if ((flag1 == 0xFFFFFFFF && flag0 == 0) ||
			 flag0 > flag1)
		{
			handle->dev_current = 0;
		}
		else /* flags are equal - almost impossible */
		{			
			handle->dev_current = 0;
		}		
	}
	
	/*
	 * If we are reading, we don't need the flag and the CRC any
	 * more, if we are writing, we will re-calculate CRC and update
	 * flags before writing out
	 */
	if (handle->dev_current) {
		handle->image	= addr1;
		handle->crc	= &redundant->crc;
		handle->flags	= &redundant->flags;
		handle->data	= redundant->data;
		free (addr0);
	} else {
		handle->image	= addr0;
		/* Other pointers are already set */
		free (addr1);
	}
				
	open_state = MTD_ENV_DEVICE_OPEN;
	
	return /*(unsigned int)*/handle;
	
err_read1:
	close (fd);
	
err_device:
	if (addr0 != NULL)
		free(addr0);
	if (addr1 != NULL)
		free(addr1);
	
err_handle:
	free((void*)handle);
	
	return NULL;
}

void env_close(struct mtd_env_handle *handle)
{
	//struct mtd_env_handle *env_handle = (struct mtd_env_handle *) hand;
	if (open_state != (unsigned int)MTD_ENV_DEVICE_OPEN) {
		DBG_PRINTF("Close before open? \n");
		return;			
	}
	close (handle->fd);
	free(handle->image);
	free((void*)handle);
	open_state = MTD_ENV_DEVICE_CLOSE;
	return;
}

/*
 *   2010/03/14 Add
 *   read firmware / write flash, update page data only.
 *   For kernel and ubifs.
 *   check =0 for kernel, it will not check 0xFF
 *   check =1 for ubifw, it will check 0xFF and avoid write.
 */
static int fw_update(FILE *fp, int dev_fd, int data_offest, int data_size, 
	int check)
{
	struct mtd_info_user mtdinfo;

	int    read_req, tmp_len;
	int    length, ret, blocksize, pagesize, write_pagesize;
	int    i, block_end, count, write_len, read_offset=0;
	
	unsigned int *ptr;
	char   *buf;
	
	loff_t blockstart;

	fseek(fp, data_offest, SEEK_SET);
	
	ioctl(dev_fd, MEMGETINFO, &mtdinfo);
	
	blocksize = mtdinfo.erasesize;
	pagesize = mtdinfo.writesize;
	
	write_pagesize = mtdinfo.writesize << 2;

	blockstart =0;
	
	buf = malloc(write_pagesize);
	
	if (!buf)
	{	/*memory available is too low...*/
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Please reboot then try to update firmware again!");
		return ERR_ALLOC_BUF;
	}

	blockstart =0;
	length = data_size;

	/*ok now we can try to write image into mtd*/
	/* write data to nand, each time write 4 pages data to nand.*/
	
	while (length >0)
	{
		ret = ioctl (dev_fd, MEMGETBADBLOCK, &blockstart);

		if (ret==0) {			
			tmp_len =0;
			
			if (check)
			{	
				fseek(fp, (data_offest+read_offset), SEEK_SET);	
			}

			while (tmp_len < blocksize)
			{
				/*read buffer, only read write_pagesize bytes*/
				if (length >= write_pagesize)
					read_req = write_pagesize;
				else
					read_req = length;

				if (fread(buf, 1, read_req, fp)!=read_req)
				{
					ret = ERR_READ_FILE;					
					goto err;
				}
				
				if (check)
				{	/*
					 * ubifs image has all page 0xFF
					 * we don't write this page
					 */
					i =0;
					block_end =0;
					count = 0;
					write_len = 0;
					ptr = (unsigned int *) buf;
										
					while (i < read_req)
					{
						i+=4;
						if(*ptr !=0xFFFFFFFF)
						{	/*this page is not empty page,
							 we will write the data to nand*/
							write_len = (i + (pagesize-1)) & ~(pagesize-1);
							i = write_len;
							ptr = (unsigned int *) (buf+write_len);
							count = 0;							
							continue;
						}
						else
						{
							count += 4;
							if (count >= pagesize)
							{	/*ok, write_len is the bytes we need write.*/
								block_end = 1;
								break;
							}
							ptr++;
						}
					}
					
					if (write_len==0)
					{	/*all page is 0xFF so we can skip*/
						length -= (blocksize - tmp_len);						
						tmp_len = blocksize;
						break;
					}
										
					read_req = write_len;
				}

				lseek (dev_fd, (blockstart+tmp_len), SEEK_SET);
				
				/*write data*/
				if (write (dev_fd, buf, read_req) != read_req)
				{
					ret = ERR_WRITE_BLK;
					DBG_PRINTF("Write Error ? \n");
					goto err;
				}
								
				length -= read_req;
				tmp_len += read_req;
				
				if (check)
				{
					if (block_end)
					{	/*
						 * tmp_len is "already write bytes"
						 * blocksize- tmp_len is "skip bytes"
						 */
						length -= (blocksize - tmp_len);						
						/*set tmp_len == blocksize, so write next block*/
						tmp_len = blocksize;
						break;
					}
				}
				
				if (length==0)
					break;		/*write last data, end of block*/
			}

			progress += (blocksize>>10);
			percent = (progress*100)/total_size;
			
			if (percent > 99)
				percent = 99;
			
			if (show_percent != percent)
			{
				show_percent = percent;
#ifdef _HAVE_ANDROID_
				char *szTmp = NULL;
				int c = asprintf( &szTmp, "%d", (show_percent-1));
				property_set("dev.fwoutput", szTmp);
#endif
				printf("%d \n", (show_percent-1));
			}
			
			read_offset += blocksize;			
		}

		blockstart += blocksize;
	}
	
	if (length <= 0)
		ret =0;		/*write success!*/
		
err:
	if (buf != NULL)
		free(buf);
		
	return ret;
}
/*
 *   2010/03/17 Add
 *   read firmware / write flash, update page data only.
 *   For kernel and ubifs.
 *   check =0 for kernel, it will not check 0xFF
 *   check =1 for ubifw, it will check 0xFF and avoid write.
 */
static int fw_update_yaffs2(FILE *fp, int dev_fd, int data_offest, 
	int data_size)
{
	struct mtd_info_user mtdinfo;
	struct mtd_page_buf  nanddata_buf;
	
	int    read_req, tmp_len;
	int    length, ret, blocksize, pagesize, pagesize_offset;
	int    i, write_len;
	
	char   *buf;
	
	loff_t blockstart;
	
//[begin]support yaffs2
/*
#ifdef NOT_SUPPORT_YAFFS2	
	printf("Not Support yet \n");
	return S_ERR;
#else
*/
//[end]

	fseek(fp, data_offest, SEEK_SET);
	
	ioctl(dev_fd, MEMGETINFO, &mtdinfo);
	
	blocksize = mtdinfo.erasesize;
	pagesize = mtdinfo.writesize ;
	
	/*32 is oob data*/
	write_len = pagesize + 32;
		
	blockstart =0;
	pagesize_offset = 0;
	
	buf = malloc(write_len);
	
	if (!buf)
	{	/*memory available is too low...*/
		printf("Please reboot then try to update firmware again!");
		return -2;
	}
	
	nanddata_buf.datptr = buf;
	nanddata_buf.oobptr = (buf+pagesize);
	nanddata_buf.ooblength = 32;
	nanddata_buf.datlength = pagesize;
		
	length = data_size;
	
	while (length > write_len)
	{
		ret = ioctl (dev_fd, MEMGETBADBLOCK, &blockstart);
		
		if (ret==0)
		{	/*start to write a new block, check the block */
			while (pagesize_offset < blocksize)
			{
				if (fread(buf, 1, write_len, fp)!=write_len)
				{
					ret = -4;
					DBG_PRINTF("Read Error ? \n");
					goto err;
				}
				
				nanddata_buf.start= (blockstart + pagesize_offset);

				/* write a page include its oob to nand */
				ret = ioctl(dev_fd, MEMWRITEPAGE, &nanddata_buf);
				
				if(ret)
				{	/*what's wrong??*/
					printf("write data with oob fails \n");
					goto err;
				}
				
				pagesize_offset += pagesize;
				length -= write_len;
				
				if (length < write_len)
				{
					/*write success*/
					break;
				}
			}
//[begin]support yaffs2
			pagesize_offset = 0;
			//printf("finish write_block \n");
//[end]
		}
//[begin]support yaffs2
		progress += (blocksize>>10);
		percent = (progress*100)/total_size;
		
		if (percent > 99)
			percent = 99;
		
		if (show_percent != percent)
		{
			show_percent = percent;
#ifdef _HAVE_ANDROID_
			char *szTmp = NULL;
			int c = asprintf( &szTmp, "%d", (show_percent-1));
			property_set("dev.fwoutput", szTmp);
#endif
			printf("%d \n", (show_percent-1));
		}
//[end]
		
		blockstart += blocksize;
	}
	
err:
	if (buf != NULL)
		free(buf);
		
	return ret;	
//[begin]yaffs2 support
//#endif
//[end]
}

static int erase_section(int dev_fd, int data_size, int erase_all)
{
	struct erase_info_user erase;
	struct mtd_info_user mtdinfo;
	
	loff_t blockstart;
	int    length, blocksize, ret;
		
	ioctl(dev_fd, MEMGETINFO, &mtdinfo);
	
	blocksize = mtdinfo.erasesize;
		
	if (data_size > mtdinfo.size)
	{	/*MLC/SLC?*/
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Invalid Firmware Setting.");
		return ERR_INVALID_FMT;
	}
	
	/* if erase_all, erase whole partition,
	 * otherwise it erase necessary blocks.
	 */
	if (erase_all)
		length = mtdinfo.size;		/*include bad blocks*/
	else				
		length = data_size;
	
	blockstart =0;

	erase.length = blocksize;
	
	while ((length >0) && (blockstart<mtdinfo.size))
	{
		ret = ioctl (dev_fd, MEMGETBADBLOCK, &blockstart);
			
		if (ret==0)
		{	/*this is good block, we can erase the block*/
			erase.start = blockstart;

			if (ioctl(dev_fd, MEMERASE, &erase) != 0)
			{	/*almost impossible, why???*/
				return ERR_MTDERR;
			}
			
			length -= blocksize;
		}
		
		blockstart += blocksize;
	}
	
	if ((erase_all==0) && (length >0))
	{	/*if no erase_all, length must be not less total blocks.*/
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Too many bad blocks to update new firmware.");
		printf("Too many bad blocks to write new firmware!\n");
		return ERR_BLK_COUNT;		
	}
			
	return S_OK;
}

int upgrade_mainfunc(int argc, char *argv[])
{
	int    /*handle, */length, ret;
	int    kernel_idx,rootfs_idx;
	int    kernel_offset, kernel_size, rootfs_offset, rootfs_size;
	int    user_offset, user_size, real_rootfs_size;

	int    read_req, i;
	int    count;
	
	unsigned int *ptr, *fw_id;
	
	char   value[32], hdr_buf[256], cmdline[512];
	unsigned char *buf;
	char   szFwPath[(MAX_PROP_LEN+1)];
	char   *ker_str, *rootfs_str, *str, *tmp;
		
	FILE   *fp, *fp_cmdline;
	int    fd;
	
	struct mtd_info_user mtdinfo;

	struct stat st;
	MD5_CTX context;
	unsigned char digest[16];
        
	// [BEGIN] Willie added for get path from property [2010-12-01]
	memset(szFwPath, (int)NULL, sizeof(char)*(MAX_PROP_LEN+1));

	// [END]

	// [BEGIN] Willie modify for get path from property [2010-12-01] 
	//if (argc!=2)
	//{
	//	printf("Usage: %s firmware_name \n", argv[0]);
	//	return -1;
	//}
	if (argc != 2) {
#ifdef _HAVE_ANDROID_
	    /* If the FW file path doesn't come from command line parameter, try from Android system property. */
		property_get("dev.fwfilename", szFwPath, "");
		if (strlen(szFwPath) == 0) {
			// FW file path is not specified! return error.
			snprintf(g_szRetInfo, MAX_PROP_LEN, "Firmware file path is not specified properly!");
			printf("Firmware file path is not specified properly!\nUsage: %s <firmware_filepathname> \n", argv[0]);
			return ERR_FILEPATH; // -1
		}
		printf("firmware file path is %s\n", szFwPath);
#else
		printf("Firmware file path is not specified properly!\nUsage: %s <firmware_filepathname> \n", argv[0]);
		return ERR_FILEPATH; // -1
#endif       
	} else {
		snprintf(szFwPath, MAX_PROP_LEN, "%s", argv[1]);
	}
    
		
	struct mtd_env_handle *handle = env_init();
	if (handle == NULL) {
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Can not initial MTD device for ENV R/W.");
		DBG_PRINTF("Can not open env_init\n");
		return ERR_MTD_INIT;
	}
	
	length = 32;
	ret = fw_getenv(handle, ENV_KERTAG, value, &length);
	if (ret < 0) {
		printf("why can not get tag?? \n");
	}

	if (value[0]==0x30)			
		kernel_idx = 1;		/*ok we should write kernel1 */
	else	
		kernel_idx = 0;		/*ok we should write kernel0 */	
		
	ret = fw_getenv(handle, ENV_ROOTTAG, value, &length); 
	if (value[0]==0x30)	
		rootfs_idx = 1;		/*ok we should write rootfs1 */
	else
		rootfs_idx = 0;		/*ok we should write rootfs0 */

	/* 2010/04/19 Check Cmdline to avoid calling this function several times.*/
	fp_cmdline = fopen("/proc/cmdline", "r");
	if (fp_cmdline == NULL) {
		/*almost impossible*/
		snprintf(g_szRetInfo, MAX_PROP_LEN, "/proc/cmdline is not found!");
		printf("No cmdline **almost impossible** \n");
		env_close(handle);
		return ERR_CMDLINE;
	}
	count = fread(cmdline, 1, 512, fp_cmdline);
	fclose(fp_cmdline);
	for (tmp = cmdline; *tmp; tmp++)
	{	/*so skrootfs should be later kernel*/	
		if (strncmp(tmp, "kernel=", 7)==0)
		{
			tmp += 7;
			if (((kernel_idx==1) && (*tmp !=0x30)) ||
				((kernel_idx==0) && (*tmp !=0x31)))
			{
				ret = ERR_WRITE_TWICE;
				goto err_writetwice;
			}										
		}
		else if (strncmp(tmp, "skrootfs=", 9)==0)
		{
			tmp += 9;			
			if (((rootfs_idx==1) && (*tmp !=0x30)) ||
				((rootfs_idx==0) && (*tmp !=0x31)))
			{
				ret = ERR_WRITE_TWICE;
				goto err_writetwice;
			}		
			break;
		}	
		else
		{
			continue;
		}		 
	}
	
	fp = fopen(szFwPath, "rb");
	if (fp == NULL) {
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Can not open firmware file: %s", szFwPath);
		printf("Can not open firmware %s \n", szFwPath);
		env_close(handle);
		return ERR_FILEPATH;
	}

	length = fread(hdr_buf, 1, 256, fp);
	if ((length != 256) || (strncmp(hdr_buf, FIRMW_HDR, 16) != 0))
	{
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Invalid firmware package format.");
		printf("Invalid firmware package format %s \n", szFwPath);
		env_close(handle);
		fclose(fp);
		return ERR_INVALID_FMT;
	}
	
	/*2010/12/13, Add Vendor ID Product ID check Begin */
	fd = open(MTD_BOOT, O_RDONLY);
	
	/*Bug read 512 return 511...*/
	if ((fd < 0)||(read(fd, cmdline, 512) < 511))  {
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Can not open mtd0 device.");
		printf("Error Can not open mtd0 \n");
		env_close(handle);
		fclose(fp);
		return ERR_CHK_VIDPID;
	}
	
	close(fd);
	
	fw_id = (unsigned int *) (cmdline+144);	
	ptr = (unsigned int*)(hdr_buf+144);
	
	if ((ptr[0]!=0xFFFFFFFF) || (ptr[1]!=0xFFFFFFFF))
	{	/*
		 *  check Vendor ID and Product ID 
		 *  If mismatch, stop to firmware upgrade
		 */
		if ((fw_id[0]!=ptr[0]) || (fw_id[1]!=ptr[1]))
		{
			snprintf(g_szRetInfo, MAX_PROP_LEN, "Invalid Firmware, Can not upgrade.");
			printf("Invalid Firmware1, Can not upgrade \n");
			printf("fw_id[0]= 0x%08x, ptr[0]=0x%08x\n", fw_id[0], ptr[0]);
                        printf("fw_id[1]= 0x%08x, ptr[1]=0x%08x\n", fw_id[1], ptr[1]);
                        env_close(handle);
			fclose(fp);
			return ERR_CHK_VIDPID;
		}
	}
	/*2010/12/13, Add Vendor ID Product ID check End */
	
	/*2011/03/11, Add Check page size and file format, Begin*/
	/*2011/03/11, fw_id bit0:0 is 4K,  bit1:0 is yaffs2*/
	if ((ptr[2]!=fw_id[2]))
	{
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Invalid Firmware, Can not upgrade.");
		printf("Invalid Firmware2, Can not upgrade \n");
		printf("fw_id[2]= 0x%08x, ptr[2]=0x%08x\n", fw_id[2], ptr[2]);
                env_close(handle);
		fclose(fp);
		return ERR_INVALID_FMT;
	}
	/*2011/03/11, Add Check page size and file format, End*/
							
	/*if you want to check checksum, file format. do here.*/
	kernel_offset =	*((unsigned int*)(hdr_buf+208));
	kernel_size   =	*((unsigned int*)(hdr_buf+212));
	rootfs_offset = *((unsigned int*)(hdr_buf+216));
	rootfs_size   = *((unsigned int*)(hdr_buf+220));	
	user_offset   = *((unsigned int*)(hdr_buf+224));
	user_size     = *((unsigned int*)(hdr_buf+228));
	
	total_size = rootfs_size + kernel_size;
	total_size = total_size >> 10;
	progress = 0;
	percent = 0;
	show_percent =0;
	  	
	buf = malloc(RDWRSIZE_MAXSIZE);

	if (!buf)
	{
		snprintf(g_szRetInfo, MAX_PROP_LEN, "Can not allocate buffer for verification.");
		DBG_PRINTF("Can not allocate buffer! \n");
		ret = ERR_ALLOC_BUF;
		goto err;
	}
	
	/*2011/01/05, Add MD5 check to verify the file data is correct*/
	printf("Verify Firmware Data... \n");
	stat (szFwPath, &st);
	count = st.st_size ;		/*the whole firmware file size*/
	count -= 512;			/*our MD5 doesn't include the first sector*/
	fseek(fp, 512, SEEK_SET);
	
	MD5Init (&context);
	
	while (count>0)
	{	
		if (count >=RDWRSIZE_MAXSIZE)		
			read_req = RDWRSIZE_MAXSIZE;		
		else
			read_req = count;
			
		if (fread(buf, 1, read_req, fp) != (size_t)read_req)
		{
			snprintf(g_szRetInfo, MAX_PROP_LEN, "Error ocurred while reading firmware package.");
			DBG_PRINTF("Read Error ? \n");
			ret = ERR_READ_FILE;
			goto err;
		}
		count -= read_req;
		MD5Update (&context, buf, read_req);						
	}
	
	MD5Final (digest, &context);
	
	for(i=0; i<16; i++)
	{
		if (digest[i] != (unsigned char) hdr_buf[160+i])
		{
			snprintf(g_szRetInfo, MAX_PROP_LEN, "Firmware file checksum error!");
			printf("Illegal Firmware \n");
			ret = ERR_CHKSUM;
			goto err;
		}
	}
	printf("Verify Firmware Ok\n");
	
	free(buf);
				
	/*2011/01/05, Add MD5 check to verify the file data is correct*/
	if (kernel_size != 0)
	{
		fd = open(((kernel_idx) ? MTD_KER1_DEVICE : MTD_KER0_DEVICE), O_RDWR);
		
		/*only erase necessary blocks */
		ret = erase_section(fd, kernel_size, 0);
		
		if(ret)
		{
			close(fd);
			goto err;
		}
	
		ret = fw_update(fp, fd, kernel_offset, kernel_size, 0);

		close(fd);		/*we don't need kernel mtd now.*/
		
		if (ret)
		{
			goto err;		
		}
		else
		{			
			value[0] = (char) (0x30 + kernel_idx);
			value[1] = 0;			/*null end*/
		 
			fw_setenv (handle, ENV_KERTAG, value);
		}	
		
	}
		
	if (rootfs_size != 0)
	{
		fd = open((rootfs_idx ? MTD_ROOT1_DEVICE : MTD_ROOT0_DEVICE), O_RDWR);
			
		/*for ubi, rootfs_size is real file system*/
		real_rootfs_size = rootfs_size;
		
		if ((fw_id[2]&0x2)==0x0)
		{	/*
			 * file system is yaffs2
			 * for this case, nand must be 4K
			 */
			if((fw_id[2]&0x1)!=0x0)
			{
				printf("Invalid Firmware \n");
				close(fd);
				return ERR_INVALID_FMT;
			}
			
			ioctl(fd, MEMGETINFO, &mtdinfo);
			
			if ((mtdinfo.writesize != 8192)&&(mtdinfo.writesize != 4096))
			{	/*our yaffs2 must be 4K 2011/03/17*/
				printf("Invalid Firmware for Nand, mtdinfo.writesize=%d \n", mtdinfo.writesize);
				close(fd);
				return ERR_INVALID_FMT;
			}
			
			/*for yaffs2, rootfs include oob size*/
			i = rootfs_size / (mtdinfo.writesize+32);
			real_rootfs_size = (i << (ffs((unsigned)mtdinfo.writesize)-1));
		}
		
		/*erase the whole partition to initial 0xFF*/
		ret = erase_section(fd, real_rootfs_size, 1);
		
		if(ret)
		{
			close(fd);
			goto err;
		}
		
		if (fw_id[2]&0x2)
		{	/*file system is ubifs*/
			ret = fw_update(fp, fd, rootfs_offset, rootfs_size, 1);
		}
		else
		{	/*file system is yaffs2*/
			ret = fw_update_yaffs2(fp, fd, rootfs_offset, rootfs_size);
		}
		
		close(fd);		/*we don't need rootfs mtd now.*/
		
		if (ret)
		{
			goto err;
		}
		else
		{
			value[0] = (char) (0x30 + rootfs_idx);
			value[1] = 0;			/*null end*/
		 
			fw_setenv (handle, ENV_ROOTTAG, value);
		}
	}
					
	if (user_size != 0) {
		FILE   *fp_data;
		/*just touch the mtd*/
		fp_data = fopen(USERDATA_UPDATE, "wb+");
		fclose(fp_data);						 
	}
	
	/*update env_setting*/
	fw_saveenv(handle);	
	fw_saveenv(handle);
	
#ifdef _HAVE_ANDROID_
   	property_set("dev.fwoutput", "100");
#endif
	printf("%d \n", 100);
	ret = S_OK;
err:	
	fclose(fp);
	
	env_close(handle);		
		
	return  ret;
	
err_writetwice:
	snprintf(g_szRetInfo, MAX_PROP_LEN, "Firmware upgrade is done. Please reboot your device right now.");
	printf("System Upgrade already Changed... \n");
	env_close(handle);
	return ret;
}

int main(int argc, char *argv[]) {
    char sztmp[MAX_PROP_LEN + 1];
    memset(g_szRetInfo, (int)NULL, sizeof(char)*(MAX_PROP_LEN+1));
    memset(sztmp, (int)NULL, sizeof(char)*(MAX_PROP_LEN+1));
    snprintf(g_szRetInfo, MAX_PROP_LEN, "OK");
    
    int ret = upgrade_mainfunc(argc, argv);
    if (ret < S_OK) {
        snprintf(sztmp, MAX_PROP_LEN, "%d,%s", ret, g_szRetInfo);
        property_set("dev.fwoutput", sztmp);
    }
    return ret;
}


