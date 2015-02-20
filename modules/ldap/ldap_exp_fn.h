/*
 * Kamailio LDAP Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
 *
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


#ifndef LDAP_EXP_FN_H
#define LDAP_EXP_FN_H


#include "../../str.h"
#include "../../pvar.h"
#include "../../parser/msg_parser.h"
#include "../../re.h"

struct ldap_result_params
{
	str         ldap_attr_name;
	int         dst_avp_val_type; /* 0: str, 1: int */
	pv_spec_t   dst_avp_spec;
};

struct ldap_result_check_params
{
	str          ldap_attr_name;
	pv_elem_p    check_str_elem_p;
};

int ldap_search_impl(
	struct sip_msg* _msg, 
	pv_elem_t* _ldap_url);

int ldap_write_result(
	struct sip_msg* _msg,
	struct ldap_result_params* _lrp,
	struct subst_expr* _se);

int ldap_result_next(void);

int ldap_filter_url_encode(
	struct sip_msg* _msg,
	pv_elem_t* _filter_component,
	pv_spec_t* _dst_avp_spec);

int rfc2254_escape(
	struct sip_msg* _msg,
	char* _value,
	char* _avp_name);

int ldap_result_check(
	struct sip_msg* _msg,
	struct ldap_result_check_params* _lrp,
	struct subst_expr* _se);

#endif /* LDAP_EXP_FN_H */
