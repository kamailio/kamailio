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
 */


#include <string.h>
#include "dprint.h"
#include "mem/mem.h"
#include "data_lump_rpl.h"


struct lump_rpl* build_lump_rpl( char* text, int len )
{
	struct lump_rpl *lump = 0;

	lump = (struct lump_rpl*) pkg_malloc(sizeof(struct lump_rpl));
	if (!lump)
	{
		LOG(L_ERR,"ERROR:build_lump_rpl : no free memory (struct)!\n");
		goto error;
	}

	lump->text.s = pkg_malloc( len );
	if (!lump->text.s)
	{
		LOG(L_ERR,"ERROR:build_lump_rpl : no free memory (%d)!\n", len );
		goto error;
	}

	memcpy(lump->text.s,text,len);
	lump->text.len = len;
	lump->next = 0;

	return lump;

error:
	if (lump) pkg_free(lump);
	return 0;
}



void add_lump_rpl(struct sip_msg * msg, struct lump_rpl* lump)
{
	struct lump_rpl *foo;

	if (!msg->reply_lump)
	{
		msg->reply_lump = lump;
	}else{
		for(foo=msg->reply_lump;foo->next;foo=foo->next);
		foo->next = lump;
	}
}



void free_lump_rpl(struct lump_rpl* lump)
{
	if (lump && lump->text.s)  pkg_free(lump->text.s);
	if (lump) pkg_free(lump);
}



