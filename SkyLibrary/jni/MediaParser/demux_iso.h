#ifndef DEMUX_ISO_H
#define DEMUX_ISO_H

#include "MediaParser.h"

int iso_check_file(FILE *fp, FileInfo *finfo);
int ISO_Parser(FILE *fp, FileInfo *finfo);

#endif // DEMUX_ISO_H
