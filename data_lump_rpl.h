/*
 * $Id$
 *
 */

#ifndef data_lump_rpl_h
#define data_lump_rpl_h

#include "msg_parser.h"


struct lump_rpl
{
	str text;
	struct lump_rpl* next;
};

struct lump_rpl* build_lump_rpl( char* , int );

int add_lump_rpl(struct sip_msg * , struct lump_rpl* );

int free_lump_rpl(struct lump_rpl* );

#endif
