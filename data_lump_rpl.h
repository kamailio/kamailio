/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * 2002-02-14 : created by bogdan
 * 2003-09-11 : lump_rpl type added - LUMP_RPL_BODY & LUMP_RPL_HDR (bogdan)
 * 2003-11-11 : build_lump_rpl merged into add_lump_rpl; type removed;
 *              flags LUMP_RPL_BODY, LUMP_RPL_NODUP and LUMP_RPL_NOFREE
 *              added (bogdan)
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

struct lump_rpl* add_lump_rpl(struct sip_msg *, char *, int , int );

void free_lump_rpl(struct lump_rpl* );

void unlink_lump_rpl(struct sip_msg *, struct lump_rpl* );

void del_nonshm_lump_rpl(  struct lump_rpl ** );

#endif
