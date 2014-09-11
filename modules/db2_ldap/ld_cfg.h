/*
 * LDAP module - Configuration file parser
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _LD_CFG_H
#define _LD_CFG_H

#include "ld_fld.h"

#include "../../str.h"
#include <sys/time.h>

/* RFC 2251:  maxInt INTEGER ::= 2147483647 -- (2^^31 - 1) -- */
#define LD_MAXINT (2147483647)


struct ld_cfg {
	str table;      /**< Name of the db api table */
	str base;       /**< The search base to be used with the table, zero terminated */
	int scope;      /**< LDAP scope */
	str filter;   /**< The search filter, zero terminated */
	str* field;  /**< An array of DB API fields, zero terminated */
	str* attr;   /**< An array of LDAP attribute names, zero terminated */
	enum ld_syntax* syntax; /**< An array of configured LDAP syntaxes */
	int n;          /**< Number of fields in the arrays */
	int sizelimit; /**< retrieve at most sizelimit entries for a search */
	int timelimit; /**< wait at most timelimit seconds for a search to complete */
	int chase_references;  /**< dereference option for LDAP library */
	int chase_referrals;   /**< follow referrals option for LDAP library */
	struct ld_cfg* next; /**< The next table in the list */
};

struct ld_con_info {
	str id;
	str host;
	unsigned int port;
	str username;
	str password;
	int authmech;
	int tls;  /**<  TLS encryption enabled */
	str ca_list;  /**< Path of the file that contains certificates of the CAs */
	str req_cert;  /**< LDAP level of certificate request behaviour */
	struct ld_con_info* next;
};

struct ld_cfg* ld_find_cfg(str* table);

char* ld_find_attr_name(enum ld_syntax* syntax, struct ld_cfg* cfg, char* fld_name);

struct ld_con_info* ld_find_conn_info(str* conn_id);

int ld_load_cfg(str* filename);

void ld_cfg_free(void);

#endif /* _LD_CFG_H */
