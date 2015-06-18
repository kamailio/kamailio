/*
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _PV_BRANCH_H_
#define _PV_BRANCH_H_

#include "../../pvar.h"

int pv_get_branchx(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_set_branchx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_branchx_name(pv_spec_p sp, str *in);

int pv_get_sbranch(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);
int pv_set_sbranch(sip_msg_t *msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_get_sndto(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_sndfrom(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_parse_snd_name(pv_spec_p sp, str *in);

int pv_get_nh(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_parse_nh_name(pv_spec_p sp, str *in);

int sbranch_set_ruri(sip_msg_t *msg);
int sbranch_append(sip_msg_t *msg);
void pv_init_sbranch(void);
int sbranch_reset(void);

#endif

