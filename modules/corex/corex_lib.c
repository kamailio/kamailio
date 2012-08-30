/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../dset.h"

#include "corex_lib.h"

/**
 * append new branches with generic parameters
 */
int corex_append_branch(sip_msg_t *msg, gparam_t *pu, gparam_t *pq)
{
	str uri = {0};
	str qv = {0};
	int ret = 0;

	qvalue_t q = Q_UNSPECIFIED;
	flag_t branch_flags = 0;

	if (pu!=NULL)
	{
		if(fixup_get_svalue(msg, pu, &uri)!=0)
		{
			LM_ERR("cannot get the URI parameter\n");
			return -1;
		}
	}

	if (pq!=NULL)
	{
		if(fixup_get_svalue(msg, pq, &qv)!=0)
		{
			LM_ERR("cannot get the Q parameter\n");
			return -1;
		}
		if(qv.len>0 && str2q(&q, qv.s, qv.len)<0)
		{
			LM_ERR("cannot parse the Q parameter\n");
			return -1;
		}
	}


	getbflagsval(0, &branch_flags);
	ret = append_branch(msg, (uri.len>0)?&uri:0, &msg->dst_uri,
			&msg->path_vec, q, branch_flags,
			msg->force_send_socket);


	if(uri.len<=0)
	{
		/* reset all branch attributes if r-uri was shifted to branch */
		reset_force_socket(msg);
		setbflagsval(0, 0);
		if(msg->dst_uri.s!=0)
			pkg_free(msg->dst_uri.s);
		msg->dst_uri.s = 0;
		msg->dst_uri.len = 0;
		if(msg->path_vec.s!=0)
			pkg_free(msg->path_vec.s);
		msg->path_vec.s = 0;
		msg->path_vec.len = 0;
	}

	return ret;
}
