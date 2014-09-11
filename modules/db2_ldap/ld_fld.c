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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \addtogroup ldap
 * @{
 */

/** \file
 * Data field conversion and type checking functions.
 */

#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#define _BSD_SOURCE /* snprintf */

#include "ld_fld.h"

#include "../../lib/srdb2/db_drv.h"
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
	if (payload->filter) pkg_free(payload->filter);
	payload->filter = NULL;
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
	/* YYYYMMDDHHMMSS[.sss][ 'Z' | ( {'+'|'-'} ZZZZ) ] */
	strptime(src->s, "%Y%m%d%H%M%S", &time);  /* Note: frac of seconds are lost in time_t representation */
	
	if (src->s[src->len-1] == 'Z' || src->s[src->len-5] == '-' || src->s[src->len-5] == '+') {
		/* GMT or specified TZ, no daylight saving time */
		#ifdef HAVE_TIMEGM
		*dst = timegm(&time);
		#else
		*dst = _timegm(&time);
		#endif /* HAVE_TIMEGM */

		if (src->s[src->len-1] != 'Z') {
			/* timezone is specified */
			memset(&time, '\0', sizeof(struct tm));
			strptime(src->s + src->len - 4, "%H%M", &time);
			switch (src->s[src->len-5]) {
				case '-':
					*dst -= time.tm_hour*3600+time.tm_min*60;
					break;
				case '+':
					*dst += time.tm_hour*3600+time.tm_min*60;
					break;
				default:
					;
			}
		}
	}
	else {
		/* it's local time */

		/* Daylight saving information got lost in the database
		 * so let timegm to guess it. This eliminates the bug when
		 * contacts reloaded from the database have different time
		 * of expiration by one hour when daylight saving is used
		 */
		time.tm_isdst = -1;
		*dst = timelocal(&time);
	}
	
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

static inline int ldap_fld2db_fld(db_fld_t* fld, str v) {

	switch(fld->type) {
		case DB_CSTR:
			fld->v.cstr = v.s;
			break;

		case DB_STR:
		case DB_BLOB:
			fld->v.lstr.s = v.s;
			fld->v.lstr.len = v.len;
			break;

		case DB_INT:
		case DB_BITMAP:
			if (v.s[0] == '\'' && v.s[v.len - 1] == 'B' &&
				v.s[v.len - 2] == '\'') {
				v.s++;
				v.len -= 3;
				if (ldap_bit2db_int(&fld->v.int4, &v) != 0) {
					ERR("ldap: Error while converting bit string '%.*s'\n",
						v.len, ZSW(v.s));
					return -1;
				}
				break;
			}

			if (v.len == 4 && !strncasecmp("TRUE", v.s, v.len)) {
				fld->v.int4 = 1;
				break;
			}

			if (v.len == 5 && !strncasecmp("FALSE", v.s, v.len)) {
				fld->v.int4 = 0;
				break;
			}

			if (ldap_int2db_int(&fld->v.int4, &v) != 0) {
				ERR("ldap: Error while converting %.*s to integer\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_DATETIME:
			if (ldap_gentime2db_datetime(&fld->v.time, &v) != 0) {
				ERR("ldap: Error while converting LDAP time value '%.*s'\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_FLOAT:
			/* We know that the ldap library zero-terminated v.s */
			if (ldap_str2db_float(&fld->v.flt, v.s) != 0) {
				ERR("ldap: Error while converting '%.*s' to float\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		case DB_DOUBLE:
			/* We know that the ldap library zero-terminated v.s */
			if (ldap_str2db_double(&fld->v.dbl, v.s) != 0) {
				ERR("ldap: Error while converting '%.*s' to double\n",
					v.len, ZSW(v.s));
				return -1;
			}
			break;

		default:
			ERR("ldap: Unsupported field type: %d\n", fld->type);
			return -1;
	}
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

#define CMP_NUM(fld_v,match_v,fld) \
	if (fld_v.fld == match_v.fld) \
		op = 0x02; \
	else if (fld_v.fld < match_v.fld) \
		op = 0x01; \
	else if (fld_v.fld > match_v.fld) \
		op = 0x04;
		
int ld_ldap2fldex(db_fld_t* fld, LDAP* ldap, LDAPMessage* msg, int init)
{
	int i;
	struct ld_fld* lfld;
	str v;

	if (fld == NULL || msg == NULL) return 0;
	for(i = 0; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(fld[i]); i++) {
		lfld = DB_GET_PAYLOAD(fld + i);
		if (init) {
			if (fld[i].type == DB_NONE) {
				switch (lfld->syntax) {
					case LD_SYNTAX_STRING:
						fld[i].type = DB_STR;
						break;
					case LD_SYNTAX_INT:
					case LD_SYNTAX_BOOL:
					case LD_SYNTAX_BIT:
						fld[i].type = DB_INT;
						break;
					case LD_SYNTAX_FLOAT:
						fld[i].type = DB_FLOAT;
						break;
						
					case LD_SYNTAX_GENTIME:
						fld[i].type = DB_DATETIME;
						break;
					case LD_SYNTAX_BIN:
						fld[i].type = DB_BITMAP;
						break;
				}

			}
			
			/* free the values of the previous object */
			if (lfld->values) ldap_value_free_len(lfld->values);
			lfld->values = ldap_get_values_len(ldap, msg, lfld->attr.s);
			lfld->index = 0;
			
			if (lfld->values == NULL || lfld->values[0] == NULL) {
				fld[i].flags |= DB_NULL;
				/* index == 0 means no value available */
				lfld->valuesnum = 0;
				if (lfld->client_side_filtering && lfld->filter) {
					int j;
					/* if the all filter conditions requires NULL value then we can accept the record */
					for (j=0; lfld->filter[j]; j++) {
						if (lfld->filter[j]->flags & DB_NULL && lfld->filter[j]->op == DB_EQ) {
							continue;
						}
						return 1; /* get next record */
					}
				}
			} else {
				/* init the number of values */
				fld[i].flags &= ~DB_NULL;
				lfld->valuesnum = ldap_count_values_len(lfld->values);
			
				if ((lfld->valuesnum > 1 || lfld->client_side_filtering) && lfld->filter) {
					
					/* in case of multivalue we must check if value fits in filter criteria.
					   LDAP returns record (having each multivalue) if one particular
					   multivalue fits in filter provided to LDAP search. We need
					   filter out these values manually. It not perfect because 
					   LDAP filtering may be based on different rule/locale than
					   raw (ASCII,...) comparision. 
					
					   We reorder values so we'll have interesting values located from top up to valuesnum at the end.

					   The same algorithm is applied for client side filtering
					   
					 */
					 
					do {
						int passed, j;
						for (j=0, passed = 1; lfld->filter[j] && passed; j++) {
							int op;  /* b0..less, b1..equal, b2..greater, zero..non equal */ 
							op = 0x00;
							if (lfld->filter[j]->flags & DB_NULL) {
								/* always non equal because field is not NULL */
							}
							else {
								
								v.s = lfld->values[lfld->index]->bv_val;
								v.len = lfld->values[lfld->index]->bv_len;
							
								if (ldap_fld2db_fld(fld + i, v) < 0) {
									passed = 0;
									break; /* for loop */
								}
								else {
									db_fld_val_t v;
									int t;
									static char buf[30];
									t = lfld->filter[j]->type;
									/* we need compare value provided in match condition with value returned by LDAP.
									   The match value should be the same type as LDAP value obtained during
									   db_cmd(). We implement some basic conversions.
									 */
									v = lfld->filter[j]->v;
									if (t == DB_CSTR) {
										v.lstr.s = v.cstr;
										v.lstr.len = strlen(v.lstr.s);
										t = DB_STR;
									} 
									switch (fld[i].type) {
										case DB_CSTR:
											fld[i].v.lstr.s = fld[i].v.cstr;
											fld[i].v.lstr.len = strlen(fld[i].v.lstr.s);
											fld[i].type = DB_STR; 
											/* no break */
										case DB_STR:
											
											switch (t) {
												case DB_INT:
													v.lstr.len = snprintf(buf, sizeof(buf)-1, "%d", v.int4);
													v.lstr.s = buf;
													break;
												/* numeric conversion for double/float not supported because of non unique string representation */
												default:
													goto skip_conv;
											}
											break;
										case DB_INT:
											switch (t) {
												case DB_DOUBLE:
													if ((double)(int)v.dbl != (double)v.dbl) 
														goto skip_conv;
													v.int4 = v.dbl;
													break;
												case DB_FLOAT:
													if ((float)(int)v.flt != (float)v.flt) 
														goto skip_conv;
													v.int4 = v.flt;
													break;
												case DB_STR: 
													if (v.lstr.len > 0) {
														char c, *p;
														int n;
														c = v.lstr.s[v.lstr.len];
														v.lstr.s[v.lstr.len] = '\0';
														n = strtol(v.lstr.s, &p, 10);
														v.lstr.s[v.lstr.len] = c;
														if ((*p) != '\0') {
															goto skip_conv;
														}
														v.int4 = n;
													}
													break;
												default:
													goto skip_conv;
											}
											break;
										case DB_FLOAT:
											switch (t) {
												case DB_DOUBLE:
													v.flt = v.dbl;
													break;
												case DB_INT:
													v.flt = v.int4;
													break;
												#ifdef  __USE_ISOC99
												case DB_STR: 
													if (v.lstr.len > 0) {
														char c, *p;
														float n;
														c = v.lstr.s[v.lstr.len];
														v.lstr.s[v.lstr.len] = '\0';
														n = strtof(v.lstr.s, &p);
														v.lstr.s[v.lstr.len] = c;
														if ((*p) != '\0') {
															goto skip_conv;
														}
														v.flt = n;
													}
													break;
												#endif
												default:
													goto skip_conv;
											}
											break;
										case DB_DOUBLE:
											switch (t) {
												case DB_FLOAT:
													v.dbl = v.flt;
													break;
												case DB_INT:
													v.dbl = v.int4;
													break;
												case DB_STR: 
													if (v.lstr.len > 0) {
														char c, *p;
														double n;
														c = v.lstr.s[v.lstr.len];
														v.lstr.s[v.lstr.len] = '\0';
														n = strtod(v.lstr.s, &p);
														v.lstr.s[v.lstr.len] = c;
														if ((*p) != '\0') {
															goto skip_conv;
														}
														v.dbl = n;
													}
													break;
												default:
													goto skip_conv;
											}
											break;
										case DB_BLOB:
										case DB_BITMAP:
										case DB_DATETIME:
										default:
											goto skip_conv;
										}
									t = fld[i].type;
								skip_conv:
									if (t == fld[i].type) {
									
										switch (fld[i].type) {
											case DB_CSTR: /* impossible, already converted to DB_STR */
											case DB_STR:
												if (fld[i].v.lstr.len == v.lstr.len) {
													op = strncmp(fld[i].v.lstr.s, v.lstr.s, v.lstr.len);
													if (op < 0) 
														op = 0x01;
													else if (op > 0)
														op = 0x04;
													else
														op = 0x02;
												}
												else if (fld[i].v.lstr.len < v.lstr.len) {
													op = strncmp(fld[i].v.lstr.s, v.lstr.s, fld[i].v.lstr.len);
													if (op < 0) 
														op = 0x01;
													else 
														op = 0x04;
												}
												else /* if (fld[i].v.lstr.len > v.lstr.len) */ {
													op = strncmp(fld[i].v.lstr.s, v.lstr.s, v.lstr.len);
													if (op > 0) 
														op = 0x04;
													else 
														op = 0x01;
												}
												break;
											case DB_BLOB:
												if (fld[i].v.blob.len == v.blob.len && memcmp(fld[i].v.blob.s, v.blob.s, v.blob.len) == 0)
													op = 0x02;
												break;											
											case DB_INT:
												CMP_NUM(fld[i].v, v, int4);
												break; 
											case DB_BITMAP:
												CMP_NUM(fld[i].v, v, bitmap);
												break; 
											case DB_DATETIME:
												CMP_NUM(fld[i].v, v, time);
												break; 
											case DB_FLOAT:
												CMP_NUM(fld[i].v, v, flt);
												break; 
											case DB_DOUBLE:
												CMP_NUM(fld[i].v, v, dbl);
												break; 
											default:
												;
										}
									}
									 
								}
							}
							switch (lfld->filter[j]->op) {
								case DB_EQ:
									passed = op == 0x02;
									break;
								case DB_NE:
									passed = (op & 0x02) == 0;
									break;
								case DB_LT:
									passed = op == 0x01;
									break;
								case DB_LEQ:
									passed = op == 0x01 || op == 0x02;
									break;
								case DB_GT:
									passed = op == 0x04;
									break;
								case DB_GEQ:
									passed = op == 0x04 || op == 0x02;
									break;
								default:
									;
							}
						}
						
						if (passed) {
							lfld->index++;	
						}
						else {
							char *save_bvval;
							int save_bvlen;
							int i;
							/* shift following values, push useless value at the end and decrease num of values */
							
							save_bvval = lfld->values[lfld->index]->bv_val;
							save_bvlen = lfld->values[lfld->index]->bv_len;
							for (i=lfld->index+1; i < lfld->valuesnum; i++) {
								 lfld->values[i-1]->bv_val = lfld->values[i]->bv_val;
								 lfld->values[i-1]->bv_len = lfld->values[i]->bv_len;
							}
							lfld->values[lfld->valuesnum-1]->bv_val = save_bvval;
							lfld->values[lfld->valuesnum-1]->bv_len = save_bvlen;
							lfld->valuesnum--;
						}
				
					} while (lfld->index < lfld->valuesnum);
						
					if (lfld->valuesnum == 0) {
						return 1;  /* get next record */
					}
				}
			}	
			/* pointer to the current value */
			lfld->index = 0;
		}

		/* this is an empty value */
		if (!lfld->valuesnum)
			continue;

		v.s = lfld->values[lfld->index]->bv_val;
		v.len = lfld->values[lfld->index]->bv_len;

		if (ldap_fld2db_fld(fld + i, v) < 0) 
			return -1;
		
	}
	return 0;
}


static inline int db_int2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	int len;
	char tmp[INT2STR_MAX_LEN + 1];

	len = snprintf(tmp, INT2STR_MAX_LEN + 1, "%-d", fld->v.int4);
	if (len < 0 || len >= INT2STR_MAX_LEN + 1) {
		BUG("ldap: Error while converting integer to string\n");
		return -1;
	}
	return sb_add(buf, tmp, len);
}


static inline int db_datetime2ldap_gentime(struct sbuf* buf, db_fld_t* fld)
{
	char tmp[16];
	struct tm* t;

	t = gmtime(&fld->v.time);
	if (strftime(tmp, sizeof(tmp), "%Y%m%d%H%M%SZ", t) != sizeof(tmp)-1) {
		ERR("ldap: Error while converting time_t value to LDAP format\n");
		return -1;
	}
	return sb_add(buf, tmp, sizeof(tmp)-1);
}


static inline int db_int2ldap_bool(struct sbuf* buf, db_fld_t* fld)
{
	if (fld->v.int4) 
	    return sb_add(buf, "TRUE", 4);
	else 
	    return sb_add(buf, "FALSE", 5);
}


static inline int db_uint2ldap_int(struct sbuf* buf, db_fld_t* fld)
{
	char* num;
	int len, v;
	
	v = fld->v.int4;
	num = int2str(v, &len);
	return sb_add(buf, num, len);
}


static inline int db_bit2ldap_bitstr(struct sbuf* buf, db_fld_t* fld)
{
	int rv, i;

	rv = 0;
	rv |= sb_add(buf, "'", 1);

	i = 1 << (sizeof(fld->v.int4) * 8 - 1);
	while(i) {
		rv |= sb_add(buf, (fld->v.int4 & i)?"1":"0", 1);
		i = i >> 1;
	}
	rv |= sb_add(buf, "'B", 2);
	return rv;
}


static inline int db_float2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	char tmp[16];
	int len;

	len = snprintf(tmp, 16, "%-10.2f", fld->v.flt);
	if (len < 0 || len >= 16) {
		BUG("ldap: Error while converting float to string\n");
		return -1;
	}
	return sb_add(buf, tmp, len);
}


static inline int db_double2ldap_str(struct sbuf* buf, db_fld_t* fld)
{
	char tmp[16];
	int len;

	len = snprintf(tmp, 16, "%-10.2f", fld->v.dbl);
	if (len < 0 || len >= 16) {
		BUG("ldap: Error while converting double to string\n");
		return -1;
	}
	return sb_add(buf, tmp, len);
}

static inline int ld_db2ldap(struct sbuf* buf, db_fld_t* fld) {
	struct ld_fld* lfld;
	
	lfld = DB_GET_PAYLOAD(fld);

	switch(fld->type) {
		case DB_CSTR:
			if (sb_add_esc(buf, fld->v.cstr,
				 fld->v.cstr ? strlen(fld->v.cstr) : 0) != 0) goto error;
			break;

		case DB_STR:
			if (sb_add_esc(buf, fld->v.lstr.s, fld->v.lstr.len) != 0) goto error;
			break;

		case DB_INT:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_INT:
			case LD_SYNTAX_FLOAT:
				if (db_int2ldap_str(buf, fld))
					goto error;
				break;

			case LD_SYNTAX_GENTIME:
				if (db_datetime2ldap_gentime(buf, fld))
					goto error;
				break;

			case LD_SYNTAX_BIT:
				if (db_bit2ldap_bitstr(buf, fld))
					goto error;
				break;

			case LD_SYNTAX_BOOL:
				if (db_int2ldap_bool(buf, fld))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert integer field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_BITMAP:
			switch(lfld->syntax) {
			case LD_SYNTAX_INT:
				if (db_uint2ldap_int(buf, fld))
					goto error;
				break;

			case LD_SYNTAX_BIT:
			case LD_SYNTAX_STRING:
				if (db_bit2ldap_bitstr(buf, fld))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert bitmap field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_DATETIME:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_GENTIME:
				if (db_datetime2ldap_gentime(buf, fld))
					goto error;
				break;

			case LD_SYNTAX_INT:
				if (db_uint2ldap_int(buf, fld))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert datetime field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		case DB_FLOAT:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_FLOAT:
				if (db_float2ldap_str(buf, fld))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert float field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}

		case DB_DOUBLE:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_FLOAT:
				if (db_float2ldap_str(buf, fld))
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert double field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
				break;
			}
			break;

		case DB_BLOB:
			switch(lfld->syntax) {
			case LD_SYNTAX_STRING:
			case LD_SYNTAX_BIN:
				if (sb_add_esc(buf, fld->v.lstr.s, fld->v.lstr.len) < 0)
					goto error;
				break;

			default:
				ERR("ldap: Cannot convert binary field %s "
					"to LDAP attribute %.*s\n",
					fld->name, lfld->attr.len, ZSW(lfld->attr.s));
				goto error;
			}
			break;

		default:
			BUG("ldap: Unsupported field type encountered: %d\n", fld->type);
			goto error;
		}
	return 0;
error:
	return -1;
}

/* skip fields belonging to a field which is requested for filtering at client side */
inline static void skip_client_side_filtering_fields(db_cmd_t* cmd, db_fld_t **fld) {
	struct ld_fld* lfld;
	db_fld_t *f;
try_next:
	if (DB_FLD_EMPTY(*fld) || DB_FLD_LAST(**fld)) return; 
	for (f=cmd->result; !DB_FLD_EMPTY(f) && !DB_FLD_LAST(*f); f++) {
		lfld = DB_GET_PAYLOAD(f);
		if (lfld->client_side_filtering && lfld->filter) {
			int j;
			for (j = 0; lfld->filter[j]; j++) {
				if (lfld->filter[j] == *fld) {
					(*fld)++;
					goto try_next;
				}
			}
		}
	}
}
		
int ld_prepare_ldap_filter(char** filter, db_cmd_t* cmd, str* add)
{
	db_fld_t* fld;
	struct ld_fld* lfld;
	int rv = 0;
	struct sbuf buf = {
		.s = NULL, .len = 0,
		.size = 0, .increment = 128
	};

	fld = cmd->match;
	skip_client_side_filtering_fields(cmd, &fld);
	
	/* Return NULL if there are no fields and no preconfigured search
	 * string supplied in the configuration file
	 */
	if ((DB_FLD_EMPTY(fld) || DB_FLD_LAST(*fld)) && ((add->s == NULL) || !add->len)) {
		*filter = NULL;
		return 0;
	}

	rv = sb_add(&buf, "(&", 2);
	if (add->s && add->len) {
		/* Add the filter component specified in the config file */
		rv |= sb_add(&buf, add->s, add->len);
	}

	for(; !DB_FLD_EMPTY(fld) && !DB_FLD_LAST(*fld); fld++, skip_client_side_filtering_fields(cmd, &fld)) {
		int op;
		lfld = DB_GET_PAYLOAD(fld);

		op = fld->op;
		
		if (fld->flags & DB_NULL) {
			switch (op) {
				case DB_EQ:
					/* fld==NULL -> (!(x=*)) */
					op = DB_NE;

				case DB_NE:
					/* fld!=NULL -> (x=*) */
					op = DB_EQ;
					break;
				default:
					ERR("ldap: Cannot compare null value field %s\n", fld->name);				
					goto error;
			}	
		}
		
		/* we need construct operators as:
		    not:  (!(fld=val))
		    </>:  (!(fld=val))(fld</>val)
		*/
		switch (op) {
			case DB_LT:
			case DB_GT:
			case DB_NE:
				rv |= sb_add(&buf, "(!(", 3);
				rv |= sb_add(&buf, lfld->attr.s, lfld->attr.len);
				rv |= sb_add(&buf, "=", 1);
				if (fld->flags & DB_NULL) {
					rv |= sb_add(&buf, "*", 1);
				}
				else {
					if (ld_db2ldap(&buf, fld) < 0) {
						goto error;
					}
				}
				rv |= sb_add(&buf, "))", 2);
				break;
			default:
			    ;
		}
		if (op != DB_NE) {
			rv |= sb_add(&buf, "(", 1);
			rv |= sb_add(&buf, lfld->attr.s, lfld->attr.len);
			switch (op) {
				case DB_LEQ:
				case DB_LT:
					rv |= sb_add(&buf, "<=", 2);
					break;
				case DB_GEQ:
				case DB_GT:
					rv |= sb_add(&buf, ">=", 2);
					break;
				case DB_EQ:
					rv |= sb_add(&buf, "=", 1);
					break;
				default:
				;
			}
			if (fld->flags & DB_NULL) {
				rv |= sb_add(&buf, "*", 1);
			}
			else {
				if (ld_db2ldap(&buf, fld) < 0) {
					goto error;
				}
			}
			rv |= sb_add(&buf, ")", 1);
		}		
	}

	rv |= sb_add(&buf, ")", 1);
	rv |= sb_add(&buf, "", 1);
	if (rv) goto error;

	*filter = buf.s;
	return 0;

error:
	if (buf.s) pkg_free(buf.s);
	return -1;
}


/** @} */
