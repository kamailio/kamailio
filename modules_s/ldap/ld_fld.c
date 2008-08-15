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
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

/** \addtogroup ldap
 * @{
 */

/** \file
 * Data field conversion and type checking functions.
 */

#define LDAP_DEPRECATED 1

#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#define _BSD_SOURCE /* snprintf */

#include "ld_fld.h"

#include "../../db/db_drv.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stdint.h>
#include <string.h>
#include <time.h>   /* strptime, XOPEN issue must be >= 4 */


/**
 * Reallocatable string buffer.
 */
struct sbuf {
	char *s;			/**< allocated memory itself */
	int   len;			/**< used memory */
	int   size;			/**< total size of allocated memory */
	int   increment;	/**< increment when realloc is necessary */
};


#define TEST_RESIZE \
	if (rsize > sb->size) { \
		asize = rsize - sb->size; \
		new_size = sb->size + (asize / sb->increment  + \
							   (asize % sb->increment > 0)) * sb->increment; \
		newp = pkg_malloc(new_size); \
		if (!newp) { \
			ERR("ldap: No memory left\n"); \
			return -1; \
		} \
		if (sb->s) { \
			memcpy(newp, sb->s, sb->len); \
			pkg_free(sb->s); \
		} \
		sb->s = newp; \
		sb->size = new_size; \
	}


static inline int sb_add(struct sbuf *sb, char* str, int len)
{
	int new_size = 0, asize;
	int rsize = sb->len + len;
	char *newp;

	TEST_RESIZE;

	memcpy(sb->s + sb->len, str, len);
	sb->len += len;
	return 0;
}


static inline int sb_add_esc(struct sbuf *sb, char* str, int len)
{
	int new_size = 0, asize, i;
	int rsize = sb->len + len * 3;
	char *newp, *w;

	TEST_RESIZE;

	w = sb->s + sb->len;
	for(i = 0; i < len; i++) {
		switch(str[i]) {
		case '*':
			*w++ = '\\'; *w++ = '2'; *w++ = 'A';
			sb->len += 3;
			break;

		case '(':
			*w++ = '\\'; *w++ = '2'; *w++ = '8';
			sb->len += 3;
			break;

		case ')':
			*w++ = '\\'; *w++ = '2'; *w++ = '9';
			sb->len += 3;
			break;

		case '\\':
			*w++ = '\\'; *w++ = '5'; *w++ = 'C';
 			sb->len += 3;
			break;

		case '\0':
			*w++ = '\\'; *w++ = '0'; *w++ = '0';
			sb->len += 3;
			break;

		default:
			*w++ = str[i];
			sb->len++;
			break;
		}
	}

	return 0;
}


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


int ld_resolve_fld(db_fld_t* fld, struct ld_cfg* cfg)
{
	int i;
	struct ld_fld* lfld;

	if (fld == NULL || cfg == NULL) return 0;

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		lfld = DB_GET_PAYLOAD(fld + i);
		lfld->attr.s = ld_find_attr_name(&lfld->syntax, cfg, fld[i].name);
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


static inline int ldap_gentime2db_datetime(time_t* dst, str* src)
{
	struct tm time;

	if (src->len < 12) return -1;

	/* It is necessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	strptime(src->s, "%Y%m%d%H%M%S", &time);

	/* Daylight saving information got lost in the database
	 * so let timegm to guess it. This eliminates the bug when
	 * contacts reloaded from the database have different time
	 * of expiration by one hour when daylight saving is used
	 */
	time.tm_isdst = -1;
#ifdef HAVE_TIMEGM
    *dst = timegm(&time);
#else
    *dst = _timegm(&time);
#endif /* HAVE_TIMEGM */
	return 0;
}


static inline int ldap_str2db_double(double* dst, char* src)
{
	*dst = atof(src);
	return 0;
}


static inline int ldap_str2db_float(float* dst, char* src)
{
	*dst = (float)atof(src);
	return 0;
}


int ld_ldap2fldinit(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg)
{
	return ld_ldap2fldex(fld, ldap, msg, 1);
}

int ld_ldap2fld(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg)
{
	return ld_ldap2fldex(fld, ldap, msg, 0);
}

int ld_incindex(db_fld_t* fld) {
	int i;
	struct ld_fld* lfld;


	if (fld == NULL) return 0;

	i = 0;
	while (!DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i])) {
		lfld = DB_GET_PAYLOAD(fld + i);
		lfld->index++;
		/* the index limit has been reached */
		if (lfld->index >= lfld->valuesnum) {
			lfld->index = 0;
		} else {
			return 0;
		}
		i++;
	}

	/* there is no more value combination left */
	return 1;
}

int ld_ldap2fldex(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg, int init)
{
	int i;
	struct ld_fld* lfld;
	str v;

	if (fld == NULL || msg == NULL) return 0;
	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		lfld = DB_GET_PAYLOAD(fld + i);

		if (init) {
			/* free the values of the previous object */
			if (lfld->values) ldap_value_free_len(lfld->values);
			lfld->values = ldap_get_values_len(ldap, msg, lfld->attr.s);

			if (lfld->values == NULL || lfld->values[0] == NULL) {
				fld[i].flags |= DB_NULL;
				/* index == 0 means no value available */
				lfld->valuesnum = 0;
			} else {
				/* init the number of values */
				lfld->valuesnum = ldap_count_values_len(lfld->values);
			}
			/* pointer to the current value */
			lfld->index = 0;
		}

		/* this is an empty value */
		if (!lfld->valuesnum)
			continue;

		v.s = lfld->values[lfld->index]->bv_val;
		v.len = lfld->values[lfld->index]->bv_len;

		switch(fld[i].type) {
		case DB_CSTR:
			fld[i].v.cstr = v.s;
			break;

		case DB_STR:
		case DB_BLOB:
			fld[i].v.lstr.s = v.s;
			fld[i].v.lstr.len = v.len;
			break;

		case DB_INT:
		case DB_BITMAP:
			if (v.s[0] == '\'' && v.s[v.len - 1] == 'B' &&
				v.s[v.len - 2] == '\'') {
				v.s++;
				v.len -= 3;
				if (ldap_bit2db_int(&fld[i].v.int4, &v) != 0) {
					ERR("ldap: Error while converting bit string '%.*s'\n",
						v.len, ZSW(v.s));
					return -1;
				}
				break;
			}

			if (v.len == 4 && !strncasecmp("TRUE", v.s, v.len)) {
				fld[i].v.int4 = 1;
				break;
			}

			if (v.len == 5 && !strncasecmp("FALSE", v.s, v.len)) {
				fld[i].v.int4 = 0;
				break;
			}

			if (ldap_int2db_int(&fld[i].v.int4, &v) != 0) {
				ERR("ldap: Error while converting %.*s to integer\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_DATETIME:
			if (ldap_gentime2db_datetime(&fld[i].v.time, &v) != 0) {
				ERR("ldap: Error while converting LDAP time value '%.*s'\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_FLOAT:
			/* We know that the ldap library zero-terminated v.s */
			if (ldap_str2db_float(&fld[i].v.flt, v.s) != 0) {
				ERR("ldap: Error while converting '%.*s' to float\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_DOUBLE:
			/* We know that the ldap library zero-terminated v.s */
			if (ldap_str2db_double(&fld[i].v.dbl, v.s) != 0) {
				ERR("ldap: Error while converting '%.*s' to double\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		default:
			ERR("ldap: Unsupported field type: %d\n", fld[i].type);
			return -1;
		}
	}
	return 0;
}


static inline int db_str2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	struct ld_fld* lfld;
	int rv;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: String attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);
	rv |= sb_add_esc(buf, fld->v.lstr.s, fld->v.lstr.len);
	return rv;
}


static inline int db_cstr2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	struct ld_fld* lfld;
	int rv;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: String attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);
	rv |= sb_add_esc(buf, fld->v.cstr,
					 fld->v.cstr ? strlen(fld->v.cstr) : 0);
	return rv;
}


static inline int db_int2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	struct ld_fld* lfld;
	int rv, len;
	char tmp[INT2STR_MAX_LEN + 1];

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: String attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);

	len = snprintf(tmp, INT2STR_MAX_LEN + 1, "%-d", fld->v.int4);
	if (len < 0 || len >= INT2STR_MAX_LEN + 1) {
		BUG("ldap: Error while converting integer to string\n");
		return -1;
	}
	rv |= sb_add(buf, tmp, len);
	return rv;
}


static inline int db_datetime2ldap_gentime(struct sbuf* buf, db_fld_t* fld)
{
	static char tmp[13];
	struct tm* t;
	struct ld_fld* lfld;
	int rv;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: GeneralizedTime attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);

	t = gmtime(&fld->v.time);
	if (strftime(tmp, 13, "%Y%m%d%H%M%S", t) != 12) {
		ERR("ldap: Error while converting time_t value to LDAP format\n");
		return -1;
	}
	rv |= sb_add(buf, tmp, 12);
	return rv;
}


static inline int db_int2ldap_bool(struct sbuf* buf, db_fld_t* fld)
{
	struct ld_fld* lfld;
	int rv;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: Boolean attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);
	if (fld->v.int4) rv |= sb_add(buf, "TRUE", 4);
	else rv |= sb_add(buf, "FALSE", 5);
	return rv;
}


static inline int db_uint2ldap_int(struct sbuf* buf, db_fld_t* fld)
{
	char* num;
	struct ld_fld* lfld;
	int rv, v, len;

	rv = 0;
	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);

	v = fld->v.int4;
	switch(fld->op) {
	case DB_EQ:  rv |= sb_add(buf, "=", 1); break;
	case DB_LT:
		rv |= sb_add(buf, "<=", 2);
		if (v == INT_MIN)
			WARN("ldap: parameter with 'less than' comparison would overflow\n");
		else
			v--;
		break;
	case DB_GT:
		rv |= sb_add(buf, ">=", 2);
		if (v == INT_MAX)
			WARN("ldap: parameter with 'greater than' comparison would overflow\n");
		else
			v++;
		break;
	case DB_LEQ: rv |= sb_add(buf, "<=", 2); break;
	case DB_GEQ: rv |= sb_add(buf, ">=", 2); break;
	default:
		ERR("ldap: Unsupported operator while converting int attribute: %d\n",
			fld->op);
		return -1;
	}

	num = int2str(v, &len);
	rv |= sb_add(buf, num, len);
	return rv;
}


static inline int db_bit2ldap_bitstr(struct sbuf* buf, db_fld_t* fld)
{
	struct ld_fld* lfld;
	int rv, i;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: Bit string attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);
	rv |= sb_add(buf, "'", 1);

	i = 1 << (sizeof(fld->v.int4) * 8 - 1);
	while(i) {
		if (fld->v.int4 & i) rv |= sb_add(buf, "1", 1);
		else rv |= sb_add(buf, "0", 1);
		i = i >> 1;
	}
	rv |= sb_add(buf, "'B", 2);
	return rv;
}


static inline int db_float2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	static char tmp[16];
	struct ld_fld* lfld;
	int rv, len;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: String attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);

	len = snprintf(tmp, 16, "%-10.2f", fld->v.flt);
	if (len < 0 || len >= 16) {
		BUG("ldap: Error while converting float to string\n");
		return -1;
	}
	rv |= sb_add(buf, tmp, len);
	return rv;
}


static inline int db_double2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	static char tmp[16];
	struct ld_fld* lfld;
	int rv, len;

	rv = 0;
	if (fld->op != DB_EQ) {
		ERR("ldap: String attributes can only be compared "
			"with '=' operator\n");
		return -1;
	}

	lfld = DB_GET_PAYLOAD(fld);
	rv |= sb_add(buf, lfld->attr.s, lfld->attr.len);
	rv |= sb_add(buf, "=", 1);

	len = snprintf(tmp, 16, "%-10.2f", fld->v.dbl);
	if (len < 0 || len >= 16) {
		BUG("ldap: Error while converting double to string\n");
		return -1;
	}
	rv |= sb_add(buf, tmp, len);
	return rv;
}


int ld_fld2ldap(char** filter, db_fld_t* fld, str* add)
{
	struct ld_fld* lfld;
	int i, rv = 0;
	struct sbuf buf = {
		.s = NULL, .len = 0,
		.size = 0, .increment = 128
	};

	/* Return NULL if there are no fields and no preconfigured search
	 * string supplied in the configuration file
	 */
	if (DB_FLD_EMPTY(fld) && ((add->s == NULL) || !add->len)) {
		*filter = NULL;
		return 0;
	}

	rv = sb_add(&buf, "(&", 2);
	if (add->s && add->len) {
		/* Add the filter component specified in the config file */
		rv |= sb_add(&buf, add->s, add->len);
	}

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		rv |= sb_add(&buf, "(", 1);
		lfld = DB_GET_PAYLOAD(fld + i);

		if (fld[i].flags & DB_NULL) {
			rv |= sb_add(&buf, lfld->attr.s, lfld->attr.len);
			rv |= sb_add(&buf, "=", 1);
			goto skip;
		}

		switch(fld[i].type) {
		case DB_CSTR:
			if (db_cstr2ldap_str(&buf, fld + i))
				goto error;
			break;

		case DB_STR:
			if (db_str2ldap_str(&buf, fld + i))
				goto error;
			break;

		case DB_INT:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_INT:
			case LD_SYNTAX_FLOAT:
				if (db_int2ldap_str(&buf, fld + i))
					goto error;
				break;

			case LD_SYNTAX_GENTIME:
				if (db_datetime2ldap_gentime(&buf, fld + i))
					goto error;
				break;

			case LD_SYNTAX_BIT:
				if (db_bit2ldap_bitstr(&buf, fld + i))
					goto error;
				break;

			case LD_SYNTAX_BOOL:
				if (db_int2ldap_bool(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert integer field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_BITMAP:
			switch(lfld->syntax) {
			case LD_SYNTAX_INT:
				if (db_uint2ldap_int(&buf, fld + i))
					goto error;
				break;

			case LD_SYNTAX_BIT:
			case LD_SYNTAX_STRING:
				if (db_bit2ldap_bitstr(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert bitmap field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_DATETIME:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_GENTIME:
				if (db_datetime2ldap_gentime(&buf, fld + i))
					goto error;
				break;

			case LD_SYNTAX_INT:
				if (db_uint2ldap_int(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert datetime field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_FLOAT:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_FLOAT:
				if (db_float2ldap_str(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert float field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}

		case DB_DOUBLE:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_FLOAT:
				if (db_float2ldap_str(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert double field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
				break;
			}
			break;

		case DB_BLOB:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_BIN:
				if (db_str2ldap_str(&buf, fld + i))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert binary field %s "
					"to LDAP attribute %.*s\n",
					fld[i].name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		default:
			BUG("ldap: Unsupported field type encountered: %d\n", fld[i].type);
			goto error;
		}

	skip:
		rv |= sb_add(&buf, ")", 1);
	}

	rv |= sb_add(&buf, ")", 1);
	rv |= sb_add(&buf, "\0", 1);
	if (rv) goto error;

	*filter = buf.s;
	return 0;

error:
	if (buf.s) pkg_free(buf.s);
	return -1;
}


/** @} */
