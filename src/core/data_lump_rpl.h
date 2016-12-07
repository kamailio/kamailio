/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Data lumps
 * \author bogdan, andrei
 * \ingroup core
 * Module: \ref core
 */



#ifndef data_lump_rpl_h
#define data_lump_rpl_h

#include "parser/msg_parser.h"


#define LUMP_RPL_HDR     (1<<1)
#define LUMP_RPL_BODY    (1<<2)
#define LUMP_RPL_NODUP   (1<<3)
#define LUMP_RPL_NOFREE  (1<<4)
#define LUMP_RPL_SHMEM   (1<<5)

struct lump_rpl
{
	str text;
	int flags;
	struct lump_rpl* next;
};

struct lump_rpl** add_lump_rpl2(struct sip_msg *, char *, int , int );


/*! \brief compatibility wrapper for the old add_lump_rpl version */
inline static struct lump_rpl* add_lump_rpl(struct sip_msg* msg,
												char* s, int len , int flags )
{
	struct lump_rpl** l;
	
	l=add_lump_rpl2(msg, s, len, flags);
	return l?(*l):0;
}


void free_lump_rpl(struct lump_rpl* );

void unlink_lump_rpl(struct sip_msg *, struct lump_rpl* );

void del_nonshm_lump_rpl(  struct lump_rpl ** );

#endif
