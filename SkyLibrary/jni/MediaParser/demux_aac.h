#ifndef DEMUX_AAC_H
#define DEMUX_AAC_H

#include "MediaParser.h"

int aac_check_file(FILE	*fp, FileInfo *finfo);
int AAC_Parser(FILE	*fp, FileInfo *finfo);

#endif // DEMUX_AAC_H
