/* 
 * Domain table related functions
 *
 * Copyright (C) 2002-2012 Juha Heinanen
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

#include "domain_mod.h"
#include "hash.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../route.h"
#include "../../pvar.h"
#include "../../str.h"

static db1_con_t* db_handle=0;
static db_func_t domain_dbf;

/* helper db functions*/

int domain_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &domain_dbf )) {
	        LM_ERR("Cannot bind to database module!\n");
		return -1;
	}
	return 0;
}



int domain_db_init(const str* db_url)
{	
	if (domain_dbf.init==0){
		LM_ERR("Unbound database module\n");
		goto error;
	}
	if (db_handle!=0)
		return 0;

	db_handle=domain_dbf.init(db_url);
	if (db_handle==0){
		LM_ERR("Cannot initialize database connection\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


void domain_db_close(void)
{
	if (db_handle && domain_dbf.close){
		domain_dbf.close(db_handle);
		db_handle=0;
	}
}



int domain_db_ver(str* name, int version)
{
	if (db_handle==0){
		LM_ERR("null database handler\n");
		return -1;
	}
	return db_check_table_version(&domain_dbf, db_handle, name, version);
}


/*
 * Check if domain is local
 */
int is_domain_local(str* _host)
{
    str did;
    struct attr_list *attrs;

    return hash_table_lookup(_host, &did, &attrs);
}

/*
 * Check if host in From uri is local
 */
int is_from_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
	struct sip_uri *puri;
	str did;
	struct attr_list *attrs;

	if ((puri=parse_from_uri(_msg))==NULL) {
		LM_ERR("Error while parsing From header\n");
		return -2;
	}

	return hash_table_lookup(&(puri->host), &did, &attrs);
}

/*
 * Check if host in Request URI is local
 */
int is_uri_host_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
    str branch;
    qvalue_t q;
    struct sip_uri puri;
    struct attr_list *attrs;
    str did;

    if ( is_route_type(REQUEST_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE) ) {
	if (parse_sip_msg_uri(_msg) < 0) {
	    LM_ERR("error while parsing R-URI\n");
	    return -1;
	}
	return hash_table_lookup(&(_msg->parsed_uri.host), &did, &attrs);
    } else if (is_route_type(FAILURE_ROUTE)) {
	branch.s = get_branch(0, &branch.len, &q, 0, 0, 0, 0, 0, 0, 0);
	if (branch.s) {
	    if (parse_uri(branch.s, branch.len, &puri) < 0) {
		LM_ERR("error while parsing branch URI\n");
		return -1;
	    }
	    return hash_table_lookup(&(puri.host), &did, &attrs);
	} else {
	    LM_ERR("branch is missing, error in script\n");
	    return -1;
	}
    } else {
	LM_ERR("unsupported route type\n");
	return -1;
    }
}


/*
 * Check if domain given as value of pseudo variable parameter is local.
 */
int w_is_domain_local(struct sip_msg* _msg, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    struct attr_list *attrs;
    str did;

    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_DBG("missing domain name\n");
		return -1;
	    }
	    return hash_table_lookup(&(pv_val.rs), &did, &attrs);
	} else {
	   LM_DBG("domain pseudo variable value is not string\n");
	   return -1;
	}
    } else {
	LM_DBG("cannot get domain pseudo variable value\n");
	return -1;
    }
}

/*
 * Check if domain is local and, if it is, add attributes as AVPs
 */
int w_lookup_domain(struct sip_msg* _msg, char* _sp, char* _prefix)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    int_str name, val;
    struct attr_list *attrs;
    str *prefix, did;
    unsigned short flags;

    sp = (pv_spec_t *)_sp;
    prefix = (str *)_prefix;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_DBG("domain name pseudo variable is missing\n");
		return -1;
	    }
	    if (hash_table_lookup(&(pv_val.rs), &did, &attrs) == 1) {
		while (attrs) {
		    if (attrs->type == 2)
			flags = AVP_NAME_STR | AVP_VAL_STR;
		    else
			flags = AVP_NAME_STR;
		    if (_prefix) {
			name.s.len = prefix->len + attrs->name.len;
			name.s.s = pkg_malloc(name.s.len);
			if (name.s.s == NULL) {
			    ERR("no pkg memory for avp name\n");
			    return -1;
			}
			memcpy(name.s.s, prefix->s, prefix->len);
			memcpy(name.s.s + prefix->len, attrs->name.s,
			       attrs->name.len);
		    } else {
			name.s = attrs->name;
		    }
		    if (add_avp(flags, name, attrs->val) < 0) {
			LM_ERR("unable to add a new AVP '%.*s'\n",
			       name.s.len, name.s.s);
			if (_prefix) pkg_free(name.s.s);
			return -1;
		    }
		    LM_DBG("added AVP '%.*s'\n", name.s.len, name.s.s);
		    if (prefix) pkg_free(name.s.s);
		    attrs = attrs->next;
		}
		flags = AVP_NAME_STR | AVP_VAL_STR;
		if (_prefix) {
		    name.s.len = prefix->len + 3;
		    name.s.s = pkg_malloc(name.s.len);
		    if (name.s.s == NULL) {
			ERR("no pkg memory for avp name\n");
			return -1;
		    }
		    memcpy(name.s.s, prefix->s, prefix->len);
		    memcpy(name.s.s + prefix->len, "did", 3);
		} else {
		    name.s.s = "did";
		    name.s.len = 3;
		}
		val.s = did;
		if (add_avp(flags, name, val) < 0) {
		    LM_ERR("unable to add a new AVP '%.*s'\n",
			   name.s.len, name.s.s);
		    if (_prefix) pkg_free(name.s.s);
		    return -1;
		}		
		LM_DBG("added AVP '%.*s'\n", name.s.len, name.s.s);
		if (_prefix) pkg_free(name.s.s);
		return 1;
	    } else {
		return -1;
	    }
	} else {
	   LM_DBG("domain name pseudo variable value is not string\n");
	   return -1;
	}
    } else {
	LM_DBG("cannot get domain name pseudo variable value\n");
	return -1;
    }
}

/*
 * Check if domain is local and, if it is, add attributes as AVPs
 */
int w_lookup_domain_no_prefix(struct sip_msg* _msg, char* _domain, char* _str)
{
    return w_lookup_domain(_msg, _domain, NULL);
}

int domain_check_self(str* host, unsigned short port, unsigned short proto)
{
    struct attr_list *attrs;
    str did;
    if (hash_table_lookup(host, &did, &attrs) > 0)
	return 1;
    return 0;
}

/*
 * Reload domain table to new hash table and when done, make new hash table
 * current one.
 */
int reload_tables ( void )
{
    db_key_t cols[4];
    db1_res_t* res = NULL;
    db_row_t* row;
    struct domain_list **new_hash_table;
    int i;
    short type;
    str did, domain, name, value;
    int_str val;

    /* Choose new hash table and free its old contents */
    if (*hash_table == hash_table_1) {
	hash_table_free(hash_table_2);
	new_hash_table = hash_table_2;
    } else {
	hash_table_free(hash_table_1);
	new_hash_table = hash_table_1;
    }

    cols[0] = &did_col;
    cols[1] = &name_col;
    cols[2] = &type_col;
    cols[3] = &value_col;

    if (domain_db_init(&db_url) < 0) {
	LM_ERR("unable to open database connection\n");
	return -1;
    }

    if (domain_dbf.use_table(db_handle, &domain_attrs_table) < 0) {
	LM_ERR("error while trying to use domain_attrs table\n");
	goto err;
    }

    if (domain_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 4, 0, &res) < 0) {
	LM_ERR("error while querying database\n");
	goto err;
    }

    row = RES_ROWS(res);

    LM_DBG("number of rows in domain_attrs table: %d\n", RES_ROW_N(res));
    
    for (i = 0; i < RES_ROW_N(res); i++) {

	row = RES_ROWS(res) + i;

	if ((VAL_NULL(ROW_VALUES(row)) == 1) ||
	    (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING)) {
	    LM_ERR("did at row <%u> is null or not string\n", i);
	    goto err;
	}
	did.s = (char *)VAL_STRING(ROW_VALUES(row));
	did.len = strlen(did.s);
	if (did.len == 0) {
	    LM_ERR("did at row <%u> is empty string\n", i);
	    goto err;
	}

	if ((VAL_NULL(ROW_VALUES(row) + 1) == 1) ||
	    (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_STRING)) {
	    LM_ERR("name at row <%u> is null or not string\n", i);
	    goto err;
	}
	name.s = (char *)VAL_STRING(ROW_VALUES(row) + 1);
	name.len = strlen(name.s);
	if (name.len == 0) {
	    LM_ERR("name at row <%u> is empty string\n", i);
	    goto err;
	}

	if ((VAL_NULL(ROW_VALUES(row) + 2) == 1) ||
	    ((VAL_TYPE(ROW_VALUES(row) + 2) != DB1_INT) &&
	     (VAL_TYPE(ROW_VALUES(row) + 2) != DB1_BIGINT))) {
	    LM_ERR("type at row <%u> is null or not int\n", i);
	    goto err;
	}
	if(VAL_TYPE(ROW_VALUES(row) + 2) == DB1_BIGINT) {
		type = (int)VAL_BIGINT(ROW_VALUES(row) + 2);
	} else {
		type = (int)VAL_INT(ROW_VALUES(row) + 2);
	}
	if ((type != 0) && (type != 2)) {
	    LM_ERR("unknown type <%d> at row <%u>\n", type, i);
	    goto err;
	}

	if ((VAL_NULL(ROW_VALUES(row) + 3) == 1) ||
	    (VAL_TYPE(ROW_VALUES(row) + 3) != DB1_STRING)) {
	    LM_ERR("value at row <%u> is null or not string\n", i);
	    goto err;
	}
	value.s = (char *)VAL_STRING(ROW_VALUES(row) + 3);
	value.len = strlen(value.s);

	if (type == 0) {
	    if (str2sint(&value, &(val.n)) == -1) {
		LM_ERR("value at row <%u> is invalid int\n", i);
		goto err;
	    }
	} else {
	    val.s = value;
	}

	if (type == 0) 
	    LM_DBG("inserting <did/name/type/value> = <%s/%s/%d/%d> into attribute list\n", did.s, name.s, type, val.n);
	else
	    LM_DBG("inserting <did/name/type/value> = <%s/%s/%d/%s> into attribute list\n", did.s, name.s, type, val.s.s);
		
	if (hash_table_attr_install(new_hash_table, &did, &name, type,
				    &val) == -1) {
	    LM_ERR("could not install attribute into hash table\n");
	    goto err;
	}
    }

    domain_dbf.free_result(db_handle, res);
    res = NULL;

    cols[0] = &domain_col;
    cols[1] = &did_col;
    
    if (domain_dbf.use_table(db_handle, &domain_table) < 0) {
	LM_ERR("error while trying to use domain table\n");
	goto err;
    }

    if (domain_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 2, 0, &res) < 0) {
	LM_ERR("error while querying database\n");
	goto err;
    }

    row = RES_ROWS(res);

    LM_DBG("number of rows in domain table: %d\n", RES_ROW_N(res));
		
    for (i = 0; i < RES_ROW_N(res); i++) {
	
	row = RES_ROWS(res) + i;

	if ((VAL_NULL(ROW_VALUES(row)) == 1) ||
	    (VAL_TYPE(ROW_VALUES(row)) != DB1_STRING)) {
	    LM_ERR("domain at row <%u> is null or not string\n", i);
	    goto err;
	}
	domain.s = (char *)VAL_STRING(ROW_VALUES(row));
	domain.len = strlen(domain.s);
	if (domain.len == 0) {
	    LM_ERR("domain at row <%u> is empty string\n", i);
	    goto err;
	}

	if ((VAL_NULL(ROW_VALUES(row) + 1) != 1) &&
	    (VAL_TYPE(ROW_VALUES(row) + 1) != DB1_STRING)) {
	    LM_ERR("did at row <%u> is not null or string\n", i);
	    goto err;
	}
	if (VAL_NULL(ROW_VALUES(row) + 1) == 1) {
	    did.s = domain.s;
	    did.len = domain.len;
	} else {
	    did.s = (char *)VAL_STRING(ROW_VALUES(row) + 1);
	    did.len = strlen(did.s);
	    if (did.len == 0) {
		LM_ERR("did at row <%u> is empty string\n", i);
		goto err;
	    }
	}

	LM_DBG("inserting <did/domain> = <%s/%s> into hash table\n",
	       did.s, domain.s);

	if (hash_table_install(new_hash_table, &did, &domain) == -1) {
	    LM_ERR("could not install domain into hash table\n");
	    domain_dbf.free_result(db_handle, res);
	    goto err;
	}
    }

    domain_dbf.free_result(db_handle, res);
    res = NULL;
    
    *hash_table = new_hash_table;

    domain_db_close();
    return 1;

 err:
    domain_dbf.free_result(db_handle, res);
    res = NULL;
    domain_db_close();
    return -1;
}
