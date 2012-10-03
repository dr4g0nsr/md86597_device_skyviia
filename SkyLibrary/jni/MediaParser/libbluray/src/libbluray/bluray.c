/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2010       hpi1
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

#include "bluray.h"
#include "register.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/strutl.h"
#include "bdnav/navigation.h"
#include "bdnav/index_parse.h"
#include "hdmv/hdmv_vm.h"
#include "file/file.h"
#ifdef DLOPEN_CRYPTO_LIBS
#include "file/dl.h"
#endif
#ifdef USING_BDJAVA
#include "bdj/bdj.h"
#endif

#ifndef DLOPEN_CRYPTO_LIBS
#include <libaacs/aacs.h>
#include <libbdplus/bdplus.h>
#endif
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>
#ifdef BLURAY_AIO
#include <aio.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef int     (*fptr_int)();
typedef int32_t (*fptr_int32)();
typedef void*   (*fptr_p_void)();

//#define BLURAY_AIO

#ifdef BLURAY_AIO
#define BUFSIZE		(192 * 5120)
#define BUFNUM		2
static int bs[BUFNUM] = { BUFSIZE, BUFSIZE };
#else
#define BUFSIZE		(192 * 32)
#endif

#define MAX_EVENTS 31  /* 2^n - 1 */
typedef struct bd_event_queue_s {
    unsigned in;  /* next free slot */
    unsigned out; /* next event */
    BD_EVENT ev[MAX_EVENTS];
} BD_EVENT_QUEUE;

typedef enum {
    title_undef = 0,
    title_hdmv,
    title_bdj,
} BD_TITLE_TYPE;

typedef struct {
    /* current clip */
    NAV_CLIP       *clip;
    BD_FILE_H      *fp;
    uint64_t       clip_size;
    uint64_t       clip_block_pos;
    uint64_t       clip_pos;

    /* current aligned unit */
    uint32_t       int_buf_off;

#ifdef BLURAY_AIO
    struct aiocb   *aiolst;
    int            aioidx;
    off_t          aiopos;
#endif
} BD_STREAM;

struct bluray {

	int is_image;
	UDF_DATA udf_data;
    /* current disc */
    char           *device_path;
    INDX_ROOT      *index;
    NAV_TITLE_LIST *title_list;

    /* current playlist */
    NAV_TITLE      *title;
    uint32_t       title_idx;
    uint64_t       s_pos;

    /* streams */
    BD_STREAM      st0; /* main path */

    /* buffer for bd_read(): current aligned unit of main stream (st0) */
#ifdef BLURAY_AIO
    uint8_t        *int_buf;
    uint8_t        *realbuf;
#else
    uint8_t        int_buf[BUFSIZE];
#endif

    /* seamless angle change request */
    int            seamless_angle_change;
    uint32_t       angle_change_pkt;
    uint32_t       angle_change_time;
    unsigned       request_angle;

    /* chapter tracking */
    uint32_t       next_chapter_start;

    /* aacs */
#ifdef DLOPEN_CRYPTO_LIBS
    void           *h_libaacs;   // library handle
#endif
    void           *aacs;
    fptr_int       libaacs_decrypt_unit;

    /* BD+ */
#ifdef DLOPEN_CRYPTO_LIBS
    void           *h_libbdplus; // library handle
#endif
    void           *bdplus;
    fptr_int32     bdplus_seek;
    fptr_int32     bdplus_fixup;

    /* player state */
    BD_REGISTERS   *regs;       // player registers
    BD_EVENT_QUEUE *event_queue; // navigation mode event queue
    BD_TITLE_TYPE  title_type;  // type of current title (in navigation mode)

    HDMV_VM        *hdmv_vm;
    uint8_t        hdmv_suspended;

    void           *bdjava;
};

#ifdef DLOPEN_CRYPTO_LIBS
#    define DL_CALL(lib,func,param,...)             \
     do {                                           \
          fptr_p_void fptr = dl_dlsym(lib, #func);  \
          if (fptr) {                               \
              fptr(param, ##__VA_ARGS__);           \
          }                                         \
      } while (0)
#else
#    define DL_CALL(lib,func,param,...)         \
     func (param, ##__VA_ARGS__)
#endif

/*
 * Navigation mode event queue
 */

static void _init_event_queue(BLURAY *bd)
{
    if (!bd->event_queue) {
        bd->event_queue = calloc(1, sizeof(struct bd_event_queue_s));
    } else {
        memset(bd->event_queue, 0, sizeof(struct bd_event_queue_s));
    }
}

static int _get_event(BLURAY *bd, BD_EVENT *ev)
{
    struct bd_event_queue_s *eq = bd->event_queue;

    if (eq) {
        if (eq->in != eq->out) {
            *ev = eq->ev[eq->out];
            eq->out = (eq->out + 1) & MAX_EVENTS;
            return 1;
        }
    }

    ev->event = BD_EVENT_NONE;

    return 0;
}

static int _queue_event(BLURAY *bd, BD_EVENT ev)
{
    struct bd_event_queue_s *eq = bd->event_queue;

    if (eq) {
        unsigned new_in = (eq->in + 1) & MAX_EVENTS;

        if (new_in != eq->out) {
            eq->ev[eq->in] = ev;
            eq->in = new_in;
            return 0;
        }

        DEBUG(DBG_BLURAY|DBG_CRIT, "_queue_event(%d, %d): queue overflow !\n", ev.event, ev.param);
    }

    return -1;
}

/*
 * clip access (BD_STREAM)
 */

static void _close_m2ts(BD_STREAM *st)
{
    if (st->fp != NULL) {
        file_close(st->fp);
        st->fp = NULL;
    }
}

#ifdef BLURAY_AIO
static void file_aio_enable(BLURAY *bd)
{
	int i, ret;
	uint8_t *nextbuf;
	BD_STREAM *st = &bd->st0;
	BD_FILE_H *fp = st->fp;
	struct finfo_t *finfo = fp->internal;

	printf("enable aio\n");

	st->aiopos = st->clip_block_pos;
	nextbuf = bd->realbuf;
	for(i=0; i<BUFNUM; i++){
		st->aiolst[i].aio_fildes = finfo->fd;
		st->aiolst[i].aio_buf = nextbuf;
		st->aiolst[i].aio_nbytes = bs[i];
		if(i != 0){ // 1st buffer has already been filled, skip it
			st->aiolst[i].aio_offset = st->aiopos;
			st->aiopos += bs[i];
			//printf("%s:%d starts new aio    idx %d buf %p ofs 0x%llx size 0x%x\n", __func__, __LINE__, i, st->aiolst[i].aio_buf, st->aiolst[i].aio_offset, st->aiolst[i].aio_nbytes);
			ret = aio_read(&st->aiolst[i]);
			if(ret < 0) perror("aio_read");
		}
		nextbuf += bs[i];
	}

	st->aioidx = 0;
}

static int file_aio_read(BLURAY *bd)
{
	int i, ret, size;
	char *nextbuf;
	BD_STREAM *st = &bd->st0;
	BD_FILE_H *fp = st->fp;
	struct finfo_t *finfo = fp->internal;

	//printf("%s:%d pos 0x%llx buf_pos 0x%x buf_len 0x%x\n", __func__, __LINE__, s->pos, s->buf_pos, s->buf_len);

	/*
	if(s->buf_len == 0){
		// cancel all asynchronous I/O
		if(aio_cancel(s->fd, NULL) == AIO_NOTCANCELED){
			for(i=0; i<nbuf; i++){
				while(aio_error(&s->aiolst[i]) == EINPROGRESS){
					//printf("%s:%d waiting for aio cancel finished idx %d\n", __func__, __LINE__, i);
					usleep(1);
				}
			}
		}

		// initialize once
		s->aiopos = s->pos;
		nextbuf = s->realbuf;
		for(i=0; i<nbuf; i++){
			s->aiolst[i].aio_fildes = s->fd;
			s->aiolst[i].aio_buf = nextbuf;
			s->aiolst[i].aio_nbytes = bs[i];
			s->aiolst[i].aio_offset = s->aiopos;
			s->aiopos += bs[i];
			nextbuf += bs[i];
			//printf("%s:%d starts new aio    idx %d buf %p ofs 0x%llx size 0x%x\n", __func__, __LINE__, i, s->aiolst[i].aio_buf, s->aiolst[i].aio_offset, s->aiolst[i].aio_nbytes);
			ret = aio_read(&s->aiolst[i]);
			if(ret < 0) perror("aio_read");
		}
		s->aioidx = 0;
	}else{
	*/
		//start new aio
		//printf("%s:%d starts new aio    idx %d buf %p ofs 0x%llx\n", __func__, __LINE__, i, st->aiolst[i].aio_buf, st->aiolst[i].aio_offset);
		st->aiolst[st->aioidx].aio_offset = st->aiopos;
		ret = aio_read(&st->aiolst[st->aioidx]);
		if(ret < 0) perror("aio_read");
		st->aiopos += st->aiolst[st->aioidx].aio_nbytes;
		st->aioidx = (st->aioidx + 1) % BUFNUM;
	//}

	//wait buffer finish
	i=0;
	while(aio_error(&st->aiolst[st->aioidx]) == EINPROGRESS){
		//printf("%s:%d waiting for aio read finished idx %d\n", __func__, __LINE__, st->aioidx);
		usleep(1);
		i++;
	}
	//printf("%s:%d wait %d finished %d times\n", __func__, __LINE__, st->aioidx, i);

	size = aio_return(&st->aiolst[st->aioidx]);
	if(size <= 0){
		printf("%s:%d aio read error idx %d buf %p ofs 0x%llx ret %d(%s)\n", __func__, __LINE__, st->aioidx, st->aiolst[st->aioidx].aio_buf, st->aiolst[st->aioidx].aio_offset, size, strerror(errno));
		return size;
	}

	//change buffer
	//printf("%s:%d aio read finished idx %d buf %p ofs 0x%llx size 0x%x\n", __func__, __LINE__, st->aioidx, st->aiolst[st->aioidx].aio_buf, st->aiolst[st->aioidx].aio_offset, size);
	bd->int_buf = st->aiolst[st->aioidx].aio_buf;

	//dump_buf(st->pos, st->buffer, 1024);

	return size;
}
#endif

static int _open_m2ts(BLURAY *bd, BD_STREAM *st)
{
    char *f_name;

    _close_m2ts(st);

    if (bd->is_image == 0)
    {
	    f_name = str_printf("%s" DIR_SEP "BDMV" DIR_SEP "STREAM" DIR_SEP "%s",
			    bd->device_path, st->clip->name);
    } else {
	    f_name = str_printf(DIR_SEP "BDMV" DIR_SEP "STREAM" DIR_SEP "%s",
			    st->clip->name);
    }

    st->clip_pos = (uint64_t)st->clip->start_pkt * 192;
    st->clip_block_pos = (st->clip_pos / BUFSIZE) * BUFSIZE;

    if ((st->fp = file_open(f_name, "rb", (void *)&bd->udf_data))) {
        file_seek(st->fp, 0, SEEK_END);
        if ((st->clip_size = file_tell(st->fp))) {
            file_seek(st->fp, st->clip_block_pos, SEEK_SET);
            st->int_buf_off = BUFSIZE;
            X_FREE(f_name);
#ifdef BLURAY_AIO
	    //printf("start_pkt 0x%x clip_pos 0x%llx  clip_block_pos 0x%llx\n", st->clip->start_pkt, st->clip_pos, st->clip_block_pos);
	    file_aio_enable(bd);
#endif
            if (bd->bdplus) {
                DL_CALL(bd->h_libbdplus, bdplus_set_title,
                        bd->bdplus, st->clip->clip_id);
            }

            if (bd->aacs) {
                uint32_t title = bd_psr_read(bd->regs, PSR_TITLE_NUMBER);
                DL_CALL(bd->h_libaacs, aacs_select_title,
                        bd->aacs, title);
            }

            if (st == &bd->st0)
                bd_psr_write(bd->regs, PSR_PLAYITEM, st->clip->ref);

            return 1;
        }

        DEBUG(DBG_BLURAY, "Clip %s empty! (%p)\n", f_name, bd);
    }

    DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open clip %s! (%p)\n",
          f_name, bd);

    X_FREE(f_name);
    return 0;
}

static int _read_block(BLURAY *bd, BD_STREAM *st, uint8_t *buf)
{
    const int len = BUFSIZE;

    if (st->fp) {
        //DEBUG(DBG_BLURAY, "Reading unit [%d bytes] at %"PRIu64"... (%p)\n", len, st->clip_block_pos, bd);
        //printf("Reading unit [%d bytes] at 0x%llx... (%p)\n", len, st->clip_block_pos, bd);

        if (len + st->clip_block_pos <= st->clip_size) {
#ifdef BLURAY_AIO
	    int read_len = file_aio_read(bd);
#else
            int read_len = file_read(st->fp, buf, len);
#endif
            if (read_len > 0) {
                if (read_len != len)
                    DEBUG(DBG_BLURAY | DBG_CRIT, "Read %d bytes at %"PRIu64" ; requested %d ! (%p)\n", read_len, st->clip_block_pos, len, bd);

                if (bd->libaacs_decrypt_unit) {
                    if (!bd->libaacs_decrypt_unit(bd->aacs, buf)) {
                        DEBUG(DBG_BLURAY, "Unable decrypt unit! (%p)\n", bd);

                        return 0;
                    } // decrypt
                } // aacs

                st->clip_block_pos += len;

                // bdplus fixup, if required.
                if (bd->bdplus_fixup && bd->bdplus) {
                    int32_t numFixes;
                    numFixes = bd->bdplus_fixup(bd->bdplus, len, buf);
#if 1
                    if (numFixes) {
                        DEBUG(DBG_BDPLUS,
                              "BDPLUS did %u fixups\n", numFixes);
                    }
#endif

                }

                DEBUG(DBG_BLURAY, "Read unit OK! (%p)\n", bd);

                return 1;
            }

            DEBUG(DBG_BLURAY | DBG_CRIT, "Read %d bytes at %"PRIu64" failed ! (%p)\n", len, st->clip_block_pos, bd);

            return 0;
        }

        DEBUG(DBG_BLURAY | DBG_CRIT, "Read past EOF ! (%p)\n", bd);

        return 0;
    }

    DEBUG(DBG_BLURAY, "No valid title selected! (%p)\n", bd);

    return 0;
}

static int64_t _seek_stream(BLURAY *bd, BD_STREAM *st,
                            NAV_CLIP *clip, uint32_t clip_pkt)
{
    if (!clip)
        return -1;

    if (!st->fp || !st->clip || clip->ref != st->clip->ref) {
        // The position is in a new clip
        st->clip = clip;
        if (!_open_m2ts(bd, st)) {
            return -1;
        }
    }

    st->clip_pos = (uint64_t)clip_pkt * 192;
    st->clip_block_pos = (st->clip_pos / BUFSIZE) * BUFSIZE;

    file_seek(st->fp, st->clip_block_pos, SEEK_SET);

    st->int_buf_off = BUFSIZE;

    return st->clip_pos;
}

/*
 * open / close
 */

static void _libaacs_close(BLURAY *bd)
{
    if (bd->aacs) {
        DL_CALL(bd->h_libaacs, aacs_close, bd->aacs);
        bd->aacs = NULL;
    }

#ifdef DLOPEN_CRYPTO_LIBS
    if (bd->h_libaacs) {
        dl_dlclose(bd->h_libaacs);
        bd->h_libaacs = NULL;
    }
#endif

    bd->libaacs_decrypt_unit = NULL;
}

static int _libaacs_open(BLURAY *bd, const char *keyfile_path)
{
    _libaacs_close(bd);

#ifdef DLOPEN_CRYPTO_LIBS
    if ((bd->h_libaacs = dl_dlopen("libaacs", "0"))) {
        DEBUG(DBG_BLURAY, "Downloaded libaacs (%p)\n", bd->h_libaacs);

        fptr_p_void fptr = dl_dlsym(bd->h_libaacs, "aacs_open");
        bd->libaacs_decrypt_unit = dl_dlsym(bd->h_libaacs, "aacs_decrypt_unit");

        if (fptr && bd->libaacs_decrypt_unit) {
            if ((bd->aacs = fptr(bd->device_path, keyfile_path))) {
                DEBUG(DBG_BLURAY, "Opened libaacs (%p)\n", bd->aacs);
                return 1;
            }
            DEBUG(DBG_BLURAY, "aacs_open() failed!\n");
        } else {
            DEBUG(DBG_BLURAY, "libaacs dlsym failed!\n");
        }
        dl_dlclose(bd->h_libaacs);
        bd->h_libaacs = NULL;

    } else {
        DEBUG(DBG_BLURAY, "libaacs not found!\n");
    }
#else
    DEBUG(DBG_BLURAY, "Using libaacs via normal linking\n");

    bd->libaacs_decrypt_unit = &aacs_decrypt_unit;

    if ((bd->aacs = aacs_open(bd->device_path, keyfile_path))) {

        DEBUG(DBG_BLURAY, "Opened libaacs (%p)\n", bd->aacs);
        return 1;
    }
    DEBUG(DBG_BLURAY, "aacs_open() failed!\n");
#endif

    bd->libaacs_decrypt_unit = NULL;

    return 0;
}

static void _libbdplus_close(BLURAY *bd)
{
    if (bd->bdplus) {
        DL_CALL(bd->h_libbdplus, bdplus_free, bd->bdplus);
        bd->bdplus = NULL;
    }

#ifdef DLOPEN_CRYPTO_LIBS
    if (bd->h_libbdplus) {
        dl_dlclose(bd->h_libbdplus);
        bd->h_libbdplus = NULL;
    }
#endif

    bd->bdplus_seek  = NULL;
    bd->bdplus_fixup = NULL;
}

static void _libbdplus_open(BLURAY *bd, const char *keyfile_path)
{
    _libbdplus_close(bd);

    // Take a quick stab to see if we want/need bdplus
    // we should fix this, and add various string functions.
    uint8_t vid[16] = {
        0xC5,0x43,0xEF,0x2A,0x15,0x0E,0x50,0xC4,0xE2,0xCA,
        0x71,0x65,0xB1,0x7C,0xA7,0xCB}; // FIXME
    BD_FILE_H *fd;
    char *tmp = NULL;
    if (bd->is_image == 0)
    {
	    tmp = str_printf("%s/BDSVM/00000.svm", bd->device_path);
    } else {
	    tmp = str_printf("/BDSVM/00000.svm");
    }
    if ((fd = file_open(tmp, "rb", (void *)&bd->udf_data))) {
        file_close(fd);

        DEBUG(DBG_BDPLUS, "attempting to load libbdplus\n");
#ifdef DLOPEN_CRYPTO_LIBS
        if ((bd->h_libbdplus = dl_dlopen("libbdplus", "0"))) {
            DEBUG(DBG_BLURAY, "Downloaded libbdplus (%p)\n", bd->h_libbdplus);

            fptr_p_void bdplus_init = dl_dlsym(bd->h_libbdplus, "bdplus_init");
            //bdplus_t *bdplus_init(path,configfile_path,*vid );
            if (bdplus_init)
                bd->bdplus = bdplus_init(bd->device_path, keyfile_path, vid);

            if (bd->bdplus) {
                // Since we will call these functions a lot, we assign them
                // now.
                bd->bdplus_seek  = dl_dlsym(bd->h_libbdplus, "bdplus_seek");
                bd->bdplus_fixup = dl_dlsym(bd->h_libbdplus, "bdplus_fixup");
            } else {
                dl_dlclose(bd->h_libbdplus);
                bd->h_libbdplus = NULL;
            }
        }
#else
        DEBUG(DBG_BLURAY,"Using libbdplus via normal linking\n");

        bd->bdplus = bdplus_init(bd->device_path, keyfile_path, vid);

        // Since we will call these functions a lot, we assign them
        // now.
        bd->bdplus_seek  = &bdplus_seek;
        bd->bdplus_fixup = &bdplus_fixup;
#endif
    } // file_open
    X_FREE(tmp);
}

static int _index_open(BLURAY *bd)
{
    char *file;

    if (bd->is_image == 0)
    {
	    file = str_printf("%s/BDMV/index.bdmv", bd->device_path);
    } else {
	    file = str_printf("/BDMV/index.bdmv");
    }
    bd->index = indx_parse((void *)&bd->udf_data, file);

    X_FREE(file);

    return !!bd->index;
}

int bd_open_udf(BLURAY *bd, const char *file_path)
{
	int nRet = 0;
	int fd;
	UDF_DATA *udf_data;

	if (bd == NULL)
		return nRet;
	udf_data = &bd->udf_data;
	fd = open(file_path, O_RDONLY);
	if (fd == -1)
	{
		return nRet;
	}
	udf_data->fd = fd;
	udf_data->udfcache_level = 1;
	udf_data->udfcache = NULL;

	nRet = 1;
	return nRet;
}

void bd_close_udf(BLURAY *bd)
{
	if ((bd->is_image == 1) && (bd->udf_data.fd > 0))
	{
		close(bd->udf_data.fd);
		bd->udf_data.fd = 0;
		if (bd->udf_data.udfcache)
			FreeBDUDFCache( bd->udf_data.udfcache );
	}
}

BLURAY *bd_open(const char* device_path, const char* keyfile_path)
{
    BLURAY *bd = calloc(1, sizeof(BLURAY));
	struct stat fileinfo;
	int ret;
#ifdef BLURAY_AIO
    bd->st0.aiolst = calloc(BUFNUM, sizeof(struct aiocb));
    bd->int_buf = bd->realbuf = memalign(512, BUFSIZE * BUFNUM);
#endif

    if (device_path) {

        bd->device_path = (char*)malloc(strlen(device_path) + 1);
        strcpy(bd->device_path, device_path);

        _libaacs_open(bd, keyfile_path);

        _libbdplus_open(bd, keyfile_path);

		ret = stat( device_path, &fileinfo );
		if( ret < 0 ) {
			X_FREE(bd);
			DEBUG(DBG_BLURAY, "Can't stat %s\n", device_path);

			return bd;
		}

		if( S_ISBLK( fileinfo.st_mode ) ||
				S_ISCHR( fileinfo.st_mode ) ||
				S_ISREG( fileinfo.st_mode ) )
		{
			bd->is_image = 1;

			ret = bd_open_udf(bd, device_path);
			if (ret == 0)
			{
				X_FREE(bd);
				DEBUG(DBG_BLURAY, "Can't open %s\n", device_path);
				return bd;
			}
			bd_register_file(file_open_udf);
			bd_register_dir(dir_open_udf);
		}
		else 
		{
			bd->is_image = 0;
			bd_register_file(file_open_linux);
			bd_register_dir(dir_open_posix);
		}
        _index_open(bd);

        bd->regs = bd_registers_init();

        DEBUG(DBG_BLURAY, "BLURAY initialized! (%p)\n", bd);
    } else {
#ifdef BLURAY_AIO
	X_FREE(bd->int_buf);
#endif
        X_FREE(bd);

        DEBUG(DBG_BLURAY | DBG_CRIT, "No device path provided!\n");
    }

    return bd;
}

void bd_close(BLURAY *bd)
{
    bd_stop_bdj(bd);

    _libaacs_close(bd);

    _libbdplus_close(bd);

    _close_m2ts(&bd->st0);

	bd_close_udf(bd);

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
    if (bd->title != NULL) {
        nav_title_close(bd->title);
    }

    if (bd->hdmv_vm)
        hdmv_vm_free(bd->hdmv_vm);

    indx_free(bd->index);
    bd_registers_free(bd->regs);

    X_FREE(bd->event_queue);
    X_FREE(bd->device_path);

    DEBUG(DBG_BLURAY, "BLURAY destroyed! (%p)\n", bd);

#ifdef BLURAY_AIO
    X_FREE(bd->st0.aiolst);
    X_FREE(bd->realbuf);
#endif
    X_FREE(bd);
}

/*
 * seeking and current position
 */

static int64_t _seek_internal(BLURAY *bd,
                              NAV_CLIP *clip, uint32_t title_pkt, uint32_t clip_pkt)
{
    if (_seek_stream(bd, &bd->st0, clip, clip_pkt) >= 0) {

        /* update title position */
        bd->s_pos = (uint64_t)title_pkt * 192;

        /* chapter tracking */
        uint32_t current_chapter = bd_get_current_chapter(bd);
        bd->next_chapter_start = bd_chapter_pos(bd, current_chapter + 1);
        bd_psr_write(bd->regs, PSR_CHAPTER,  current_chapter + 1);

        DEBUG(DBG_BLURAY, "Seek to %"PRIu64" (%p)\n",
              bd->s_pos, bd);

        if (bd->bdplus_seek && bd->bdplus) {
            bd->bdplus_seek(bd->bdplus, bd->st0.clip_block_pos);
        }
    }

    return bd->s_pos;
}

/* _change_angle() should be used only before call to _seek_internal() ! */
static void _change_angle(BLURAY *bd)
{
    if (bd->seamless_angle_change) {
        bd->st0.clip = nav_set_angle(bd->title, bd->st0.clip, bd->request_angle, (void *)&bd->udf_data);
        bd->seamless_angle_change = 0;
        bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

        /* force re-opening .m2ts file in _seek_internal() */
        _close_m2ts(&bd->st0);
    }
}

int64_t bd_seek_time(BLURAY *bd, uint64_t tick)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    tick /= 2;

    if (tick < bd->title->duration) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_time_search(bd->title, tick, &clip_pkt, &out_pkt);

        return _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

uint64_t bd_tell_time(BLURAY *bd)
{
    uint32_t clip_pkt = 0, out_pkt = 0, out_time = 0;

    if (bd && bd->title) {
        nav_packet_search(bd->title, bd->s_pos / 192, &clip_pkt, &out_pkt, &out_time);
    }

    return ((uint64_t)out_time) * 2;
}

int64_t bd_seek_chapter(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    if (chapter < bd->title->chap_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);

        return _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

int64_t bd_chapter_pos(BLURAY *bd, unsigned chapter)
{
    uint32_t clip_pkt, out_pkt;

    if (chapter < bd->title->chap_list.count) {
        // Find the closest access unit to the requested position
        nav_chapter_search(bd->title, chapter, &clip_pkt, &out_pkt);
        return (int64_t)out_pkt * 192;
    }

    return -1;
}

uint32_t bd_get_current_chapter(BLURAY *bd)
{
    return nav_chapter_get_current(bd->st0.clip, bd->st0.clip_pos / 192);
}

int64_t bd_seek_mark(BLURAY *bd, unsigned mark)
{
    uint32_t clip_pkt, out_pkt;
    NAV_CLIP *clip;

    if (mark < bd->title->mark_list.count) {

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_mark_search(bd->title, mark, &clip_pkt, &out_pkt);

        return _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

int64_t bd_seek(BLURAY *bd, uint64_t pos)
{
    uint32_t pkt, clip_pkt, out_pkt, out_time;
    NAV_CLIP *clip;

    if (pos < (uint64_t)bd->title->packets * 192) {
        pkt = pos / 192;

        _change_angle(bd);

        // Find the closest access unit to the requested position
        clip = nav_packet_search(bd->title, pkt, &clip_pkt, &out_pkt, &out_time);

        return _seek_internal(bd, clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

uint64_t bd_get_title_size(BLURAY *bd)
{
    if (bd && bd->title) {
        return (uint64_t)bd->title->packets * 192;
    }
    return UINT64_C(0);
}

uint64_t bd_tell(BLURAY *bd)
{
    return bd ? bd->s_pos : INT64_C(0);
}

/*
 * read
 */

static int64_t _clip_seek_time(BLURAY *bd, uint64_t tick)
{
    uint32_t clip_pkt, out_pkt;

    if (tick < bd->st0.clip->out_time) {

        // Find the closest access unit to the requested position
        nav_clip_time_search(bd->st0.clip, tick, &clip_pkt, &out_pkt);

        return _seek_internal(bd, bd->st0.clip, out_pkt, clip_pkt);
    }

    return bd->s_pos;
}

int bd_read(BLURAY *bd, unsigned char *buf, int len)
{
    BD_STREAM *st = &bd->st0;
    int out_len;

    if (st->fp) {
        out_len = 0;
        DEBUG(DBG_BLURAY, "Reading [%d bytes] at %"PRIu64"... (%p)\n", len, bd->s_pos, bd);
        //printf("Reading [0x%x bytes] at 0x%llx (%p)\n", len, bd->s_pos, bd);

        while (len > 0) {
            uint32_t clip_pkt;

            unsigned int size = len;
            // Do we need to read more data?
            clip_pkt = st->clip_pos / 192;
            if (bd->seamless_angle_change) {
                if (clip_pkt >= bd->angle_change_pkt) {
                    if (clip_pkt >= st->clip->end_pkt) {
                        st->clip = nav_next_clip(bd->title, st->clip);
                        if (!_open_m2ts(bd, st)) {
                            return -1;
                        }
                        bd->s_pos = st->clip->pos;
                    } else {
                        _change_angle(bd);
                        _clip_seek_time(bd, bd->angle_change_time);
                    }
                    bd->seamless_angle_change = 0;
                } else {
                    uint64_t angle_pos;

                    angle_pos = bd->angle_change_pkt * 192;
                    if (angle_pos - st->clip_pos < size)
                    {
                        size = angle_pos - st->clip_pos;
                    }
                }
            }
            if (st->int_buf_off == BUFSIZE || clip_pkt >= st->clip->end_pkt) {

                // Do we need to get the next clip?
                if (st->clip == NULL) {
                    // We previously reached the last clip.  Nothing
                    // else to read.
                    return 0;
                }
                if (clip_pkt >= st->clip->end_pkt) {
                    st->clip = nav_next_clip(bd->title, st->clip);
                    if (st->clip == NULL) {
                        DEBUG(DBG_BLURAY, "End of title (%p)\n", bd);
                        return out_len;
                    }
                    if (!_open_m2ts(bd, st)) {
                        return -1;
                    }
                }
                if (_read_block(bd, st, bd->int_buf)) {

                    st->int_buf_off = st->clip_pos % BUFSIZE;

                } else {
                    return out_len;
                }
            }
            if (size > (unsigned int)BUFSIZE - st->int_buf_off) {
                size = BUFSIZE - st->int_buf_off;
            }
            memcpy(buf, bd->int_buf + st->int_buf_off, size);
            buf += size;
            len -= size;
            out_len += size;
            st->clip_pos += size;
            st->int_buf_off += size;
            bd->s_pos += size;
        }

        /* chapter tracking */
        if (bd->s_pos > bd->next_chapter_start) {
            uint32_t current_chapter = bd_get_current_chapter(bd);
            bd->next_chapter_start = bd_chapter_pos(bd, current_chapter + 1);
            bd_psr_write(bd->regs, PSR_CHAPTER, current_chapter + 1);
        }

        DEBUG(DBG_BLURAY, "%d bytes read OK! (%p)\n", out_len, bd);

        return out_len;
    }

    DEBUG(DBG_BLURAY, "No valid title selected! (%p)\n", bd);

    return -1;
}

/*
 * select title / angle
 */

static int _open_playlist(BLURAY *bd, const char *f_name)
{
    if (bd->title) {
        nav_title_close(bd->title);
    }

    if (bd->is_image == 1)
	    bd->title = nav_title_open(bd->device_path, f_name, (void *)&bd->udf_data);
    else
	    bd->title = nav_title_open(bd->device_path, f_name, NULL);
    if (bd->title == NULL) {
        DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s! (%p)\n",
              f_name, bd);
        return 0;
    }

    bd->seamless_angle_change = 0;
    bd->s_pos = 0;

    bd->next_chapter_start = bd_chapter_pos(bd, 1);

    bd_psr_write(bd->regs, PSR_PLAYLIST, atoi(bd->title->name));
    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);
    bd_psr_write(bd->regs, PSR_CHAPTER, 1);

    // Get the initial clip of the playlist
    bd->st0.clip = nav_next_clip(bd->title, NULL);
    if (_open_m2ts(bd, &bd->st0)) {
        DEBUG(DBG_BLURAY, "Title %s selected! (%p)\n", f_name, bd);
        return 1;
    }
    return 0;
}

int bd_select_playlist(BLURAY *bd, uint32_t playlist)
{
    char *f_name = str_printf("%05d.mpls", playlist);
    int result;

    if (bd->title_list) {
        /* update current title */
        unsigned i;
        for (i = 0; i < bd->title_list->count; i++) {
            if (playlist == bd->title_list->title_info[i].mpls_id) {
                bd->title_idx = i;
                break;
            }
        }
    }

    result = _open_playlist(bd, f_name);

    X_FREE(f_name);
    return result;
}

// Select a title for playback
// The title index is an index into the list
// established by bd_get_titles()
int bd_select_title(BLURAY *bd, uint32_t title_idx)
{
    const char *f_name;

    // Open the playlist
    if (bd->title_list == NULL) {
        DEBUG(DBG_BLURAY, "Title list not yet read! (%p)\n", bd);
        return 0;
    }
    if (bd->title_list->count <= title_idx) {
        DEBUG(DBG_BLURAY, "Invalid title index %d! (%p)\n", title_idx, bd);
        return 0;
    }

    bd->title_idx = title_idx;
    f_name = bd->title_list->title_info[title_idx].name;

    return _open_playlist(bd, f_name);
}

uint32_t bd_get_current_title(BLURAY *bd)
{
    return bd->title_idx;
}

int bd_select_angle(BLURAY *bd, unsigned angle)
{
    unsigned orig_angle;

    if (bd->title == NULL) {
        DEBUG(DBG_BLURAY, "Title not yet selected! (%p)\n", bd);
        return 0;
    }

    orig_angle = bd->title->angle;

    bd->st0.clip = nav_set_angle(bd->title, bd->st0.clip, angle, (void *)&bd->udf_data);

    if (orig_angle == bd->title->angle) {
        return 1;
    }

    bd_psr_write(bd->regs, PSR_ANGLE_NUMBER, bd->title->angle + 1);

    if (!_open_m2ts(bd, &bd->st0)) {
        DEBUG(DBG_BLURAY|DBG_CRIT, "Error selecting angle %d ! (%p)\n", angle, bd);
        return 0;
    }

    return 1;
}

unsigned bd_get_current_angle(BLURAY *bd)
{
    return bd->title->angle;
}


void bd_seamless_angle_change(BLURAY *bd, unsigned angle)
{
    uint32_t clip_pkt;

    clip_pkt = (bd->st0.clip_pos + 191) / 192;
    bd->angle_change_pkt = nav_angle_change_search(bd->st0.clip, clip_pkt,
                                                   &bd->angle_change_time);
    bd->request_angle = angle;
    bd->seamless_angle_change = 1;
}

/*
 * title lists
 */

uint32_t bd_get_titles(BLURAY *bd, uint8_t flags)
{
    if (!bd) {
        DEBUG(DBG_BLURAY | DBG_CRIT, "bd_get_titles(NULL) failed (%p)\n", bd);
        return 0;
    }

    if (bd->title_list != NULL) {
        nav_free_title_list(bd->title_list);
    }
	if (bd->is_image == 1)
		bd->title_list = nav_get_title_list(bd->device_path, flags, (void *)&bd->udf_data);
	else
		bd->title_list = nav_get_title_list(bd->device_path, flags, NULL);

    if (!bd->title_list) {
        DEBUG(DBG_BLURAY | DBG_CRIT, "nav_get_title_list(%s) failed (%p)\n", bd->device_path, bd);
        return 0;
    }

    return bd->title_list->count;
}

static void _copy_streams(BLURAY_STREAM_INFO *streams, MPLS_STREAM *si, int count)
{
    int ii;

    for (ii = 0; ii < count; ii++) {
        streams[ii].coding_type = si[ii].coding_type;
        streams[ii].format = si[ii].format;
        streams[ii].rate = si[ii].rate;
        streams[ii].char_code = si[ii].char_code;
        memcpy(streams[ii].lang, si[ii].lang, 4);
        streams[ii].pid = si[ii].pid;
    }
}

static BLURAY_TITLE_INFO* _fill_title_info(NAV_TITLE* title, uint32_t title_idx, uint32_t playlist)
{
    BLURAY_TITLE_INFO *title_info;
    unsigned int ii;

    title_info = calloc(1, sizeof(BLURAY_TITLE_INFO));
    title_info->idx = title_idx;
    title_info->playlist = playlist;
    title_info->duration = (uint64_t)title->duration * 2;
    title_info->angle_count = title->angle_count;
    title_info->chapter_count = title->chap_list.count;
    title_info->chapters = calloc(title_info->chapter_count, sizeof(BLURAY_TITLE_CHAPTER));
    for (ii = 0; ii < title_info->chapter_count; ii++) {
        title_info->chapters[ii].idx = ii;
        title_info->chapters[ii].start = (uint64_t)title->chap_list.mark[ii].title_time * 2;
        title_info->chapters[ii].duration = (uint64_t)title->chap_list.mark[ii].duration * 2;
        title_info->chapters[ii].offset = (uint64_t)title->chap_list.mark[ii].title_pkt * 192;
    }
    title_info->clip_count = title->clip_list.count;
    title_info->clips = calloc(title_info->clip_count, sizeof(BLURAY_CLIP_INFO));
    for (ii = 0; ii < title_info->clip_count; ii++) {
        MPLS_PI *pi = &title->pl->play_item[ii];
        BLURAY_CLIP_INFO *ci = &title_info->clips[ii];
        ci->video_stream_count = pi->stn.num_video;
        ci->audio_stream_count = pi->stn.num_audio;
        ci->pg_stream_count = pi->stn.num_pg + pi->stn.num_pip_pg;
        ci->ig_stream_count = pi->stn.num_ig;
        ci->sec_video_stream_count = pi->stn.num_secondary_video;
        ci->sec_audio_stream_count = pi->stn.num_secondary_audio;
        ci->video_streams = calloc(ci->video_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->audio_streams = calloc(ci->audio_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->pg_streams = calloc(ci->pg_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->ig_streams = calloc(ci->ig_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->sec_video_streams = calloc(ci->sec_video_stream_count, sizeof(BLURAY_STREAM_INFO));
        ci->sec_audio_streams = calloc(ci->sec_audio_stream_count, sizeof(BLURAY_STREAM_INFO));
        _copy_streams(ci->video_streams, pi->stn.video, ci->video_stream_count);
        _copy_streams(ci->audio_streams, pi->stn.audio, ci->audio_stream_count);
        _copy_streams(ci->pg_streams, pi->stn.pg, ci->pg_stream_count);
        _copy_streams(ci->ig_streams, pi->stn.ig, ci->ig_stream_count);
        _copy_streams(ci->sec_video_streams, pi->stn.secondary_video, ci->sec_video_stream_count);
        _copy_streams(ci->sec_audio_streams, pi->stn.secondary_audio, ci->sec_audio_stream_count);
    }

    return title_info;
}

static BLURAY_TITLE_INFO *_get_title_info(BLURAY *bd, uint32_t title_idx, uint32_t playlist, const char *mpls_name)
{
    NAV_TITLE *title;
    BLURAY_TITLE_INFO *title_info;

	if (bd->is_image == 1)
		title = nav_title_open(bd->device_path, mpls_name, (void *)&bd->udf_data);
	else
		title = nav_title_open(bd->device_path, mpls_name, NULL);
    if (title == NULL) {
        DEBUG(DBG_BLURAY | DBG_CRIT, "Unable to open title %s! (%p)\n",
              mpls_name, bd);
        return NULL;
    }

    title_info = _fill_title_info(title, title_idx, playlist);

    nav_title_close(title);
    return title_info;
}

BLURAY_TITLE_INFO* bd_get_title_info(BLURAY *bd, uint32_t title_idx)
{
    if (bd->title_list == NULL) {
        DEBUG(DBG_BLURAY, "Title list not yet read! (%p)\n", bd);
        return NULL;
    }
    if (bd->title_list->count <= title_idx) {
        DEBUG(DBG_BLURAY, "Invalid title index %d! (%p)\n", title_idx, bd);
        return NULL;
    }

    return _get_title_info(bd,
                           title_idx, bd->title_list->title_info[title_idx].mpls_id,
                           bd->title_list->title_info[title_idx].name);
}

void bd_free_title_max2_dur(BLURAY_TITLE_MAX_DUR *title)
{
	if (title == NULL)
		return;
	if (title->dur_first)
		free(title->dur_first);
	if (title->dur_second)
		free(title->dur_second);
	free(title);
}

BLURAY_TITLE_MAX_DUR * bd_get_title_max2_dur(BLURAY *bd)
{
	NAV_TITLE_LIST * title_list;
	NAV_TITLE_INFO * title_info;
	int ii;
	BLURAY_TITLE_MAX_DUR *title;

    if (bd->title_list == NULL) {
        DEBUG(DBG_BLURAY, "Title list not yet read! (%p)\n", bd);
        return NULL;
    }
	title_list = bd->title_list;
    if (title_list->title_info == NULL) {
        DEBUG(DBG_BLURAY, "Title info not yet read! (%p)\n", bd);
        return NULL;
    }
	title = calloc(sizeof(BLURAY_TITLE_MAX_DUR), 1);
	title_info = title_list->title_info;

	for (ii = 0; ii < title_list->count; ii++)
	{
		//printf("%d name: %s, title_id: %d, duration: %u, ref: %d\n", 
		//		ii, title_info->name, title_info->mpls_id, title_info->duration,
		//		title_info->ref);
		if (title_info->duration != 0)
		{
			if (title_info->duration > title->dur_first_time)
			{
				if (title->dur_first != NULL)
				{
					if(title->dur_second != NULL)
					{
						free(title->dur_second);
					}
					title->dur_second_time = title->dur_first_time;
					title->dur_second_count = title->dur_first_count;
					title->dur_second = title->dur_first;
				}
				title->dur_first = malloc(sizeof(BLURAY_TITLE_MPLS_ID));
				title->dur_first_time = title_info->duration;
				title->dur_first_count = 1;
				title->dur_first[0].title_id = ii;
				title->dur_first[0].mpls_id = title_info->mpls_id;
			}
			else if (title_info->duration == title->dur_first_time)
			{
				title->dur_first_count++;
				title->dur_first = realloc((void *)title->dur_first, \
						sizeof(BLURAY_TITLE_MPLS_ID) * title->dur_first_count);
				title->dur_first[title->dur_first_count - 1].title_id = ii;
				title->dur_first[title->dur_first_count - 1].mpls_id = title_info->mpls_id;
			}
			else if (title_info->duration > title->dur_second_time)
			{
				if (title->dur_second == NULL)
					title->dur_second = malloc(sizeof(BLURAY_TITLE_MPLS_ID));
				title->dur_second_time = title_info->duration;
				title->dur_second_count = 1;
				title->dur_second[0].title_id = ii;
				title->dur_second[0].mpls_id = title_info->mpls_id;
			}
			else if (title_info->duration == title->dur_second_time)
			{
				title->dur_second_count++;
				title->dur_second = realloc((void *)title->dur_second, \
						sizeof(BLURAY_TITLE_MPLS_ID) * title->dur_second_count);
				title->dur_second[title->dur_second_count - 1].title_id = ii;
				title->dur_second[title->dur_second_count - 1].mpls_id = title_info->mpls_id;
			}
		}

		title_info++;
	}

	if (title->dur_first_count == 0)
	{
		bd_free_title_max2_dur(title);
		title = NULL;
	}

	return title;
}

BLURAY_TITLE_INFO* bd_get_playlist_info(BLURAY *bd, uint32_t playlist)
{
    char *f_name = str_printf("%05d.mpls", playlist);
    BLURAY_TITLE_INFO *title_info;

    title_info = _get_title_info(bd, 0, playlist, f_name);

    X_FREE(f_name);

    return title_info;
}

void bd_free_title_info(BLURAY_TITLE_INFO *title_info)
{
    unsigned int ii;

    X_FREE(title_info->chapters);
    for (ii = 0; ii < title_info->clip_count; ii++) {
        X_FREE(title_info->clips[ii].video_streams);
        X_FREE(title_info->clips[ii].audio_streams);
        X_FREE(title_info->clips[ii].pg_streams);
        X_FREE(title_info->clips[ii].ig_streams);
        X_FREE(title_info->clips[ii].sec_video_streams);
        X_FREE(title_info->clips[ii].sec_audio_streams);
    }
    X_FREE(title_info->clips);
    X_FREE(title_info);
}

/*
 * player settings
 */

int bd_set_player_setting(BLURAY *bd, uint32_t idx, uint32_t value)
{
    static const struct { uint32_t idx; uint32_t  psr; } map[] = {
        { BLURAY_PLAYER_SETTING_PARENTAL,       PSR_PARENTAL },
        { BLURAY_PLAYER_SETTING_AUDIO_CAP,      PSR_AUDIO_CAP },
        { BLURAY_PLAYER_SETTING_AUDIO_LANG,     PSR_AUDIO_LANG },
        { BLURAY_PLAYER_SETTING_PG_LANG,        PSR_PG_AND_SUB_LANG },
        { BLURAY_PLAYER_SETTING_MENU_LANG,      PSR_MENU_LANG },
        { BLURAY_PLAYER_SETTING_COUNTRY_CODE,   PSR_COUNTRY },
        { BLURAY_PLAYER_SETTING_REGION_CODE,    PSR_REGION },
        { BLURAY_PLAYER_SETTING_VIDEO_CAP,      PSR_VIDEO_CAP },
        { BLURAY_PLAYER_SETTING_TEXT_CAP,       PSR_TEXT_CAP },
        { BLURAY_PLAYER_SETTING_PLAYER_PROFILE, PSR_PROFILE_VERSION },
    };

    unsigned i;

    if (idx == BLURAY_PLAYER_SETTING_PLAYER_PROFILE) {
        value = ((value & 0xf) << 16) | 0x0200;  /* version fixed to BD-RO Part 3, version 2.0 */
    }

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (idx == map[i].idx) {
            return bd_psr_setting_write(bd->regs, idx, value);
        }
    }

    return 0;
}

static uint32_t _string_to_uint(const char *s, int n)
{
    uint32_t val = 0;

    if (n > 4)
        n = 4;

    while (n--)
        val = (val << 8) | s[n];

    return val;
}

int bd_set_player_setting_str(BLURAY *bd, uint32_t idx, const char *s)
{
    switch (idx) {
        case BLURAY_PLAYER_SETTING_AUDIO_LANG:
        case BLURAY_PLAYER_SETTING_PG_LANG:
        case BLURAY_PLAYER_SETTING_MENU_LANG:
            return bd_set_player_setting(bd, idx, s ? _string_to_uint(s, 3) : 0xffffff);

        case BLURAY_PLAYER_SETTING_COUNTRY_CODE:
            return bd_set_player_setting(bd, idx, s ? _string_to_uint(s, 3) : 0xffff  );

        default:
            return 0;
    }
}

/*
 * bdj
 */

int bd_start_bdj(BLURAY *bd, const char *start_object)
{
#ifdef USING_BDJAVA
    if (bd->bdjava == NULL) {
        bd->bdjava = bdj_open(bd->device_path, start_object, bd, bd->regs);
        return 0;
    } else {
        DEBUG(DBG_BLURAY | DBG_CRIT, "BD-J is already running (%p)\n", bd);
        return -1;
    }
#else
    DEBUG(DBG_BLURAY | DBG_CRIT, "%s.bdjo: BD-J not compiled in (%p)\n", start_object, bd);
#endif
    return -1;
}

void bd_stop_bdj(BLURAY *bd)
{
    if (bd->bdjava != NULL) {
#ifdef USING_BDJAVA
        bdj_close((BDJAVA*)bd->bdjava);
#else
        DEBUG(DBG_BLURAY, "BD-J not compiled in (%p)\n", bd);
#endif
        bd->bdjava = NULL;
    }
}

/*
 * Navigation mode interface
 */

/*
 * notification events to APP
 */
static void _process_psr_event(void *handle, BD_PSR_EVENT *ev)
{
    BLURAY *bd = (BLURAY*)handle;

    DEBUG(DBG_BLURAY, "PSR event %d %d (%p)\n", ev->psr_idx, ev->new_val, bd);

    switch (ev->psr_idx) {

        /* current playback position */

        case PSR_ANGLE_NUMBER: _queue_event(bd, (BD_EVENT){BD_EVENT_ANGLE, ev->new_val}); break;
        case PSR_TITLE_NUMBER: _queue_event(bd, (BD_EVENT){BD_EVENT_TITLE, ev->new_val}); break;
        case PSR_PLAYLIST: _queue_event(bd, (BD_EVENT){BD_EVENT_PLAYLIST, ev->new_val}); break;
        case PSR_PLAYITEM: _queue_event(bd, (BD_EVENT){BD_EVENT_PLAYITEM, ev->new_val}); break;
        case PSR_CHAPTER:  _queue_event(bd, (BD_EVENT){BD_EVENT_CHAPTER,  ev->new_val}); break;

        /* Interactive Graphics */

        case PSR_SELECTED_BUTTON_ID:
            _queue_event(bd, (BD_EVENT){BD_EVENT_SELECTED_BUTTON_ID, ev->new_val});
            break;

        case PSR_MENU_PAGE_ID:
            _queue_event(bd, (BD_EVENT){BD_EVENT_MENU_PAGE_ID, ev->new_val});
            break;

        /* stream selection */

        case PSR_IG_STREAM_ID:
            _queue_event(bd, (BD_EVENT){BD_EVENT_IG_STREAM, ev->new_val});
            break;

        case PSR_PRIMARY_AUDIO_ID:
            _queue_event(bd, (BD_EVENT){BD_EVENT_AUDIO_STREAM, ev->new_val});
            break;

        case PSR_PG_STREAM:
            if ((ev->new_val & 0x80000fff) != (ev->old_val & 0x80000fff)) {
                _queue_event(bd, (BD_EVENT){BD_EVENT_PG_TEXTST,        !!(ev->new_val & 0x80000000)});
                _queue_event(bd, (BD_EVENT){BD_EVENT_PG_TEXTST_STREAM,    ev->new_val & 0xfff});
            }
            break;

        case PSR_SECONDARY_AUDIO_VIDEO:
            /* secondary video */
            if ((ev->new_val & 0x8f00ff00) != (ev->old_val & 0x8f00ff00)) {
                _queue_event(bd, (BD_EVENT){BD_EVENT_SECONDARY_VIDEO, !!(ev->new_val & 0x80000000)});
                _queue_event(bd, (BD_EVENT){BD_EVENT_SECONDARY_VIDEO_SIZE, (ev->new_val >> 24) & 0xf});
                _queue_event(bd, (BD_EVENT){BD_EVENT_SECONDARY_VIDEO_STREAM, (ev->new_val & 0xff00) >> 8});
            }
            /* secondary audio */
            if ((ev->new_val & 0x400000ff) != (ev->old_val & 0x400000ff)) {
                _queue_event(bd, (BD_EVENT){BD_EVENT_SECONDARY_AUDIO, !!(ev->new_val & 0x40000000)});
                _queue_event(bd, (BD_EVENT){BD_EVENT_SECONDARY_AUDIO_STREAM, ev->new_val & 0xff});
            }
            break;

        default:;
    }
}

static int _play_bdj(BLURAY *bd, const char *name)
{
    bd->title_type = title_bdj;

#ifdef USING_BDJAVA
    bd_stop_bdj(bd);
    return bd_start_bdj(bd, name);
#else
    DEBUG(DBG_BLURAY|DBG_CRIT, "_bdj_play(BDMV/BDJ/%s.jar) not implemented (%p)\n", name, bd);
    return -1;
#endif
}

static int _play_hdmv(BLURAY *bd, unsigned id_ref)
{
    bd->title_type = title_hdmv;

#ifdef USING_BDJAVA
    bd_stop_bdj(bd);
#endif

    if (!bd->hdmv_vm) {
        bd->hdmv_vm = hdmv_vm_init(bd->device_path, bd->regs);
    }
    bd->hdmv_suspended = 0;

    return hdmv_vm_select_object(bd->hdmv_vm, id_ref);
}

#define TITLE_FIRST_PLAY 0xffff   /* 10.4.3.2 (E) */
#define TITLE_TOP_MENU   0x0000   /* 5.2.3.3 */

int bd_play_title(BLURAY *bd, unsigned title)
{
    /* first play object ? */
    if (title == TITLE_FIRST_PLAY) {
        INDX_PLAY_ITEM *p = &bd->index->first_play;

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0xffff); /* 5.2.3.3 */

        if (p->object_type == indx_object_type_hdmv) {
            if (p->hdmv.id_ref == 0xffff) {
                /* no first play title (5.2.3.3) */
                bd->title_type = title_hdmv;
                return 0;
            }
            return _play_hdmv(bd, p->hdmv.id_ref);
        }

        if (p->object_type == indx_object_type_bdj) {
            return _play_bdj(bd, p->bdj.name);
        }

        return -1;
    }

    /* bd_play not called ? */
    if (bd->title_type == title_undef) {
        DEBUG(DBG_BLURAY|DBG_CRIT, "bd_call_title(): bd_play() not called !\n");
        return -1;
    }

    /* top menu ? */
    if (title == TITLE_TOP_MENU) {
        INDX_PLAY_ITEM *p = &bd->index->top_menu;

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, 0); /* 5.2.3.3 */

        if (p->object_type == indx_object_type_hdmv) {
            if (p->hdmv.id_ref == 0xffff) {
                /* no top menu (5.2.3.3) */
                bd->title_type = title_hdmv;
                return -1;
            }
            return _play_hdmv(bd, p->hdmv.id_ref);
        }

        if (p->object_type == indx_object_type_bdj) {
            return _play_bdj(bd, p->bdj.name);
        }

        return -1;
    }

    /* valid title from disc index ? */
    if (title > 0 && title <= bd->index->num_titles) {
        INDX_TITLE *t = &bd->index->titles[title-1];

        bd_psr_write(bd->regs, PSR_TITLE_NUMBER, title); /* 5.2.3.3 */

        if (t->object_type == indx_object_type_hdmv) {
            return _play_hdmv(bd, t->hdmv.id_ref);
        } else {
            return _play_bdj(bd, t->bdj.name);
        }
    }

    return -1;
}

int bd_play(BLURAY *bd)
{
    /* reset player state */

    bd->title_type = title_undef;

    if (bd->hdmv_vm) {
        hdmv_vm_free(bd->hdmv_vm);
        bd->hdmv_vm = NULL;
        bd->hdmv_suspended = 1;
    }

    _init_event_queue(bd);

    bd_psr_register_cb(bd->regs, _process_psr_event, bd);

    return bd_play_title(bd, TITLE_FIRST_PLAY);
}

int bd_menu_call(BLURAY *bd)
{
    if (bd->title_type == title_undef) {
        // bd_play not called
        return -1;
    }

    return bd_play_title(bd, TITLE_TOP_MENU);
}

void * bd_get_udfdata(BLURAY *bd)
{
	return (void *)&(bd->udf_data);
}

static void _process_hdmv_vm_event(BLURAY *bd, HDMV_EVENT *hev)
{
    DEBUG(DBG_BLURAY, "HDMV event: %d %d\n", hev->event, hev->param);

    switch (hev->event) {
        case HDMV_EVENT_TITLE:
            bd_play_title(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_PL:
            bd_select_playlist(bd, hev->param);
            bd->hdmv_suspended = 1;
            break;

        case HDMV_EVENT_PLAY_PI:
            //bd_seek_pi(bd, hev->param);
            DEBUG(DBG_BLURAY|DBG_CRIT, "HDMV_EVENT_PLAY_PI: not implemented\n");
            break;

        case HDMV_EVENT_PLAY_PM:
            bd_seek_mark(bd, hev->param);
            break;

        case HDMV_EVENT_PLAY_STOP:
            DEBUG(DBG_BLURAY|DBG_CRIT, "HDMV_EVENT_PLAY_STOP: not tested !\n");
            // stop current playlist
            bd_seek(bd, (uint64_t)bd->title->packets * 192 - 1);
            bd->st0.clip = NULL;
            // resume suspended movie object
            bd->hdmv_suspended = 0;
            break;

        case HDMV_EVENT_STILL:
            _queue_event(bd, (BD_EVENT){BD_EVENT_STILL, hev->param});
            break;

        case HDMV_EVENT_ENABLE_BUTTON:
            _queue_event(bd, (BD_EVENT){BD_EVENT_ENABLE_BUTTON, hev->param});
            break;

        case HDMV_EVENT_DISABLE_BUTTON:
            _queue_event(bd, (BD_EVENT){BD_EVENT_DISABLE_BUTTON, hev->param});
            break;

        case HDMV_EVENT_POPUP_OFF:
            _queue_event(bd, (BD_EVENT){BD_EVENT_POPUP_OFF, 0});
            break;
        case HDMV_EVENT_END:
        case HDMV_EVENT_NONE:
        default:
            break;
    }
}

static int _run_hdmv(BLURAY *bd)
{
    HDMV_EVENT hdmv_ev;

    /* run VM */
    if (hdmv_vm_run(bd->hdmv_vm, &hdmv_ev) < 0) {
        _queue_event(bd, (BD_EVENT){BD_EVENT_ERROR, 0});
        return -1;
    }

    /* process all events */
    do {
        _process_hdmv_vm_event(bd, &hdmv_ev);

    } while (!hdmv_vm_get_event(bd->hdmv_vm, &hdmv_ev));

    return 0;
}

int bd_read_ext(BLURAY *bd, unsigned char *buf, int len, BD_EVENT *event)
{
    if (_get_event(bd, event)) {
        return 0;
    }

    /* run HDMV VM ? */
    if (bd->title_type == title_hdmv) {

        while (!bd->hdmv_suspended) {

            if (_run_hdmv(bd) < 0) {
                DEBUG(DBG_BLURAY|DBG_CRIT, "bd_read_ext(): HDMV VM error\n");
                bd->title_type = title_undef;
                return -1;
            }
            if (_get_event(bd, event)) {
                return 0;
            }
        }
    }

    if (len < 1) {
        /* just polled events ? */
        return 0;
    }

    int bytes = bd_read(bd, buf, len);

    if (bytes == 0) {
        if (bd->title_type == title_hdmv) {
            DEBUG(DBG_BLURAY, "bd_read_ext(): reached end of playlist. hdmv_suspended=%d\n", bd->hdmv_suspended);
            bd->hdmv_suspended = 0;
        }
    }

    _get_event(bd, event);

    return bytes;
}

int bd_get_event(BLURAY *bd, BD_EVENT *event)
{
    if (!bd->event_queue) {
        _init_event_queue(bd);

        bd_psr_register_cb(bd->regs, _process_psr_event, bd);
    }

    if (_get_event(bd, event)) {
        return 0;
    }

    return 1;
}
