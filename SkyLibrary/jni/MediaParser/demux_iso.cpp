#include <stdio.h>
#include <stdint.h>
#include "demux_iso.h"
#include "common.h"
#include "read_data.h"
#include "dvdread/dvd_reader.h"
#include "dvdread/ifo_read.h"

#define FIRST_AC3_AID 128
#define FIRST_DTS_AID 136
#define FIRST_MPG_AID 0
#define FIRST_PCM_AID 160

#define mp_msg(msg, ...)
//#define mp_msg(msg, ...) printf(msg, ## __VA_ARGS__)

#define MSGTR_DVDaudioStreamInfo "audio stream: %d format: %s (%s) language: %s aid: %d.\n"

typedef struct bd_priv_t {
	dvd_reader_t *dvd;
	ifo_handle_t *vmg_file;
} bd_priv;

static int dvd_set_video(FileInfo *finfo, video_attr_t *attr)
{
	int nRet = 0;
	int width = 0, height = 0;
	BITMAP_INFO_HEADER *bih;
	if ((finfo == NULL) || (attr == NULL))
		return nRet;
/*
	if(attr->mpeg_version == 0
			&& attr->video_format == 0
			&& attr->display_aspect_ratio == 0
			&& attr->permitted_df == 0
			&& attr->unknown1 == 0
			&& attr->line21_cc_1 == 0
			&& attr->line21_cc_2 == 0
			&& attr->video_format == 0
			&& attr->letterboxed == 0
			&& attr->film_mode == 0) {
		printf("-- DVD video Unspecified --\n");
		return nRet;
	}
*/
	bih = &finfo->bih;

	switch(attr->mpeg_version) {
		case 0:
			finfo->VideoType = VIDEO_MPEG1;
			bih->biCompression = mmioFOURCC('M', 'P', 'G', '1');
			break;
		case 1:
			finfo->VideoType = VIDEO_MPEG2;
			bih->biCompression = mmioFOURCC('M', 'P', 'G', '2');
			break;
		default:
			printf("-- DVD video format error: %d--\n", attr->mpeg_version);
	}

	height = 480;
	if(attr->video_format != 0)
		height = 576;
	switch(attr->picture_size) {
		case 0:
			width = 720;
			break;
		case 1:
			width = 704;
			break;
		case 2:
			width = 352;
			break;
		case 3:
			width = 352;
			height /= 2;
			break;
		default:
			printf("-- DVD video resolution error: %d %d--\n", attr->video_format, attr->picture_size);
	}

	bih->biWidth = width;
	bih->biHeight = height;
	nRet = 1;
	return nRet;
}

static void dvd_set_time(FileInfo *finfo, dvd_time_t *dtime)
{
	float fps = 0;
	if ((finfo == NULL) || (dtime == NULL))
		return;
	dtime->hour = (((dtime->hour >> 4) * 10) + (dtime->hour & 0xf));
	dtime->minute = (((dtime->minute >> 4) * 10) + (dtime->minute & 0xf));
	dtime->second = (((dtime->second >> 4) * 10) + (dtime->second & 0xf));
	finfo->FileDuration = ((dtime->hour * 60) + dtime->minute) * 60 + dtime->second;
	switch((dtime->frame_u & 0xc0) >> 6) {
		case 1:
			fps = 25.00;
			break;
		case 3:
			fps = 29.97;
			break;
	}
	finfo->FPS = fps;
	mp_msg("duration: %d, fps: %2.2f\n", finfo->FileDuration, finfo->FPS);
}

static int dvd_set_audio(FileInfo *finfo, audio_attr_t *attr)
{
	int nRet = 0;
	WAVEFORMATEX *wf;
	if ((finfo == NULL) || (attr == NULL))
		return nRet;

#if 0
	if(attr->audio_format == 0
			&& attr->multichannel_extension == 0
			&& attr->lang_type == 0
			&& attr->application_mode == 0
			&& attr->quantization == 0
			&& attr->sample_frequency == 0
			&& attr->channels == 0
			&& attr->lang_extension == 0
			&& attr->unknown1 == 0
			&& attr->unknown3 == 0) {
		printf("-- DVD audio Unspecified --\n");
		return nRet;
	}
#endif
	wf = &finfo->wf;
	switch(attr->audio_format)
	{
		case 0: // ac3
			finfo->AudioType = AUDIO_A52;
			wf->wFormatTag = AUDIO_A52;
			break;
		case 2: // mpeg 1
		case 3: // mpeg 2 ext
			finfo->AudioType = AUDIO_MP2;
			wf->wFormatTag = AUDIO_MP2;
			break;
		case 4: // lpcm
			finfo->AudioType = AUDIO_LPCM_BE;
			break;
		case 6: // dts
			finfo->AudioType = AUDIO_DTS;
			wf->wFormatTag = AUDIO_DTS;
			break;
		default:
			printf("-- DVD audio format error: %d--\n", attr->audio_format);
			break;
	}

	switch(attr->sample_frequency) {
		case 0:
			wf->nSamplesPerSec = 48000;
			break;
		default:
			printf("-- DVD audio sample frequency error: %d--\n", attr->sample_frequency);
	}

	wf->nChannels = attr->channels + 1;
	nRet = 1;
	return nRet;
}

int iso_check_file(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	dvd_reader_t *dvd = NULL;
	ifo_handle_t *vmg_file = NULL;
	tt_srpt_t *tt_srpt = NULL;
	ifo_handle_t *vts_file = NULL;
	dvd_file_t *title = NULL;
	bd_priv *p_bd_priv = NULL;

	mp_msg("iso_check_file\n");

	dvd = DVDOpen(finfo->filepath);
	if (dvd == NULL)
		goto iso_check_end;

	vmg_file = ifoOpen(dvd, 0);
	if (vmg_file == NULL)
		goto iso_check_end;

	/*
	tt_srpt = vmg_file->tt_srpt;
	if (tt_srpt == NULL)
		goto iso_check_end;

	vts_file = ifoOpen( dvd, tt_srpt->title[0].title_set_nr );
	if (vts_file == NULL)
	{
		goto iso_check_end;
	}
	*/

	/*
	title = DVDOpenFile(dvd, tt_srpt->title[0].title_set_nr, DVD_READ_TITLE_VOBS);
	if (title == NULL)
	{
		goto iso_check_end;
	}
	*/

	nRet = 1;
	p_bd_priv = (bd_priv *)malloc(sizeof(bd_priv));
	p_bd_priv->dvd = dvd;
	p_bd_priv->vmg_file = vmg_file;
	finfo->priv = (void *)p_bd_priv;

iso_check_end:
	DVDCloseFile(title);
	ifoClose(vts_file);
	if (nRet == 0)
	{
		if (dvd != NULL)
			DVDClose(dvd);
		if (vmg_file != NULL)
			ifoClose(vmg_file);
		if (p_bd_priv != NULL)
			free(p_bd_priv);
	}

	return nRet;
}

int ISO_Parser(FILE *fp, FileInfo *finfo)
{
	int nRet = 0;
	dvd_reader_t *dvd = NULL;
	ifo_handle_t *vmg_file = NULL;
	tt_srpt_t *tt_srpt = NULL;
	ifo_handle_t *vts_file = NULL;
	pgc_t *pgc = NULL;
	dvd_file_t *title = NULL;
	int dvd_title, ttn = 0;
	int ii, jj;
	bd_priv *p_bd_priv = NULL;

	mp_msg("ISO_Parser\n");
	if (finfo->priv != NULL)
	{
		p_bd_priv = (bd_priv *)finfo->priv;
		dvd = p_bd_priv->dvd;
		vmg_file = p_bd_priv->vmg_file;
	} else {
		dvd = DVDOpen(finfo->filepath);
		if (dvd == NULL)
			goto iso_check_end;
		vmg_file = ifoOpen(dvd, 0);
		if (vmg_file == NULL)
			goto iso_check_end;
	}

	if (vmg_file == NULL)
		goto iso_check_end;

	tt_srpt = vmg_file->tt_srpt;
	if (tt_srpt == NULL)
		goto iso_check_end;

	// choise most chapters as default title
	for (ii = 0, jj = 0; ii < tt_srpt->nr_of_srpts; ii++)
	{
		mp_msg("nr_of_srpts: %d, open %d nr_of_ptt(chapter): %d title_set_nr: %d, vts_ttn: %d\n",
				tt_srpt->nr_of_srpts, ii,
				tt_srpt->title[ii].nr_of_ptts, tt_srpt->title[ii].title_set_nr,
				tt_srpt->title[ii].vts_ttn
				);
		if (tt_srpt->title[ii].nr_of_ptts > jj)
		{
			jj = tt_srpt->title[ii].nr_of_ptts;
			dvd_title = tt_srpt->title[ii].title_set_nr;
			ttn = tt_srpt->title[ii].vts_ttn;
		}
	}
	vts_file = ifoOpen( dvd, dvd_title );
	if (vts_file == NULL)
		goto iso_check_end;

	//title = DVDOpenFile(dvd, dvd_title, DVD_READ_TITLE_VOBS);
	//title = DVDOpenFile(dvd, 0, DVD_READ_MENU_VOBS);
	//if (title == NULL)
	//	goto iso_check_end;

	if (ttn > 0)
		pgc = vts_file->vts_pgcit ? vts_file->vts_pgcit->pgci_srp[ttn - 1].pgc : NULL;
    /**
     * Get video info
     */
	if (vmg_file->vmgi_mat)
	{
		dvd_set_video(finfo, &vmg_file->vmgi_mat->vmgm_video_attr);
		finfo->bVideo = 1;
	}
    /**
     * Get duration and fps
     */
	if (pgc != NULL)
	{
		dvd_set_time(finfo, &pgc->playback_time);
	}
    /**
     * Check number of audio channels and types
     */
	if(vts_file->vts_pgcit) {
		int i;
		for(i=0;i<8;i++)
		{
			if(pgc->audio_control[i] & 0x8000) {
				audio_attr_t * audio = &vts_file->vtsi_mat->vts_audio_attr[i];
				dvd_set_audio(finfo, audio);
				finfo->bAudio = 1;
				break;
			}
		}
	}

	nRet = 1;

iso_check_end:
	DVDCloseFile(title);
	ifoClose(vts_file);
	ifoClose(vmg_file);
	DVDClose(dvd);
	if (finfo->priv)
	{
		free(finfo->priv);
		finfo->priv = NULL;
	}

	return nRet;
}

