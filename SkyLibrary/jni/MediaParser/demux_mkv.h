#ifndef DEMUX_MKV_H
#define DEMUX_MKV_H

#include "MediaParser.h"

int mkv_check_file(FILE *fp, FileInfo *finfo);
int MKV_Parser(FILE *fp, FileInfo *finfo);

#endif // DEMUX_MKV_H
