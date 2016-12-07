/*
 * $Id$
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 * \brief Domain module headers
 */


#ifndef DOMAINPOLICY_MOD_H
#define DOMAINPOLICY_MOD_H


#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../usr_avp.h"


/*
 * Module parameters variables
 */
extern str domainpolicy_table;		/*!< Domainpolicy table name */
extern str domainpolicy_col_rule;   	/*!< Rule column name */
extern str domainpolicy_col_type;   	/*!< Type column name */
extern str domainpolicy_col_att;   	/*!< Attribute column name */
extern str domainpolicy_col_val;   	/*!< Value column name */


/*
 * Other module variables
 */
extern int_str port_override_name, transport_override_name, 
		domain_prefix_name, domain_suffix_name, domain_replacement_name,
		send_socket_name, target_name;

extern unsigned short port_override_avp_name_str;
extern unsigned short transport_override_avp_name_str;
extern unsigned short domain_prefix_avp_name_str;
extern unsigned short domain_suffix_avp_name_str;
extern unsigned short domain_replacement_avp_name_str;
extern unsigned short send_socket_avp_name_str;


#endif
