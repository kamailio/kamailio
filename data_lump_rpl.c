/*
 * $Id$
 *
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
 *
 * History:
 * 2002-02-14 : created by bogdan
 * 2003-09-11 : lump_rpl type added - LUMP_RPL_BODY & LUMP_RPL_HDR (bogdan)
 * 2003-11-11 : build_lump_rpl merged into add_lump_rpl; types -> flags ;
 *              flags LUMP_RPL_NODUP and LUMP_RPL_NOFREE added (bogdan)
 */


#include <string.h>
#include "dprint.h"
#include "mem/mem.h"
#include "data_lump_rpl.h"



struct lump_rpl* add_lump_rpl(struct sip_msg *msg, char *s, int len, int flags)
{
	struct lump_rpl *lump = 0;
	struct lump_rpl *foo;

	/* some checkings */
	if ( (flags&(LUMP_RPL_HDR|LUMP_RPL_BODY))==(LUMP_RPL_HDR|LUMP_RPL_BODY)
	|| (flags&(LUMP_RPL_HDR|LUMP_RPL_BODY))==0) {
		LOG(L_ERR,"ERROR:add_lump_rpl: bad type flags (none or both)!\n");
		goto error;
	}
	if (len<=0 || s==0) {
		LOG(L_ERR,"ERROR:add_lump_rpl: I won't add an empty lump!\n");
		goto error;
	}

	/* build the lump */
	lump = (struct lump_rpl*) pkg_malloc
		( sizeof(struct lump_rpl) + ((flags&LUMP_RPL_NODUP)?0:len) );
	if (!lump) {
		LOG(L_ERR,"ERROR:add_lump_rpl : no free pkg memory !\n");
		goto error;
	}

	if (flags&LUMP_RPL_NODUP) {
		lump->text.s = s;
	} else {
		lump->text.s = ((char*)lump)+sizeof(struct lump_rpl);
		memcpy( lump->text.s, s, len);
	}
	lump->text.len = len;
	lump->flags = flags;
	lump->next = 0;

	/* add the lump to the msg */
	if (!msg->reply_lump) {
		msg->reply_lump = lump;
	}else{
		if (!(flags&LUMP_RPL_BODY))
			for(foo=msg->reply_lump;foo->next;foo=foo->next);
		else
			for(foo=msg->reply_lump;foo->next;foo=foo->next)
				if (lump->flags&LUMP_RPL_BODY) {
					LOG(L_ERR,"ERROR:add_lump_rpl: LUMP_RPL_BODY "
						"already added!\n");
					pkg_free(lump);
					goto error;
				}
		foo->next = lump;
	}

	return lump;
error:
	return 0;
}



void free_lump_rpl(struct lump_rpl* lump)
{
	if (lump) {
		if (!((lump->flags)&LUMP_RPL_NOFREE) && ((lump->flags)&LUMP_RPL_NODUP)
		&& lump->text.s)
			pkg_free(lump->text.s);
		pkg_free(lump);
	}
}


void unlink_lump_rpl(struct sip_msg * msg, struct lump_rpl* lump)
{
	struct lump_rpl *foo,*prev;

	/* look for the lump to be unlink */
	foo = msg->reply_lump;
	prev = 0;
	while( foo && foo!=lump ) {
		prev = foo;
		foo = foo->next;
	}

	/* if the lump was found into the list -> unlink it */
	if (foo) {
		if (prev)
			prev->next = foo->next;
		else
			msg->reply_lump = foo->next;
	}
}


