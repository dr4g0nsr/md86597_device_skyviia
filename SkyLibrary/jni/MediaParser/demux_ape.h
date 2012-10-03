#ifndef DEMUX_APE_H
#define DEMUX_APE_H

#include "MediaParser.h"

int ape_check_file(FILE	*fp, FileInfo *finfo);
int APE_Parser(FILE	*fp, FileInfo *finfo);

#endif // DEMUX_APE_H
