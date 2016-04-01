
/*
 * allow_address related functions
 *
 * Copyright (C) 2006 Juha Heinanen
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
#include "../../lib/srdb1/db.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"
#include "../../mod_fix.h"
#include "../../ut.h"
#include "../../resolve.h"

#define TABLE_VERSION 6

struct addr_list ***addr_hash_table = NULL; /* Ptr to current hash table ptr */
struct addr_list **addr_hash_table_1 = NULL; /* Pointer to hash table 1 */
struct addr_list **addr_hash_table_2 = NULL; /* Pointer to hash table 2 */

struct subnet **subnet_table = NULL;  /* Ptr to current subnet table */
struct subnet *subnet_table_1 = NULL; /* Ptr to subnet table 1 */
struct subnet *subnet_table_2 = NULL; /* Ptr to subnet table 2 */

struct domain_name_list ***domain_list_table = NULL; /* Ptr to current domain name table */
static struct domain_name_list **domain_list_table_1 = NULL; /* Ptr to domain name table 1 */
static struct domain_name_list **domain_list_table_2 = NULL; /* Ptr to domain name table 2 */


static db1_con_t* db_handle = 0;
static db_func_t perm_dbf;

static inline ip_addr_t *strtoipX(str *ips)
{
	/* try to figure out INET class */
	if(ips->s[0] == '[' || memchr(ips->s, ':', ips->len)!=NULL)
	{
		/* IPv6 */
		return str2ip6(ips);
	} else {
		/* IPv4 */
		return str2ip(ips);
	}
}

/*
 * Reload addr table to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_table(void)
{
	db_key_t cols[5];
	db1_res_t* res = NULL;
	db_row_t* row;
	db_val_t* val;

	struct addr_list **new_hash_table;
	struct subnet *new_subnet_table;
	struct domain_name_list **new_domain_name_table;
	int i;
	unsigned int gid;
	unsigned int port;
	unsigned int mask;
	str ips;
	ip_addr_t *ipa;
	char *tagv;

	cols[0] = &grp_col;
	cols[1] = &ip_addr_col;
	cols[2] = &mask_col;
	cols[3] = &port_col;
	cols[4] = &tag_col;

	if (perm_dbf.use_table(db_handle, &address_table) < 0) {
		LM_ERR("failed to use table\n");
		return -1;
	}

	if (perm_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 5, 0, &res) < 0) {
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

	/* Choose new domain name table */
	if (*domain_list_table == domain_list_table_1) {
		empty_domain_name_table(domain_list_table_2);
		new_domain_name_table = domain_list_table_2;
	} else {
		empty_domain_name_table(domain_list_table_1);
		new_domain_name_table = domain_list_table_1;
	}


	row = RES_ROWS(res);

	LM_DBG("Number of rows in address table: %d\n", RES_ROW_N(res));

	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		/* basic checks to db values */
		if (ROW_N(row + i) != 5)
		{
			LM_DBG("failure during checks of db address table: Colums %d - expected 5\n", ROW_N(row + i));
			goto dberror;
		}
		if ((VAL_TYPE(val) != DB1_INT) || VAL_NULL(val) || (VAL_INT(val) <= 0))
		{
			LM_DBG("failure during checks of database value 1 (group) in address table\n");
			goto dberror;
		}
		if ((VAL_TYPE(val + 1) != DB1_STRING) && (VAL_TYPE(val + 1) != DB1_STR))
		{
			LM_DBG("failure during checks of database value 2 (IP address) in address table - not a string value\n");
			goto dberror;
		}
		if (VAL_NULL(val + 1))
		{
			LM_DBG("failure during checks of database value 2 (IP address) in address table - NULL value not permitted\n");
			goto dberror;
		}
		if ((VAL_TYPE(val + 2) != DB1_INT) || VAL_NULL(val + 2))
		{
			LM_DBG("failure during checks of database value 3 (subnet size/CIDR) in address table\n");
			goto dberror;
		}
		if ((VAL_TYPE(val + 3) != DB1_INT) || VAL_NULL(val + 3))
		{
			LM_DBG("failure during checks of database value 4 (port) in address table\n");
			goto dberror;
		}
		gid = VAL_UINT(val);
		ips.s = (char *)VAL_STRING(val + 1);
		ips.len = strlen(ips.s);
		mask = VAL_UINT(val + 2);
		port = VAL_UINT(val + 3);
		tagv = VAL_NULL(val + 4)?NULL:(char *)VAL_STRING(val + 4);
		ipa = strtoipX(&ips);
		if ( ipa==NULL )
		{
			LM_DBG("Domain name: %.*s\n", ips.len, ips.s);
		//	goto dberror;
		} else {
			if(ipa->af == AF_INET6) {
				if((int)mask<0 || mask>128) {
					LM_DBG("failure during IP mask check for v6\n");
					goto dberror;
				}
			} else {
				if((int)mask<0 || mask>32) {
					LM_DBG("failure during IP mask check for v4\n");
					goto dberror;
				}
			}
		}

		if ( ipa ) {
			if ( (ipa->af==AF_INET6 && mask==128) || (ipa->af==AF_INET && mask==32) ) {
				if (addr_hash_table_insert(new_hash_table, gid, ipa, port, tagv)
						== -1) {
					LM_ERR("hash table problem\n");
					perm_dbf.free_result(db_handle, res);
					return -1;
				}
				LM_DBG("Tuple <%u, %s, %u> inserted into address hash table\n",
						gid, ips.s, port);
			} else {
				if (subnet_table_insert(new_subnet_table, gid, ipa, mask,
								port, tagv)
							== -1) {
					LM_ERR("subnet table problem\n");
					perm_dbf.free_result(db_handle, res);
					return -1;
				}
				LM_DBG("Tuple <%u, %s, %u, %u> inserted into subnet table\n",
						gid, ips.s, port, mask);
			}
		} else {
				if (domain_name_table_insert(new_domain_name_table, gid, &ips,
							port, tagv)
						== -1) {
				LM_ERR("domain name table problem\n");
				perm_dbf.free_result(db_handle, res);
				return -1;
			}
			LM_DBG("Tuple <%u, %s, %u> inserted into domain name table\n",
					gid, ips.s, port);
		}
	}

	perm_dbf.free_result(db_handle, res);

	*addr_hash_table = new_hash_table;
	*subnet_table = new_subnet_table;
	*domain_list_table = new_domain_name_table;

	LM_DBG("address table reloaded successfully.\n");

	return 1;

dberror:
	LM_ERR("database problem - invalid record\n");
	perm_dbf.free_result(db_handle, res);
	return -1;
}

/*
 * Wrapper to reload addr table from mi or rpc
 * we need to open the db_handle
 */
int reload_address_table_cmd(void)
{
	if (!db_handle) {
		db_handle = perm_dbf.init(&db_url);
		if (!db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}
	}

	if (reload_address_table () != 1) {
		perm_dbf.close(db_handle);
		db_handle = 0;
		return -1;
	}

	perm_dbf.close(db_handle);
	db_handle = 0;

	return 1;
}

/*
 * Initialize data structures
 */
int init_addresses(void)
{
	if (!db_url.s) {
		LM_INFO("db_url parameter of permissions module not set, "
				"disabling allow_address\n");
		return 0;
	} else {
		if (db_bind_mod(&db_url, &perm_dbf) < 0) {
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

	db_handle = perm_dbf.init(&db_url);
	if (!db_handle) {
		LM_ERR("unable to connect database\n");
		return -1;
	}

	if(db_check_table_version(&perm_dbf, db_handle, &address_table, TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		perm_dbf.close(db_handle);
		return -1;
	}

	addr_hash_table_1 = new_addr_hash_table();
	if (!addr_hash_table_1) return -1;

	addr_hash_table_2  = new_addr_hash_table();
	if (!addr_hash_table_2) goto error;

	addr_hash_table = (struct addr_list ***)shm_malloc
		(sizeof(struct addr_list **));
	if (!addr_hash_table) {
		LM_ERR("no more shm memory for addr_hash_table\n");
		goto error;
	}

	*addr_hash_table = addr_hash_table_1;

	subnet_table_1 = new_subnet_table();
	if (!subnet_table_1) goto error;

	subnet_table_2 = new_subnet_table();
	if (!subnet_table_2) goto error;

	subnet_table = (struct subnet **)shm_malloc(sizeof(struct subnet *));
	if (!subnet_table) {
		LM_ERR("no more shm memory for subnet_table\n");
		goto error;
	}

	*subnet_table = subnet_table_1;

	domain_list_table_1 = new_domain_name_table();
	if (!domain_list_table_1) goto error;

	domain_list_table_2 = new_domain_name_table();
	if (!domain_list_table_2) goto error;

	domain_list_table = (struct domain_name_list ***)shm_malloc(sizeof(struct domain_name_list **));
	if (!domain_list_table) {
		LM_ERR("no more shm memory for domain name table\n");
		goto error;
	}

	*domain_list_table = domain_list_table_1;


	if (reload_address_table() == -1) {
		LM_CRIT("reload of address table failed\n");
		goto error;
	}

	perm_dbf.close(db_handle);
	db_handle = 0;

	return 0;

error:
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

	if (domain_list_table_1) {
		free_domain_name_table(domain_list_table_1);
		domain_list_table_1 = 0;
	}
	if (domain_list_table_2) {
		free_domain_name_table(domain_list_table_2);
		domain_list_table_2 = 0;
	}
	if (domain_list_table) {
		shm_free(domain_list_table);
		domain_list_table = 0;
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
	if (!db_url.s) return 0;
	db_handle = perm_dbf.init(&db_url);
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
	if (domain_list_table_1) free_domain_name_table(domain_list_table_1);
	if (domain_list_table_2) free_domain_name_table(domain_list_table_2);
	if (domain_list_table) shm_free(domain_list_table);
}


/*
 * Checks if an entry exists in cached address table that belongs to a
 * given address group and has given ip address and port.  Port value
 * 0 in cached address table matches any port.
 */
int allow_address(struct sip_msg* _msg, char* _addr_group, char* _addr_sp,
		char* _port_sp)
{
	unsigned int port;
	int addr_group;
	str ips;
	struct ip_addr *ipa;

	if(fixup_get_ivalue(_msg, (gparam_p)_addr_group, &addr_group) !=0 ) {
		LM_ERR("cannot get group value\n");
		return -1;
	}

	if (_addr_sp==NULL
			|| (fixup_get_svalue(_msg, (gparam_p)_addr_sp, &ips) < 0)) {
		LM_ERR("cannot get value of address pvar\n");
		return -1;
	}

	ipa=strtoipX(&ips);

	if (_port_sp==NULL
			|| (fixup_get_ivalue(_msg, (gparam_p)_port_sp, (int*)&port) < 0)) {
		LM_ERR("cannot get value of port pvar\n");
		return -1;
	}

	if ( ipa ) {
		if (addr_hash_table
				&& match_addr_hash_table(*addr_hash_table, addr_group,
					ipa, port) == 1) {
			return 1;
		} else {
			if(subnet_table) {
				return match_subnet_table(*subnet_table, addr_group, ipa, port);
			}
		}
	} else {
		if(domain_list_table) {
			return match_domain_name_table(*domain_list_table, addr_group, &ips, port);
		}
	}
	return -1;
}


/*
 * allow_source_address("group") equals to allow_address("group", "$si", "$sp")
 * but is faster.
 */
int allow_source_address(struct sip_msg* _msg, char* _addr_group, char* _str2) 
{
	int addr_group = 1;

	if(_addr_group!=NULL
			&& fixup_get_ivalue(_msg, (gparam_p)_addr_group, &addr_group) !=0 ) {
		LM_ERR("cannot get group value\n");
		return -1;
	}

	LM_DBG("looking for <%u, %x, %u>\n",
			addr_group, _msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);

	if (addr_hash_table && match_addr_hash_table(*addr_hash_table, addr_group,
				&_msg->rcv.src_ip, _msg->rcv.src_port) == 1) {
		return 1;
	} else {
		if(subnet_table) {
			return match_subnet_table(*subnet_table, addr_group,
				&_msg->rcv.src_ip,
				_msg->rcv.src_port);
		}
	}
	return -1;
}


/*
 * Checks if source address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_source_address_group(struct sip_msg* _msg, char* _str1, char* _str2) 
{
	int group = -1;

	LM_DBG("looking for <%x, %u> in address table\n",
			_msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);
	if(addr_hash_table) {
		group = find_group_in_addr_hash_table(*addr_hash_table,
					&_msg->rcv.src_ip, _msg->rcv.src_port);
		LM_DBG("Found <%d>\n", group);

		if (group != -1) return group;
	}

	LM_DBG("looking for <%x, %u> in subnet table\n",
			_msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);
	if(subnet_table) {
		group = find_group_in_subnet_table(*subnet_table,
				&_msg->rcv.src_ip, _msg->rcv.src_port);
	}
	LM_DBG("Found <%d>\n", group);
	return group;

}

/*
 * Checks if address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_address_group(struct sip_msg* _msg, char* _addr, char* _port)
{
	int group = -1;

	unsigned int port;
	str ips;
	ip_addr_t *ipa;

	if (_addr==NULL
			|| (fixup_get_svalue(_msg, (gparam_p)_addr, &ips) < 0)) {
		LM_ERR("cannot get value of address pvar\n");
		return -1;
	}
	if (_port==NULL
			|| (fixup_get_ivalue(_msg, (gparam_p)_port, (int*)&port) < 0)) {
		LM_ERR("cannot get value of port pvar\n");
		return -1;
	}

	ipa=strtoipX(&ips);

	if ( ipa ) {
		LM_DBG("looking for <%.*s, %u> in address table\n",
				ips.len, ips.s, port);
		if(addr_hash_table) {
			group = find_group_in_addr_hash_table(*addr_hash_table,
						ipa, port);
			LM_DBG("Found address in group <%d>\n", group);

			if (group != -1) return group;
		}
		if(subnet_table) {
			LM_DBG("looking for <%.*s, %u> in subnet table\n",
					ips.len, ips.s, port);
			group = find_group_in_subnet_table(*subnet_table,
						ipa, port);
			LM_DBG("Found a match of subnet in group <%d>\n", group);
		}
	} else {
		LM_DBG("looking for <%.*s, %u> in domain_name table\n",
				ips.len, ips.s, port);
		if(domain_list_table) {
			group = find_group_in_domain_name_table(*domain_list_table,
						&ips, port);
			LM_DBG("Found a match of domain_name in group <%d>\n", group);
		}
	}

	LM_DBG("Found <%d>\n", group);
	return group;
}
