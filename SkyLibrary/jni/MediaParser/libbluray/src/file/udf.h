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

#ifndef LIBBLURAY_FILE_UDF_H
#define LIBBLURAY_FILE_UDF_H

#define DVD_VIDEO_LB_LEN 2048
#define MAX_UDF_FILE_NAME_LEN 2048

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int fd;

	/* Filesystem cache */
	int udfcache_level; /* 0 - turned off, 1 - on */
	void *udfcache;
} UDF_DATA;

typedef struct {
	UDF_DATA *udf_data;

	uint64_t lb_start;
	uint64_t lb_end;
	uint64_t seek_pos;
	uint64_t file_size;
} UDF_FILE;

typedef struct {
	UDF_DATA *udf_data;

	/* dir operation */
	uint32_t aloc_num;
	uint32_t file_num;
	uint32_t dir_pos;
	char (*file_name)[256];
} UDF_DIR;

/**
 * Looks for a file on the UDF disc/imagefile and returns the block number
 * where it begins, or 0 if it is not found.  The filename should be an
 * absolute pathname on the UDF filesystem, starting with '/'.  For example,
 * '/VIDEO_TS/VTS_01_1.IFO'.  On success, filesize will be set to the size of
 * the file in bytes.
 */
uint32_t BDUDFFindFile( UDF_FILE *device, const char *filename, uint64_t *size );
uint32_t BDUDFDirFile( UDF_DIR *device, const char *filename);

#ifdef __cplusplus
};
#endif
#endif /* LIBBLURAY_FILE_UDF_H */
