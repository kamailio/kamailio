/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * -------
 * 2003-03-20  regex support in modparam (janakj)
 * 2004-03-12  extra flag USE_FUNC_PARAM added to modparam type -
 *             instead of copying the param value, a func is called (bogdan)
 * 2005-07-01  PARAM_STRING & PARAM_STR support
 */


#include "modparam.h"
#include "dprint.h"
#include "mem/mem.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>


int set_mod_param(char* _mod, char* _name, modparam_t _type, void* _val)
{
	void* ptr;
	modparam_t param_type;

	if (!_mod) {
		LOG(L_ERR, "set_mod_param(): Invalid _mod parameter value\n");
		return -1;
	}

	if (!_name) {
		LOG(L_ERR, "set_mod_param(): Invalid _name parameter value\n");
		return -2;
	}


	ptr = find_param_export(find_module_by_name(_mod), _name, _type, &param_type);
	if (!ptr) {
		LOG(L_ERR, "set_mod_param(): Parameter not found\n");
		return -3;
	}

	if (param_type & PARAM_USE_FUNC) {
		if ( ((param_func_t)(ptr))(param_type, _val) < 0) {
			return -4;
		}
	}
	else {
		switch(PARAM_TYPE_MASK(param_type)) {
			case PARAM_STRING:
				*((char**)ptr) = strdup((char*)_val);
				break;

			case PARAM_STR:
				((str*)ptr)->s = strdup((char*)_val);
				((str*)ptr)->len = strlen(((str*)ptr)->s);
				break;

			case PARAM_INT:
				*((int*)ptr) = (int)(long)_val;
				break;
		}
	}
	return 0;
}


int set_mod_param_regex(char* regex, char* name, modparam_t type, void* val)
{
	struct sr_module* t;
	regex_t preg;
	int mod_found, len;
	char* reg;
	void *ptr;
	modparam_t param_type;

	len = strlen(regex);
	reg = pkg_malloc(len + 2 + 1);
	if (reg == 0) {
		LOG(L_ERR, "set_mod_param_regex(): No memory left\n");
		return -1;
	}
	reg[0] = '^';
	memcpy(reg + 1, regex, len);
	reg[len + 1] = '$';
	reg[len + 2] = '\0';

	if (regcomp(&preg, reg, REG_EXTENDED | REG_NOSUB | REG_ICASE)) {
		LOG(L_ERR, "set_mod_param_regex(): Error while compiling regular expression\n");
		pkg_free(reg);
		return -2;
	}

	mod_found = 0;
	for(t = modules; t; t = t->next) {
		if (regexec(&preg, t->exports->name, 0, 0, 0) == 0) {
			DBG("set_mod_param_regex: '%s' matches module '%s'\n", regex, t->exports->name);
			mod_found = 1;
			ptr = find_param_export(t, name, type, &param_type);
			if (ptr) {
				DBG("set_mod_param_regex: found <%s> in module %s [%s]\n", name, t->exports->name, t->path);
				if (param_type & PARAM_USE_FUNC) {
					if ( ((param_func_t)(ptr))(param_type, val) < 0) {
						regfree(&preg);
						pkg_free(reg);
						return -4;
					}
				}
				else {
					switch(PARAM_TYPE_MASK(param_type)) {
						case PARAM_STRING:
							*((char**)ptr) = strdup((char*)val);
							break;

						case PARAM_STR:
							((str*)ptr)->s = strdup((char*)val);
							((str*)ptr)->len = strlen(((str*)ptr)->s);
							break;

						case PARAM_INT:
							*((int*)ptr) = (int)(long)val;
							break;
					}
				}
			}
			else {
				LOG(L_ERR, "set_mod_param_regex: parameter <%s> not found in module <%s>\n",
				    name, t->exports->name);
				regfree(&preg);
				pkg_free(reg);
				return -3;
			}
		}
	}

	regfree(&preg);
	pkg_free(reg);
	if (!mod_found) {
		LOG(L_ERR, "set_mod_param_regex: No module matching <%s> found\n", regex);
		return -4;
	}
	return 0;
}
