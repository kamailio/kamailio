/*
 * $Id$
 *
 * adding/removing headers or any other data chunk from a message
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */
/* History:
 * --------
 *  2003-01-29  s/int/enum ... more convenient for gdb (jiri)
 *  2003-03-31  added subst lumps -- they expand in ip addr, port a.s.o (andrei)
 *  2003-04-01  added opt (condition) lumps (andrei)
 *  2003-04-02  added more subst lumps: SUBST_{SND,RCV}_ALL  
 *              => ip:port;transport=proto (andrei)
 *
 */


#ifndef data_lump_h
#define data_lump_h

#include "lump_struct.h"
#include "parser/msg_parser.h"

/* adds a header to the end */
struct lump* append_new_lump(struct lump** list, char* new_hdr,
							 int len, int type);
/* inserts a header to the beginning */
struct lump* insert_new_lump(struct lump** list, char* new_hdr,
							  int len, int type);
struct lump* insert_new_lump_after(struct lump* after,
									char* new_hdr, int len, int type);
struct lump* insert_new_lump_before(struct lump* before, char* new_hdr,
									int len,int type);
/* substitutions (replace with ip address, port etc) */
struct lump* insert_subst_lump_after(struct lump* after,  enum lump_subst subst,
									int type);
struct lump* insert_subst_lump_before(struct lump* before,enum lump_subst subst,
									int type);

/* conditional lumps */
struct lump* insert_cond_lump_after(struct lump* after, enum lump_conditions c,
									int type);
struct lump* insert_cond_lump_before(struct lump* after, enum lump_conditions c,
									int type);

/* removes an already existing header */
struct lump* del_lump(struct sip_msg* msg, int offset, int len, int type);
/* set an anchor */
struct lump* anchor_lump(struct sip_msg* msg, int offset, int len, int type);



/* duplicates a lump list shallowly in pkg-mem */
struct lump* dup_lump_list( struct lump *l );
/* frees a shallowly duplicated lump list */
void free_duped_lump_list(struct lump* l);

#endif
