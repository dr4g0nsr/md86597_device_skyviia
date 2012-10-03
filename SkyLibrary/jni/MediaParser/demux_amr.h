#ifndef DEMUX_AMR_H
#define DEMUX_AMR_H

#include "MediaParser.h"

int amr_check_file(FILE	*fp, FileInfo *finfo);
int AMR_Parser(FILE	*fp, FileInfo *finfo);

#endif // DEMUX_AMR_H
