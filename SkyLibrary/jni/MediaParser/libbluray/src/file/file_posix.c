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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

//#define BLURAY_DIO

#ifdef BLURAY_DIO
static int has_m2ts_ext(char *fn)
{
	char *ptr = strrchr(fn, '.');
	if(ptr == NULL) return 0;
	ptr++;
	if(strcasecmp(ptr, "m2ts")) return 0;
	return 1;
}

static int enable_dio(int fd)
{
    #define O_DIRECT 0200000
    int flags = fcntl(fd, F_GETFL) | O_DIRECT;
    if(fcntl(fd, F_SETFL, flags) == -1)
	printf("%s: failed to enable direct I/O\n", __func__);
    else
	printf("%s: enable direct I/O\n", __func__);
}
#endif

static void file_close_linux(BD_FILE_H *file)
{
    if (file) {
#ifdef BLURAY_DIO
	close(((struct finfo_t *)file->internal)->fd);
	X_FREE(file->internal);
#else
        fclose((FILE *)file->internal);
#endif
        DEBUG(DBG_FILE, "Closed LINUX file (%p)\n", file);
        //printf("Closed LINUX file (%p)\n", file);

        X_FREE(file);
    }
}

static int64_t file_seek_linux(BD_FILE_H *file, int64_t offset, int32_t origin)
{
#ifdef BLURAY_DIO
    struct finfo_t *finfo = file->internal;
    finfo->pos = lseek(finfo->fd, offset, origin);
    //printf("%s: seek to 0x%llx (%p)\n", __func__, file->pos, file);
    return finfo->pos;
#else
#if defined(_WIN32)
    return fseeko64((FILE *)file->internal, offset, origin);
#else
    return fseeko((FILE *)file->internal, offset, origin);
#endif
#endif
}

static int64_t file_tell_linux(BD_FILE_H *file)
{
#ifdef BLURAY_DIO
    //printf("%s: current pos 0x%llx (%p)\n", __func__, file->pos, file);
    return ((struct finfo_t *)file->internal)->pos;
#else
#if defined(_WIN32)
    return ftello64((FILE *)file->internal);
#else
    return ftello((FILE *)file->internal);
#endif
#endif
}

static int file_eof_linux(BD_FILE_H *file)
{
#ifdef BLURAY_DIO
    return ((struct finfo_t *)file->internal)->eof;
#else
    return feof((FILE *)file->internal);
#endif
}

static int64_t file_read_linux(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
#ifdef BLURAY_DIO
    struct finfo_t *finfo = file->internal;
    ssize_t ret = read(finfo->fd, buf, size);
    //printf("buf %p size 0x%llx ret %d errno %d\n", buf, size, ret, errno);
    switch(ret){
    case -1:
	printf("%s: read 0x%llx bytes failed %d(%s)\n", __func__, size, errno, strerror(errno));
	break;
    case 0:
	printf("%s: eof at pos 0x%llx\n", __func__, finfo->pos);
	finfo->eof = 1;
	break;
    default:
	finfo->pos += ret;
	break;
    }
    return ret;
#else
    return fread(buf, 1, size, (FILE *)file->internal);
#endif
}

static int64_t file_write_linux(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
#ifdef BLURAY_DIO
    return write(((struct finfo_t *)file->internal)->fd, buf, size);
#else
    return fwrite(buf, 1, size, (FILE *)file->internal);
#endif
}

BD_FILE_H *file_open_linux(const char* filename, const char *mode, void *data)
{
    BD_FILE_H *file = malloc(sizeof(BD_FILE_H));
#ifdef BLURAY_DIO
    int fd;
    struct finfo_t *finfo = malloc(sizeof(struct finfo_t));
    bzero(finfo, sizeof(struct finfo_t));
    file->internal = finfo;
#else
    FILE *fp = NULL;
#endif

    DEBUG(DBG_FILE, "Opening LINUX file %s... (%p)\n", filename, file);
    //printf("Opening LINUX file %s... (%p)\n", filename, file);
    file->close = file_close_linux;
    file->seek = file_seek_linux;
    file->read = file_read_linux;
    file->write = file_write_linux;
    file->tell = file_tell_linux;
    file->eof = file_eof_linux;
#ifdef BLURAY_DIO
    finfo->m2ts = has_m2ts_ext(filename);
    //printf("%s is %sm2ts\n", filename, file->m2ts ? "" : "not ");

    fd = open(filename, O_RDONLY);
    if(fd >= 0){
        if(finfo->m2ts){
	    enable_dio(fd);
        }
        finfo->fd = fd;
        return file;
    }
#else
    if ((fp = fopen(filename, mode))) {
        file->internal = fp;
        return file;
    }
#endif
    DEBUG(DBG_FILE, "Error opening file! (%p)\n", file);
    //printf("Error opening file! (%p)\n", file);

    X_FREE(file);

    return NULL;
}

//BD_FILE_H* (*file_open)(const char* filename, const char *mode) = file_open_linux;
