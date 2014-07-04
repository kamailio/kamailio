/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 */

#include "dlg_mod_internal.h"
#include "db_dlg.h"
#include <cds/serialize.h>
#include <cds/logger.h>

/*****************************************************************/

/* static void trace_dlg(const char *s, dlg_t *d)
{
	rr_t *r;
	
	TRACE_LOG("%s: callid = %.*s \nrem tag = %.*s \nloc tag = %.*s\n"
			"loc uri = %.*s\n rem uri = %.*s\n rem target = %.*s\n", 
			s,
			FMT_STR(d->id.call_id),
			FMT_STR(d->id.rem_tag),
			FMT_STR(d->id.loc_tag),
			FMT_STR(d->loc_uri),
			FMT_STR(d->rem_uri),
			FMT_STR(d->rem_target)
			);
	r = d->route_set;
	while (r) {
		TRACE_LOG(" ... name = %.*s\n uri = %.*s", 
				FMT_STR(r->nameaddr.name),  
				FMT_STR(r->nameaddr.uri));
		r = r->next;
	}
}*/

int db_store_dlg(db_con_t* conn, dlg_t *dlg, str *dst_id)
{	
	return -1;
}

int db_load_dlg(db_con_t* conn, str *id, dlg_t **dst_dialog)
{
	return -1;
}
