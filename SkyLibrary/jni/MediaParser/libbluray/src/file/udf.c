/*
 * This code is based on dvdudf by:
 *   Christian Wolff <scarabaeus@convergence.de>.
 *
 * Modifications by:
 *   Billy Biggs <vektor@dumbterm.net>.
 *   Björn Englund <d4bjorn@dtek.chalmers.se>.
 *
 * dvdudf: parse and read the UDF volume information of a DVD Video
 * Copyright (C) 1999 Christian Wolff for convergence integrated media
 * GmbH The author can be reached at scarabaeus@convergence.de, the
 * project's page is at http://linuxtv.org/dvd/
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include "udf.h"

/* Private but located in/shared with dvd_reader.c */
static int UDFReadBlocksRaw( UDF_DATA *device, uint32_t lb_number,
                             size_t block_count, unsigned char *data,
                             int encrypted )
{
	int ret = 0;
	uint64_t pos = lb_number * DVD_VIDEO_LB_LEN;
	uint64_t tmp;
	ssize_t read_num;

	if (encrypted == 1)
		return ret;
	tmp = (uint64_t) lseek64(device->fd, pos, SEEK_SET);
	if (tmp != pos)
	{
		//printf("seek error: %llu\n", tmp);
		return ret;
	}
	read_num = read(device->fd, data, block_count * DVD_VIDEO_LB_LEN);
	if (read_num > 0)
		ret = read_num /DVD_VIDEO_LB_LEN;

	return ret;
}

/* It's required to either fail or deliver all the blocks asked for. */
static int DVDReadLBUDF( UDF_DATA *device, uint32_t lb_number,
                         size_t block_count, unsigned char *data,
                         int encrypted )
{
  int ret;
  size_t count = block_count;

  while(count > 0) {

    ret = UDFReadBlocksRaw(device, lb_number, count, data, encrypted);

    if(ret <= 0) {
      /* One of the reads failed or nothing more to read, too bad.
       * We won't even bother returning the reads that went ok. */
      return ret;
    }

    count -= (size_t)ret;
    lb_number += (uint32_t)ret;
  }

  return block_count;
}

#ifndef NULL
#define NULL ((void *)0)
#endif

#define UDF_TYPE1_MAP15         0x1511U
#define UDF_VIRTUAL_MAP15       0x1512U
#define UDF_VIRTUAL_MAP20       0x2012U
#define UDF_SPARABLE_MAP15      0x1522U
#define UDF_METADATA_MAP25      0x2511U

#define UDF_ID_VIRTUAL          "*UDF Virtual Partition"
#define UDF_ID_SPARABLE         "*UDF Sparable Partition"
#define UDF_ID_METADATA         "*UDF Metadata Partition"

struct EntityIdentifier {
	uint8_t *flags;
	char *identifier;
	uint8_t *indetifierSuffix;
};

struct Metadata {
	uint32_t meta_file_loc;
	uint32_t mirror_file_loc;
	uint32_t bitmap_file_loc;
	uint32_t alloc_unit_size;
	uint16_t alig_unit_size;
	uint8_t flags;
};

struct AD {
  uint32_t Location;
  uint64_t Length;
  uint8_t  Flags;
  uint16_t Partition;
};

struct PartitionMaps {
	uint32_t MapType;
	uint16_t VolSequenNum;
	uint16_t PartitionNum;
	struct Metadata mdata;
	struct AD meta_ad;
};

struct Volume {
	uint32_t SequenNum;
	//char CharSet[64];
	struct AD FSD;
	char VolumeDesc[128];
	uint32_t BlockSize;
	uint32_t MapNum;
	uint32_t Location;
	uint32_t Length;
	struct PartitionMaps *Maps;
};

struct Partition {
  int valid;
  uint16_t Flags;
  uint16_t Number;
  char Contents[32];
  uint32_t AccessType;
  uint32_t Start;
  uint32_t Length;
  uint32_t Identifier;
  struct Volume vol;
};

struct extent_ad {
  uint32_t location;
  uint32_t length;
};

struct avdp_t {
  struct extent_ad mvds;
  struct extent_ad rvds;
};

struct pvd_t {
  uint8_t VolumeIdentifier[32];
  uint8_t VolumeSetIdentifier[128];
};

struct lbudf {
  uint32_t lb;
  uint8_t *data;
  /* needed for proper freeing */
  uint8_t *data_base;
};

struct icbmap {
  uint32_t lbn;
  struct AD file;
  uint8_t filetype;
};

struct udf_cache {
  int avdp_valid;
  struct avdp_t avdp;
  int pvd_valid;
  struct pvd_t pvd;
  int partition_valid;
  struct Partition partition;
  int rooticb_valid;
  struct AD rooticb;
  int lb_num;
  struct lbudf *lbs;
  int map_num;
  struct icbmap *maps;
};

typedef enum {
  PartitionCache, RootICBCache, LBUDFCache, MapCache, AVDPCache, PVDCache
} UDFCacheType;

/**
 * Set the level of caching on udf
 * level = 0 (no caching)
 * level = 1 (caching filesystem info)
 */
static int BDUDFCacheLevel(UDF_DATA *device, int level)
{
  if(level > 0) {
    level = 1;
  } else if(level < 0) {
    return device->udfcache_level;
  }

  device->udfcache_level = level;

  return level;
}

static void *GetBDUDFCacheHandle(UDF_DATA *device)
{
  return device->udfcache;
}

static void SetBDUDFCacheHandle(UDF_DATA *device, void *cache)
{
  device->udfcache = cache;
}

static void UDFFreeVolume(struct Volume *vol)
{
	if (vol)
	{
		if (vol->Maps)
		{
			free(vol->Maps);
			vol->Maps = NULL;
		}
	}
}

static void UDFFreePartition(struct Partition *part)
{
	if (part)
	{
		UDFFreeVolume(&(part->vol));
	}
}

void FreeBDUDFCache(void *cache)
{
  struct udf_cache *c = (struct udf_cache *)cache;
  if(c == NULL)
    return;

  if(c->lbs) {
    int n;
    for(n = 0; n < c->lb_num; n++)
      free(c->lbs[n].data_base);
    free(c->lbs);
  }
  if(c->maps)
    free(c->maps);
  UDFFreePartition(&c->partition);
  free(c);
}

static int GetBDUDFCache(UDF_DATA *device, UDFCacheType type,
                       uint32_t nr, void *data)
{
  int n;
  struct udf_cache *c;

  if(BDUDFCacheLevel(device, -1) <= 0)
    return 0;

  c = (struct udf_cache *)GetBDUDFCacheHandle(device);

  if(c == NULL)
  {
    return 0;
  }

  switch(type) {
  case AVDPCache:
    if(c->avdp_valid) {
      *(struct avdp_t *)data = c->avdp;
      return 1;
    }
    break;
  case PVDCache:
    if(c->pvd_valid) {
      *(struct pvd_t *)data = c->pvd;
      return 1;
    }
    break;
  case PartitionCache:
    if(c->partition_valid) {
      *(struct Partition *)data = c->partition;
      return 1;
    }
    break;
  case RootICBCache:
    if(c->rooticb_valid) {
      *(struct AD *)data = c->rooticb;
      return 1;
    }
    break;
  case LBUDFCache:
    for(n = 0; n < c->lb_num; n++) {
      if(c->lbs[n].lb == nr) {
        *(uint8_t **)data = c->lbs[n].data;
        return 1;
      }
    }
    break;
  case MapCache:
    for(n = 0; n < c->map_num; n++) {
      if(c->maps[n].lbn == nr) {
        *(struct icbmap *)data = c->maps[n];
        return 1;
      }
    }
    break;
  default:
    break;
  }

  return 0;
}

static int SetUDFCache(UDF_DATA *device, UDFCacheType type,
                       uint32_t nr, void *data)
{
  int n;
  struct udf_cache *c;
  void *tmp;

  if(BDUDFCacheLevel(device, -1) <= 0)
    return 0;

  c = (struct udf_cache *)GetBDUDFCacheHandle(device);

  if(c == NULL) {
    c = (struct udf_cache *)calloc(1, sizeof(struct udf_cache));
    /* fprintf(stderr, "calloc: %d\n", sizeof(struct udf_cache)); */
    if(c == NULL)
      return 0;
    SetBDUDFCacheHandle(device, c);
  }


  switch(type) {
  case AVDPCache:
    c->avdp = *(struct avdp_t *)data;
    c->avdp_valid = 1;
    break;
  case PVDCache:
    c->pvd = *(struct pvd_t *)data;
    c->pvd_valid = 1;
    break;
  case PartitionCache:
	if (c->partition.vol.Maps != NULL)
	{
		free(c->partition.vol.Maps);
	}
    c->partition = *(struct Partition *)data;
    c->partition_valid = 1;
    break;
  case RootICBCache:
    c->rooticb = *(struct AD *)data;
    c->rooticb_valid = 1;
    break;
  case LBUDFCache:
    for(n = 0; n < c->lb_num; n++) {
      if(c->lbs[n].lb == nr) {
        /* replace with new data */
        c->lbs[n].data_base = ((uint8_t **)data)[0];
        c->lbs[n].data = ((uint8_t **)data)[1];
        c->lbs[n].lb = nr;
        return 1;
      }
    }
    c->lb_num++;
    tmp = realloc(c->lbs, c->lb_num * sizeof(struct lbudf));
    /*
    fprintf(stderr, "realloc lb: %d * %d = %d\n",
    c->lb_num, sizeof(struct lbudf),
    c->lb_num * sizeof(struct lbudf));
    */
    if(tmp == NULL) {
      if(c->lbs) free(c->lbs);
      c->lb_num = 0;
      return 0;
    }
    c->lbs = (struct lbudf*)tmp;
    c->lbs[n].data_base = ((uint8_t **)data)[0];
    c->lbs[n].data = ((uint8_t **)data)[1];
    c->lbs[n].lb = nr;
    break;
  case MapCache:
    for(n = 0; n < c->map_num; n++) {
      if(c->maps[n].lbn == nr) {
        /* replace with new data */
        c->maps[n] = *(struct icbmap *)data;
        c->maps[n].lbn = nr;
        return 1;
      }
    }
    c->map_num++;
    tmp = realloc(c->maps, c->map_num * sizeof(struct icbmap));
    /*
    fprintf(stderr, "realloc maps: %d * %d = %d\n",
      c->map_num, sizeof(struct icbmap),
      c->map_num * sizeof(struct icbmap));
    */
    if(tmp == NULL) {
      if(c->maps) free(c->maps);
      c->map_num = 0;
      return 0;
    }
    c->maps = (struct icbmap*)tmp;
    c->maps[n] = *(struct icbmap *)data;
    c->maps[n].lbn = nr;
    break;
  default:
    return 0;
  }

  return 1;
}


/* For direct data access, LSB first */
#define GETN1(p) ((uint8_t)data[p])
#define GETN2(p) ((uint16_t)data[p] | ((uint16_t)data[(p) + 1] << 8))
#define GETN3(p) ((uint32_t)data[p] | ((uint32_t)data[(p) + 1] << 8)    \
                  | ((uint32_t)data[(p) + 2] << 16))
#define GETN4(p) ((uint32_t)data[p]                     \
                  | ((uint32_t)data[(p) + 1] << 8)      \
                  | ((uint32_t)data[(p) + 2] << 16)     \
                  | ((uint32_t)data[(p) + 3] << 24))
/* This is wrong with regard to endianess */
#define GETN(p, n, target) memcpy(target, &data[p], n)

static int Unicodedecode( uint8_t *data, int len, char *target )
{
  int p = 1, i = 0;

  if( ( data[ 0 ] == 8 ) || ( data[ 0 ] == 16 ) ) do {
    if( data[ 0 ] == 16 ) p++;  /* Ignore MSB of unicode16 */
    if( p < len ) {
      target[ i++ ] = data[ p++ ];
    }
  } while( p < len );

  target[ i ] = '\0';
  return 0;
}

static void UDFEntIdentifier( uint8_t *data, struct EntityIdentifier *ident)
{
	ident->flags = (uint8_t *)&data[0];
	ident->identifier = (char *)&data[1];
	ident->indetifierSuffix = &data[24];
}

static int UDFDescriptor( uint8_t *data, uint16_t *TagID )
{
  *TagID = GETN2(0);
  /* TODO: check CRC 'n stuff */
  return 0;
}

static int UDFExtentAD( uint8_t *data, uint32_t *Length, uint32_t *Location )
{
  *Length   = GETN4(0);
  *Location = GETN4(4);
  return 0;
}

static int UDFShortAD( uint8_t *data, struct AD *ad,
                       struct Partition *partition )
{
  uint32_t leng = GETN4(0);
  ad->Flags = leng >> 30;
  ad->Length = (leng & 0x3FFFFFFF);
  ad->Location = GETN4(4);
  ad->Partition = partition->Number; /* use number of current partition */
  return 0;
}

static int UDFLongAD( uint8_t *data, struct AD *ad )
{
  uint32_t leng = GETN4(0);
  ad->Flags = leng >> 30;
  ad->Length = (leng & 0x3FFFFFFF);
  ad->Location = GETN4(4);
  ad->Partition = GETN2(8);
  /* GETN(10, 6, Use); */
  return 0;
}

static int UDFExtAD( uint8_t *data, struct AD *ad )
{
  uint32_t leng = GETN4(0);
  ad->Flags = leng >> 30;
  ad->Length = (leng & 0x3FFFFFFF);
  ad->Location = GETN4(12);
  ad->Partition = GETN2(16);
  /* GETN(10, 6, Use); */
  return 0;
}

static int UDFICB( uint8_t *data, uint8_t *FileType, uint16_t *Flags )
{
  *FileType = GETN1(11);
  *Flags = GETN2(18);
  return 0;
}


static int UDFPartition( uint8_t *data, uint16_t *Flags, uint16_t *Number,
                         char *Contents, uint32_t *Start, uint32_t *Length )
{
  uint32_t volume;
  volume = GETN4(16);
  *Flags = GETN2(20);
  *Number = GETN2(22);
  GETN(24, 32, Contents);
  *Start = GETN4(188);
  *Length = GETN4(192);
  return 0;
}

/**
 * Reads the volume descriptor and checks the parameters.  Returns 0 on OK, 1
 * on error.
 */
static int UDFLogVolume( uint8_t *data, struct Volume *vol)
{
  uint32_t MT_L, N_PM, volume;
  uint32_t ii, type, length, pos;
  struct PartitionMaps *maps;
  volume = GETN4(16);
  Unicodedecode(&data[84], 128, vol->VolumeDesc);
  vol->BlockSize = GETN4(212);  /* should be 2048 */
  memset((void *)&vol->FSD, 0, sizeof(struct AD));
  UDFLongAD(&data[248], &vol->FSD);
  //printf("File Set Descriptor: block: %d, part: %d\n", vol->FSD.Location, vol->FSD.Partition);
  MT_L = GETN4(264);    /* should be 6 */
  N_PM = GETN4(268);    /* should be 1 */
  vol->MapNum = N_PM;
  if ((N_PM >= 1) && (vol->Maps == NULL))
  {
	  vol->Maps = (struct PartitionMaps *)calloc(1, sizeof(struct PartitionMaps) * N_PM);
  }
  UDFExtentAD( &data[432], &vol->Length, &vol->Location);
  //printf("location: %d, length: %d\n", vol->Location, vol->Length);
  pos = 440;
  for (ii = 0; ii < N_PM; ii++)
  {
	  maps = &vol->Maps[ii];
	  type = GETN1(pos);
	  length = GETN1(pos+1);
	  if (type == 1)
	  {
		  maps->MapType = UDF_TYPE1_MAP15;
		  maps->VolSequenNum = GETN2(pos+2);
		  maps->PartitionNum = GETN2(pos+4);
	  }
	  else if (type == 2)
	  {
		  struct EntityIdentifier ident;
		  uint32_t version;
		  UDFEntIdentifier(&data[pos+4], &ident);
		  version = ((uint16_t)ident.indetifierSuffix[1] << 8) | (uint16_t)(ident.indetifierSuffix[0]);
		  //printf("identifier: '%s', version: %04x\n", ident.identifier, version);
		  if (strncmp(ident.identifier, UDF_ID_VIRTUAL, strlen(UDF_ID_VIRTUAL)) == 0)
		  {
			  if (version < 0x0200)
				  maps->MapType = UDF_VIRTUAL_MAP15;
			  else
				  maps->MapType = UDF_VIRTUAL_MAP20;
		  }
		  else if (strncmp(ident.identifier, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)) == 0)
		  {
			  maps->MapType = UDF_SPARABLE_MAP15;
		  }
		  else if (strncmp(ident.identifier, UDF_ID_METADATA, strlen(UDF_ID_METADATA)) == 0)
		  {
			  maps->MapType = UDF_METADATA_MAP25;
		  }

		  maps->VolSequenNum = GETN2(pos+36);
		  maps->PartitionNum = GETN2(pos+38);
		  maps->mdata.meta_file_loc = GETN4(pos+40);
		  maps->mdata.mirror_file_loc = GETN4(pos+44);
		  maps->mdata.bitmap_file_loc = GETN4(pos+48);
		  maps->mdata.alloc_unit_size = GETN4(pos+52);
		  maps->mdata.alig_unit_size = GETN2(pos+56);
		  maps->mdata.flags = data[pos+58];
		  //printf("meta_loc: %u, mirr_loc: %u, bit_loc: %u, alloc_unit: %u, alig_unit: %hu, flags: %hhu\n", maps->mdata.meta_file_loc, maps->mdata.mirror_file_loc, maps->mdata.bitmap_file_loc, maps->mdata.alloc_unit_size, maps->mdata.alig_unit_size, maps->mdata.flags);
	  }
	  //printf("Volume Sequence Number: %hu, Partition Number: %hu\n", maps->VolSequenNum, maps->PartitionNum);
	  pos += length;
  }
  //printf("volume: %d, MT_L: %d, N_PM: %d\n", volume, MT_L, N_PM);
  if (vol->BlockSize != DVD_VIDEO_LB_LEN) return 1;
  return 0;
}

static int UDFExtFileEntry( uint8_t *data, uint8_t *FileType,
                         struct Partition *partition, struct AD *ad )
{
  uint16_t flags;
  uint32_t L_EA, L_AD;
  unsigned int p;
  struct AD temp_ad;
  int is_init = 0;

  memset((void *)&temp_ad, 0, sizeof(struct AD));
  UDFICB( &data[ 16 ], FileType, &flags );

  /* Init ad for an empty file (i.e. there isn't a AD, L_AD == 0 ) */
  //ad->Length = GETN4( 60 ); /* Really 8 bytes a 56 */
  ad->Length = 0;
  ad->Flags = 0;
  ad->Location = 0; /* what should we put here?  */
  ad->Partition = partition->Number; /* use number of current partition */

  L_EA = GETN4( 208 );
  L_AD = GETN4( 212 );

  if (216 + L_EA + L_AD > DVD_VIDEO_LB_LEN)
    return 0;

  p = 216 + L_EA;
  while( p < 216 + L_EA + L_AD ) {
    switch( flags & 0x0007 ) {
    case 0:
      UDFShortAD( &data[ p ], &temp_ad, partition );
      p += 8;
      break;
    case 1:
      UDFLongAD( &data[ p ], &temp_ad );
      p += 16;
      break;
    case 2:
      UDFExtAD( &data[ p ], &temp_ad );
      p += 20;
      break;
    case 3:
      switch( L_AD ) {
      case 8:
        UDFShortAD( &data[ p ], &temp_ad, partition );
        break;
      case 16:
        UDFLongAD( &data[ p ], &temp_ad );
        break;
      case 20:
        UDFExtAD( &data[ p ], &temp_ad );
        break;
      }
      p += L_AD;
      break;
    default:
      p += L_AD;
      break;
    }
	if (is_init == 0)
	{
		memcpy((void *)ad, (void *)&temp_ad, sizeof(struct AD));
		is_init = 1;
	} else {
		// only update file size
		ad->Length += temp_ad.Length;
	}
  }
  return 0;
}

static int UDFFileEntry( uint8_t *data, uint8_t *FileType,
                         struct Partition *partition, struct AD *ad )
{
  uint16_t flags;
  uint32_t L_EA, L_AD;
  unsigned int p;

  UDFICB( &data[ 16 ], FileType, &flags );

  /* Init ad for an empty file (i.e. there isn't a AD, L_AD == 0 ) */
  ad->Length = GETN4( 60 ); /* Really 8 bytes a 56 */
  ad->Flags = 0;
  ad->Location = 0; /* what should we put here?  */
  ad->Partition = partition->Number; /* use number of current partition */

  L_EA = GETN4( 168 );
  L_AD = GETN4( 172 );

  if (176 + L_EA + L_AD > DVD_VIDEO_LB_LEN)
    return 0;

  p = 176 + L_EA;
  while( p < 176 + L_EA + L_AD ) {
    switch( flags & 0x0007 ) {
    case 0:
      UDFShortAD( &data[ p ], ad, partition );
      p += 8;
      break;
    case 1:
      UDFLongAD( &data[ p ], ad );
      p += 16;
      break;
    case 2:
      UDFExtAD( &data[ p ], ad );
      p += 20;
      break;
    case 3:
      switch( L_AD ) {
      case 8:
        UDFShortAD( &data[ p ], ad, partition );
        break;
      case 16:
        UDFLongAD( &data[ p ], ad );
        break;
      case 20:
        UDFExtAD( &data[ p ], ad );
        break;
      }
      p += L_AD;
      break;
    default:
      p += L_AD;
      break;
    }
  }
  return 0;
}

static int UDFFileIdentifier( uint8_t *data, uint8_t *FileCharacteristics,
                              char *FileName, struct AD *FileICB )
{
  uint8_t L_FI;
  uint16_t L_IU;

  *FileCharacteristics = GETN1(18);
  L_FI = GETN1(19);
  memset((void *)FileICB, 0, sizeof(struct AD));
  UDFLongAD(&data[20], FileICB);
  L_IU = GETN2(36);
  if (L_FI) Unicodedecode(&data[38 + L_IU], L_FI, FileName);
  else FileName[0] = '\0';
  return 4 * ((38 + L_FI + L_IU + 3) / 4);
}

static uint32_t UDFGetBlock(struct Partition *partition, uint32_t part_num, uint32_t block)
{
	uint32_t nRet = 0;
	struct PartitionMaps *Maps;
	struct Volume *vol;

	vol = &partition->vol;
	if (part_num > vol->MapNum)
	{
		//printf("Wrong partition number: %d of %d\n", part_num, vol->MapNum);
		return nRet;
	}
	Maps = &vol->Maps[part_num];
	switch (Maps->MapType)
	{
		case UDF_TYPE1_MAP15:
			nRet = partition->Start + block;
			break;
		case UDF_METADATA_MAP25:
			//printf("part: %d, %d\n", Maps->meta_ad.Partition, Maps->meta_ad.Location + block);
			nRet = UDFGetBlock(partition, Maps->meta_ad.Partition, Maps->meta_ad.Location + block);
			break;
		default:
			//printf("Unknow map type: %x\n", Maps->MapType);
			break;
	}
	//printf("UDFGetBlock: %d\n", nRet);
	return nRet;
}

/**
 * Maps ICB to FileAD
 * ICB: Location of ICB of directory to scan
 * FileType: Type of the file
 * File: Location of file the ICB is pointing to
 * return 1 on success, 0 on error;
 */
static int UDFMapICB( UDF_DATA *device, struct AD ICB, uint8_t *FileType,
                      struct Partition *partition, struct AD *File )
{
  uint8_t LogBlock_base[DVD_VIDEO_LB_LEN + 2048];
  uint8_t *LogBlock = (uint8_t *)(((uintptr_t)LogBlock_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum;
  uint16_t TagID;
  struct icbmap tmpmap;


  //lbnum = partition->Start + ICB.Location;
  lbnum = UDFGetBlock(partition, ICB.Partition, ICB.Location);
  //printf("lbnum: %d\n", lbnum);
  tmpmap.lbn = lbnum;
  if(GetBDUDFCache(device, MapCache, lbnum, &tmpmap)) {
    *FileType = tmpmap.filetype;
    memcpy(File, &tmpmap.file, sizeof(tmpmap.file));
    return 1;
  }

  do {
    if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
		return 0;
      TagID = 0;
	} else
      UDFDescriptor( LogBlock, &TagID );

	//printf("icb tagid: %d\n", TagID);
    if( TagID == 261 ) {
      UDFFileEntry( LogBlock, FileType, partition, File );
      memcpy(&tmpmap.file, File, sizeof(tmpmap.file));
      tmpmap.filetype = *FileType;
      SetUDFCache(device, MapCache, tmpmap.lbn, &tmpmap);
      return 1;
    }
	else if ( TagID == 266 )
	{
      UDFExtFileEntry( LogBlock, FileType, partition, File );
	  //printf("in file: part: %u, block: %u\n", File->Partition, File->Location);
      memcpy(&tmpmap.file, File, sizeof(tmpmap.file));
      tmpmap.filetype = *FileType;
      SetUDFCache(device, MapCache, tmpmap.lbn, &tmpmap);
      return 1;
	}
  } while( ( lbnum <= lbnum + ( ICB.Length - 1 )
             / DVD_VIDEO_LB_LEN ) && ( TagID != 261 ) );

  return 0;
}

/**
 * Dir: Location of directory to scan
 * FileName: Name of file to look for
 * FileICB: Location of ICB of the found file
 * return 1 on success, 0 on error;
 */
static int UDFScanDir( UDF_DATA *device, struct AD Dir, char *FileName,
                       struct Partition *partition, struct AD *FileICB,
                       int cache_file_info)
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint8_t directory_base[ 2 * DVD_VIDEO_LB_LEN + 2048];
  uint8_t *directory = (uint8_t *)(((uintptr_t)directory_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum;
  uint16_t TagID;
  uint8_t filechar;
  unsigned int p;
  uint8_t *cached_dir_base = NULL, *cached_dir;
  uint32_t dir_lba;
  struct AD tmpICB;
  int found = 0;
  int in_cache = 0;

  /* Scan dir for ICB of file */
  //lbnum = partition->Start + Dir.Location;
  lbnum = UDFGetBlock(partition, Dir.Partition, Dir.Location);

  if(BDUDFCacheLevel(device, -1) > 0) {
    /* caching */

    if(!GetBDUDFCache(device, LBUDFCache, lbnum, &cached_dir)) {
      dir_lba = (Dir.Length + DVD_VIDEO_LB_LEN) / DVD_VIDEO_LB_LEN;
      if((cached_dir_base = (uint8_t*)malloc(dir_lba * DVD_VIDEO_LB_LEN + 2048)) == NULL)
        return 0;
      cached_dir = (uint8_t *)(((uintptr_t)cached_dir_base & ~((uintptr_t)2047)) + 2048);
      if( DVDReadLBUDF( device, lbnum, dir_lba, cached_dir, 0) <= 0 ) {
        free(cached_dir_base);
        cached_dir_base = NULL;
        cached_dir = NULL;
      }
      /*
      if(cached_dir) {
        fprintf(stderr, "malloc dir: %d\n",  dir_lba * DVD_VIDEO_LB_LEN);
      }
      */
      {
        uint8_t *data[2];
        data[0] = cached_dir_base;
        data[1] = cached_dir;
        SetUDFCache(device, LBUDFCache, lbnum, data);
      }
    } else
      in_cache = 1;

    if(cached_dir == NULL)
      return 0;

    p = 0;

    while( p < Dir.Length ) {
      UDFDescriptor( &cached_dir[ p ], &TagID );
      if( TagID == 257 ) {
        p += UDFFileIdentifier( &cached_dir[ p ], &filechar,
                                filename, &tmpICB );
        if(cache_file_info && !in_cache) {
          uint8_t tmpFiletype;
          struct AD tmpFile;

          if( !strcasecmp( FileName, filename ) ) {
            memcpy(FileICB, &tmpICB, sizeof(tmpICB));
            found = 1;
          }
          UDFMapICB(device, tmpICB, &tmpFiletype, partition, &tmpFile);
	  //printf("length: %llu\n", tmpFile.Length);
        } else {
          if( !strcasecmp( FileName, filename ) ) {
            memcpy(FileICB, &tmpICB, sizeof(tmpICB));
            return 1;
          }
        }
      } else {
        if(cache_file_info && (!in_cache) && found)
          return 1;
        return 0;
      }
    }
    if(cache_file_info && (!in_cache) && found)
      return 1;
    return 0;
  }

  if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 )
    return 0;

  p = 0;
  while( p < Dir.Length ) {
    if( p > DVD_VIDEO_LB_LEN ) {
      ++lbnum;
      p -= DVD_VIDEO_LB_LEN;
      Dir.Length -= DVD_VIDEO_LB_LEN;
      if( DVDReadLBUDF( device, lbnum, 2, directory, 0 ) <= 0 ) {
        return 0;
      }
    }
    UDFDescriptor( &directory[ p ], &TagID );
    if( TagID == 257 ) {
      p += UDFFileIdentifier( &directory[ p ], &filechar,
                              filename, FileICB );
      if( !strcasecmp( FileName, filename ) ) {
        return 1;
      }
    } else
      return 0;
  }

  return 0;
}

static int UDFCacheDir( UDF_DIR *device, struct AD Dir,
                       struct Partition *partition, struct AD *FileICB)
{
	char filename[ MAX_UDF_FILE_NAME_LEN ];
	uint8_t directory_base[ 2 * DVD_VIDEO_LB_LEN + 2048];
	uint8_t *directory = (uint8_t *)(((uintptr_t)directory_base & ~((uintptr_t)2047)) + 2048);
	uint32_t lbnum;
	uint16_t TagID;
	uint8_t filechar;
	unsigned int p;
	uint8_t *cached_dir_base = NULL, *cached_dir;
	uint32_t dir_lba;
	struct AD tmpICB;
	int in_cache = 0;
	uint32_t file_num = 0;

	/* Scan dir for ICB of file */
	//lbnum = partition->Start + Dir.Location;
	lbnum = UDFGetBlock(partition, Dir.Partition, Dir.Location);

	if(BDUDFCacheLevel(device->udf_data, -1) > 0) {
		/* caching */

		if(!GetBDUDFCache(device->udf_data, LBUDFCache, lbnum, &cached_dir)) {
			dir_lba = (Dir.Length + DVD_VIDEO_LB_LEN) / DVD_VIDEO_LB_LEN;
			if((cached_dir_base = (uint8_t*)malloc(dir_lba * DVD_VIDEO_LB_LEN + 2048)) == NULL)
				return 0;
			cached_dir = (uint8_t *)(((uintptr_t)cached_dir_base & ~((uintptr_t)2047)) + 2048);
			if( DVDReadLBUDF( device->udf_data, lbnum, dir_lba, cached_dir, 0) <= 0 ) {
				free(cached_dir_base);
				cached_dir_base = NULL;
				cached_dir = NULL;
			}
			/*
			   if(cached_dir) {
			   fprintf(stderr, "malloc dir: %d\n",  dir_lba * DVD_VIDEO_LB_LEN);
			   }
			   */
			{
				uint8_t *data[2];
				data[0] = cached_dir_base;
				data[1] = cached_dir;
				SetUDFCache(device->udf_data, LBUDFCache, lbnum, data);
			}
		} else
			in_cache = 1;

		if(cached_dir == NULL)
			return 0;

		p = 0;

		device->aloc_num = 5;
		device->file_name = malloc(256 * device->aloc_num);
		while( p < Dir.Length ) {
			UDFDescriptor( &cached_dir[ p ], &TagID );
			//printf("aaa TagID: %d, len: %d\n", TagID, Dir.Length);
			if( TagID == 257 ) {
				p += UDFFileIdentifier( &cached_dir[ p ], &filechar,
						filename, &tmpICB );
				if (filename[0] == '\0')
					continue;
				if (file_num + 1 > device->aloc_num)
				{
					device->aloc_num += 5;
					device->file_name = realloc((void *)device->file_name, 256 * device->aloc_num);
				}
				//printf("add filename: '%s'\n", filename);
				strncpy((char *)(device->file_name + file_num), filename, 256);
				device->file_num = ++file_num;
			} else {
				return 0;
			}
		}
		return 1;
	}

	if( DVDReadLBUDF( device->udf_data, lbnum, 2, directory, 0 ) <= 0 )
		return 0;

	p = 0;
	while( p < Dir.Length ) {
		if( p > DVD_VIDEO_LB_LEN ) {
			++lbnum;
			p -= DVD_VIDEO_LB_LEN;
			Dir.Length -= DVD_VIDEO_LB_LEN;
			if( DVDReadLBUDF( device->udf_data, lbnum, 2, directory, 0 ) <= 0 ) {
				return 0;
			}
		}
		UDFDescriptor( &directory[ p ], &TagID );
		if( TagID == 257 ) {
			p += UDFFileIdentifier( &directory[ p ], &filechar,
					filename, FileICB );
			//printf("filename2: %s\n", filename);
		}
	}

	return 0;
}

static int UDFGetAVDP( UDF_DATA *device,
                       struct avdp_t *avdp)
{
  uint8_t Anchor_base[ DVD_VIDEO_LB_LEN + 2048 ];
  uint8_t *Anchor = (uint8_t *)(((uintptr_t)Anchor_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum, MVDS_location, MVDS_length;
  uint16_t TagID;
  uint32_t lastsector;
  int terminate;
  struct avdp_t;

  if(GetBDUDFCache(device, AVDPCache, 0, avdp))
    return 1;

  /* Find Anchor */
  lastsector = 0;
  lbnum = 256;   /* Try #1, prime anchor */
  terminate = 0;

  for(;;) {
    if( DVDReadLBUDF( device, lbnum, 1, Anchor, 0 ) > 0 ) {
      UDFDescriptor( Anchor, &TagID );
    } else {
      TagID = 0;
    }
    if (TagID != 2) {
      /* Not an anchor */
      if( terminate ) return 0; /* Final try failed */

      if( lastsector ) {
        /* We already found the last sector.  Try #3, alternative
         * backup anchor.  If that fails, don't try again.
         */
        lbnum = lastsector;
        terminate = 1;
      } else {
        /* TODO: Find last sector of the disc (this is optional). */
        if( lastsector )
          /* Try #2, backup anchor */
          lbnum = lastsector - 256;
        else
          /* Unable to find last sector */
          return 0;
      }
    } else
      /* It's an anchor! We can leave */
      break;
  }
  /* Main volume descriptor */
  UDFExtentAD( &Anchor[ 16 ], &MVDS_length, &MVDS_location );
  avdp->mvds.location = MVDS_location;
  avdp->mvds.length = MVDS_length;

  /* Backup volume descriptor */
  UDFExtentAD( &Anchor[ 24 ], &MVDS_length, &MVDS_location );
  avdp->rvds.location = MVDS_location;
  avdp->rvds.length = MVDS_length;

  SetUDFCache(device, AVDPCache, 0, avdp);

  return 1;
}

#if 0
static int UDFExtFileEntry(uint8_t *data, struct Partition *partition, struct AD *ad)
{
	int nRet = -1;
	uint8_t FileType;
	uint16_t TagID;
	uint16_t flags;
	uint32_t L_EA, L_AD;
	unsigned int p;

	UDFDescriptor( data, &TagID );
	if (TagID != 266)
	{
		//printf("Not ExtFileEntry!!!\n");
		return nRet;
	}
	UDFICB( &data[ 16 ], &FileType, &flags );

	ad->Length = GETN4( 60 ); /* Really 8 bytes a 56 */
	ad->Flags = 0;
	ad->Location = 0; /* what should we put here?  */
	ad->Partition = partition->Number; /* use number of current partition */

	L_EA = GETN4( 208 );
	L_AD = GETN4( 212 );

	if (216 + L_EA + L_AD > DVD_VIDEO_LB_LEN)
		return nRet;

	p = 216 + L_EA;
	while( p < 216 + L_EA + L_AD ) {
		switch( flags & 0x0007 ) {
			case 0:
				UDFShortAD( &data[ p ], ad, partition );
				p += 8;
				break;
			case 1:
				UDFLongAD( &data[ p ], ad );
				p += 16;
				break;
			default:
				p += L_AD;
				break;
		}
	}

	return nRet;
}
#endif

/**
 * Looks for partition on the disc.  Returns 1 if partition found, 0 on error.
 *   partnum: Number of the partition, starting at 0.
 *   part: structure to fill with the partition information
 */
static int UDFFindPartition( UDF_DATA *device, int partnum,
                             struct Partition *part )
{
  uint8_t LogBlock_base[ DVD_VIDEO_LB_LEN + 2048 ];
  uint8_t *LogBlock = (uint8_t *)(((uintptr_t)LogBlock_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum, MVDS_location, MVDS_length;
  uint16_t TagID;
  int i, volvalid;
  struct avdp_t avdp;
  uint8_t file_type;

  if(!UDFGetAVDP(device, &avdp))
    return 0;

  /* Main volume descriptor */
  MVDS_location = avdp.mvds.location;
  MVDS_length = avdp.mvds.length;

  part->valid = 0;
  volvalid = 0;
  //part->VolumeDesc[ 0 ] = '\0';
  i = 1;
  do {
    /* Find Volume Descriptor */
    lbnum = MVDS_location;
    do {

      if( DVDReadLBUDF( device, lbnum++, 1, LogBlock, 0 ) <= 0 ) {
		  return 0;
        TagID = 0;
	  } else
        UDFDescriptor( LogBlock, &TagID );

      if( ( TagID == 5 ) && ( !part->valid ) ) {
        /* Partition Descriptor */
        UDFPartition( LogBlock, &part->Flags, &part->Number,
                      part->Contents, &part->Start, &part->Length );
        part->valid = ( partnum == part->Number );
      } else if( ( TagID == 6 ) && ( !volvalid ) ) {
        /* Logical Volume Descriptor */
        if( UDFLogVolume( LogBlock, &part->vol) ) {
          /* TODO: sector size wrong! */
        } else
          volvalid = 1;
      }

    } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
               / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
             && ( ( !part->valid ) || ( !volvalid ) ) );

    if( ( !part->valid) || ( !volvalid ) ) {
      /* Backup volume descriptor */
      MVDS_location = avdp.mvds.location;
      MVDS_length = avdp.mvds.length;
    }
  } while( i-- && ( ( !part->valid ) || ( !volvalid ) ) );

  /* load metadata for udf 2.50 */
  if (volvalid == 1)
  {
	  struct PartitionMaps *Maps;
	  for (i = 0; i < (int)part->vol.MapNum; i++)
	  {
		  Maps = &part->vol.Maps[i];
		  if (Maps->MapType == UDF_METADATA_MAP25)
		  {
			  /* metadta file location */
			  //printf("metadata at partition: %d loc: %d\n", Maps->PartitionNum, Maps->mdata.meta_file_loc);
			  lbnum = UDFGetBlock(part, Maps->PartitionNum, Maps->mdata.meta_file_loc);
			  if( DVDReadLBUDF( device, lbnum, 1, LogBlock, 0 ) <= 0 )
			  {
			  } else {
				  UDFDescriptor( LogBlock, &TagID );
				  if (TagID == 266)
				  {
					  UDFExtFileEntry(LogBlock, &file_type, part, &Maps->meta_ad);
				  }
				  //UDFExtFileEntry(LogBlock, part, &Maps->meta_ad);
				  //printf("ad: %u, %u, %x, %u\n", Maps->meta_ad.Location, Maps->meta_ad.Length, Maps->meta_ad.Flags, Maps->meta_ad.Partition);
			  }
			  //printf("mirror at partition: %d loc: %d\n", Maps->PartitionNum, Maps->mdata.mirror_file_loc);
			  /*
			  if( DVDReadLBUDF( device, part->Start + Maps->mdata.mirror_file_loc, 1, LogBlock, 0 ) <= 0 )
			  {
			  } else {
				  printf("%02x %02x %02x %02x\n", LogBlock[0], LogBlock[1], LogBlock[2], LogBlock[3]);
			  }
			  */
		  }
	  }

	  // use file set descriptor to find root icb
	  /*
	  lbnum = UDFGetBlock(part, part->vol.FSD.Partition, part->vol.FSD.Location);
	  if( DVDReadLBUDF( device, lbnum, 1, LogBlock, 0 ) <= 0 )
	  {
	  } else {
		  printf("%02x %02x\n", LogBlock[0], LogBlock[1]);
	  }
	  */
  }

  /* We only care for the partition, not the volume */
  return part->valid;
}

uint32_t BDUDFFindFile( UDF_FILE *device, const char *filename,
                      uint64_t *filesize )
{
  uint8_t LogBlock_base[ DVD_VIDEO_LB_LEN + 2048 ];
  uint8_t *LogBlock = (uint8_t *)(((uintptr_t)LogBlock_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum;
  uint16_t TagID;
  struct Partition partition;
  struct AD RootICB, File, ICB;
  char tokenline[ MAX_UDF_FILE_NAME_LEN ];
  char *token;
  uint8_t filetype;

  *filesize = 0;
  tokenline[0] = '\0';
  strncat(tokenline, filename, MAX_UDF_FILE_NAME_LEN - 1);
  memset(&ICB, 0, sizeof(struct AD));
  memset(&File, 0, sizeof(struct AD));
  memset(&RootICB, 0, sizeof(struct AD));
  memset(&partition, 0, sizeof(struct Partition));

  if(!(GetBDUDFCache(device->udf_data, PartitionCache, 0, &partition) &&
       GetBDUDFCache(device->udf_data, RootICBCache, 0, &RootICB))) {
    /* Find partition, 0 is the standard location for DVD Video.*/
    if( !UDFFindPartition( device->udf_data, 0, &partition ) )
	{
		UDFFreePartition(&partition);
		return 0;
	}

    /* Find root dir ICB */
    //lbnum = partition.Start;
  lbnum = UDFGetBlock(&partition, partition.vol.FSD.Partition, partition.vol.FSD.Location);
    do {
      if( DVDReadLBUDF( device->udf_data, lbnum++, 1, LogBlock, 0 ) <= 0 )
        TagID = 0;
      else
        UDFDescriptor( LogBlock, &TagID );

      /* File Set Descriptor */
      if( TagID == 256 )  /* File Set Descriptor */
	  {
        UDFLongAD( &LogBlock[ 400 ], &RootICB );
		partition.Number = RootICB.Partition;
	  }
    } while( ( lbnum < partition.Start + partition.Length )
             && ( TagID != 8 ) && ( TagID != 256 ) );

    SetUDFCache(device->udf_data, PartitionCache, 0, &partition);

    /* Sanity checks. */
    if( TagID != 256 )
      return 0;
    //if( RootICB.Partition != 0 )
    //  return 0;
    SetUDFCache(device->udf_data, RootICBCache, 0, &RootICB);
  }

  /* Find root dir */
  if( !UDFMapICB( device->udf_data, RootICB, &filetype, &partition, &File ) )
    return 0;
  if( filetype != 4 )
    return 0;  /* Root dir should be dir */
  {
    int cache_file_info = 0;
    /* Tokenize filepath */
    token = strtok(tokenline, "/");

    while( token != NULL ) {
      if( !UDFScanDir( device->udf_data, File, token, &partition, &ICB,
                       cache_file_info))
        return 0;
      if( !UDFMapICB( device->udf_data, ICB, &filetype, &partition, &File ) )
        return 0;
      //if(!strcmp(token, "BDMV"))
      //  cache_file_info = 1;
      token = strtok( NULL, "/" );
    }
  }

  /* Sanity check. */
  //if( File.Partition != 0 )
  //  return 0;
  *filesize = File.Length;
  /* Hack to not return partition.Start for empty files. */
  if( !File.Location )
    return 0;
  else
    return UDFGetBlock(&partition, File.Partition, File.Location);
    //return partition.Start + File.Location;
}

uint32_t BDUDFDirFile( UDF_DIR *device, const char *filename)
{
  uint8_t LogBlock_base[ DVD_VIDEO_LB_LEN + 2048 ];
  uint8_t *LogBlock = (uint8_t *)(((uintptr_t)LogBlock_base & ~((uintptr_t)2047)) + 2048);
  uint32_t lbnum;
  uint16_t TagID;
  struct Partition partition;
  struct AD RootICB, File, ICB;
  char tokenline[ MAX_UDF_FILE_NAME_LEN ];
  char *token;
  uint8_t filetype;

  tokenline[0] = '\0';
  strncat(tokenline, filename, MAX_UDF_FILE_NAME_LEN - 1);
  memset(&ICB, 0, sizeof(struct AD));
  memset(&File, 0, sizeof(struct AD));
  memset(&RootICB, 0, sizeof(struct AD));
  memset(&partition, 0, sizeof(struct Partition));

  if(!(GetBDUDFCache(device->udf_data, PartitionCache, 0, &partition) &&
       GetBDUDFCache(device->udf_data, RootICBCache, 0, &RootICB))) {
    /* Find partition, 0 is the standard location for DVD Video.*/
    if( !UDFFindPartition( device->udf_data, 0, &partition ) )
	{
		UDFFreePartition(&partition);
		return 0;
	}

    /* Find root dir ICB */
    //lbnum = partition.Start;
  lbnum = UDFGetBlock(&partition, partition.vol.FSD.Partition, partition.vol.FSD.Location);
    do {
      if( DVDReadLBUDF( device->udf_data, lbnum++, 1, LogBlock, 0 ) <= 0 )
        TagID = 0;
      else
        UDFDescriptor( LogBlock, &TagID );

      /* File Set Descriptor */
      if( TagID == 256 )  /* File Set Descriptor */
	  {
        UDFLongAD( &LogBlock[ 400 ], &RootICB );
		partition.Number = RootICB.Partition;
	  }
    } while( ( lbnum < partition.Start + partition.Length )
             && ( TagID != 8 ) && ( TagID != 256 ) );

    SetUDFCache(device->udf_data, PartitionCache, 0, &partition);

    /* Sanity checks. */
    if( TagID != 256 )
      return 0;
    //if( RootICB.Partition != 0 )
    //  return 0;
    SetUDFCache(device->udf_data, RootICBCache, 0, &RootICB);
  }

  /* Find root dir */
  if( !UDFMapICB( device->udf_data, RootICB, &filetype, &partition, &File ) )
    return 0;
  if( filetype != 4 )
    return 0;  /* Root dir should be dir */
  {
    /* Tokenize filepath */
    token = strtok(tokenline, "/");

    while( token != NULL ) {
      if( !UDFScanDir( device->udf_data, File, token, &partition, &ICB, 0) )
        return 0;
	  //printf("icb: part: %u, block: %u\n", ICB.Partition, ICB.Location);
      if( !UDFMapICB( device->udf_data, ICB, &filetype, &partition, &File ) )
        return 0;
	  //printf("file: part: %u, block: %u\n", File.Partition, File.Location);
      token = strtok( NULL, "/" );
    }
	if( UDFCacheDir( device, File, &partition, &ICB) )
	{
		return 1;
	}
  }

  return 0;
}

#if 0
/**
 * Gets a Descriptor .
 * Returns 1 if descriptor found, 0 on error.
 * id, tagid of descriptor
 * bufsize, size of BlockBuf (must be >= DVD_VIDEO_LB_LEN).
 */
static int UDFGetDescriptor( UDF_DATA *device, int id,
                             uint8_t *descriptor, int bufsize)
{
  uint32_t lbnum, MVDS_location, MVDS_length;
  struct avdp_t avdp;
  uint16_t TagID;
  uint32_t lastsector;
  int i, terminate;
  int desc_found = 0;
  /* Find Anchor */
  lastsector = 0;
  lbnum = 256;   /* Try #1, prime anchor */
  terminate = 0;
  if(bufsize < DVD_VIDEO_LB_LEN)
    return 0;

  if(!UDFGetAVDP(device, &avdp))
    return 0;

  /* Main volume descriptor */
  MVDS_location = avdp.mvds.location;
  MVDS_length = avdp.mvds.length;

  i = 1;
  do {
    /* Find  Descriptor */
    lbnum = MVDS_location;
    do {
      if( DVDReadLBUDF( device, lbnum++, 1, descriptor, 0 ) <= 0 )
        TagID = 0;
      else
        UDFDescriptor( descriptor, &TagID );
      if( (TagID == id) && ( !desc_found ) )
        /* Descriptor */
        desc_found = 1;
    } while( ( lbnum <= MVDS_location + ( MVDS_length - 1 )
               / DVD_VIDEO_LB_LEN ) && ( TagID != 8 )
             && ( !desc_found) );

    if( !desc_found ) {
      /* Backup volume descriptor */
      MVDS_location = avdp.rvds.location;
      MVDS_length = avdp.rvds.length;
    }
  } while( i-- && ( !desc_found )  );

  return desc_found;
}

static int UDFGetPVD(UDF_FILE *device, struct pvd_t *pvd)
{
  uint8_t pvd_buf_base[DVD_VIDEO_LB_LEN + 2048];
  uint8_t *pvd_buf = (uint8_t *)(((uintptr_t)pvd_buf_base & ~((uintptr_t)2047)) + 2048);
  if(GetBDUDFCache(device, PVDCache, 0, pvd))
    return 1;

  if(!UDFGetDescriptor( device, 1, pvd_buf, sizeof(pvd_buf)))
    return 0;

  memcpy(pvd->VolumeIdentifier, &pvd_buf[24], 32);
  memcpy(pvd->VolumeSetIdentifier, &pvd_buf[72], 128);
  SetUDFCache(device, PVDCache, 0, pvd);
  return 1;
}

/**
 * Gets the Volume Identifier string, in 8bit unicode (latin-1)
 * volid, place to put the string
 * volid_size, size of the buffer volid points to
 * returns the size of buffer needed for all data
 */
int UDFGetVolumeIdentifier(UDF_FILE *device, char *volid,
                           unsigned int volid_size)
{
  struct pvd_t pvd;
  unsigned int volid_len;

  /* get primary volume descriptor */
  if(!UDFGetPVD(device, &pvd))
    return 0;

  volid_len = pvd.VolumeIdentifier[31];
  if(volid_len > 31)
    /* this field is only 32 bytes something is wrong */
    volid_len = 31;
  if(volid_size > volid_len)
    volid_size = volid_len;
  Unicodedecode(pvd.VolumeIdentifier, volid_size, volid);

  return volid_len;
}

/**
 * Gets the Volume Set Identifier, as a 128-byte dstring (not decoded)
 * WARNING This is not a null terminated string
 * volsetid, place to put the data
 * volsetid_size, size of the buffer volsetid points to
 * the buffer should be >=128 bytes to store the whole volumesetidentifier
 * returns the size of the available volsetid information (128)
 * or 0 on error
 */
int UDFGetVolumeSetIdentifier(UDF_FILE *device, uint8_t *volsetid,
                              unsigned int volsetid_size)
{
  struct pvd_t pvd;

  /* get primary volume descriptor */
  if(!UDFGetPVD(device, &pvd))
    return 0;


  if(volsetid_size > 128)
    volsetid_size = 128;

  memcpy(volsetid, pvd.VolumeSetIdentifier, volsetid_size);

  return 128;
}
#endif

