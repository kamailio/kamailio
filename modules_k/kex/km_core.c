/**
 * $Id$
 *
 * Copyright (C) 2009
 *
 * This file is part of SIP-Router.org, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../dset.h"
#include "../../flags.h"
#include "../../mod_fix.h"
#include "km_core.h"

int w_km_append_branch(struct sip_msg *msg, char *uri, str *sq)
{
	str suri;
	int ret;
	int q = Q_UNSPECIFIED;
	flag_t branch_flags = 0;

	getbflagsval(0, &branch_flags);
	if (uri==NULL) {
		ret = km_append_branch(msg, 0, &msg->dst_uri, &msg->path_vec,
			q, branch_flags, msg->force_send_socket);
		/* reset all branch info */
		msg->force_send_socket = 0;
		setbflagsval(0, 0);
		if(msg->dst_uri.s!=0)
			pkg_free(msg->dst_uri.s);
		msg->dst_uri.s = 0;
		msg->dst_uri.len = 0;
		if(msg->path_vec.s!=0)
			pkg_free(msg->path_vec.s);
		msg->path_vec.s = 0;
		msg->path_vec.len = 0;
	} else {
		if(fixup_get_svalue(msg, (gparam_p)uri, &suri)!=0)
		{
			LM_ERR("cannot get the URI parameter\n");
			return -1;
		}
		ret = km_append_branch(msg, &suri, &msg->dst_uri, 
			&msg->path_vec, q, branch_flags,
			msg->force_send_socket);
	}
	return ret;
}

int w_setdsturi(struct sip_msg *msg, char *uri, str *s2)
{
	str s;

	/* todo: fixup */
	s.s = uri;
	s.len = strlen(uri);
	
	if(set_dst_uri(msg, &s)!=0)
		return -1;
	return 1;

}

int w_resetdsturi(struct sip_msg *msg, char *uri, str *s2)
{
	if(msg->dst_uri.s!=0)
		pkg_free(msg->dst_uri.s);
	msg->dst_uri.s = 0;
	msg->dst_uri.len = 0;
	return 1;
}

int w_isdsturiset(struct sip_msg *msg, char *uri, str *s2)
{
	if(msg->dst_uri.s==0 || msg->dst_uri.len<=0)
		return -1;
	return 1;
}

