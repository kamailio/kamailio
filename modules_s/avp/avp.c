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
#include "../../db/db.h"
#include "../../dset.h"
#include "../../mod_fix.h"
#include "../../usr_avp.h"
#include "../../db/db.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

MODULE_VERSION

/* #define EXTRA_DEBUG */

static int load_avp(struct sip_msg* msg, char* attr, char*);
static int set_iattr(struct sip_msg* msg, char* attr, char *val);
static int set_sattr(struct sip_msg* msg, char* attr, char *val);
static int print_sattr(struct sip_msg* msg, char *attr, char *val);
static int uri2attr(struct sip_msg* msg, char* attr, char*);
static int attr2uri(struct sip_msg* msg, char* attr, char*);


static int avp_mod_init(void);
static int avp_init_child(int rank);
static int check_load_param(void** param, int param_no);
static int avp_query_db(str* key);

char* avp_db_url = "mysql://ser:heslo@localhost/ser";    /* Database URL */
char* avp_table = "usr_preferences";

char* key_column = "uuid";
char* attr_column = "attribute";
char* val_column = "value";

db_con_t* db_handle = 0;
static db_func_t dbf;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"avp_load", load_avp, 1, check_load_param, REQUEST_ROUTE | FAILURE_ROUTE },
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
	{"db_url",  STR_PARAM, &avp_db_url  },
	{"pref_table",   STR_PARAM, &avp_table   },
	{"key_column",  STR_PARAM, &key_column  },
	{"attr_column", STR_PARAM, &attr_column },
	{"val_column",  INT_PARAM, &val_column  },
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
    "caller_uuid",
    "callee_uuid",
    NULL
};

static int avp_mod_init(void)
{
        DBG("avp - initializing\n");

	if (bind_dbmod(avp_db_url, &dbf)) {
		LOG(L_ERR, "ERROR: avp_mod_init: unable to bind db\n");
		return -1;
	}
    
        return 0;
}

static int avp_init_child(int rank)
{
    DBG("avp - initializing child %i\n",rank);

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

    db_use_table(db_handle,avp_table);
    err = db_query(db_handle,keys,ops,vals,cols,1,2,0,&res);
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
    db_free_query(db_handle,res);
    return err;
}


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
