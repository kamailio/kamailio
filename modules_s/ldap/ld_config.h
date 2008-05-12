/*
 * LDAP module - Configuration file parser
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _LD_CONFIG_H
#define _LD_CONFIG_H

#include "../../str.h"


struct ld_config {
	str table;      /**< Name of the db api table */
	char* base;     /**< The search base to be used with the table */
	int scope;      /**< LDAP scope */
	char* filter;   /**< The search filter */
	char** fields;  /**< An array of DB API fields */
	char** attrs;   /**< An array of LDAP attribute names */
	int n;          /**< Number of fields in the arrays */
	struct ld_config* next; /**< The next table in the list */
};

extern struct ld_config* ld_cfg_root;

struct ld_config* ld_find_config(str* table);

char* ld_find_attr_name(struct ld_config* cfg, char* fld_name);

int ld_load_config(str* filename);


#endif /* _LD_CONFIG_H */
