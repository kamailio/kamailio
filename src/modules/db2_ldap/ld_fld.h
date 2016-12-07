/*
 * $Id$
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _LD_FLD_H
#define _LD_FLD_H

/** \addtogroup ldap
 * @{
 */

/** \file
 * Implementation of ld_fld data structure representing LDAP fields and
 * related functions.
 */

#include "ld_cfg.h"

struct ld_cfg;

#include "../../lib/srdb2/db_gen.h"
#include "../../lib/srdb2/db_fld.h"
#include "../../lib/srdb2/db_cmd.h"

#include <ldap.h>

enum ld_syntax {
	LD_SYNTAX_STRING = 0,
	LD_SYNTAX_GENTIME,
	LD_SYNTAX_INT,
	LD_SYNTAX_BIT,
	LD_SYNTAX_BOOL,
	LD_SYNTAX_BIN,
	LD_SYNTAX_FLOAT
};

struct ld_fld {
	db_drv_t gen;
	str attr;               /**< Name of corresponding LDAP attribute */
	enum ld_syntax syntax;  /**< LDAP attribute syntax */
	struct berval** values; /**< Values retrieved from the LDAP result */    	
	unsigned int valuesnum;
	unsigned int index;
	db_fld_t** filter;	/**< filter applied on the field pointing to db_cmd_t.match[] */
	int client_side_filtering;  /**< do not pass filter to LDAP server but filter result set */
};


/** Creates a new LDAP specific payload.
 * This function creates a new LDAP specific payload structure and
 * attaches the structure to the generic db_fld structure.
 * @param fld A generic db_fld structure to be exended.
 * @param table Name of the table on the server.
 * @retval 0 on success.
 * @retval A negative number on error.
 */
int ld_fld(db_fld_t* fld, char* table);


int ld_resolve_fld(db_fld_t* fld, struct ld_cfg* cfg);

int ld_prepare_ldap_filter(char** filter, db_cmd_t* cmd, str* add);

int ld_incindex(db_fld_t* fld);

int ld_ldap2fldinit(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg);

int ld_ldap2fld(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg);

int ld_ldap2fldex(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg, int init);

/** @} */

#endif /* _LD_FLD_H */
