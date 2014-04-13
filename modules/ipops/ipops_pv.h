/**
 * $Id$
 *
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _IPOPS_PV_H_
#define _IPOPS_PV_H_

#include "../../pvar.h"

int pv_parse_dns_name(pv_spec_t *sp, str *in);
int pv_get_dns(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);

int dns_init_pv(char *path);
void dns_destroy_pv(void);
int dns_update_pv(str *tomatch, str *name);

int pv_parse_hn_name(pv_spec_p sp, str *in);
int pv_get_hn(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

#endif

