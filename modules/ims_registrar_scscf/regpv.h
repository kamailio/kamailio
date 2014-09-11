/*
 * $Id$
 *
 * Export vontact attrs as PV
 *
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

/*!
 * \file
 * \brief SIP registrar module - export contacts as PV
 * \ingroup registrar   
 */  


#ifndef _REGPV_H_
#define _REGPV_H_

#include "../../pvar.h"

int pv_get_ulc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_set_ulc(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_ulc_name(pv_spec_p sp, str *in);

int pv_fetch_contacts(struct sip_msg* msg, char* table, char* uri,
		char* profile);
int pv_free_contacts(struct sip_msg* msg, char* profile, char *s2);

void regpv_free_profiles(void);

#endif
