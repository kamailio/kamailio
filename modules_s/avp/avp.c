/*
 * $Id$
 *
 * Copyright (C) 2004 FhG Fokus
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
 */

#include <string.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../dprint.h"

MODULE_VERSION


/* name of attributed used to store flags with command flags2attr */
#define FLAGS_ATTR "flags"
#define FLAGS_ATTR_LEN (sizeof(FLAGS_ATTR) - 1)


static int set_iattr(struct sip_msg*, char*, char*);
static int set_sattr(struct sip_msg*, char*, char*);
static int print_sattr(struct sip_msg*, char*, char*);
static int uri2attr(struct sip_msg*, char*, char*);
static int attr2uri(struct sip_msg*, char*, char*);
static int is_sattr_set( struct sip_msg*, char*, char*);
static int flags2attr(struct sip_msg*, char*, char*);
static int avp_exists (struct sip_msg*, char*, char*);

static int iattr_fixup(void** param, int param_no);
static int str_fixup(void** param, int param_no);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"set_iattr",    set_iattr,    2, iattr_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"flags2attr",   flags2attr,   0, 0,           REQUEST_ROUTE | FAILURE_ROUTE},
	{"set_sattr",    set_sattr,    2, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"uri2attr",     uri2attr,     1, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"print_sattr",  print_sattr,  1, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"attr2uri",     attr2uri,     1, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"is_sattr_set", is_sattr_set, 1, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_exists",   avp_exists,   2, str_fixup,   REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};


struct module_exports exports = {
	"avp", 
	cmds,           /* Exported commands */
	params,         /* Exported parameters */
	0,              /* module initialization function */
	0,              /* response function*/
	0,              /* destroy function */
	0,              /* oncancel function */
	0               /* per-child init function */
};


static int set_sattr(struct sip_msg* msg, char* attr, char* val) 
{
	int_str name, value;

	name.s = (str*)attr;
	value.s = (str*)val;

	if (add_avp(AVP_NAME_STR | AVP_VAL_STR, name, value) !=0 ) {
		LOG(L_ERR, "set_sattr: add_avp failed\n");
		return -1;
	}

	DBG("set_sattr ok\n");
	return 1;
}


static int set_iattr(struct sip_msg* msg, char* attr, char* nr) 
{
	int_str name, value;

	value.n = (int)nr;
	name.s = (str*)attr;

	if (add_avp(AVP_NAME_STR, name, value) != 0) {
		LOG(L_ERR, "set_iattr: add_avp failed\n");
		return -1;
	}

	LOG(L_DBG, "set_iattr ok\n");
	return 1;
}	


static int flags2attr(struct sip_msg* msg, char* foo, char* bar)
{
	str s_name;
	int_str name, value;

	s_name.s = FLAGS_ATTR;
	s_name.len = FLAGS_ATTR_LEN;

	name.s = &s_name;
	value.n = msg->flags;

	if (add_avp(AVP_NAME_STR, name, value) != 0) {
		LOG(L_ERR, "flags2attr: add_avp failed\n");
		return -1;
	}

	DBG("flags2attr ok\n");
	return 1;
}


static int print_sattr(struct sip_msg* msg, char* attr, char* s2)
{
	str s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	name.s = (str*)attr;

	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, name, &value);
	if (avp_entry == 0) {
		LOG(L_ERR, "print_sattr: AVP '%.*s' not found\n", name.s->len, ZSW(name.s->s));
		return -1;
	}

	s_value.s = value.s->s;
	s_value.len = value.s->len;
	LOG(L_INFO, "AVP: '%.*s'='%.*s'\n",
	    ((str*)attr)->len, ZSW(((str*)attr)->s), s_value.len, ZSW(s_value.s));
	return 1;
}


static int uri2attr(struct sip_msg* msg, char* attr, char* foo)
{
	str uri;

	get_request_uri(msg, &uri);
	return set_sattr(msg, attr, (char *)&uri);
}


static int is_sattr_set(struct sip_msg* msg, char* attr, char* foo)
{
	int_str name, value;
	struct usr_avp* avp_entry;

	name.s = (str*)attr;
	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				     name, &value);
	if (avp_entry == 0) {
		return -1;
	}

	return 1;
}


static int attr2uri(struct sip_msg* msg, char* attr, char* foo)
{
	str s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	name.s=(str*)attr;

	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, 
				     name, &value);
	if (avp_entry == 0) {
		LOG(L_ERR, "attr2uri: AVP '%.*s' not found\n", name.s->len, ZSW(name.s->s));
		return -1;
	}

	s_value.s = value.s->s;
	s_value.len = value.s->len;
	if (rewrite_uri(msg, &s_value) < 0) {
		LOG(L_ERR, "attr2uri: no attribute found\n");
		return -1;
	}
	return 1;
}


/*
 *  returns 1 if msg contains an AVP with the given name and value, 
 *  returns -1 otherwise
 */
static int avp_exists(struct sip_msg* msg, char* key, char* value)
{	
	int_str avp_key, avp_value;
	struct usr_avp *avp_entry;
	str* val_str, *key_str;

	key_str = (str*)key;	
	val_str = (str*)value;
	
	avp_key.s = (str*)key;
	avp_entry = search_first_avp(AVP_NAME_STR, avp_key, &avp_value);
	
	if (avp_entry == 0) {
		DBG("avp_exists: AVP '%.*s' not found\n", key_str->len, ZSW(key_str->s));
		return -1;
	}

	while (avp_entry != 0) {
		if (avp_entry->flags & AVP_VAL_STR) {
			if ((avp_value.s->len == val_str->len) &&
			    !memcmp(avp_value.s->s, val_str->s, avp_value.s->len)) {
				DBG("avp_exists str ('%.*s', '%.*s'): TRUE\n", 
				    key_str->len, ZSW(key_str->s),
				    val_str->len, ZSW(val_str->s));
				return 1;
			}
		} else {
			if (avp_value.n == str2s(val_str->s, val_str->len, 0)){
				DBG("avp_exists (%.*s, %.*s): TRUE\n", 
				    key_str->len, ZSW(key_str->s), 
				    val_str->len, ZSW(val_str->s));
				return 1;
			}
		}
		avp_entry = search_next_avp (avp_entry, &avp_value);
	}

	DBG("avp_exists ('%.*s', '%.*s'): FALSE\n", 
	    key_str->len, ZSW(key_str->s), 
	    val_str->len, ZSW(val_str->s));
	return -1;
}


static int iattr_fixup(void** param, int param_no)
{
	unsigned long num;
	int err;
	
	str* s;
	
	if (param_no == 1) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "iattr_fixup: No memory left\n");
			return E_UNSPEC;
		}
		
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	} else if (param_no == 2) {
		num = str2s(*param, strlen(*param), &err);
		
		if (err == 0) {
			pkg_free(*param);
			*param=(void*)num;
		} else {
			LOG(L_ERR, "iattr_fixup: Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}


/*  
 * Convert char* parameter to str* parameter   
 */
static int str_fixup(void** param, int param_no)
{
	str* s;
	
	if (param_no == 1 || param_no == 2 ) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup: No memory left\n");
			return E_UNSPEC;
		}
		
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	
	return 0;
}
