/*
 * $Id$
 */

#ifndef  cfg_parser_h
#define cfg_parser_h

#include <stdio.h>

#define CFG_EMPTY   0
#define CFG_COMMENT 1
#define CFG_SKIP    2
#define CFG_RULE    3
#define CFG_ERROR  -1

#define MAX_LINE_SIZE 800

struct cfg_line{
	int type;
	char* method;
	char* uri;
	char* address;
};


int cfg_parse_line(char* line, struct cfg_line* cl);
int cfg_parse_stream(FILE* stream);

#endif
