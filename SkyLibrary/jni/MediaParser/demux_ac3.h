#ifndef DEMUX_AC3_H
#define DEMUX_AC3_H

#include "MediaParser.h"

int ac3_check_file(FILE	*fp, FileInfo *finfo);
int AC3_Parser(FILE	*fp, FileInfo *finfo);

#endif // DEMUX_AC3_H
