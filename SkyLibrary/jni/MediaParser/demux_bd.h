#ifndef DEMUX_BD_H
#define DEMUX_BD_H

#include "MediaParser.h"

typedef struct list_priv
{
	int sid;
	int type;
	int format;
	char lang[4];
} list_priv_t;

typedef struct bd_priv
{
	int title;
	int file_id;
	int chapter_num;
	int video_num;
	list_priv_t *video_list;
	int audio_num;
	list_priv_t *audio_list;
	int sub_num;
	list_priv_t *sub_list;
} bd_priv_t;

int bd_check_file(FILE *fp, FileInfo *finfo);
int BD_Parser(FILE *fp, FileInfo *finfo, int is_dir);
void BD_Parser_free(FileInfo *finfo);
const char * bd_lookup_codec(int val);
const char * bd_lookup_audio_format(int val);
const char * bd_lookup_country_code(char *lang);

#endif // DEMUX_BD_H
