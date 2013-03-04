/* 
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
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


/*!
 * \file
 * \brief DB_POSTGRES :: Data field conversion and type checking functions.
 * \ingroup db_postgres
 * Module: \ref db_postgres
 */

#include "pg_fld.h"
#include "pg_con.h" /* flags */
#include "pg_mod.h"

#include "../../lib/srdb2/db_drv.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>

/**
 * This is the epoch time in time_t format, this value is used to convert
 * timestamp values to/from PostgreSQL format.
 *  2000-01-01 00:00:00 +0000 as the value of time_t in UTC
 */
#define PG_EPOCH_TIME ((int64_t)946684800)


/** Frees memory used by a pg_fld structure.
 * This function frees all memory used by a pg_fld structure
 * @param fld Generic db_fld_t* structure being freed.
 * @param payload The postgresql extension structure to be freed
 */
static void pg_fld_free(db_fld_t* fld, struct pg_fld* payload)
{
	db_drv_free(&payload->gen);
	if (payload->name) pkg_free(payload->name);
	pkg_free(payload);
}


int pg_fld(db_fld_t* fld, char* table)
{
	struct pg_fld* res;

	res = (struct pg_fld*)pkg_malloc(sizeof(struct pg_fld));
	if (res == NULL) {
		ERR("postgres: No memory left\n");
		return -1;
	}
	memset(res, '\0', sizeof(struct pg_fld));
	if (db_drv_init(&res->gen, pg_fld_free) < 0) goto error;

	DB_SET_PAYLOAD(fld, res);
	return 0;

 error:
	if (res) pkg_free(res);
	return -1;
}


union ull {
	uint64_t ui64;
	uint32_t ui32[2];
};

static inline uint64_t htonll(uint64_t in)
{
	union ull* p = (union ull*)&in;
	
	return ((uint64_t)htonl(p->ui32[0]) << 32) + (uint64_t)htonl(p->ui32[1]);
}


static inline uint64_t ntohll(uint64_t in)
{
	union ull* p = (union ull*)&in;
	return ((uint64_t)ntohl(p->ui32[0]) << 32) + (uint64_t)ntohl(p->ui32[1]);
}


static inline void db_int2pg_int4(struct pg_params* dst, int i, 
								  db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.int4[0] = htonl(src->v.int4);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 4;
}


static inline void db_int2pg_int2(struct pg_params* dst, int i, 
								  db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.int2[0] = htons(src->v.int4);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 2;
}


static inline void db_int2pg_timestamp(struct pg_params* dst, int i, 
									   db_fld_t* src, unsigned int flags)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	if (flags & PG_INT8_TIMESTAMP) {
		pfld->v.int8 = ((int64_t)src->v.int4 - PG_EPOCH_TIME) * 1000000;
	} else {
		pfld->v.dbl = (double)src->v.int4 - (double)PG_EPOCH_TIME;
	}
	pfld->v.int8 = htonll(pfld->v.int8);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 8;
}


static inline void db_int2pg_int8(struct pg_params* dst, int i,
								  db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.int4[0] = 0;
	pfld->v.int4[1] = htonl(src->v.int4);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 8;
}


static inline void db_int2pg_bool(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	if (src->v.int4) pfld->v.byte[0] = 1;
	else pfld->v.byte[0] = 0;

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 1;
}


static inline void db_int2pg_inet(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.byte[0] = AF_INET; /* Address family */
	pfld->v.byte[1] = 32; /* Netmask */
	pfld->v.byte[2] = 0; /* is CIDR */
	pfld->v.byte[3] = 4; /* Number of bytes */
	pfld->v.int4[1] = htonl(src->v.int4); /* Actuall IP address */

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 8;
}


static inline void db_float2pg_float4(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.int4[0] = htonl(src->v.int4);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 4;
}


static inline void db_float2pg_float8(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.dbl = src->v.flt;
	pfld->v.int8 = htonll(pfld->v.int8);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 8;
}


static inline void db_double2pg_float8(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.int8 = htonll(src->v.int8);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 8;
}


static inline void db_double2pg_float4(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);
	pfld->v.flt = src->v.dbl;
	pfld->v.int4[0] = htonl(pfld->v.int4[0]);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
	dst->len[i] = 4;
}


static inline void db_int2pg_bit(struct pg_params* dst, int i, db_fld_t* src)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(src);

	pfld->v.int4[0] = htonl(32);
	pfld->v.int4[1] = htonl(src->v.int4);

	dst->fmt[i] = 1;
	dst->val[i] = pfld->v.byte;
   	dst->len[i] = 8;
}


static inline void db_str2pg_string(struct pg_params* dst, int i,
									db_fld_t* src)
{
	dst->fmt[i] = 1;
	dst->val[i] = src->v.lstr.s;
	dst->len[i] = src->v.lstr.len;
}


static inline void db_cstr2pg_string(struct pg_params* dst, int i,
									 db_fld_t* src)
{
	dst->fmt[i] = 0;
	dst->val[i] = src->v.cstr;
}


int pg_fld2pg(struct pg_params* dst, int off, pg_type_t* types,
			  db_fld_t* src, unsigned int flags)
{
	int i;
	struct pg_fld* pfld;

	if (src == NULL) return 0;

	for(i = 0; !DB_FLD_EMPTY(src) && !DB_FLD_LAST(src[i]); i++) {
		pfld = DB_GET_PAYLOAD(src + i);

		/* NULL value */
		if (src[i].flags & DB_NULL) {
			dst->val[off + i] = NULL;
			dst->len[off + i] = 0;
			continue;
		}

		switch(src[i].type) {
		case DB_INT:
			if (pfld->oid == types[PG_INT2].oid)
				db_int2pg_int2(dst, off + i, src + i);
			else if (pfld->oid == types[PG_INT4].oid)
				db_int2pg_int4(dst, off + i, src + i);
			else if ((pfld->oid == types[PG_TIMESTAMP].oid) ||
					 (pfld->oid == types[PG_TIMESTAMPTZ].oid))
				db_int2pg_timestamp(dst, off + i, src + i, flags);
			else if (pfld->oid == types[PG_INT8].oid)
				db_int2pg_int8(dst, off + i, src + i);
			else if (pfld->oid == types[PG_INET].oid)
				db_int2pg_inet(dst, off + i, src + i);
			else if (pfld->oid == types[PG_BOOL].oid)
				db_int2pg_bool(dst, off + i, src + i);
			else if (pfld->oid == types[PG_BIT].oid)
				db_int2pg_bit(dst, off + i, src + i);
			else if (pfld->oid == types[PG_VARBIT].oid)
				db_int2pg_bit(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_BITMAP:
			if (pfld->oid == types[PG_INT4].oid)
				db_int2pg_int4(dst, off + i, src + i);
			else if (pfld->oid == types[PG_INT8].oid)
				db_int2pg_int8(dst, off + i, src + i);
			else if (pfld->oid == types[PG_BIT].oid)
				db_int2pg_bit(dst, off + i, src + i);
			else if (pfld->oid == types[PG_VARBIT].oid)
				db_int2pg_bit(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_DATETIME:
			if (pfld->oid == types[PG_INT4].oid)
				db_int2pg_int4(dst, off + i, src + i);
			else if ((pfld->oid == types[PG_TIMESTAMP].oid) ||
					 (pfld->oid == types[PG_TIMESTAMPTZ].oid))
				db_int2pg_timestamp(dst, off + i, src + i, flags);
			else if (pfld->oid == types[PG_INT8].oid)
				db_int2pg_int8(dst, off + i, src + i);
			else goto bug;
			break;
			 
		case DB_FLOAT:
			if (pfld->oid == types[PG_FLOAT4].oid)
				db_float2pg_float4(dst, off + i, src + i);
			else if (pfld->oid == types[PG_FLOAT8].oid)
				db_float2pg_float8(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_DOUBLE:
			if (pfld->oid == types[PG_FLOAT4].oid)
				db_double2pg_float4(dst, off + i, src + i);
			else if (pfld->oid == types[PG_FLOAT8].oid)
				db_double2pg_float8(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_STR:
			if (pfld->oid == types[PG_VARCHAR].oid ||
				pfld->oid == types[PG_BYTE].oid ||
				pfld->oid == types[PG_CHAR].oid ||
				pfld->oid == types[PG_TEXT].oid ||
				pfld->oid == types[PG_BPCHAR].oid)
				db_str2pg_string(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_CSTR:
			if (pfld->oid == types[PG_VARCHAR].oid ||
				pfld->oid == types[PG_BYTE].oid ||
				pfld->oid == types[PG_CHAR].oid ||
				pfld->oid == types[PG_TEXT].oid ||
				pfld->oid == types[PG_BPCHAR].oid)
				db_cstr2pg_string(dst, off + i, src + i);
			else goto bug;
			break;

		case DB_BLOB:
			if (pfld->oid == types[PG_BYTE].oid)
				db_str2pg_string(dst, off + i, src + i);
			else goto bug;
			break;

		default:
			BUG("postgres: Unsupported field type %d in field %s\n",
				src[i].type, src[i].name);
			return -1;
		}
	}

	return 0;

 bug:
	BUG("postgres: Error while converting DB API type %d to Postgres Oid %d\n",
		src[i].type, pfld->oid);
	return -1;

}


int pg_check_fld2pg(db_fld_t* fld, pg_type_t* types)
{
	int i;
	const char* name = "UNKNOWN";
	struct pg_fld* pfld;

	if (fld == NULL) return 0;

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		pfld = DB_GET_PAYLOAD(fld + i);
		switch(fld[i].type) {
		case DB_INT:
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_INT8].oid) continue;
			if (pfld->oid == types[PG_BOOL].oid) continue;
			if (pfld->oid == types[PG_INET].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMP].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMPTZ].oid) continue;
			if (pfld->oid == types[PG_BIT].oid) continue;
			if (pfld->oid == types[PG_VARBIT].oid) continue;
			break;

		case DB_BITMAP:
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_INT8].oid) continue;
			if (pfld->oid == types[PG_BIT].oid) continue;
			if (pfld->oid == types[PG_VARBIT].oid) continue;
			break;

		case DB_FLOAT:
		case DB_DOUBLE:
			if (pfld->oid == types[PG_FLOAT4].oid) continue;
			if (pfld->oid == types[PG_FLOAT8].oid) continue;
			break;

		case DB_CSTR:
		case DB_STR:
			if (pfld->oid == types[PG_BYTE].oid) continue;
			if (pfld->oid == types[PG_CHAR].oid) continue;
			if (pfld->oid == types[PG_TEXT].oid) continue;
			if (pfld->oid == types[PG_BPCHAR].oid) continue;
			if (pfld->oid == types[PG_VARCHAR].oid) continue;
			break;

		case DB_BLOB:
			if (pfld->oid == types[PG_BYTE].oid) continue;
			break;

		case DB_DATETIME:
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_INT8].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMP].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMPTZ].oid) continue;
			break;

		default:
			BUG("postgres: Unsupported field type %d, bug in postgres module\n",
				fld[i].type);
			return -1;
		}

		pg_oid2name(&name, types, pfld->oid);
		ERR("postgres: Cannot convert column '%s' of type %s "
			"to PostgreSQL column type '%s'\n", 
			fld[i].name, db_fld_str[fld[i].type], name);
		return -1;
	}
	return 0;
}


int pg_resolve_param_oids(db_fld_t* vals, db_fld_t* match, int n1, int n2, PGresult* types)
{
	struct pg_fld* pfld;
	int i;

	if (n1 + n2 != PQnparams(types)) {
		ERR("postgres: Number of command parameters do not match\n");
		return -1;
	}

	for(i = 0; i < n1; i++) {
		pfld = DB_GET_PAYLOAD(vals + i);
		pfld->oid = PQparamtype(types, i);
	}

	for(i = 0; i < n2; i++) {
		pfld = DB_GET_PAYLOAD(match + i);
		pfld->oid = PQparamtype(types, n1 + i);
	}

	return 0;
}


int pg_resolve_result_oids(db_fld_t* fld, int n, PGresult* types)
{
	struct pg_fld* pfld;
	int i;
	if (fld == NULL) return 0;

	if (n != PQnfields(types)) {
		ERR("postgres: Result field numbers do not match\n");
		return -1;
	}

	for(i = 0; i < n; i++) {
		pfld = DB_GET_PAYLOAD(fld + i);
		pfld->oid = PQftype(types, i);
	}

	return 0;
}


int pg_check_pg2fld(db_fld_t* fld, pg_type_t* types)
{
	int i;
	const char* name = "UNKNOWN";
	struct pg_fld* pfld;

	if (fld == NULL) return 0;

	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		pfld = DB_GET_PAYLOAD(fld + i);

		if (pfld->oid == 0) {
			ERR("postgres: Unknown type fields not supported\n");
			return -1;
		}

		switch(fld[i].type) {
		case DB_INT:
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_INT8].oid) continue;
			if (pfld->oid == types[PG_BOOL].oid) continue;
			if (pfld->oid == types[PG_INET].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMP].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMPTZ].oid) continue;
			if (pfld->oid == types[PG_BIT].oid) continue;
			if (pfld->oid == types[PG_VARBIT].oid) continue;
			break;

		case DB_BITMAP:
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_INT8].oid) continue;
			if (pfld->oid == types[PG_BIT].oid) continue;
			if (pfld->oid == types[PG_VARBIT].oid) continue;
			break;

		case DB_FLOAT:
			if (pfld->oid == types[PG_FLOAT4].oid) continue;
			break;

		case DB_DOUBLE:
			if (pfld->oid == types[PG_FLOAT4].oid) continue;
			if (pfld->oid == types[PG_FLOAT8].oid) continue;
			break;

		case DB_CSTR:
			if (pfld->oid == types[PG_CHAR].oid) continue;
			if (pfld->oid == types[PG_TEXT].oid) continue;
			if (pfld->oid == types[PG_BPCHAR].oid) continue;
			if (pfld->oid == types[PG_VARCHAR].oid) continue;
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			break;

		case DB_STR:
		case DB_BLOB:
			if (pfld->oid == types[PG_BYTE].oid) continue;
			if (pfld->oid == types[PG_CHAR].oid) continue;
			if (pfld->oid == types[PG_TEXT].oid) continue;
			if (pfld->oid == types[PG_BPCHAR].oid) continue;
			if (pfld->oid == types[PG_VARCHAR].oid) continue;
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			break;

		case DB_DATETIME:
			if (pfld->oid == types[PG_INT2].oid) continue;
			if (pfld->oid == types[PG_INT4].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMP].oid) continue;
			if (pfld->oid == types[PG_TIMESTAMPTZ].oid) continue;
			break;

		default:
			BUG("postgres: Unsupported field type %d, bug in postgres module\n",
				fld[i].type);
			return -1;
		}

		pg_oid2name(&name, types, pfld->oid);
		ERR("postgres: Cannot convert column '%s' of type %s "
			"to DB API field of type %s\n", 
			fld[i].name, name, db_fld_str[fld[i].type]);
		return -1;
	}
	return 0;
}


static inline int pg_int2_2_db_cstr(db_fld_t* fld, char* val, int len)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(fld);
	int size, v;

	v = (int16_t)ntohs(*((int16_t*)val));

    size = snprintf(pfld->buf, INT2STR_MAX_LEN, "%-d", v);
    if (size < 0 || size >= INT2STR_MAX_LEN) {
        BUG("postgres: Error while converting integer to string\n");
        return -1;
    }

	fld->v.cstr = pfld->buf;
	return 0;
}


static inline int pg_int4_2_db_cstr(db_fld_t* fld, char* val, int len)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(fld);
	int size, v;

	v = (int32_t)ntohl(*((int32_t*)val));

    size = snprintf(pfld->buf, INT2STR_MAX_LEN, "%-d", v);
    if (len < 0 || size >= INT2STR_MAX_LEN) {
        BUG("postgres: Error while converting integer to string\n");
        return -1;
    }

	fld->v.cstr = pfld->buf;
	return 0;
}


static inline int pg_int2_2_db_str(db_fld_t* fld, char* val, int len)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(fld);
	int size, v;

	v = (int16_t)ntohs(*((int16_t*)val));

    size = snprintf(pfld->buf, INT2STR_MAX_LEN, "%-d", v);
    if (size < 0 || size >= INT2STR_MAX_LEN) {
        BUG("postgres: Error while converting integer to string\n");
        return -1;
    }

	fld->v.lstr.s = pfld->buf;
	fld->v.lstr.len = size;
	return 0;
}


static inline int pg_int4_2_db_str(db_fld_t* fld, char* val, int len)
{
	struct pg_fld* pfld = DB_GET_PAYLOAD(fld);
	int size, v;

	v = (int32_t)ntohl(*((int32_t*)val));

    size = snprintf(pfld->buf, INT2STR_MAX_LEN, "%-d", v);
    if (size < 0 || size >= INT2STR_MAX_LEN) {
        BUG("postgres: Error while converting integer to string\n");
        return -1;
    }

	fld->v.lstr.s = pfld->buf;
	fld->v.lstr.len = size;
	return 0;
}


static inline int pg_int2_2_db_int(db_fld_t* fld, char* val, int len)
{
	fld->v.int4 = (int16_t)ntohs(*((int16_t*)val));
	return 0;
}


static inline int pg_int4_2_db_int(db_fld_t* fld, char* val, int len)
{
	fld->v.int4 = (int32_t)ntohl(*((int32_t*)val));
	return 0;
}


static inline int pg_int8_2_db_int(db_fld_t* fld, char* val, int len)
{
	fld->v.int8 = (int64_t)ntohll(*((int64_t*)val));
	return 0;
}


static inline int pg_bool2db_int(db_fld_t* fld, char* val, int len)
{
	fld->v.int4 = val[0];
	return 0;
}


static inline int pg_inet2db_int(db_fld_t* fld, char* val, int len)
{
	if (len != 8 || val[2] != 0) {
		ERR("postgres: Unsupported 'inet' format, column %s\n", fld->name);
		return -1;
	}

	if (val[0] != AF_INET) {
		ERR("postgres: Unsupported address family %d in field %s\n",
			val[0], fld->name);
		return -1;
	}

	if (val[1] != 32) {
		WARN("postgres: Netmasks shorter than 32-bits not supported, "
			 "column %s\n", fld->name);
	}

	if (val[3] != 4) {
		ERR("postgres: Unsupported IP address size %d in column %s\n",
			val[3], fld->name);
		return -1;
	}

	fld->v.int4 = (int32_t)ntohl(((int32_t*)val)[1]);
	return 0;
}


static inline int pg_timestamp2db_int(db_fld_t* fld, char* val, int len, 
									  unsigned int flags)
{
	if (flags & PG_INT8_TIMESTAMP) {
		/* int8 format */
		fld->v.int4 = (int64_t)ntohll(((int64_t*)val)[0]) / (int64_t)1000000 + PG_EPOCH_TIME;
	} else {
		/* double format */
		fld->v.int4 = PG_EPOCH_TIME + ntohll(((int64_t*)val)[0]);
	}
	return 0;
}


static inline int pg_bit2db_int(db_fld_t* fld, char* val, int len)
{
	int size;

	size = ntohl(*(uint32_t*)val);
	if (size != 32) {
		ERR("postgres: Unsupported bit field size (%d), column %s\n",
			size, fld->name);
		return -1;
	}
	fld->v.int4 = ntohl(((uint32_t*)val)[1]);
	return 0;
}


static inline int pg_float42db_float(db_fld_t* fld, char* val, int len)
{
	fld->v.int4 = (uint32_t)ntohl(*(uint32_t*)val);
	return 0;
}


static inline int pg_float42db_double(db_fld_t* fld, char* val, int len)
{
	float tmp;

	tmp = ntohl(*(uint32_t*)val);
	fld->v.dbl = tmp;
	return 0;
}


static inline int pg_float82db_double(db_fld_t* fld, char* val, int len)
{
	fld->v.int8 = ntohll(*(uint64_t*)val);
	return 0;
}


static inline int pg_string2db_cstr(db_fld_t* fld, char* val, int len)
{
	fld->v.cstr = val;
	return 0;
}


static inline int pg_string2db_str(db_fld_t* fld, char* val, int len)
{
	fld->v.lstr.s = val;
	fld->v.lstr.len = len;
	return 0;
}



int pg_pg2fld(db_fld_t* dst, PGresult* src, int row, 
			  pg_type_t* types, unsigned int flags)
{
	char* val;
	int i, len, ret;
	Oid type;
	
	if (dst == NULL || src == NULL) return 0;
	ret = 0;

	for(i = 0; !DB_FLD_EMPTY(dst) && !DB_FLD_LAST(dst[i]); i++) {
		if (PQgetisnull(src, row, i)) {
			dst[i].flags |= DB_NULL;
			continue;
		} else {
			dst[i].flags &= ~DB_NULL;
		}

		type = PQftype(src, i);
		val = PQgetvalue(src, row, i);
		len = PQgetlength(src, row, i);		

		switch(dst[i].type) {
		case DB_INT:
			if (type == types[PG_INT2].oid)
				ret |= pg_int2_2_db_int(dst + i, val, len);
			else if (type == types[PG_INT4].oid)
				ret |= pg_int4_2_db_int(dst + i, val, len);
			else if (type == types[PG_INT8].oid)
				ret |= pg_int8_2_db_int(dst + i, val, len);
			else if (type == types[PG_BOOL].oid)
				ret |= pg_bool2db_int(dst + i, val, len);
			else if (type == types[PG_INET].oid)
				ret |= pg_inet2db_int(dst + i, val, len);
			else if ((type == types[PG_TIMESTAMP].oid) ||
					 (type == types[PG_TIMESTAMPTZ].oid))
				ret |= pg_timestamp2db_int(dst + i, val, len, flags);
			else if (type == types[PG_BIT].oid)
				ret |= pg_bit2db_int(dst + i, val, len);
			else if (type == types[PG_VARBIT].oid)
				ret |= pg_bit2db_int(dst + i, val, len);
			else goto bug;
			break;

		case DB_FLOAT:
			if (type == types[PG_FLOAT4].oid)
				ret |= pg_float42db_float(dst + i, val, len);
			else goto bug;
			break;

		case DB_DOUBLE:
			if (type == types[PG_FLOAT4].oid)
				ret |= pg_float42db_double(dst + i, val, len);
			else if (type == types[PG_FLOAT8].oid)
				ret |= pg_float82db_double(dst + i, val, len);
			else goto bug;
			break;

		case DB_DATETIME:
			if (type == types[PG_INT2].oid)
				ret |= pg_int2_2_db_int(dst + i, val, len);
			else if (type == types[PG_INT4].oid)
				ret |= pg_int4_2_db_int(dst + i, val, len);
			else if ((type == types[PG_TIMESTAMP].oid) ||
					 (type == types[PG_TIMESTAMPTZ].oid))
				ret |= pg_timestamp2db_int(dst + i, val, len, flags);
			else goto bug;
			break;

		case DB_CSTR:
			if ((type == types[PG_CHAR].oid) ||
				(type == types[PG_TEXT].oid) ||
				(type == types[PG_BPCHAR].oid) ||
				(type == types[PG_VARCHAR].oid))
				ret |= pg_string2db_cstr(dst + i, val, len);
			else if (type == types[PG_INT2].oid)
				ret |= pg_int2_2_db_cstr(dst + i, val, len);
			else if (type == types[PG_INT4].oid)
				ret |= pg_int4_2_db_cstr(dst + i, val, len);
			else goto bug;
			break;

		case DB_STR:
		case DB_BLOB:
			if ((type == types[PG_BYTE].oid) ||
				(type == types[PG_CHAR].oid) ||
				(type == types[PG_TEXT].oid) ||
				(type == types[PG_BPCHAR].oid) ||
				(type == types[PG_VARCHAR].oid))
				ret |= pg_string2db_str(dst + i, val, len);
			else if (type == types[PG_INT2].oid)
				ret |= pg_int2_2_db_str(dst + i, val, len);
			else if (type == types[PG_INT4].oid)
				ret |= pg_int4_2_db_str(dst + i, val, len);
			else goto bug;
			break;

		case DB_BITMAP:
			if (type == types[PG_INT2].oid)
				ret |= pg_int2_2_db_int(dst + i, val, len);
			else if (type == types[PG_INT4].oid)
				ret |= pg_int4_2_db_int(dst + i, val, len);
			else if (type == types[PG_INT8].oid)
				ret |= pg_int8_2_db_int(dst + i, val, len);
			else if (type == types[PG_BIT].oid)
				ret |= pg_bit2db_int(dst + i, val, len);
			else if (type == types[PG_VARBIT].oid)
				ret |= pg_bit2db_int(dst + i, val, len);
			else goto bug;
			break;

		default:
			BUG("postgres: Unsupported field type %d in field %s\n",
				dst[i].type, dst[i].name);
			return -1;			
		}
	}
	return ret;

 bug:
	BUG("postgres: Error while converting Postgres Oid %d to DB API type %d\n",
		type, dst[i].type);
	return -1;
}
