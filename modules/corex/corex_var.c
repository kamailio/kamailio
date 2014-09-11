/**
 * $Id$
 *
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
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
#include "../../action.h"

#include "corex_var.h"

/**
 *
 */
int pv_parse_cfg_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 4:
			if(strncmp(in->s, "line", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "name", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV af key: %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_cfg(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	char *n;

	if(param==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			n = get_cfg_crt_name();
			if(n==0)
				return pv_get_null(msg, param, res);
			return pv_get_strzval(msg, param, res, n);
		default:
			return pv_get_sintval(msg, param, res, get_cfg_crt_line());
	}
}

