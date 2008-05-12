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
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup ldap
 * @{ 
 */

/** \file 
 * Data field conversion and type checking functions.
 */

#define LDAP_DEPRECATED 1

#include "ld_fld.h"

#include "../../db/db_drv.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <stdint.h>
#include <string.h>

/** Frees memory used by a ld_fld structure.
 * This function frees all memory used by a ld_fld structure
 * @param fld Generic db_fld_t* structure being freed.
 * @param payload The ldap extension structure to be freed
 */
static void ld_fld_free(db_fld_t* fld, struct ld_fld* payload)
{
	db_drv_free(&payload->gen);
	if (payload->values) ldap_value_free_len(payload->values);
	payload->values = NULL;
	pkg_free(payload);
}


int ld_fld(db_fld_t* fld, char* table)
{
	struct ld_fld* res;

	res = (struct ld_fld*)pkg_malloc(sizeof(struct ld_fld));
	if (res == NULL) {
		ERR("ldap: No memory left\n");
		return -1;
	}
	memset(res, '\0', sizeof(struct ld_fld));
	if (db_drv_init(&res->gen, ld_fld_free) < 0) goto error;

	DB_SET_PAYLOAD(fld, res);
	return 0;

 error:
	if (res) pkg_free(res);
	return -1;
}


int ld_resolve_fld(db_fld_t* fld, struct ld_config* cfg)
{
	int i;
	struct ld_fld* lfld;

	if (fld == NULL || cfg == NULL) return 0;

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		lfld = DB_GET_PAYLOAD(fld + i);
		lfld->attr.s = ld_find_attr_name(cfg, fld[i].name);
		if (lfld->attr.s == NULL) lfld->attr.s = fld[i].name;
		if (lfld->attr.s) lfld->attr.len = strlen(lfld->attr.s);
	}
	return 0;
}


static inline int ldap_int2db_int(int* dst, str* src)
{
	if (str2sint(src, dst) != 0) {
		ERR("ldap: Error while converting value '%.*s' to integer\n",
			src->len, ZSW(src->s));
		return -1;
	}
	return 0;
}


static inline int ldap_bit2db_int(int* dst, str* src)
{
	int i, v;

	if (src->len > 32) {
		WARN("ldap: bitString '%.*s'B is longer than 32 bits, truncating\n",
			 src->len, ZSW(src->s));
	}
	v = 0;
	for(i = 0; i < src->len; i++) {
		v <<= 1;
		v += src->s[i] - '0';
	}
	*dst = v;
	return 0;
}


int ld_ldap2fld(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg)
{
	int i;
	struct ld_fld* lfld;
	str tmp;

	if (fld == NULL || msg == NULL) return 0;
	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {	
		lfld = DB_GET_PAYLOAD(fld + i);

		if (lfld->values) ldap_value_free_len(lfld->values);
		lfld->values = ldap_get_values_len(ldap, msg, lfld->attr.s);

		if (lfld->values == NULL || lfld->values[0] == NULL) {
			fld[i].flags |= DB_NULL;
			continue;
		}

		if (lfld->values[1] != NULL) {
			ERR("ldap: Multivalue attributes not yet supported: %.*s\n", 
				lfld->attr.len, lfld->attr.s);
			return -1;
		}

		switch(fld[i].type) {
		case DB_CSTR:
			fld[i].v.cstr = lfld->values[0]->bv_val;
			break;

		case DB_STR:
		case DB_BLOB:
			fld[i].v.lstr.s = lfld->values[0]->bv_val;
			fld[i].v.lstr.len = lfld->values[0]->bv_len;
			break;

		case DB_INT:
		case DB_BITMAP:
			tmp.s = lfld->values[0]->bv_val;
			tmp.len = lfld->values[0]->bv_len;


			if (tmp.s[0] == '\'' && 
				tmp.s[tmp.len - 1] == 'B' &&
				tmp.s[tmp.len - 2] == '\'') {

				tmp.s++;
				tmp.len -= 3;
				return ldap_bit2db_int(&fld[i].v.int4, &tmp);
			} else {
				return ldap_int2db_int(&fld[i].v.int4, &tmp);
			}
			break;
			
		default:
			ERR("ldap: Unsupported field type: %d\n", fld[i].type);
			return -1;
		}
	}
	return 0;
}


/** @} */
