/*
 * This code is based on u-boot/tools/env/fw_env.c
 * ENV NAND Version.
 *
 *
 * (C) Copyright 2000-2008
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetski, DENX Software Engineering, lg@denx.de.
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

#define MTD_DEVICE	"/dev/mtd/mtd1"

#define MTD_ENV_DEVICE_OPEN	0xFFFFFFFF
#define MTD_ENV_DEVICE_CLOSE	0x00000000
/*
typedef	unsigned char		uint8_t;
typedef	unsigned short		uint16_t;
typedef	unsigned int		uint32_t;
*/
#define ENV_DEBUG

#ifdef	ENV_DEBUG
#define	ENV_PRINTF(fmt, args...)	printf(fmt , ##args)
#else
#define ENV_PRINTF(fmt, args...)
#endif

// [BEGIN] Willie added for debug write nand flash issue
#define LOG_TAG "env_lib"
#include <utils/Log.h>
// [END]

/*
 * REMARK: The following setting is match U-boot setting
 *        so change it must change u-boot, too!
 */
 
#define CONFIG_ENV_SIGNATURE	0x23594B53		/*SKY#, little-endian express*/

#define CONFIG_ENV_SIZE 	0x2000			/*8K*/
#define ENV_SIZE		(CONFIG_ENV_SIZE-12)

#define SERACH_BLCOK		2			/*CONFIG_ENV_RANGE/erasesize*/

#define ENV_BLOCK0_OFFSET		0x0		/*From mtd1, block0*/


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

static int open_state = MTD_ENV_DEVICE_CLOSE;

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
void env_die(const char *s)
{
    fprintf(stderr,"error: %s (%s)\n", s, strerror(errno));
    exit(-1);
}

static uint32_t crc32 (uint32_t crc, uint8_t *buf, unsigned int len)
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

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static char *envmatch (char * s1, char * s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '=')
			return s2;
	if (*s1 == '\0' && *(s2 - 1) == '=')
		return s2;
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
	int badblock = ioctl (fd, MEMGETBADBLOCK, blockstart);

	if (badblock < 0) {
		ENV_PRINTF("Cannot read bad block mark");
		return badblock;	/*almost impossible*/
	}

	if (badblock) {
		ENV_PRINTF ("Bad block at 0x%llx, "
			 "skipping\n", *blockstart);
		return badblock;
	}

	return 0;
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
	
	if (blk_id ==0)
	{
		blockstart = ENV_BLOCK0_OFFSET;
	}
	else
	{
		blockstart = ENV_BLOCK0_OFFSET + (2*blocklen) ; 
	}
	
	i=0;
	
	/*get good block location*/
	while (i < SERACH_BLCOK)
	{
		rc = flash_bad_block (fd, &blockstart);
		
		if (rc == 0)
			break;		/*good block*/
			
		if (rc < 0)		/* block test failed */
			return -1;	/* almost impossible*/

		/* this block is bad, check next one */
		blockstart += blocklen;
		i++;
	}
	
	if (i >= SERACH_BLCOK)
	{
//[begin]support yaffs2
/*
		ENV_PRINTF ("Too few good blocks within range\n");
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
	
	if (blk_id ==0)
		handle->offset0 = (uint32_t) (blockstart);
	else
		handle->offset1 = (uint32_t) (blockstart);
			
	/*search environment setting inside the block, backward.*/
	block_seek = blocklen - CONFIG_ENV_SIZE;
	
	tmp = (unsigned long *) buf;
	
	while (block_seek >=0)
	{			
		lseek (fd, blockstart + block_seek, SEEK_SET);
		
		rc = read (fd, buf, readlen);
		
		/*rc should be -1 (ECC_ERROR??) or "block_seek" */
				
		if (rc != readlen) {			
			printf("rc is %d \n", rc);
			printf("Read error on %s: %s\n",
				 MTD_DEVICE , strerror (errno));
			break;
		}
		
		
		/*check buffer data is valid data*/
		for (i=0; i<16 ; i++)
		{	/*we only check the first 64 bytes*/
			if(tmp[i]!=0xFFFFFFFF)
			{
				is_empty = 0;	/*not empty block*/
				break;
			}
		}
			
		if (i<16)
		{	/*not empty page*/
			image = buf;
						
			if (image->sig_flag==CONFIG_ENV_SIGNATURE)
			{
				if( image->crc == crc32 (0, (uint8_t *) image->data, ENV_SIZE))
				{	/*ok we got it.*/	
					
					//printf("crc ok  %08x \n",block_seek);
					/*remember page offset.*/
					if (blk_id ==0)
					{
						handle->offset0 = (uint32_t) (blockstart + block_seek);
					}
					else
					{
						handle->offset1 = (uint32_t) (blockstart + block_seek);
					}
					return 0;		/*if find, code will return directly.*/
				}
			}
		}
		
		/*empty page, search previous backward page*/
		block_seek -= CONFIG_ENV_SIZE;
	}
	
	/*garbege data blocks... erase the buffer in write_buf.*/
	if (blk_id ==0)
	{	/*so next update will start at blockstart, so it erase the block!*/
		handle->offset0 = (uint32_t) (blockstart + blocklen - CONFIG_ENV_SIZE);
	}
	else
	{	/*so next update will start at blockstart, so it erase the block!*/
		handle->offset1 = (uint32_t) (blockstart + blocklen - CONFIG_ENV_SIZE);
	}

	/*empty blocks?? garbege data blocks?? */
	ENV_PRINTF ("page empty or garbege data block? \n");
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
	int	  write_err =0;
	
	uint32_t  *src, *cmp;
	
	fd = handle->fd;
	blocklen = handle->erasesize;
	
	if (blk_id == 0)
		offset = (handle->offset0);
	else
		offset = (handle->offset1);

	blockstart = offset & ~(blocklen - 1);	/*blockstart is current valid section*/
	block_seek = offset & (blocklen - 1);	/*the page inside blocks*/
	
write_correct:
					
	while(1)
	{	
		/*try to write next "section"*/
		block_seek += CONFIG_ENV_SIZE;
	
		if (block_seek >= blocklen)
		{	/*we must erase the block, then write the first section.*/	
			erase.start =  blockstart;
			erase.length = blocklen;
		
			if (ioctl (fd, MEMERASE, &erase) != 0) {
				ENV_PRINTF ("MTD erase error on  %s\n",	
					 strerror (errno));
				return -1;
			}
			
			if (lseek (fd, blockstart, SEEK_SET) == -1) {
				ENV_PRINTF ("Seek error on %s\n",
				 	strerror (errno));
				return -1;
			}

			if (write (fd, buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				printf ("Write error: %s\n",
					 strerror (errno));
				return -1;
			}
						
			/*read compare.*/
			if (lseek (fd, blockstart , SEEK_SET) == -1) {
				ENV_PRINTF ("Seek error on %s\n",
					 strerror (errno));
				return -1;
			}

			if (read (fd, cmp_buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				printf ("Read error: %s\n",
					 strerror (errno));
				return -1;
			}
			
			src = (uint32_t *) buf;
			cmp = (uint32_t *) cmp_buf;
		
			for (i=0; i<(CONFIG_ENV_SIZE/4); i++)
			{
				if (src[i] != cmp[i])
				{	/*data write != read, error? write next */
					printf("almost impossible that write error\n");
					block_seek = 0;
					
					if (write_err == 1)
					{	/*
						* write_err should be zero, if it is 1
						* it means the whole block wrong?
						*/
						printf("this block is bad? \n");
						return -1;	/*almost impossible*/
					}

					write_err = 1;
					
					goto write_correct;
				}
			}
		
			/*adjust current block section location.*/
			if (blk_id == 0)
				handle->offset0 = blockstart;
			else
				handle->offset1 = blockstart;
				
			return 0;
					
		}
		else
		{	/*we can write directly.*/						
			if (lseek (fd, blockstart+block_seek , SEEK_SET) == -1) {
				ENV_PRINTF ("Seek error on %s\n",
					 strerror (errno));
				return -1;
			}

			if (write (fd, buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				printf ("Write error: %s\n",
					 strerror (errno));
				return -1;
			}
		
			/*read compare.*/
			if (lseek (fd, blockstart+block_seek , SEEK_SET) == -1) {
				ENV_PRINTF ("Seek error on %s\n",
					 strerror (errno));
				return -1;
			}

			if (read (fd, cmp_buf, CONFIG_ENV_SIZE) != CONFIG_ENV_SIZE) {
				printf ("Read error: %s\n",
					 strerror (errno));
				return -1;
			}
		
			src = (uint32_t *) buf;
			cmp = (uint32_t *) cmp_buf;
		
			for (i=0; i<(CONFIG_ENV_SIZE/4); i++)
			{
				if (src[i] != cmp[i])
				{	/*data write != read, error? write next */
					break;	/*try to write next section*/
				}
			}
			
			if (i!=(CONFIG_ENV_SIZE/4))
			{
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
	
	return 0;	
}

/*
 * Search the environment for a variable.
 * return -1 if no found.
 * return 0 if found, buf will be the string.
 */
int fw_getenv (unsigned int hand, char *name, char *buf, int *length)
{
	char *env, *nxt, *env_end;
	struct mtd_env_handle *handle;
	
	if ((buf==NULL) || (name==NULL)||(*length==0)||(hand==0))
		return -1;	/*Invalid setting*/
	
	handle = (struct mtd_env_handle *) hand;
	
	env_end = handle->data + ENV_SIZE;

	for (env = handle->data; *env; env = nxt + 1) {
		char *val;
		int  n , len;

		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= env_end) {
				printf("## Error: "
					"environment not terminated\n");
				return -1;
			}
		}
		
		val = envmatch (name, env);
		
		if (!val)
			continue;
		
		/*ok we find the string.*/
		n = 0; 
		len = *length;
		
		while ((len > n++) && (*buf++ = *val++) != '\0')
			;
			
		if (len == n)
			*buf = '\0';
			
		*length = n;
			
		return 0;
	}
	
	return -1;
}

int fw_printenv (unsigned int hand)
{
	char *env, *nxt, *env_end;
	struct mtd_env_handle *handle;

	if (hand==0)
		return -1;

	handle = (struct mtd_env_handle *) hand;

	env_end = handle->data + ENV_SIZE;
	for (env = handle->data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= env_end) {
				printf("## Error: environment not terminated\n");
				return -1;
			}
		}
		printf("%s\n", env);
	}
	return 0;
}

/*
 * Deletes or sets environment variables. Returns -1 and sets errno error codes:
 * 0	  - OK
 * EROFS  - certain variables ("ethaddr", "serial#") cannot be
 *	    modified or deleted
 *
 *
 */
int fw_setenv (unsigned int hand, char *tag, char *buf)
{
	int  i, len;
	char *env, *nxt, *env_end;
	char *oldval = NULL;
	char *name, *val;
	struct mtd_env_handle *handle;
	
	if ((hand==0)||(tag==NULL))
	{
		errno = EINVAL;
		return  -1;
	}
	
	handle = (struct mtd_env_handle *) hand;	
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
		#if 1
		if (/*(strcmp (name, "ethaddr") == 0) ||
			*/(strcmp (name, "serial#") == 0)) {
			errno = EROFS;
			return -1;
		}
		#endif
		

		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
			*++env = '\0';
		}		
	}
	
	/* Delete only ? */
	if (buf==NULL)
		return 0;
		
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CONFIG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */

	len += strlen (buf) + 1;
	
	if (len > (env_end - env)) {
		ENV_PRINTF (
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
	
	return 0;

}

int fw_saveenv(unsigned int hand)
{
	struct mtd_env_handle *handle;
	off_t  offset;
	int    rc, dev_target, id;
	char   *verify_buf = NULL;
		
	if (hand==0)
		return -1;
		
	verify_buf = malloc(CONFIG_ENV_SIZE);
	
	if (!verify_buf)
	{	/*almost impossible*/
		return -1;		
	}
		
	handle = (struct mtd_env_handle *) hand;
	
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

	return 0;		
}

unsigned int env_init (void)
{
	struct mtd_env_handle *handle;
	int    crc0_ok=0, crc1_ok=0;
	int    rc, fd;
	
	void   *addr0, *addr1;
	struct env_image_redundant *redundant;
	struct mtd_info_user mtdinfo;
	
	unsigned int flag0=0, flag1=0;
	
	if (open_state != MTD_ENV_DEVICE_CLOSE)
	{	/*already init.*/
		ENV_PRINTF("Multiple env_init??\n");
		return 0;
	}

	handle = malloc(sizeof(struct mtd_env_handle));

	if (handle == NULL)
	{
		ENV_PRINTF("Can not allocate handle\n");
		return 0;
	}

	memset(handle , 0 , sizeof(struct mtd_env_handle));
	
	addr0 = calloc (1, CONFIG_ENV_SIZE);
	addr1 = calloc (1, CONFIG_ENV_SIZE);
	
	if ((addr0 == NULL)||(addr1==NULL))
	{
		ENV_PRINTF("Can not allocate buffer for env_data\n");
		goto err_handle;
	}
	
	handle->image = addr0;
	redundant = addr0;
	
	handle->crc	= &redundant->crc;
	handle->flags	= &redundant->flags;
	handle->data	= redundant->data;
		
	fd = open (MTD_DEVICE, O_RDWR);
	
	if (fd < 0) {
		ENV_PRINTF("Can not open mtd device %s: %s\n", 
			MTD_DEVICE, strerror (errno));
		goto err_device;
	}
	
	handle->fd = fd;
	
	ioctl (fd, MEMGETINFO, &mtdinfo);
	
	handle->erasesize = mtdinfo.erasesize;
	
	rc = flash_read_buf(handle, handle->image, 0);
	
	if (rc==0)
	{
		crc0_ok = 1 ;
		/*Notice: handle->flags is pointer, so we must add "*" 
	  	to access the value*/
		flag0 = *handle->flags;
	}
	else
	{	/*bad block, empty or garbege?*/
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
		ENV_PRINTF("Use block 0 \n");
	} else if (!crc0_ok && crc1_ok) {
		handle->dev_current = 1;
		ENV_PRINTF("Use block 1 \n");
	} else if (!crc0_ok && !crc1_ok) {
		/*almost impossible. It should be fixed in u-boot.*/
		printf("Error: both block fail\n");
		goto err_read1;
	}
	else 
	{			
		if ((flag0 ==0xFFFFFFFF && flag1 == 0) ||
		    flag1 > flag0)
		{
			ENV_PRINTF("Use block 1 \n");
			handle->dev_current = 1;
		}
		else if ((flag1 == 0xFFFFFFFF && flag0 == 0) ||
			 flag0 > flag1)
		{
			ENV_PRINTF("Use block 0Fg \n");
			handle->dev_current = 0;
		}
		else /* flags are equal - almost impossible */
		{
			ENV_PRINTF("Use block 0Eq \n");
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
	
	return (unsigned int) handle ;
	
err_read1:
	close (fd);
	
err_device:
	if (addr0 != NULL)
		free(addr0);
	if (addr1 != NULL)
		free(addr1);
	
err_handle:
	free((void*)handle);
	
	return 0;
}

void env_close(unsigned int hand)
{
	struct mtd_env_handle *env_handle = (struct mtd_env_handle *) hand;
	
	if (open_state != MTD_ENV_DEVICE_OPEN)
	{
		ENV_PRINTF("Close before open? \n");
		return;			
	}
	
	close (env_handle->fd);
	
	free(env_handle->image);
	
	free((void*)env_handle);
	
	open_state = MTD_ENV_DEVICE_CLOSE;
	
	return;
}

#if 0
int main(void)
{
	int handle, length, rc;	
	char value[128];
	char findtag[] = "bootdelay";
	char findtag1[] = "bootargs";
	
	char delay[] = "10";
	
	handle =  env_init();
	
	if (handle == 0)
	{
		ENV_PRINTF("Can not open env_init\n");
		return 0;
	}

	length = 128;
	
	rc = fw_getenv (handle, findtag1, value, &length);
	
	if(rc != -1)
	{
		printf("we find string %s value is %s \n", ((&findtag1[0])), value);
	}
	
	/*test save environemnt*/
	fw_saveenv(handle);
	fw_saveenv(handle);
	
	//fw_saveenv(handle);
	//fw_saveenv(handle);
	//fw_saveenv(handle);
	//fw_saveenv(handle);

	
	env_close(handle);
	
	return  0;
}
#endif
