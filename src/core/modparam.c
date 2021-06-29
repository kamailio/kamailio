/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Configuration parameters for modules (modparams)
 * \ingroup core
 * Module: \ref core
 */


#include "modparam.h"
#include "dprint.h"
#include "fmsg.h"
#include "pvar.h"
#include "str_list.h"
#include "mem/mem.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>

static str_list_t *_ksr_modparam_strlist = NULL;

static char *get_mod_param_type_str(int ptype)
{
	if(ptype & PARAM_USE_FUNC) {
		if(ptype & PARAM_STRING) {
			return "func-string";
		} else if (ptype & PARAM_INT) {
			return "func-int";
		} else if (ptype & PARAM_STR) {
			return "func-str";
		} else {
			return "func-unknown";
		}
	}
	if(ptype & PARAM_STRING) {
		return "string";
	} else if (ptype & PARAM_INT) {
		return "int";
	} else if (ptype & PARAM_STR) {
		return "str";
	} else {
		return "unknown";
	}
}

int set_mod_param(char* _mod, char* _name, modparam_t _type, void* _val)
{
	return set_mod_param_regex(_mod, _name, _type, _val);
}

int set_mod_param_regex(char* regex, char* name, modparam_t type, void* val)
{
	struct sr_module* t;
	regex_t preg;
	int mod_found, len;
	char* reg;
	void *ptr, *val2;
	modparam_t param_type;
	str s;

	if (!regex) {
		LM_ERR("Invalid mod parameter value\n");
		return -5;
	}
	if (!name) {
		LM_ERR("Invalid name parameter value\n");
		return -6;
	}

	len = strlen(regex);
	reg = pkg_malloc(len + 4 + 1);
	if (reg == 0) {
		PKG_MEM_ERROR;
		return -1;
	}
	reg[0] = '^';
	reg[1] = '(';
	memcpy(reg + 2, regex, len);
	reg[len + 2] = ')';
	reg[len + 3] = '$';
	reg[len + 4] = '\0';

	if (regcomp(&preg, reg, REG_EXTENDED | REG_NOSUB | REG_ICASE)) {
		LM_ERR("Error while compiling regular expression\n");
		pkg_free(reg);
		return -2;
	}

	mod_found = 0;
	for(t = modules; t; t = t->next) {
		if (regexec(&preg, t->exports.name, 0, 0, 0) == 0) {
			LM_DBG("'%s' matches module '%s'\n", regex, t->exports.name);
			mod_found = 1;
			/* PARAM_STR (PARAM_STRING) may be assigned also to PARAM_STRING(PARAM_STR) so let get both module param */
			ptr = find_param_export(t, name, type | ((type & (PARAM_STR|PARAM_STRING))?PARAM_STR|PARAM_STRING:0), &param_type);
			if (ptr) {
				     /* type casting */
				if (type == PARAM_STRING && PARAM_TYPE_MASK(param_type) == PARAM_STR) {
					s.s = (char*)val;
					s.len = s.s ? strlen(s.s) : 0;
					val2 = &s;
				} else if (type == PARAM_STR && PARAM_TYPE_MASK(param_type) == PARAM_STRING) {
					s = *(str*)val;
					val2 = s.s;	/* zero terminator expected */
				} else {
					val2 = val;
				}
				LM_DBG("found <%s> in module %s [%s]\n", name, t->exports.name, t->path);
				if (param_type & PARAM_USE_FUNC) {
					if ( ((param_func_t)(ptr))(param_type, val2) < 0) {
						regfree(&preg);
						pkg_free(reg);
						return -4;
					}
				}
				else {
					switch(PARAM_TYPE_MASK(param_type)) {
						case PARAM_STRING:
							*((char**)ptr) = pkg_malloc(strlen((char*)val2)+1);
							if (!*((char**)ptr)) {
								PKG_MEM_ERROR;
								regfree(&preg);
								pkg_free(reg);
								return -1;
							}
							strcpy(*((char**)ptr), (char*)val2);
							break;

						case PARAM_STR:
							((str*)ptr)->s = pkg_malloc(((str*)val2)->len+1);
							if (!((str*)ptr)->s) {
								PKG_MEM_ERROR;
								regfree(&preg);
								pkg_free(reg);
								return -1;
							}
							memcpy(((str*)ptr)->s, ((str*)val2)->s, ((str*)val2)->len);
							((str*)ptr)->len = ((str*)val2)->len;
							((str*)ptr)->s[((str*)ptr)->len] = 0;
							break;

						case PARAM_INT:
							*((int*)ptr) = (int)(long)val2;
							break;
					}
				}
			}
			else {
				LM_ERR("parameter <%s> of type <%d:%s> not found in module <%s>\n",
						name, type, get_mod_param_type_str(type), t->exports.name);
				regfree(&preg);
				pkg_free(reg);
				return -3;
			}
		}
	}

	regfree(&preg);
	pkg_free(reg);
	if (!mod_found) {
		LM_ERR("No module matching <%s> found\n", regex);
		return -4;
	}
	return 0;
}

int modparamx_set(char* mname, char* pname, modparam_t ptype, void* pval)
{
	str seval;
	str sfmt;
	sip_msg_t *fmsg;
	char* emname;
	char* epname;
	pv_spec_t *pvs;
	pv_value_t pvv;
	str_list_t *sb;

	emname = mname;
	if(strchr(mname, '$') != NULL) {
		fmsg = faked_msg_get_next();
		sfmt.s = mname;
		sfmt.len = strlen(sfmt.s);
		if(pv_eval_str(fmsg, &seval, &sfmt)>=0) {
			sb = str_list_block_add(&_ksr_modparam_strlist, seval.s, seval.len);
			if(sb==NULL) {
				LM_ERR("failed to handle parameter type: %d\n", ptype);
				return -1;
			}
			emname = sb->s.s;
		}
	}

	epname = pname;
	if(strchr(pname, '$') != NULL) {
		fmsg = faked_msg_get_next();
		sfmt.s = pname;
		sfmt.len = strlen(sfmt.s);
		if(pv_eval_str(fmsg, &seval, &sfmt)>=0) {
			sb = str_list_block_add(&_ksr_modparam_strlist, seval.s, seval.len);
			if(sb==NULL) {
				LM_ERR("failed to handle parameter type: %d\n", ptype);
				return -1;
			}
			epname = sb->s.s;
		}
	}

	switch(ptype) {
		case PARAM_STRING:
			if(strchr((char*)pval, '$') != NULL) {
				fmsg = faked_msg_get_next();
				sfmt.s = (char*)pval;
				sfmt.len = strlen(sfmt.s);
				if(pv_eval_str(fmsg, &seval, &sfmt)>=0) {
					sb = str_list_block_add(&_ksr_modparam_strlist, seval.s, seval.len);
					if(sb==NULL) {
						LM_ERR("failed to handle parameter type: %d\n", ptype);
						return -1;
					}
					return set_mod_param_regex(emname, epname, PARAM_STRING,
							(void*)sb->s.s);
				} else {
					LM_ERR("failed to evaluate parameter [%s]\n", (char*)pval);
					return -1;
				}
			} else {
				return set_mod_param_regex(emname, epname, PARAM_STRING, pval);
			}
		case PARAM_INT:
			return set_mod_param_regex(emname, epname, PARAM_INT, pval);
		case PARAM_VAR:
			sfmt.s = (char*)pval;
			sfmt.len = strlen(sfmt.s);
			seval.len = pv_locate_name(&sfmt);
			if(seval.len != sfmt.len) {
				LM_ERR("invalid pv [%.*s] (%d/%d)\n", sfmt.len, sfmt.s,
						seval.len, sfmt.len);
				return -1;
			}
			pvs = pv_cache_get(&sfmt);
			if(pvs==NULL) {
				LM_ERR("cannot get pv spec for [%.*s]\n", sfmt.len, sfmt.s);
				return -1;
			}

			fmsg = faked_msg_get_next();
			memset(&pvv, 0, sizeof(pv_value_t));
			if(pv_get_spec_value(fmsg, pvs, &pvv) != 0) {
				LM_ERR("unable to get pv value for [%.*s]\n", sfmt.len, sfmt.s);
				return -1;
			}
			if(pvv.flags&PV_VAL_NULL) {
				LM_ERR("unable to get pv value for [%.*s]\n", sfmt.len, sfmt.s);
				return -1;
			}
			if(pvv.flags&PV_TYPE_INT) {
				return set_mod_param_regex(emname, epname, PARAM_INT,
						(void*)(long)pvv.ri);
			}
			if(pvv.rs.len<0) {
				LM_ERR("invalid pv string value for [%.*s]\n", sfmt.len, sfmt.s);
				return -1;
			}
			if(pvv.rs.s[pvv.rs.len] != '\0') {
				LM_ERR("non 0-terminated pv string value for [%.*s]\n",
						sfmt.len, sfmt.s);
				return -1;
			}
			sb = str_list_block_add(&_ksr_modparam_strlist, pvv.rs.s, pvv.rs.len);
			if(sb==NULL) {
				LM_ERR("failed to handle parameter type: %d\n", ptype);
				return -1;
			}

			return set_mod_param_regex(emname, epname, PARAM_STRING,
							(void*)sb->s.s);
		default:
			LM_ERR("invalid parameter type: %d\n", ptype);
			return -1;
	}
}

int set_mod_param_serialized(char* mval)
{
#define MPARAM_MBUF_SIZE 256
	char mbuf[MPARAM_MBUF_SIZE];
	char *mname = NULL;
	char *mparam = NULL;
	char *sval = NULL;
	int ival = 0;
	int ptype = PARAM_STRING;
	char *p = NULL;

	if(strlen(mval) >= MPARAM_MBUF_SIZE) {
		LM_ERR("argument is too long: %s\n", mval);
		return -1;
	}
	strcpy(mbuf, mval);
	mname = mbuf;
	p = strchr(mbuf, ':');
	if(p==NULL) {
		LM_ERR("invalid format for argument: %s\n", mval);
		return -1;
	}
	*p = '\0';
	p++;
	mparam = p;
	p = strchr(p, ':');
	if(p==NULL) {
		LM_ERR("invalid format for argument: %s\n", mval);
		return -1;
	}
	*p = '\0';
	p++;
	if(*p=='i' || *p=='I') {
		ptype = PARAM_INT;
	} else if(*p=='s' || *p=='S') {
		ptype = PARAM_STRING;
	} else {
		LM_ERR("invalid format for argument: %s\n", mval);
		return -1;
	}
	p++;
	if(*p!=':') {
		LM_ERR("invalid format for argument: %s\n", mval);
		return -1;
	}
	p++;
	sval = p;

	if(ptype == PARAM_STRING) {
		return set_mod_param_regex(mname, mparam, PARAM_STRING, sval);
	} else {
		if(strlen(sval) <= 0) {
			LM_ERR("invalid format for argument: %s\n", mval);
			return -1;
		}
		strz2sint(sval, &ival);
		return set_mod_param_regex(mname, mparam, PARAM_INT, (void*)(long)ival);
	}
}
