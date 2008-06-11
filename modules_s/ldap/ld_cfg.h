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
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _LD_CFG_H
#define _LD_CFG_H

#include "ld_fld.h"

#include "../../str.h"


struct ld_cfg {
	str table;      /**< Name of the db api table */
	str base;       /**< The search base to be used with the table, zero terminated */
	int scope;      /**< LDAP scope */
	str filter;   /**< The search filter, zero terminated */
	str* field;  /**< An array of DB API fields, zero terminated */
	str* attr;   /**< An array of LDAP attribute names, zero terminated */
	enum ld_syntax* syntax; /**< An array of configured LDAP syntaxes */
	int n;          /**< Number of fields in the arrays */
	struct ld_cfg* next; /**< The next table in the list */
};

struct ld_con_info {
	str id;
	str host;
	struct ld_con_info* next;
};

struct ld_cfg* ld_find_cfg(str* table);

char* ld_find_attr_name(enum ld_syntax* syntax, struct ld_cfg* cfg, char* fld_name);

int ld_load_cfg(str* filename);

void ld_cfg_free(void);

#endif /* _LD_CFG_H */
