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
	
	if (!_mod) {
		LOG(L_ERR, "set_mod_param(): Invalid _mod parameter value\n");
		return -1;
	}

	if (!_name) {
		LOG(L_ERR, "set_mod_param(): Invalid _name parameter value\n");
		return -2;
	}

	ptr = find_param_export(_mod, _name, _type);
	if (!ptr) {
		LOG(L_ERR, "set_mod_param(): Parameter not found\n");
		return -3;
	}

	switch(_type) {
	case STR_PARAM:
		*((char**)ptr) = strdup((char*)_val);
		break;

	case INT_PARAM:
		*((int*)ptr) = (int)(long)_val;
		break;
	}

	return 0;
}


int set_mod_param_regex(char* regex, char* name, modparam_t type, void* val)
{
	struct sr_module* t;
	param_export_t* param;
	regex_t preg;
	int mod_found, len;
	char* reg;
	int n;

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
			DBG("set_mod_param_regex: %s matches module %s\n",
					regex, t->exports->name);
			mod_found = 1;
			for(param=t->exports->params;param && param->name ; param++) {
				if ((strcmp(name, param->name) == 0) &&
				( PARAM_TYPE_MASK(param->type) == type)) {
					DBG("set_mod_param_regex: found <%s> in module %s [%s]\n",
						name, t->exports->name, t->path);

					if (param->type&USE_FUNC_PARAM) {
						n = ((param_func_t)(param->param_pointer))
							(type, (param_func_param_t)(char*)val );
						if (n<0)
							return -4;
					} else {
						switch(type) {
							case STR_PARAM:
								*((char**)(param->param_pointer)) =
									strdup((char*)val);
								break;
							case INT_PARAM:
								*((int*)(param->param_pointer)) =
									(int)(long)val;
								break;
						}
					}

					break;
				}
			}
			if (!param || !param->name) {
				LOG(L_ERR, "set_mod_param_regex: parameter <%s> not found in module <%s>\n",
				    name, t->exports->name);
				regfree(&preg);
				pkg_free(reg);
				return -3;
			}
		}
	}

	regfree(&preg);
	if (!mod_found) {
		LOG(L_ERR, "set_mod_param_regex: No module matching %s found\n|", regex);
		pkg_free(reg);
		return -4;
	}

	pkg_free(reg);
	return 0;
}
