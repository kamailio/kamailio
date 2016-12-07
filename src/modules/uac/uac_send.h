/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#ifndef _UAC_SEND_H_
#define _UAC_SEND_H_

#include "../../pvar.h"

int pv_get_uac_req(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_set_uac_req(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_uac_req_name(pv_spec_p sp, str *in);
void uac_req_init(void);
int uac_req_send(void);
int w_uac_req_send(struct sip_msg *msg, char *s1, char *s2);

#endif
