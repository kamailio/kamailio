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
#include "mem/mem.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>

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
	reg = pkg_malloc(len + 2 + 1);
	if (reg == 0) {
		LM_ERR("No memory left\n");
		return -1;
	}
	reg[0] = '^';
	memcpy(reg + 1, regex, len);
	reg[len + 1] = '$';
	reg[len + 2] = '\0';

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
								LM_ERR("No memory left\n");
								regfree(&preg);
								pkg_free(reg);
								return -1;
							}
							strcpy(*((char**)ptr), (char*)val2);
							break;

						case PARAM_STR:
							((str*)ptr)->s = pkg_malloc(((str*)val2)->len+1);
							if (!((str*)ptr)->s) {
								LM_ERR("No memory left\n");
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
				LM_ERR("parameter <%s> of type <%d> not found in module <%s>\n",
						name, type, t->exports.name);
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
