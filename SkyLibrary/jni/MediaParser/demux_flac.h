#ifndef DEMUX_FLAC_H
#define DEMUX_FLAC_H

#include "MediaParser.h"

int flac_check_file(FILE	*fp, FileInfo *finfo);
int FLAC_Parser(FILE	*fp, FileInfo *finfo);

#endif // DEMUX_FLAC_H
