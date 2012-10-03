#ifndef DEMUX_OGG_H
#define DEMUX_OGG_H

#include "MediaParser.h"

int ogg_check_file(FILE *fp, FileInfo *finfo);
int OGG_Parser(FILE *fp, FileInfo *finfo);

#endif
