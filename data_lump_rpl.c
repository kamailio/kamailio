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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * 2002-02-14 : created by bogdan
 * 2003-09-11 : lump_rpl type added - LUMP_RPL_BODY & LUMP_RPL_HDR (bogdan)
 * 2003-11-11 : build_lump_rpl merged into add_lump_rpl; types -> flags ;
 *              flags LUMP_RPL_NODUP and LUMP_RPL_NOFREE added (bogdan)
 * 2006-10-16   add_lump_rpl2 added: same as the old add_lump_rpl, but
 *               returns a lump_rpl**, making a specific lump removal much
 *               more easy (andrei)
 */
/*!
 * \file
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */



#include <string.h>
#include "dprint.h"
#include "mem/mem.h"
#include "data_lump_rpl.h"



struct lump_rpl** add_lump_rpl2(struct sip_msg *msg, char *s, 
									int len, int flags)
{
	struct lump_rpl *lump = 0;
	struct lump_rpl *foo;
	struct lump_rpl** ret;

	/* some checking */
	if ( (flags&(LUMP_RPL_HDR|LUMP_RPL_BODY))==(LUMP_RPL_HDR|LUMP_RPL_BODY)
	|| (flags&(LUMP_RPL_HDR|LUMP_RPL_BODY))==0 || (flags&LUMP_RPL_SHMEM) ) {
		LOG(L_ERR,"ERROR:add_lump_rpl: bad flags combination (%d)!\n",flags);
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
		ret=&msg->reply_lump;
	}else{
		if (!(flags&LUMP_RPL_BODY))
			for(foo=msg->reply_lump;foo->next;foo=foo->next);
		else
			for(foo=msg->reply_lump; ;foo=foo->next) {
				if (foo->flags&LUMP_RPL_BODY) {
					LOG(L_ERR,"ERROR:add_lump_rpl: LUMP_RPL_BODY "
						"already added!\n");
					pkg_free(lump);
					goto error;
				}
				if (foo->next==0)
					break;
			}
		foo->next = lump;
		ret= &(foo->next);
	}

	return ret;
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

void del_nonshm_lump_rpl(struct lump_rpl** list)
{
        struct lump_rpl* it, *tmp;
        struct lump_rpl** pred;

        it = *list;
        pred = list;

        while(it) {
                if (!(it->flags & LUMP_RPL_SHMEM)) {
                        tmp = it;
                        *pred = it->next;
                        it = it->next;
                        free_lump_rpl(tmp);
                        continue;
                }

                pred = &it->next;
                it = it->next;
        }
}

