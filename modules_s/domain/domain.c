/* 
 * $Id$
 *
 * Domain table related functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
#include "domain_mod.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../ut.h"


/*
 * Search the list of domains for domain with given did
 */
static domain_t* domain_search(domain_t* list, str* did)
{
	while(list) {
		if (list->did.len == did->len &&
		    !memcmp(list->did.s, did->s, did->len)) {
			return list;
		}
		list = list->next;
	}
	return 0;
}


/*
 * Add a new domain name to did
 */
static int domain_add(domain_t* d, str* domain, unsigned int flags)
{
	str* p1;
	unsigned int* p2;
	str dom;

	if (!d || !domain) {
		LOG(L_ERR, "domain:domain_add: Invalid parameter value\n");
		return -1;
	}

	dom.s = shm_malloc(domain->len);
	if (!dom.s) goto error;
	memcpy(dom.s, domain->s, domain->len);
	dom.len = domain->len;
	strlower(&dom);

	p1 = (str*)shm_realloc(d->domain, sizeof(str) * (d->n + 1));
	if (!p1) goto error;
	p2 = (unsigned int*)shm_realloc(d->flags, sizeof(unsigned int) * (d->n + 1));
	if (!p2) goto error;
	
	d->domain = p1;
	d->domain[d->n] = dom;
	d->flags = p2;
	d->flags[d->n] = flags;
	d->n++;
	return 0;

 error:
	LOG(L_ERR, "domain:domain_add: Unable to add new domain name (out of memory)\n");
	if (dom.s) shm_free(dom.s);
	return -1;
}


/*
 * Release all memory allocated for given domain structure
 */
static void free_domain(domain_t* d)
{
	int i;
	if (!d) return;
	if (d->did.s) shm_free(d->did.s);

	for(i = 0; i < d->n; i++) {
		if (d->domain[i].s) shm_free(d->domain[i].s);
	}
	shm_free(d->domain);
	shm_free(d->flags);
	if (d->attrs) destroy_avp_list(&d->attrs);
	shm_free(d);
}




/*
 * Create a new domain structure which will initialy have
 * one domain name
 */
static domain_t* new_domain(str* did, str* domain, unsigned int flags)
{
	domain_t* d;
	int_str name, val;
	str name_s = STR_STATIC_INIT(AVP_DID);

	d = (domain_t*)shm_malloc(sizeof(domain_t));
	if (!d) goto error;
	memset(d, 0, sizeof(domain_t));
	d->did.s = shm_malloc(did->len);
	if (!d->did.s) goto error;
	memcpy(d->did.s, did->s, did->len);
	d->did.len = did->len;

	d->domain = (str*)shm_malloc(sizeof(str));
	if (!d->domain) goto error;
	d->domain[0].s = shm_malloc(domain->len);
	if (!d->domain[0].s) goto error;
	memcpy(d->domain[0].s, domain->s, domain->len);
	d->domain[0].len = domain->len;
	strlower(d->domain);
	
	d->flags = (unsigned int*)shm_malloc(sizeof(unsigned int));
	if (!d->flags) goto error;
	d->flags[0] = flags;
	d->n = 1;

	     /* Create an attribute containing did of the domain */
	name.s = name_s;
	val.s = *did;
	if (add_avp_list(&d->attrs, AVP_CLASS_DOMAIN | AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) goto error;

	return d;

 error:
	LOG(L_ERR, "domain:new_domain: Unable to create new domain structure\n");
	free_domain(d);
	return 0;
}


/*
 * Release all memory allocated for entire domain list
 */
void free_domain_list(domain_t* list)
{
	domain_t* ptr;
	if (!list) return;

	while(list) {
		ptr = list;
		list = list->next;
		free_domain(ptr);
	}
}


/*
 * Load attributes from domain_attrs table
 */
int db_load_domain_attrs(domain_t* d)
{
    int_str name, v;
    str avp_name, avp_val;
    int i, type, n;
    db_key_t keys[1], cols[4];
    db_res_t* res;
    db_val_t kv[1], *val;
    unsigned short flags;
    
    if (!con) {
	LOG(L_ERR, "domain:db_load_domain_attrs: Invalid database handle\n");
		return -1;
    }
    
    keys[0] = domattr_did.s;
    kv[0].type = DB_STR;
    kv[0].nul = 0;
    kv[0].val.str_val = d->did;
    
    cols[0] = domattr_name.s;
    cols[1] = domattr_type.s;
    cols[2] = domattr_value.s;
    cols[3] = domattr_flags.s;
    
    if (db.use_table(con, domattr_table.s) < 0) {
	LOG(L_ERR, "domain:db_load_domain_attrs Error in use_table\n");
	return -1;
    }
    
    if (db.query(con, keys, 0, kv, cols, 1, 4, 0, &res) < 0) {
	LOG(L_ERR, "domain:db_load_domain_attrs: Error while quering database\n");
	return -1;
    }
    
    n = 0;
    for(i = 0; i < res->n; i++) {
	val = res->rows[i].values;
	
	if (val[0].nul || val[1].nul || val[3].nul) {
	    LOG(L_ERR, "domain:db_load_domain_attrs: Skipping row containing NULL entries\n");
	    continue;
	}
	
	if ((val[3].val.int_val & DB_LOAD_SER) == 0) continue;
	
	n++;
	     /* Get AVP name */
	avp_name.s = (char*)val[0].val.string_val;
	avp_name.len = strlen(avp_name.s);
	name.s = avp_name;
	
	     /* Get AVP type */
	type = val[1].val.int_val;
	
	     /* Test for NULL value */
	if (val[2].nul) {
	    avp_val.s = 0;
	    avp_val.len = 0;
	} else {
	    avp_val.s = (char*)val[2].val.string_val;
	    avp_val.len = strlen(avp_val.s);
	}
	
	flags = AVP_CLASS_DOMAIN | AVP_NAME_STR;
	if (type == AVP_VAL_STR) {
		 /* String AVP */
	    v.s = avp_val;
	    flags |= AVP_VAL_STR;
	} else {
		 /* Integer AVP */
	    str2int(&avp_val, (unsigned*)&v.n);
	}

	if (add_avp_list(&d->attrs, flags, name, v) < 0) {
	    LOG(L_ERR, "domain:db_load_domain_attrs: Error while adding domain attribute %.*s to domain %.*s, skipping\n",
		avp_name.len, ZSW(avp_name.s),
		d->did.len, ZSW(d->did.s));
	    continue;
	}
    }
    DBG("domain:db_load_domain_attrs: %d domain attributes found, %d loaded\n", res->n, n);
    db.free_result(con, res);
    return 0;
}


/*
 * Create domain list from domain table
 */
int load_domains(domain_t** dest)
{
	db_key_t cols[3];
	db_res_t* res;
	db_row_t* row;
	db_val_t* val;
	unsigned int flags, i;
	str did, domain;
	domain_t* d, *list;

	if (!con) {
		LOG(L_ERR, "domain:load_domains: Invalid database handle\n");
		return -1;
	}

	list = 0;
	cols[0] = did_col.s;
	cols[1] = domain_col.s;
	cols[2] = flags_col.s;

	if (db.use_table(con, domain_table.s) < 0) {
		LOG(L_ERR, "domain:load_domains: Error while trying to use domain table\n");
		return -1;
	}

	if (db.query(con, NULL, 0, NULL, cols, 0, 3, 0, &res) < 0) {
		LOG(L_ERR, "domain:load_domains: Error while querying database\n");
		return -1;
	}

	row = res->rows;
	DBG("domain:load_domains: Number of rows in domain table: %d\n", res->n);
		
	for (i = 0; i < res->n; i++) {
		val = row[i].values;

		     /* Do not assume that the database server performs any constrain
		      * checking (dbtext does not) and perform sanity checks here to
		      * make sure that we only load good entried
		      */
		if (val[0].nul || val[1].nul || val[2].nul) {
			LOG(L_ERR, "domain:load_domains: Row with NULL column(s), skipping\n");
			continue;
		}

		did.s = (char*)val[0].val.string_val;
		did.len = strlen(did.s);
		domain.s = (char*)val[1].val.string_val;
		domain.len = strlen(domain.s);
		flags = val[2].val.int_val;

		     /* Skip entries that are disabled/scheduled for removal */
		if (flags & DB_DISABLED) continue;
		     /* Skip entries that are for serweb/ser-ctl only */
		if (!(flags & DB_LOAD_SER)) continue;
		
		DBG("domain:load_domains: Processing entry (%.*s, %.*s, %u)\n",
		    did.len, ZSW(did.s),
		    domain.len, ZSW(domain.s),
		    flags);

		d = domain_search(list, &did);
		if (d) {
			     /* DID exists in the list, update it */
			if (domain_add(d, &domain, flags) < 0) goto error;
		} else {
			     /* DID does not exist yet, create a new entry */
			d = new_domain(&did, &domain, flags);
			if (!d) goto error;
			d->next = list;
			list = d;
		}
	}

	db.free_result(con, res);

	if (load_domain_attrs) {
		d = list;
		while(d) {
			if (db_load_domain_attrs(d) < 0) goto error;
			d = d->next;
		}
	}

	*dest = list;
	return 0;

 error:
	free_domain_list(list);
	return 1;
}
