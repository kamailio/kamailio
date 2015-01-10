/*
 * Kamailio H.350 Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
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
 *
 */

#ifndef H350_EXP_FN_H
#define H350_EXP_FN_H

#include "../../parser/msg_parser.h"
#include "../../pvar.h"
#include "../../parser/msg_parser.h"

#define E_H350_SUCCESS 1
#define E_H350_INTERNAL -1
#define E_H350_NO_SUCCESS -2

struct h350_auth_lookup_avp_params
{
	pv_spec_t   username_avp_spec;
	pv_spec_t   password_avp_spec;
};

int h350_exp_fn_init(void);

int h350_sipuri_lookup(struct sip_msg* _msg, pv_elem_t* _sip_uri);

int h350_auth_lookup(
	struct sip_msg* _msg, 
	pv_elem_t* _digest_username, 
	struct h350_auth_lookup_avp_params* _avp_specs);

int h350_call_preferences(struct sip_msg* _msg, pv_elem_t* _avp_name_prefix);

int h350_service_level(struct sip_msg* _msg, pv_elem_t* _avp_name_prefix);

#endif /* H350_EXP_FN_H */
