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
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <arpa/inet.h>

#include "permissions.h"
#include "hash.h"
#include "../../core/config.h"
#include "../../lib/srdb1/db.h"
#include "../../core/ip_addr.h"
#include "../../core/resolve.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_from.h"
#include "../../core/usr_avp.h"
#include "../../core/mod_fix.h"
#include "../../core/ut.h"

#define TABLE_VERSION 6

struct addr_list ***perm_addr_table =
		NULL; /* Ptr to current address hash table ptr */
struct addr_list **perm_addr_table_1 =
		NULL; /* Pointer to address hash table 1 */
struct addr_list **perm_addr_table_2 =
		NULL; /* Pointer to address hash table 2 */

struct subnet **perm_subnet_table = NULL;  /* Ptr to current subnet table */
struct subnet *perm_subnet_table_1 = NULL; /* Ptr to subnet table 1 */
struct subnet *perm_subnet_table_2 = NULL; /* Ptr to subnet table 2 */

struct domain_name_list ***perm_domain_table =
		NULL; /* Ptr to current domain name table */
static struct domain_name_list **perm_domain_table_1 =
		NULL; /* Ptr to domain name table 1 */
static struct domain_name_list **perm_domain_table_2 =
		NULL; /* Ptr to domain name table 2 */

static db1_con_t *perm_db_handle = 0;
static db_func_t perm_dbf;

extern str perm_address_file;

typedef struct address_tables_group
{
	struct addr_list **address_table;
	struct subnet *subnet_table;
	struct domain_name_list **domain_table;

} address_tables_group_t;

static inline ip_addr_t *strtoipX(str *ips)
{
	/* try to figure out INET class */
	if(ips->s[0] == '[' || memchr(ips->s, ':', ips->len) != NULL) {
		/* IPv6 */
		return str2ip6(ips);
	} else {
		/* IPv4 */
		return str2ip(ips);
	}
}

int reload_address_insert(address_tables_group_t *atg, unsigned int gid,
		str *ips, unsigned int mask, unsigned int port, str *tagv)
{
	ip_addr_t *ipa;

	ipa = strtoipX(ips);
	if(ipa == NULL) {
		LM_DBG("Domain name: %.*s\n", ips->len, ips->s);
		/* return -1; */
	} else {
		if(ipa->af == AF_INET6) {
			if((int)mask < 0 || mask > 128) {
				LM_DBG("failure during IP mask check for v6\n");
				return -1;
			}
			if(mask == 0) {
				mask = 128;
			}
		} else {
			if((int)mask < 0 || mask > 32) {
				LM_DBG("failure during IP mask check for v4\n");
				return -1;
			}
			if(mask == 0) {
				mask = 32;
			}
		}
	}

	if(ipa != NULL) {
		if((ipa->af == AF_INET6 && mask == 128)
				|| (ipa->af == AF_INET && mask == 32)) {
			if(addr_hash_table_insert(atg->address_table, gid, ipa, port, tagv)
					== -1) {
				LM_ERR("hash table problem\n");
				return -1;
			}
			LM_DBG("Tuple <%u, %.*s, %u> inserted into address hash table\n",
					gid, ips->len, ips->s, port);
		} else {
			if(subnet_table_insert(
					   atg->subnet_table, gid, ipa, mask, port, tagv)
					== -1) {
				LM_ERR("subnet table problem\n");
				return -1;
			}
			LM_DBG("Tuple <%u, %.*s, %u, %u> inserted into subnet table\n", gid,
					ips->len, ips->s, port, mask);
		}
	} else {
		if(domain_name_table_insert(atg->domain_table, gid, ips, port, tagv)
				== -1) {
			LM_ERR("domain name table problem\n");
			return -1;
		}
		LM_DBG("Tuple <%u, %.*s, %u> inserted into domain name table\n", gid,
				ips->len, ips->s, port);
	}
	return 0;
}

/*
 * Reload addr table from database to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_db_table(address_tables_group_t *atg)
{
	db_key_t cols[5];
	db1_res_t *res = NULL;
	db_row_t *row;
	db_val_t *val;

	int i;
	unsigned int gid;
	unsigned int port;
	unsigned int mask;
	str ips;
	str tagv;

	cols[0] = &perm_grp_col;
	cols[1] = &perm_ip_addr_col;
	cols[2] = &perm_mask_col;
	cols[3] = &perm_port_col;
	cols[4] = &perm_tag_col;

	if(perm_dbf.use_table(perm_db_handle, &perm_address_table) < 0) {
		LM_ERR("failed to use table\n");
		return -1;
	}

	if(perm_dbf.query(perm_db_handle, NULL, 0, NULL, cols, 0, 5, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		return -1;
	}

	row = RES_ROWS(res);

	LM_DBG("Number of rows in address table: %d\n", RES_ROW_N(res));

	for(i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		/* basic checks to db values */
		if(ROW_N(row + i) != 5) {
			LM_DBG("failure during checks of db address table: Columns %d - "
				   "expected 5\n",
					ROW_N(row + i));
			goto dberror;
		}
		if((VAL_TYPE(val) != DB1_INT) || VAL_NULL(val) || (VAL_INT(val) <= 0)) {
			LM_DBG("failure during checks of database value 1 (group) in "
				   "address table\n");
			goto dberror;
		}
		if((VAL_TYPE(val + 1) != DB1_STRING)
				&& (VAL_TYPE(val + 1) != DB1_STR)) {
			LM_DBG("failure during checks of database value 2 (IP address) in "
				   "address table - not a string value\n");
			goto dberror;
		}
		if(VAL_NULL(val + 1)) {
			LM_DBG("failure during checks of database value 2 (IP address) in "
				   "address table - NULL value not permitted\n");
			goto dberror;
		}
		if((VAL_TYPE(val + 2) != DB1_INT) || VAL_NULL(val + 2)) {
			LM_DBG("failure during checks of database value 3 (subnet "
				   "size/CIDR) in address table\n");
			goto dberror;
		}
		if((VAL_TYPE(val + 3) != DB1_INT) || VAL_NULL(val + 3)) {
			LM_DBG("failure during checks of database value 4 (port) in "
				   "address table\n");
			goto dberror;
		}
		gid = VAL_UINT(val);
		ips.s = (char *)VAL_STRING(val + 1);
		ips.len = strlen(ips.s);
		mask = VAL_UINT(val + 2);
		port = VAL_UINT(val + 3);
		tagv.s = VAL_NULL(val + 4) ? NULL : (char *)VAL_STRING(val + 4);
		if(tagv.s != NULL) {
			tagv.len = strlen(tagv.s);
		}
		if(reload_address_insert(atg, gid, &ips, mask, port, &tagv) < 0) {
			goto dberror;
		}
	}

	perm_dbf.free_result(perm_db_handle, res);

	return 1;

dberror:
	LM_ERR("database problem - invalid record\n");
	perm_dbf.free_result(perm_db_handle, res);
	return -1;
}

/**
 * macros for parsing address file
 */
#define PERM_FADDR_SKIPWS(p)                                                 \
	do {                                                                     \
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) { \
			p++;                                                             \
		}                                                                    \
	} while(0)

#define PERM_FADDR_PARSESTR(p, vstr)                                    \
	do {                                                                \
		vstr.s = p;                                                     \
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' \
				&& *p != '#') {                                         \
			p++;                                                        \
		}                                                               \
		vstr.len = p - vstr.s;                                          \
	} while(0)

#define PERM_FADDR_PARSENUM(p, vnum)       \
	do {                                   \
		vnum = 0;                          \
		while(*p >= '0' && *p <= '9') {    \
			vnum = vnum * 10 + (*p - '0'); \
			p++;                           \
		}                                  \
	} while(0)

/* end-of-data jump at start of comment or end-of-line */
#define PERM_FADDR_EODJUMP(p, jumplabel) \
	do {                                 \
		if(*p == '\0' || *p == '#') {    \
			goto jumplabel;              \
		}                                \
	} while(0)

/*
 * Reload addr table from file to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_file_table(address_tables_group_t *atg)
{
	char line[1024], *p;
	FILE *f = NULL;
	int i = 0;
	int n = 0;
	unsigned int gid;
	unsigned int mask;
	unsigned int port;
	str ips;
	str tagv;

	f = fopen(perm_address_file.s, "r");
	if(f == NULL) {
		LM_ERR("can't open list file [%s]\n", perm_address_file.s);
		return -1;
	}

	p = fgets(line, 1024, f);
	while(p) {
		i++;
		gid = 0;
		ips.s = NULL;
		ips.len = 0;
		mask = 0;
		port = 0;
		tagv.s = NULL;
		tagv.len = 0;

		/* comment line */
		PERM_FADDR_SKIPWS(p);
		PERM_FADDR_EODJUMP(p, next_line);

		/* group id */
		PERM_FADDR_PARSENUM(p, gid);

		PERM_FADDR_SKIPWS(p);
		PERM_FADDR_EODJUMP(p, error);

		/* address - ip/domain */
		PERM_FADDR_PARSESTR(p, ips);

		PERM_FADDR_SKIPWS(p);
		PERM_FADDR_EODJUMP(p, add_record);

		/* mask */
		PERM_FADDR_PARSENUM(p, mask);

		PERM_FADDR_SKIPWS(p);
		PERM_FADDR_EODJUMP(p, add_record);

		/* port */
		PERM_FADDR_PARSENUM(p, port);

		PERM_FADDR_SKIPWS(p);
		PERM_FADDR_EODJUMP(p, add_record);

		/* tag */
		PERM_FADDR_PARSESTR(p, tagv);

	add_record:
		if(reload_address_insert(atg, gid, &ips, mask, port, &tagv) < 0) {
			goto error;
		}
		n++;

	next_line:
		p = fgets(line, 1024, f);
	}

	LM_DBG("processed file: %s (%d lines)- added %d records\n",
			perm_address_file.s, i, n);

	fclose(f);
	return 1;

error:
	if(f != NULL) {
		fclose(f);
	}
	return -1;
}

/*
 * Reload addr table to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_table(void)
{
	int ret = 0;
	address_tables_group_t atg;

	/* Choose new hash table and free its old contents */
	if(*perm_addr_table == perm_addr_table_1) {
		empty_addr_hash_table(perm_addr_table_2);
		atg.address_table = perm_addr_table_2;
	} else {
		empty_addr_hash_table(perm_addr_table_1);
		atg.address_table = perm_addr_table_1;
	}

	/* Choose new subnet table */
	if(*perm_subnet_table == perm_subnet_table_1) {
		empty_subnet_table(perm_subnet_table_2);
		atg.subnet_table = perm_subnet_table_2;
	} else {
		empty_subnet_table(perm_subnet_table_1);
		atg.subnet_table = perm_subnet_table_1;
	}

	/* Choose new domain name table */
	if(*perm_domain_table == perm_domain_table_1) {
		empty_domain_name_table(perm_domain_table_2);
		atg.domain_table = perm_domain_table_2;
	} else {
		empty_domain_name_table(perm_domain_table_1);
		atg.domain_table = perm_domain_table_1;
	}

	if(perm_address_file.s == NULL) {
		ret = reload_address_db_table(&atg);
	} else {
		ret = reload_address_file_table(&atg);
	}
	if(ret != 1) {
		return ret;
	}

	*perm_addr_table = atg.address_table;
	*perm_subnet_table = atg.subnet_table;
	*perm_domain_table = atg.domain_table;

	LM_DBG("address table reloaded successfully.\n");


	return ret;
}

/*
 * Wrapper to reload addr table from mi or rpc
 * we need to open the db_handle
 */
int reload_address_table_cmd(void)
{
	if(perm_address_file.s == NULL) {
		if(!perm_db_url.s) {
			LM_ERR("db_url not set\n");
			return -1;
		}

		if(!perm_db_handle) {
			perm_db_handle = perm_dbf.init(&perm_db_url);
			if(!perm_db_handle) {
				LM_ERR("unable to connect database\n");
				return -1;
			}
		}
	}

	if(reload_address_table() != 1) {
		if(perm_address_file.s == NULL) {
			perm_dbf.close(perm_db_handle);
			perm_db_handle = 0;
		}
		return -1;
	}

	if(perm_address_file.s == NULL) {
		perm_dbf.close(perm_db_handle);
		perm_db_handle = 0;
	}

	return 1;
}

/*
 * Initialize data structures
 */
int init_addresses(void)
{
	perm_addr_table_1 = perm_addr_table_2 = 0;
	perm_addr_table = 0;

	if(perm_address_file.s == NULL) {
		if(!perm_db_url.s) {
			LM_INFO("db_url parameter of permissions module not set, "
					"disabling allow_address\n");
			return 0;
		} else {
			if(db_bind_mod(&perm_db_url, &perm_dbf) < 0) {
				LM_ERR("load a database support module\n");
				return -1;
			}

			if(!DB_CAPABILITY(perm_dbf, DB_CAP_QUERY)) {
				LM_ERR("database module does not implement 'query' function\n");
				return -1;
			}
		}

		perm_db_handle = perm_dbf.init(&perm_db_url);
		if(!perm_db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}

		if(db_check_table_version(&perm_dbf, perm_db_handle,
				   &perm_address_table, TABLE_VERSION)
				< 0) {
			DB_TABLE_VERSION_ERROR(perm_address_table);
			perm_dbf.close(perm_db_handle);
			perm_db_handle = 0;
			return -1;
		}
	}

	perm_addr_table_1 = new_addr_hash_table();
	if(!perm_addr_table_1)
		return -1;

	perm_addr_table_2 = new_addr_hash_table();
	if(!perm_addr_table_2)
		goto error;

	perm_addr_table =
			(struct addr_list ***)shm_malloc(sizeof(struct addr_list **));
	if(!perm_addr_table) {
		LM_ERR("no more shm memory for addr_hash_table\n");
		goto error;
	}

	*perm_addr_table = perm_addr_table_1;

	perm_subnet_table_1 = new_subnet_table();
	if(!perm_subnet_table_1)
		goto error;

	perm_subnet_table_2 = new_subnet_table();
	if(!perm_subnet_table_2)
		goto error;

	perm_subnet_table = (struct subnet **)shm_malloc(sizeof(struct subnet *));
	if(!perm_subnet_table) {
		LM_ERR("no more shm memory for subnet_table\n");
		goto error;
	}

	*perm_subnet_table = perm_subnet_table_1;

	perm_domain_table_1 = new_domain_name_table();
	if(!perm_domain_table_1)
		goto error;

	perm_domain_table_2 = new_domain_name_table();
	if(!perm_domain_table_2)
		goto error;

	perm_domain_table = (struct domain_name_list ***)shm_malloc(
			sizeof(struct domain_name_list **));
	if(!perm_domain_table) {
		LM_ERR("no more shm memory for domain name table\n");
		goto error;
	}

	*perm_domain_table = perm_domain_table_1;


	if(reload_address_table() == -1) {
		LM_CRIT("reload of address table failed\n");
		goto error;
	}

	if(perm_address_file.s == NULL) {
		perm_dbf.close(perm_db_handle);
		perm_db_handle = 0;
	}

	return 0;

error:
	if(perm_addr_table_1) {
		free_addr_hash_table(perm_addr_table_1);
		perm_addr_table_1 = 0;
	}
	if(perm_addr_table_2) {
		free_addr_hash_table(perm_addr_table_2);
		perm_addr_table_2 = 0;
	}
	if(perm_addr_table) {
		shm_free(perm_addr_table);
		perm_addr_table = 0;
	}
	if(perm_subnet_table_1) {
		free_subnet_table(perm_subnet_table_1);
		perm_subnet_table_1 = 0;
	}
	if(perm_subnet_table_2) {
		free_subnet_table(perm_subnet_table_2);
		perm_subnet_table_2 = 0;
	}
	if(perm_subnet_table) {
		shm_free(perm_subnet_table);
		perm_subnet_table = 0;
	}

	if(perm_domain_table_1) {
		free_domain_name_table(perm_domain_table_1);
		perm_domain_table_1 = 0;
	}
	if(perm_domain_table_2) {
		free_domain_name_table(perm_domain_table_2);
		perm_domain_table_2 = 0;
	}
	if(perm_domain_table) {
		shm_free(perm_domain_table);
		perm_domain_table = 0;
	}

	if(perm_address_file.s == NULL) {
		perm_dbf.close(perm_db_handle);
		perm_db_handle = 0;
	}
	return -1;
}


/*
 * Close connections and release memory
 */
void clean_addresses(void)
{
	if(perm_addr_table_1)
		free_addr_hash_table(perm_addr_table_1);
	if(perm_addr_table_2)
		free_addr_hash_table(perm_addr_table_2);
	if(perm_addr_table)
		shm_free(perm_addr_table);
	if(perm_subnet_table_1)
		free_subnet_table(perm_subnet_table_1);
	if(perm_subnet_table_2)
		free_subnet_table(perm_subnet_table_2);
	if(perm_subnet_table)
		shm_free(perm_subnet_table);
	if(perm_domain_table_1)
		free_domain_name_table(perm_domain_table_1);
	if(perm_domain_table_2)
		free_domain_name_table(perm_domain_table_2);
	if(perm_domain_table)
		shm_free(perm_domain_table);
}


int allow_address(sip_msg_t *_msg, int addr_group, str *ips, int port)
{
	struct ip_addr *ipa;

	ipa = strtoipX(ips);

	if(ipa) {
		if(perm_addr_table
				&& match_addr_hash_table(*perm_addr_table, addr_group, ipa,
						   (unsigned int)port)
						   == 1) {
			return 1;
		} else {
			if(perm_subnet_table) {
				return match_subnet_table(*perm_subnet_table, addr_group, ipa,
						(unsigned int)port);
			}
		}
	} else {
		if(perm_domain_table) {
			return match_domain_name_table(
					*perm_domain_table, addr_group, ips, (unsigned int)port);
		}
	}
	return -1;
}

/*
 * Checks if an entry exists in cached address table that belongs to a
 * given address group and has given ip address and port.  Port value
 * 0 in cached address table matches any port.
 */
int w_allow_address(
		struct sip_msg *_msg, char *_addr_group, char *_addr_sp, char *_port_sp)
{
	int port;
	int addr_group;
	str ips;

	if(fixup_get_ivalue(_msg, (gparam_p)_addr_group, &addr_group) != 0) {
		LM_ERR("cannot get group value\n");
		return -1;
	}

	if(_addr_sp == NULL
			|| (fixup_get_svalue(_msg, (gparam_p)_addr_sp, &ips) < 0)) {
		LM_ERR("cannot get value of address pvar\n");
		return -1;
	}

	if(_port_sp == NULL
			|| (fixup_get_ivalue(_msg, (gparam_p)_port_sp, &port) < 0)) {
		LM_ERR("cannot get value of port pvar\n");
		return -1;
	}

	return allow_address(_msg, addr_group, &ips, port);
}

int allow_source_address(sip_msg_t *_msg, int addr_group)
{
	LM_DBG("looking for <%u, %x, %u>\n", addr_group,
			_msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);

	if(perm_addr_table
			&& match_addr_hash_table(*perm_addr_table, addr_group,
					   &_msg->rcv.src_ip, _msg->rcv.src_port)
					   == 1) {
		return 1;
	} else {
		if(perm_subnet_table) {
			return match_subnet_table(*perm_subnet_table, addr_group,
					&_msg->rcv.src_ip, _msg->rcv.src_port);
		}
	}
	return -1;
}

/*
 * w_allow_source_address("group") equals to allow_address("group", "$si", "$sp")
 * but is faster.
 */
int w_allow_source_address(struct sip_msg *_msg, char *_addr_group, char *_str2)
{
	int addr_group = 1;

	if(_addr_group != NULL
			&& fixup_get_ivalue(_msg, (gparam_p)_addr_group, &addr_group)
					   != 0) {
		LM_ERR("cannot get group value\n");
		return -1;
	}
	return allow_source_address(_msg, addr_group);
}


/*
 * Checks if source address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int ki_allow_source_address_group(sip_msg_t *_msg)
{
	int group = -1;

	LM_DBG("looking for <%x, %u> in address table\n",
			_msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);
	if(perm_addr_table) {
		group = find_group_in_addr_hash_table(
				*perm_addr_table, &_msg->rcv.src_ip, _msg->rcv.src_port);
		LM_DBG("Found <%d>\n", group);

		if(group != -1)
			return group;
	}

	LM_DBG("looking for <%x, %u> in subnet table\n",
			_msg->rcv.src_ip.u.addr32[0], _msg->rcv.src_port);
	if(perm_subnet_table) {
		group = find_group_in_subnet_table(
				*perm_subnet_table, &_msg->rcv.src_ip, _msg->rcv.src_port);
	}
	LM_DBG("Found <%d>\n", group);
	return group;
}

/*
 * Checks if source address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_source_address_group(struct sip_msg *_msg, char *_str1, char *_str2)
{
	return ki_allow_source_address_group(_msg);
}

/*
 * Checks if address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int ki_allow_address_group(sip_msg_t *_msg, str *_addr, int _port)
{
	int group = -1;

	ip_addr_t *ipa;

	ipa = strtoipX(_addr);

	if(ipa) {
		LM_DBG("looking for <%.*s, %u> in address table\n", _addr->len,
				_addr->s, (unsigned int)_port);
		if(perm_addr_table) {
			group = find_group_in_addr_hash_table(
					*perm_addr_table, ipa, (unsigned int)_port);
			LM_DBG("Found address in group <%d>\n", group);

			if(group != -1)
				return group;
		}
		if(perm_subnet_table) {
			LM_DBG("looking for <%.*s, %u> in subnet table\n", _addr->len,
					_addr->s, _port);
			group = find_group_in_subnet_table(
					*perm_subnet_table, ipa, (unsigned int)_port);
			LM_DBG("Found a match of subnet in group <%d>\n", group);
		}
	} else {
		LM_DBG("looking for <%.*s, %u> in domain_name table\n", _addr->len,
				_addr->s, (unsigned int)_port);
		if(perm_domain_table) {
			group = find_group_in_domain_name_table(
					*perm_domain_table, _addr, (unsigned int)_port);
			LM_DBG("Found a match of domain_name in group <%d>\n", group);
		}
	}

	LM_DBG("Found <%d>\n", group);
	return group;
}

int allow_address_group(struct sip_msg *_msg, char *_addr, char *_port)
{
	int port;
	str ips;

	if(_addr == NULL || (fixup_get_svalue(_msg, (gparam_p)_addr, &ips) < 0)) {
		LM_ERR("cannot get value of address pvar\n");
		return -1;
	}
	if(_port == NULL || (fixup_get_ivalue(_msg, (gparam_p)_port, &port) < 0)) {
		LM_ERR("cannot get value of port pvar\n");
		return -1;
	}

	return ki_allow_address_group(_msg, &ips, port);
}
