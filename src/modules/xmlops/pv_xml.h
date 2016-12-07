/**
 *
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
		       
#ifndef _PV_XML_H_
#define _PV_XML_H_

#include "../../sr_module.h"
#include "../../pvar.h"

int pv_get_xml(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_set_xml(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);

int pv_parse_xml_name(pv_spec_p sp, str *in);

int pv_xml_ns_param(modparam_t type, void *val);

#endif
