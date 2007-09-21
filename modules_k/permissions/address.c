/*
 *
 * allow_address related functions
 *
 * Copyright (C) 2006 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-09-01  Introduced allow_address function
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <arpa/inet.h>

#include "permissions.h"
#include "hash.h"
#include "../../config.h"
#include "../../db/db.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"
#include "../../pvar.h"
#include "../../ut.h"

#define TABLE_VERSION 3

struct addr_list ***addr_hash_table; /* Ptr to current hash table ptr */
struct addr_list **addr_hash_table_1;     /* Pointer to hash table 1 */
struct addr_list **addr_hash_table_2;     /* Pointer to hash table 2 */

struct subnet **subnet_table;        /* Ptr to current subnet table */
struct subnet *subnet_table_1;       /* Ptr to subnet table 1 */
struct subnet *subnet_table_2;       /* Ptr to subnet table 2 */

/* Address group of allow_address queries */
static unsigned int addr_group = 0;               

static db_con_t* db_handle = 0;
static db_func_t perm_dbf;


/*
 * Reload addr table to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_table(void)
{
    db_key_t cols[4];
    db_res_t* res = NULL;
    db_row_t* row;
    db_val_t* val;

    struct addr_list **new_hash_table;
    struct subnet *new_subnet_table;
    int i;
    struct in_addr ip_addr;

    cols[0] = grp_col;
    cols[1] = ip_addr_col;
    cols[2] = mask_col;
    cols[3] = port_col;

    if (perm_dbf.use_table(db_handle, address_table) < 0) {
	    LM_ERR("failed to use table\n");
		return -1;
    }
    
    if (perm_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 4, 0, &res) < 0) {
	    LM_ERR("failed to query database\n");
		return -1;
    }

    /* Choose new hash table and free its old contents */
    if (*addr_hash_table == addr_hash_table_1) {
	empty_addr_hash_table(addr_hash_table_2);
	new_hash_table = addr_hash_table_2;
    } else {
	empty_addr_hash_table(addr_hash_table_1);
	new_hash_table = addr_hash_table_1;
    }

    /* Choose new subnet table */
    if (*subnet_table == subnet_table_1) {
	empty_subnet_table(subnet_table_2);
	new_subnet_table = subnet_table_2;
    } else {
	empty_subnet_table(subnet_table_1);
	new_subnet_table = subnet_table_1;
    }

    row = RES_ROWS(res);

    LM_DBG("Number of rows in address table: %d\n", RES_ROW_N(res));
		
    for (i = 0; i < RES_ROW_N(res); i++) {
	val = ROW_VALUES(row + i);
	if ((ROW_N(row + i) == 4) &&
	    (VAL_TYPE(val) == DB_INT) && !VAL_NULL(val) &&
	    (VAL_TYPE(val + 1) == DB_STRING) && !VAL_NULL(val + 1) &&
	    inet_aton((char *)VAL_STRING(val + 1), &ip_addr) != 0 &&
	    (VAL_TYPE(val + 2) == DB_INT) && !VAL_NULL(val + 2) && 
	    ((unsigned int)VAL_INT(val + 2) > 0) && 
	    ((unsigned int)VAL_INT(val + 2) <= 32) &&
	    (VAL_TYPE(val + 3) == DB_INT) && !VAL_NULL(val + 3)) {
	    if ((unsigned int)VAL_INT(val + 2) == 32) {
		if (addr_hash_table_insert(new_hash_table,
					   (unsigned int)VAL_INT(val),
					   (unsigned int)ip_addr.s_addr,
					   (unsigned int)VAL_INT(val + 3))
		    == -1) {
		    LM_ERR("hash table problem\n");
		    perm_dbf.free_result(db_handle, res);
		    return -1;
		}
		LM_DBG("Tuple <%u, %s, %u> inserted into address hash "
		    "table\n", (unsigned int)VAL_INT(val),
		    (char *)VAL_STRING(val + 1),
		    (unsigned int)VAL_INT(val + 2));
	    } else {
		if (subnet_table_insert(new_subnet_table,
					(unsigned int)VAL_INT(val),
					(unsigned int)ip_addr.s_addr,
					(unsigned int)VAL_INT(val + 2),
					(unsigned int)VAL_INT(val + 3))
		    == -1) {
		    LM_ERR("subnet table problem\n");
		    perm_dbf.free_result(db_handle, res);
		    return -1;
		}
		LM_DBG("Tuple <%u, %s, %u, %u> inserted into subnet "
		    "table\n", (unsigned int)VAL_INT(val),
		    (char *)VAL_STRING(val + 1),
		    (unsigned int)VAL_INT(val + 2),
		    (unsigned int)VAL_INT(val + 3));
	    }
	} else {
	    LM_ERR("database problem\n");
	    perm_dbf.free_result(db_handle, res);
	    return -1;
	}
    }

    perm_dbf.free_result(db_handle, res);

    *addr_hash_table = new_hash_table;
    *subnet_table = new_subnet_table;

    LM_DBG("address table reloaded successfully.\n");
	
    return 1;
}


/*
 * Initialize data structures
 */
int init_addresses(void)
{
    int ver;
    str name;

    if (!db_url) {
	LM_INFO("db_url parameter of permissions module not set, "
	    "disabling allow_addr\n");
	return 0;
    } else {
	if (bind_dbmod(db_url, &perm_dbf) < 0) {
	    LM_ERR("load a database support module\n");
	    return -1;
	}

	if (!DB_CAPABILITY(perm_dbf, DB_CAP_QUERY)) {
	    LM_ERR("database module does not implement 'query' function\n");
	    return -1;
	}
    }
    
    addr_hash_table_1 = addr_hash_table_2 = 0;
    addr_hash_table = 0;

    db_handle = perm_dbf.init(db_url);
    if (!db_handle) {
		LM_ERR("unable to connect database\n");
		return -1;
    }

    name.s = address_table;
    name.len = strlen(address_table);
    ver = table_version(&perm_dbf, db_handle, &name);

    if (ver < 0) {
	LM_ERR("failed to query table version\n");
	perm_dbf.close(db_handle);
	return -1;
    } else if (ver < TABLE_VERSION) {
	    LM_ERR("invalid table version %d - expected %d\n", ver,TABLE_VERSION);
	perm_dbf.close(db_handle);
	return -1;
    }

    addr_hash_table_1 = new_addr_hash_table();
    if (!addr_hash_table_1) return -1;
		
    addr_hash_table_2  = new_addr_hash_table();
    if (!addr_hash_table_2) goto error;
		
    addr_hash_table = (struct addr_list ***)shm_malloc
	(sizeof(struct addr_list **));
    if (!addr_hash_table) goto error;
    
    *addr_hash_table = addr_hash_table_1;

    subnet_table_1 = new_subnet_table();
    if (!subnet_table_1) goto error;

    subnet_table_2 = new_subnet_table();
    if (!subnet_table_2) goto error;

    subnet_table = (struct subnet **)shm_malloc(sizeof(struct subnet *));
    if (!subnet_table) goto error;

    *subnet_table = subnet_table_1;

    if (reload_address_table() == -1) {
	LM_CRIT("reload of address table failed\n");
	goto error;
    }

    perm_dbf.close(db_handle);
    db_handle = 0;

    return 0;

error:
	LM_ERR("no more shm memory\n");
    if (addr_hash_table_1) {
	free_addr_hash_table(addr_hash_table_1);
	addr_hash_table_1 = 0;
    }
    if (addr_hash_table_2) {
	free_addr_hash_table(addr_hash_table_2);
	addr_hash_table_2 = 0;
    }
    if (addr_hash_table) {
	shm_free(addr_hash_table);
	addr_hash_table = 0;
    }
    if (subnet_table_1) {
	free_subnet_table(subnet_table_1);
	subnet_table_1 = 0;
    }
    if (subnet_table_2) {
	free_subnet_table(subnet_table_2);
	subnet_table_2 = 0;
    }
    if (subnet_table) {
	shm_free(subnet_table);
	subnet_table = 0;
    }
    perm_dbf.close(db_handle);
    db_handle = 0;
    return -1;
}



/*
 * Open database connection if necessary
 */
int mi_init_addresses(void)
{
    if (!db_url || db_handle) return 0;
    db_handle = perm_dbf.init(db_url);
    if (!db_handle) {
	LM_ERR("unable to connect database\n");
	return -1;
    }
    return 0;
}


/*
 * Close connections and release memory
 */
void clean_addresses(void)
{
    if (addr_hash_table_1) free_addr_hash_table(addr_hash_table_1);
    if (addr_hash_table_2) free_addr_hash_table(addr_hash_table_2);
    if (addr_hash_table) shm_free(addr_hash_table);
    if (subnet_table_1) free_subnet_table(subnet_table_1);
    if (subnet_table_2) free_subnet_table(subnet_table_2);
    if (subnet_table) shm_free(subnet_table);
}


/*
 * Sets address group to be used by subsequent allow_address() tests.
 */
int set_address_group(struct sip_msg* _msg, char* _addr_group, char* _str2) 
{
    int_or_pvar_t *i_or_p;
    pv_value_t pv_val;

    i_or_p = (int_or_pvar_t *)_addr_group;

    if (i_or_p->pvar) {
	if (pv_get_spec_value(_msg, i_or_p->pvar, &pv_val) == 0) {
	    if (pv_val.flags & PV_VAL_INT) {
		addr_group = pv_val.ri;
	    } else if (pv_val.flags & PV_VAL_STR) {
		if (str2int(&(pv_val.rs), &addr_group) == -1) {
			LM_ERR("failed to convert group string to int\n");
		    return -1;
		}
	    } else {
		LM_ERR("failed to convert group string to int\n");
		return -1;
	    }
	} else {
	    LM_ERR("cannot get pseudo variable value\n");
	    return -1;
	}
    } else {
	addr_group = i_or_p->i;
    }

    LM_DBG("set addr_group to <%u>\n", addr_group);

    return 1;
}


/*
 * Checks if an entry exists in cached address table that belongs to
 * pre-assigned group and has ip address and port given in pseudo
 * variable parameters.  Port value 0 in cached address table matches
 * any port.
 */
int allow_address(struct sip_msg* _msg, char* _addr_sp, char* _port_sp) 
{
    pv_spec_t *addr_sp, *port_sp;
    pv_value_t pv_val;

    unsigned int addr, port;
    struct in_addr addr_struct;

    addr_sp = (pv_spec_t *)_addr_sp;
    port_sp = (pv_spec_t *)_port_sp;

    if (addr_sp && (pv_get_spec_value(_msg, addr_sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    addr = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if (inet_aton(pv_val.rs.s, &addr_struct) == 0) {
		LM_ERR("failed to convert IP address string to in_addr\n");
		return -1;
	    } else {
		addr = addr_struct.s_addr;
	    }
	} else {
	    LM_ERR("failed to convert IP address string to in_addr\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get pseudo variable value\n");
	return -1;
    }

    if (port_sp && (pv_get_spec_value(_msg, port_sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_INT) {
	    port = pv_val.ri;
	} else if (pv_val.flags & PV_VAL_STR) {
	    if (str2int(&(pv_val.rs), &port) == -1) {
		LM_ERR("failed to convert port string to int\n");
		return -1;
	    }
	} else {
	    LM_ERR("failed to convert port string to int\n");
	    return -1;
	}
    } else {
	LM_ERR("cannot get pseudo variable value\n");
	return -1;
    }

    if (match_addr_hash_table(*addr_hash_table, addr_group, addr, port) == 1)
	return 1;
    else
	return match_subnet_table(*subnet_table, addr_group, addr, port);
}


/*
 * allow_source_address(group) equals to
 * set_address_group(group); allow_address("$si", "$sp");
 * but is faster.  group can be an integer string or pseudo variable.
 */
int allow_source_address(struct sip_msg* _msg, char* _addr_group, char* _str2) 
{
    int_or_pvar_t *i_or_p;
    pv_value_t pv_val;
    unsigned int group;

    i_or_p = (int_or_pvar_t *)_addr_group;

    if (i_or_p->pvar) {
	if (pv_get_spec_value(_msg, i_or_p->pvar, &pv_val) == 0) {
	    if (pv_val.flags & PV_VAL_INT) {
		group = pv_val.ri;
	    } else if (pv_val.flags & PV_VAL_STR) {
		if (str2int(&(pv_val.rs), &group) == -1) {
		    LM_ERR("failed to convert converting group string to int\n");
		    return -1;
		}
	    } else {
		LM_ERR("failed to convert group string to int\n");
		return -1;
	    }
	} else {
	    LM_ERR("cannot get pseudo variable value\n");
	    return -1;
	}
    } else {
	group = i_or_p->i;
    }

    LM_DBG("looking for <%u, %x, %u>\n",
	group, _msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);

    if (match_addr_hash_table(*addr_hash_table, group,
			      _msg->rcv.src_ip.u.addr32[0],
			      _msg->rcv.src_port) == 1)
	return 1;
    else
	return match_subnet_table(*subnet_table, group,
				  _msg->rcv.src_ip.u.addr32[0],
				  _msg->rcv.src_port);
}
