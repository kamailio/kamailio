/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dset.h"
#include "../../mod_fix.h"
#include "../../usr_avp.h"

#ifdef WITH_DB_SUPPORT
#include "../../db/db.h"
#endif

#ifdef WITH_RADIUS_SUPPORT
#include <radiusclient.h>
#include "../acc/dict.h"
#include "../../parser/digest/digest_parser.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#endif

#include <string.h>
#include <assert.h>
#include <stdlib.h>

MODULE_VERSION

/* #define EXTRA_DEBUG */

#ifdef WITH_DB_SUPPORT
static int load_avp(struct sip_msg* msg, char* attr, char*);
#endif
#ifdef WITH_RADIUS_SUPPORT
static int load_avp_radius(struct sip_msg* msg, char* attr, char*);
#endif
static int set_iattr(struct sip_msg* msg, char* attr, char *val);
static int set_sattr(struct sip_msg* msg, char* attr, char *val);
static int print_sattr(struct sip_msg* msg, char *attr, char *val);
static int uri2attr(struct sip_msg* msg, char* attr, char*);
static int attr2uri(struct sip_msg* msg, char* attr, char*);

static int avp_mod_init(void);
static int avp_init_child(int rank);
static int check_load_param(void** param, int param_no);
#ifdef WITH_DB_SUPPORT
static int avp_query_db(str* key);

char* avp_db_url = "mysql://ser:heslo@localhost/ser";    /* Database URL */
char* avp_table = "usr_preferences";

char* key_column = "uuid";
char* attr_column = "attribute";
char* val_column = "value";

db_con_t* db_handle = 0;
static db_func_t dbf;
#endif

#ifdef WITH_RADIUS_SUPPORT
static char *radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";
static int caller_service_type = -1;
static int callee_service_type = -1;
void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];
#endif


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
#ifdef WITH_DB_SUPPORT
	{"avp_load", load_avp, 1, check_load_param, REQUEST_ROUTE | FAILURE_ROUTE },
#endif
#ifdef WITH_RADIUS_SUPPORT
	{"avp_load_radius", load_avp_radius, 1, check_load_param, REQUEST_ROUTE | FAILURE_ROUTE },
#endif
	{"set_iattr", set_iattr, 2, str_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"set_sattr", set_sattr, 2, str_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"uri2attr", uri2attr, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"print_sattr", print_sattr, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"attr2uri", attr2uri, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
#ifdef WITH_DB_SUPPORT
	{"db_url",  STR_PARAM, &avp_db_url  },
	{"pref_table",   STR_PARAM, &avp_table   },
	{"key_column",  STR_PARAM, &key_column  },
	{"attr_column", STR_PARAM, &attr_column },
	{"val_column",  INT_PARAM, &val_column  },
#endif
#ifdef WITH_RADIUS_SUPPORT
	{"radius_config",    STR_PARAM, &radius_config   },
	{"caller_service_type",     INT_PARAM, &caller_service_type    },
	{"callee_service_type",     INT_PARAM, &callee_service_type    },
#endif
	{0, 0, 0}
};


struct module_exports exports = {
    "avp", 
    cmds,         /* Exported commands */
    params,       /* Exported parameters */
    avp_mod_init,  /* module initialization function */
    0,            /* response function*/
    0,            /* destroy function */
    0,            /* oncancel function */
    avp_init_child /* per-child init function */
};


/** Load parameter lookup */
char* load_param_lookup[] = {
#ifdef WITH_DB_SUPPORT
    "caller_uuid",
    "callee_uuid",
#endif
#ifdef WITH_RADIUS_SUPPORT
    "caller",
    "callee",
#endif
    NULL
};

static int avp_mod_init(void)
{
        DBG("avp - initializing\n");

#ifdef WITH_DB_SUPPORT
	if (bind_dbmod(avp_db_url, &dbf)) {
		LOG(L_ERR, "ERROR: avp_mod_init: unable to bind db\n");
		return -1;
	}
#endif

#ifdef WITH_RADIUS_SUPPORT
	memset(attrs, 0, sizeof(attrs));
	memset(attrs, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n			= "Service-Type";
	attrs[A_USER_NAME].n	                = "User-Name";
	attrs[A_SIP_AVP].n			= "SIP-AVP";
	vals[V_SIP_CALLER_AVPS].n		= "SIP-Caller-AVPs";
	vals[V_SIP_CALLEE_AVPS].n		= "SIP-Callee-AVPs";

	/* open log */
	rc_openlog("ser");
	/* read config */
	if ((rh = rc_read_config(radius_config)) == NULL) {
		LOG(L_ERR, "ERROR: avp: error opening radius config file: %s\n", 
			radius_config );
		return -1;
	}
	/* read dictionary */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))!=0) {
		LOG(L_ERR, "ERROR: avp: error reading radius dictionary\n");
		return -1;
	}

	INIT_AV(rh, attrs, vals, "avp", -1, -1);

	if (caller_service_type != -1)
		vals[V_SIP_CALLER_AVPS].v = caller_service_type;
	if (callee_service_type != -1)
		vals[V_SIP_CALLEE_AVPS].v = callee_service_type;
#endif
    
        return 0;
}

static int avp_init_child(int rank)
{
    DBG("avp - initializing child %i\n",rank);

#ifdef WITH_DB_SUPPORT
    if (avp_db_url){
	db_handle=dbf.init(avp_db_url);
        if (db_handle==0){
	    LOG(L_ERR, "ERROR: avp_db_init: unable to connect to the "
		"database\n");
	    return -1;
	}
	return 0;
    }
    LOG(L_CRIT, "BUG: avp_db_init: null db url\n");
    return -1;
#endif

#ifdef WITH_RADIUS_SUPPORT    
    return 0;
#endif

}


static int check_load_param(void** param, int param_no)
{
    char** p = load_param_lookup;

    if(param_no==1){
	
	while(*p){
	    if(!strcmp(*param,*p))
		return 0;
	    p++;
	}

	LOG(L_ERR, "ERROR: avp: check_load_param: bad argument <%s>\n",
	    (char*)*param);
	return E_CFG;
    }
    return 0;
}
	

#ifdef WITH_DB_SUPPORT
static int load_avp(struct sip_msg* msg, char* attr, char* _dummy)
{
    struct usr_avp *uuid=0;

    int_str attr_istr,val_istr;
    str attr_str;

    attr_istr.s = &attr_str;
    attr_str.s = attr;
    attr_str.len = strlen(attr);

    uuid = search_first_avp(AVP_NAME_STR, attr_istr, &val_istr);
    if(!uuid){
	LOG(L_ERR,"ERROR: avp_load: %s not found\n",attr);
	return -1;
    }

    if(!(uuid->flags & AVP_VAL_STR)){
	LOG(L_ERR,"ERROR: avp_load: value for <%s> should "
	    "be of type string\n",attr);
	return -1;
    }

    DBG("load_avp: found <%s;%*.s>\n",attr,
	val_istr.s->len,val_istr.s->s);

    return avp_query_db(val_istr.s);
}

static int avp_query_db(str* key)
{
    int       err=-1;

    db_key_t  cols[2];
    db_key_t  keys[1];
    db_op_t   ops [1];
    db_val_t  vals[1];

    db_res_t* res=0;
    db_row_t* cur_row=0;

    int_str name,val;
    str name_str,val_str;


    cols[0] = attr_column;
    cols[1] = val_column;

    keys[0] = key_column;
    ops[0]  = "=";
    VAL_TYPE(&(vals[0])) = DB_STR;
    VAL_NULL(&(vals[0])) = 0;
    VAL_STR(&(vals[0]))  = *key;

    dbf.use_table(db_handle,avp_table);
    err = dbf.query(db_handle,keys,ops,vals,cols,1,2,0,&res);
    if(err){
	LOG(L_ERR,"ERROR: avp: db_query() failed.");
	err=-1;
	goto error;
    }

    name.s = &name_str;
    val.s  = &val_str;

    if(res->n==0){
	DBG("load_avp: no AVPs found in the DB\n");
	err=-1;
	goto error;
    }

    for( cur_row = res->rows+res->n-1; 
	 cur_row >= res->rows; cur_row--){

	name_str.s = (char*)VAL_STRING(&(cur_row->values[0]));
	name_str.len = strlen(name_str.s);

	val_str.s = (char*)VAL_STRING(&(cur_row->values[1]));
	val_str.len = strlen(val_str.s);
	
	err = add_avp(AVP_NAME_STR|AVP_VAL_STR,name,val);
	if(err != 0){
	    LOG(L_ERR,"ERROR: avp: add_avp() failed\n");
	    err=-1;
	    goto error;
	}

	DBG("avp_load: AVP '%s'/'%s' has been added\n",
	    name_str.s,val_str.s);
    }

    err = 1;

 error:
    dbf.free_result(db_handle,res);
    return err;
}
#endif

#ifdef WITH_RADIUS_SUPPORT
static inline void attr_name_value(VALUE_PAIR *vp, str *name, str *value)
{
    int i;
    for (i = 0; i < vp->lvalue; i++) {
	if (*(vp->strvalue + i) == ':') {
	    name->s = vp->strvalue;
	    name->len = i;
	    if (i == (vp->lvalue - 1)) {
		value->s = (char *)0;
		value->len = 0;
	    } else {
		value->s = vp->strvalue + i + 1;
		value->len = vp->lvalue - i - 1;
	    }
	    return;
	}
    }
    name->len = value->len = 0;
    name->s = value->s = (char *)0;
}
		
static int load_avp_radius(struct sip_msg* _msg, char* _attr, char* _dummy)
{
    static char msg[4096];

    str attr_str;
    struct hdr_field* h;
    dig_cred_t* cred = 0;
    str user_name, user, domain;
    int_str name, val;
    str name_str, val_str;

    attr_str.s = _attr;
    attr_str.len = strlen(_attr);

    VALUE_PAIR *send, *received, *vp;
    UINT4 service;

    send = received = 0;

    if (attr_str.s[5] == 'r') {
	/* If "caller", take Radius username from authorized credentials */
	get_authorized_cred(_msg->proxy_auth, &h);
	if (!h) {
	    LOG(L_ERR, "load_avp_radius(): No authoried credentials\n");
	    return -1;
	}
	cred = &((auth_body_t*)(h->parsed))->digest;
	user = cred->username.user;
	domain = cred->realm;
	service = vals[V_SIP_CALLER_AVPS].v;
    } else {
	/* If "callee", take Radius username from Request-URI */
	if (parse_sip_msg_uri(_msg) < 0) {
	    LOG(L_ERR, "load_avp_radius(): Error while parsing Request-URI\n");
	    return -1;
	}
	if (_msg->parsed_uri.user.len == 0) {	
	    LOG(L_ERR, "load_avp_radius(): Request-URI user is missing\n");
	    return -1;
	}
	user =_msg->parsed_uri.user; 
	domain = _msg->parsed_uri.host;
	service = vals[V_SIP_CALLEE_AVPS].v;
    }
    user_name.len = user.len + domain.len + 1;
    user_name.s = (char*)pkg_malloc(user_name.len);
    if (!user_name.s) {
	LOG(L_ERR, "avp_load_radius(): No memory left\n");
	return -1;
    }
    memcpy(user_name.s, user.s, user.len);
    user_name.s[user.len] = '@';
    memcpy(user_name.s + user.len + 1, domain.s, domain.len);
    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
		       user_name.s, user_name.len, 0)) {
	LOG(L_ERR, "avp_load_radius(): Error adding PW_USER_NAME\n");
	rc_avpair_free(send);
	pkg_free(user_name.s);
	return -1;
    }
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
	LOG(L_ERR, "avp_load_radius(): Error adding PW_SERVICE_TYPE\n");
	rc_avpair_free(send);
	pkg_free(user_name.s);
	return -1;
    }
    if (rc_auth(rh, 0, send, &received, msg) == OK_RC) {
	DBG("avp_load_radius(): Success\n");
	rc_avpair_free(send);
	pkg_free(user_name.s);
	vp = received;
	while ((vp = rc_avpair_get(vp, attrs[A_SIP_AVP].v, 0))) {
	    name.s = &name_str;
	    val.s = &val_str;
	    attr_name_value(vp, &name_str, &val_str);
	    add_avp(AVP_NAME_STR|AVP_VAL_STR, name, val);
	    DBG("avp_load_radius: AVP '%.*s'/'%.*s' has been added\n",
		name_str.len, name_str.s, val_str.len, val_str.s);
	    vp = vp->next;
	}
	rc_avpair_free(received);
	return 1;
    } else {
	DBG("avp_load_radius(): Failure\n");
	rc_avpair_free(send);
	rc_avpair_free(received);
	pkg_free(user_name.s);
	return -1;
    }
}
#endif


static int set_sattr(struct sip_msg* msg, char* attr, char *val) 
{
	str *s_name, *s_value;
	int_str name, value;

	s_name=(str *) attr; name.s=s_name;
	s_value=(str *) val; value.s=s_value;

	if (add_avp(AVP_NAME_STR | AVP_VAL_STR,name,value) !=0) {
		LOG(L_ERR, "ERR: set_sattr: add_avp failed\n");
		return -1;
	}
	LOG(L_DBG, "DEBUG: set_sattr ok\n");
	return 1;
}


static int set_iattr(struct sip_msg* msg, char* attr, char *val) 
{
	str *s_name;
	int nr;
	int_str name, value;

	s_name=(str *) attr; name.s=s_name;
	nr=atoi(val);
	value.n=nr;

	if (add_avp(AVP_NAME_STR, name, value)!=0) {
		LOG(L_ERR, "ERR: set_iattr: add_avp failed\n");
		return -1;
	}
	LOG(L_DBG, "DEBUG: set_iattr ok\n");
	return 1;
}	

static int print_sattr(struct sip_msg* msg, char *attr, char *val)
{
	str s_name, s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	s_name.s=attr;s_name.len=strlen(attr);
	name.s=&s_name;

	avp_entry = search_first_avp ( AVP_NAME_STR | AVP_VAL_STR, 
		name, &value);
	if (avp_entry==0) {
		LOG(L_ERR, "ERROR: print_sattr: no AVP found\n");
		return -1;
	}
	s_value.s=value.s->s;
	s_value.len=value.s->len;
	LOG(L_INFO, "Attribute (%s) has value: %.*s\n",
		attr, s_value.len, s_value.s );
	return 1;
}

static int uri2attr(struct sip_msg* msg, char* attr, char *foo)
{
	str uri;
	str s_attr;

	get_request_uri(msg, &uri);
	s_attr.s=attr;s_attr.len=strlen(attr);
	return set_sattr(msg, (char *) &s_attr, (char *) &uri);
}


static int attr2uri(struct sip_msg* msg, char* attr, char *foo)
{
	str s_name, s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	s_name.s=attr;s_name.len=strlen(attr);
	name.s=&s_name;

	avp_entry = search_first_avp ( AVP_NAME_STR | AVP_VAL_STR, 
		name, &value);
	if (avp_entry==0) {
		LOG(L_ERR, "ERROR: print_sattr: no AVP found\n");
		return -1;
	}
	s_value.s=value.s->s;
	s_value.len=value.s->len;
	if (rewrite_uri(msg, &s_value)<0) {
		LOG(L_ERR, "ERROR: attr2uri: no attribute found\n");
		return -1;
	}
	return 1;
}
