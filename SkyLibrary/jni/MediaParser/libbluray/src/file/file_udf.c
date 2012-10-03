/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "file.h"
#include "util/macro.h"
#include "util/logging.h"
#include "libbluray/bluray.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

static void file_close_udf(BD_FILE_H *file)
{
    if (file) {
		if (file->internal != NULL)
			free(file->internal);

		file->internal = NULL;

        DEBUG(DBG_FILE, "Closed UDF file (%p)\n", file);

        X_FREE(file);
    }
}

static int64_t file_seek_udf(BD_FILE_H *file, int64_t offset, int32_t origin)
{
	UDF_FILE *udf_file = (UDF_FILE *)file->internal;
	// maybe do more range check
	switch (origin)
	{
		case SEEK_SET:
			udf_file->seek_pos = lseek64(udf_file->udf_data->fd, offset + udf_file->lb_start, SEEK_SET);
			break;
		case SEEK_END:
			udf_file->seek_pos = lseek64(udf_file->udf_data->fd, offset + udf_file->lb_end, SEEK_SET);
			break;
		case SEEK_CUR:
			udf_file->seek_pos = lseek64(udf_file->udf_data->fd, offset + udf_file->seek_pos, SEEK_SET);
			break;
	}
	return udf_file->seek_pos - udf_file->lb_start;
}

static int64_t file_tell_udf(BD_FILE_H *file)
{
	UDF_FILE *udf_file = (UDF_FILE *)file->internal;
	return udf_file->seek_pos - udf_file->lb_start;
}

static int file_eof_udf(BD_FILE_H *file)
{
	UDF_FILE *udf_file = (UDF_FILE *)file->internal;
    return (udf_file->seek_pos >= udf_file->lb_end)? 1 : 0;
}

static int64_t file_read_udf(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
	ssize_t ret;
	UDF_FILE *udf_file = (UDF_FILE *)file->internal;
	lseek64(udf_file->udf_data->fd, udf_file->seek_pos, SEEK_SET);
    ret = read(udf_file->udf_data->fd, buf, size);
	if (ret > 0)
		udf_file->seek_pos += ret;
	return ret;
}

static int64_t file_write_udf(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
	if ((file == NULL) || (buf == NULL) || (size == 0))
		return -1;
    return -1;
}

BD_FILE_H *file_open_udf(const char* filename, const char *mode, void *data)
{
	uint64_t ret;
	uint64_t file_size;
    BD_FILE_H *file = malloc(sizeof(BD_FILE_H));
	UDF_FILE *udf_info = calloc(1, sizeof(UDF_FILE));
	UDF_DATA *udf_data = data;

    DEBUG(DBG_FILE, "Opening UDF file %s... (%p)\n", filename, file);
    file->close = file_close_udf;
    file->seek = file_seek_udf;
    file->read = file_read_udf;
    file->write = file_write_udf;
    file->tell = file_tell_udf;
    file->eof = file_eof_udf;

    if ((filename != NULL) && (mode != NULL) && udf_data && (udf_data->fd > 0)) {
		file->internal = (void *)udf_info;
		udf_info->udf_data = udf_data;

		ret = BDUDFFindFile(udf_info, filename, &file_size);

		if (ret > 0)
		{
			udf_info->lb_start = ret * DVD_VIDEO_LB_LEN;
			udf_info->lb_end = udf_info->lb_start + file_size;
			udf_info->seek_pos = udf_info->lb_start;
			return file;
		}
    }

    DEBUG(DBG_FILE, "Error opening file! (%p)\n", file);

    X_FREE(udf_info);
    X_FREE(file);

    return NULL;
}

static void dir_close_udf(BD_DIR_H *dir)
{
	UDF_DIR *udf_info = NULL;
	if (dir == NULL)
		return;
    if (dir) {
		if (dir->internal != NULL)
		{
			udf_info = (UDF_DIR *)dir->internal;
			udf_info->aloc_num = 0;
			udf_info->file_num = 0;
			udf_info->dir_pos = 0;
			if (udf_info->file_name != NULL)
			{
				free(udf_info->file_name);
				udf_info->file_name = NULL;
			}
			free(dir->internal);
		}
        dir->internal = NULL;

        DEBUG(DBG_DIR, "Closed UDF dir (%p)\n", dir);

        X_FREE(dir);
    }
}

static int dir_read_udf(BD_DIR_H *dir, BD_DIRENT *entry)
{
	UDF_DIR *udf_file = NULL;
    int result = -1;

	if (dir == NULL)
		return result;
	if (dir->internal == NULL)
		return result;

	udf_file = (UDF_DIR *)dir->internal;
	if (udf_file->file_num > 0)
	{
		if (udf_file->dir_pos + 1 > udf_file->file_num)
			return 1;
		strncpy(entry->d_name, (const char *)(udf_file->file_name + udf_file->dir_pos), 256);
		udf_file->dir_pos++;
	}
	else
	{
		return 1;
	}
    return 0;
}

BD_DIR_H *dir_open_udf(const char* dirname, void *data)
{
    BD_DIR_H *dir = malloc(sizeof(BD_DIR_H));
	UDF_DIR *udf_info = calloc(1, sizeof(UDF_DIR));
	UDF_DATA *udf_data = data;

    DEBUG(DBG_DIR, "Opening UDF dir %s... (%p)\n", dirname, dir);
    dir->close = dir_close_udf;
    dir->read = dir_read_udf;

    if (udf_data && (udf_data->fd > 0)) {
        dir->internal = udf_info;
		udf_info->udf_data = udf_data;
		udf_info->aloc_num = 0;
		udf_info->file_num = 0;
		udf_info->dir_pos = 0;
		udf_info->file_name = NULL;
		if (BDUDFDirFile( udf_info, dirname) == 1)
		{
			return dir;
		}
	}

    DEBUG(DBG_DIR, "Error opening dir! (%p)\n", dir);

    X_FREE(dir);
    X_FREE(udf_info);

    return NULL;
}

