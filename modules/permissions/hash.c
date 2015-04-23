/*
 * Hash functions for cached trusted and address tables
 *
 * Copyright (C) 2003-2012 Juha Heinanen
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
 */

#include <sys/types.h>
#include <regex.h>
#include "../../mem/shm_mem.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../hashes.h"
#include "../../usr_avp.h"
#include "../../ip_addr.h"
#include "../../pvar.h"
#include "hash.h"
#include "trusted.h"
#include "address.h"

#define perm_hash(_s)  core_hash( &(_s), 0, PERM_HASH_SIZE)


/* tag AVP specs */
static int     tag_avp_type;
static int_str tag_avp;

extern int peer_tag_mode;



/*
 * Parse and set tag AVP specs
 */
int init_tag_avp(str *tag_avp_param)
{
	pv_spec_t avp_spec;
	unsigned short avp_flags;

	if (tag_avp_param->s && tag_avp_param->len > 0) {
		if (pv_parse_spec(tag_avp_param, &avp_spec)==0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non "
					"AVP %.*s peer_tag_avp definition\n", tag_avp_param->len, tag_avp_param->s);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &tag_avp, &avp_flags)!=0) {
			LM_ERR("[%.*s]- invalid "
					"peer_tag_avp AVP definition\n", tag_avp_param->len, tag_avp_param->s);
			return -1;
		}
		tag_avp_type = avp_flags;
	} else {
		tag_avp.n = 0;
	}
	return 0;
}


/*
 * Gets tag avp specs
 */
void get_tag_avp(int_str *tag_avp_p, int *tag_avp_type_p)
{
	*tag_avp_p = tag_avp;
	*tag_avp_type_p = tag_avp_type;
}


/*
 * Create and initialize a hash table
 */
struct trusted_list** new_hash_table(void)
{
	struct trusted_list** ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct trusted_list **)shm_malloc
		(sizeof(struct trusted_list*) * PERM_HASH_SIZE);
	if (!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct trusted_list*) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_hash_table(struct trusted_list** table)
{
	if (!table)
		return;

	empty_hash_table(table);
	shm_free(table);
}


/* 
 * Add <src_ip, proto, pattern, tag> into hash table, where proto is integer
 * representation of string argument proto.
 */
int hash_table_insert(struct trusted_list** table, char* src_ip, 
		char* proto, char* pattern, char* tag)
{
	struct trusted_list *np;
	unsigned int hash_val;

	np = (struct trusted_list *) shm_malloc(sizeof(*np));
	if (np == NULL) {
		LM_ERR("cannot allocate shm memory for table entry\n");
		return -1;
	}

	if (strcasecmp(proto, "any") == 0) {
		np->proto = PROTO_NONE;
	} else if (strcasecmp(proto, "udp") == 0) {
		np->proto = PROTO_UDP;
	} else if (strcasecmp(proto, "tcp") == 0) {
		np->proto = PROTO_TCP;
	} else if (strcasecmp(proto, "tls") == 0) {
		np->proto = PROTO_TLS;
	} else if (strcasecmp(proto, "sctp") == 0) {
		np->proto = PROTO_SCTP;
	} else if (strcasecmp(proto, "ws") == 0) {
		np->proto = PROTO_WS;
	} else if (strcasecmp(proto, "wss") == 0) {
		np->proto = PROTO_WSS;
	} else if (strcasecmp(proto, "none") == 0) {
		shm_free(np);
		return 1;
	} else {
		LM_CRIT("unknown protocol\n");
		shm_free(np);
		return -1;
	}

	np->src_ip.len = strlen(src_ip);
	np->src_ip.s = (char *) shm_malloc(np->src_ip.len+1);

	if (np->src_ip.s == NULL) {
		LM_CRIT("cannot allocate shm memory for src_ip string\n");
		shm_free(np);
		return -1;
	}

	(void) strncpy(np->src_ip.s, src_ip, np->src_ip.len);
	np->src_ip.s[np->src_ip.len] = 0;

	if (pattern) {
		np->pattern = (char *) shm_malloc(strlen(pattern)+1);
		if (np->pattern == NULL) {
			LM_CRIT("cannot allocate shm memory for pattern string\n");
			shm_free(np->src_ip.s);
			shm_free(np);
			return -1;
		}
		(void) strcpy(np->pattern, pattern);
	} else {
		np->pattern = 0;
	}

	if (tag) {
		np->tag.len = strlen(tag);
		np->tag.s = (char *) shm_malloc((np->tag.len) + 1);
		if (np->tag.s == NULL) {
			LM_CRIT("cannot allocate shm memory for pattern string\n");
			shm_free(np->src_ip.s);
			shm_free(np->pattern);
			shm_free(np);
			return -1;
		}
		(void) strcpy(np->tag.s, tag);
	} else {
		np->tag.len = 0;
		np->tag.s = 0;
	}

	hash_val = perm_hash(np->src_ip);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/* 
 * Check if an entry exists in hash table that has given src_ip and protocol
 * value and pattern that matches to From URI.  If an entry exists and tag_avp
 * has been defined, tag of the entry is added as a value to tag_avp.
 * Returns number of matches or -1 if none matched.
 */
int match_hash_table(struct trusted_list** table, struct sip_msg* msg,
		char *src_ip_c_str, int proto)
{
	str uri;
	char uri_string[MAX_URI_SIZE + 1];
	regex_t preg;
	struct trusted_list *np;
	str src_ip;
	int_str val;
	int count = 0;

	src_ip.s = src_ip_c_str;
	src_ip.len = strlen(src_ip.s);

	if (IS_SIP(msg))
	{
		if (parse_from_header(msg) < 0) return -1;
		uri = get_from(msg)->uri;
		if (uri.len > MAX_URI_SIZE) {
			LM_ERR("from URI too large\n");
			return -1;
		}
		memcpy(uri_string, uri.s, uri.len);
		uri_string[uri.len] = (char)0;
	}

	for (np = table[perm_hash(src_ip)]; np != NULL; np = np->next) {
	    if ((np->src_ip.len == src_ip.len) && 
		(strncmp(np->src_ip.s, src_ip.s, src_ip.len) == 0) &&
		((np->proto == PROTO_NONE) || (proto == PROTO_NONE) ||
		 (np->proto == proto))) {
		if (np->pattern && IS_SIP(msg)) {
		    if (regcomp(&preg, np->pattern, REG_NOSUB)) {
			LM_ERR("invalid regular expression\n");
			continue;
		    }
		    if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
			regfree(&preg);
			continue;
		    }
		    regfree(&preg);
		}
		/* Found a match */
		if (tag_avp.n && np->tag.s) {
		    val.s = np->tag;
		    if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
			LM_ERR("setting of tag_avp failed\n");
			return -1;
		    }
		}
		if (!peer_tag_mode)
		    return 1;
		count++;
	    }
	}
	if (!count)
	    return -1;
	else 
	    return count;
}


/*! \brief
 * MI Interface :: Print trusted entries stored in hash table 
 */
int hash_table_mi_print(struct trusted_list** table, struct mi_node* rpl)
{
	int i;
	struct trusted_list *np;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if (addf_mi_node_child(rpl, 0, 0, 0,
						"%4d <%.*s, %d, %s, %s>",
						i,
						np->src_ip.len, ZSW(np->src_ip.s),
						np->proto,
						np->pattern?np->pattern:"NULL",
						np->tag.len?np->tag.s:"NULL") == 0) {
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}

/*! \brief
 * RPC interface :: Print trusted entries stored in hash table 
 */
int hash_table_rpc_print(struct trusted_list** hash_table, rpc_t* rpc, void* c)
{
	int i;
	struct trusted_list *np;
	void* th;
	void* ih;

	if (rpc->add(c, "{", &th) < 0)
	{
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			if(rpc->struct_add(th, "d{", 
					"table", i,
					"item", &ih) < 0)
                        {
                                rpc->fault(c, 500, "Internal error creating rpc ih");
                                return -1;
                        }

			if(rpc->struct_add(ih, "s", "ip", np->src_ip.s) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "dss", "proto",  np->proto,
						"pattern",  np->pattern ? np->pattern : "NULL",
						"tag",  np->tag.len ? np->tag.s : "NULL") < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}

/* 
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_hash_table(struct trusted_list **table)
{
	int i;
	struct trusted_list *np, *next;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if (np->src_ip.s) shm_free(np->src_ip.s);
			if (np->pattern) shm_free(np->pattern);
			if (np->tag.s) shm_free(np->tag.s);
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}


/*
 * Create and initialize an address hash table
 */
struct addr_list** new_addr_hash_table(void)
{
	struct addr_list** ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct addr_list **)shm_malloc
		(sizeof(struct addr_list*) * PERM_HASH_SIZE);
	if (!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct addr_list*) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_addr_hash_table(struct addr_list** table)
{
	if (!table)
		return;

	empty_addr_hash_table(table);
	shm_free(table);
}


/* 
 * Add <grp, ip_addr, port> into hash table
 */
int addr_hash_table_insert(struct addr_list** table, unsigned int grp,
		ip_addr_t *addr, unsigned int port, char *tagv)
{
	struct addr_list *np;
	unsigned int hash_val;
	str addr_str;
	int len;

	len = sizeof(struct addr_list);
	if(tagv!=NULL)
		len += strlen(tagv) + 1;

	np = (struct addr_list *) shm_malloc(len);
	if (np == NULL) {
		LM_ERR("no shm memory for table entry\n");
		return -1;
	}

	memset(np, 0, len);

	np->grp = grp;
	memcpy(&np->addr, addr, sizeof(ip_addr_t));
	np->port = port;
	if(tagv!=NULL)
	{
		np->tag.s = (char*)np + sizeof(struct addr_list);
		np->tag.len = strlen(tagv);
		strcpy(np->tag.s, tagv);
	}

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;
	hash_val = perm_hash(addr_str);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/* 
 * Check if an entry exists in hash table that has given group, ip_addr, and
 * port.  Port 0 in hash table matches any port.
 */
int match_addr_hash_table(struct addr_list** table, unsigned int group,
		ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;
	avp_value_t val;

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;

	for (np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if ( (np->grp == group)
				&& ((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			if (tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return 1;
		}
	}

	return -1;
}


/* 
 * Check if an ip_addr/port entry exists in hash table in any group.
 * Returns first group in which ip_addr/port is found.
 * Port 0 in hash table matches any port. 
 */
int find_group_in_addr_hash_table(struct addr_list** table,
		ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;
	avp_value_t val;

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;

	for (np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if (((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			if (tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return np->grp;
		}
	}

	return -1;
}

/*! \brief
 * MI: Print addresses stored in hash table 
 */
int addr_hash_table_mi_print(struct addr_list** table, struct mi_node* rpl)
{
	int i;
	struct addr_list *np;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if (addf_mi_node_child(rpl, 0, 0, 0,
						"%4d <%u, %s, %u> [%s]",
						i, np->grp, ip_addr2a(&np->addr),
						np->port, (np->tag.s==NULL)?"":np->tag.s) == 0)
				return -1;
			np = np->next;
		}
	}
	return 0;
}

/*! \brief
 * RPC: Print addresses stored in hash table 
 */
int addr_hash_table_rpc_print(struct addr_list** table, rpc_t* rpc, void* c)
{
	int i;
	void* th;
	void* ih;
	struct addr_list *np;


	if (rpc->add(c, "{", &th) < 0)
	{
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if(rpc->struct_add(th, "dd{", 
					"table", i,
					"group", np->grp,
					"item", &ih) < 0)
                        {
                                rpc->fault(c, 500, "Internal error creating rpc ih");
                                return -1;
                        }

			if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&np->addr)) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "ds", "port",  np->port,
						"tag",  np->tag.len ? np->tag.s : "NULL") < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}


/* 
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_addr_hash_table(struct addr_list **table)
{
	int i;
	struct addr_list *np, *next;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}


/*
 * Create and initialize a subnet table
 */
struct subnet* new_subnet_table(void)
{
	struct subnet* ptr;

	/* subnet record [PERM_MAX_SUBNETS] contains in its grp field 
	   the number of subnet records in the subnet table */
	ptr = (struct subnet *)shm_malloc
		(sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	if (!ptr) {
		LM_ERR("no shm memory for subnet table\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	return ptr;
}


/* 
 * Add <grp, subnet, mask, port, tag> into subnet table so that table is
 * kept in increasing ordered according to grp.
 */
int subnet_table_insert(struct subnet* table, unsigned int grp,
		ip_addr_t *subnet, unsigned int mask,
		unsigned int port, char *tagv)
{
	int i;
	unsigned int count;
	str tags;

	count = table[PERM_MAX_SUBNETS].grp;

	if (count == PERM_MAX_SUBNETS) {
		LM_CRIT("subnet table is full\n");
		return 0;
	}

	if(tagv==NULL)
	{
		tags.s = NULL;
		tags.len = 0;
	} else {
		tags.len = strlen(tagv);
		tags.s = (char*)shm_malloc(tags.len+1);
		if(tags.s==NULL)
		{
			LM_ERR("No more shared memory\n");
			return 0;
		}
		strcpy(tags.s, tagv);
	}

	i = count - 1;

	while ((i >= 0) && (table[i].grp > grp)) {
		table[i + 1] = table[i];
		i--;
	}

	table[i + 1].grp = grp;
	memcpy(&table[i + 1].subnet, subnet, sizeof(ip_addr_t));
	table[i + 1].port = port;
	table[i + 1].mask = mask;
	table[i + 1].tag = tags;

	table[PERM_MAX_SUBNETS].grp = count + 1;

	return 1;
}


/* 
 * Check if an entry exists in subnet table that matches given group, ip_addr,
 * and port.  Port 0 in subnet table matches any port.
 */
int match_subnet_table(struct subnet* table, unsigned int grp,
		ip_addr_t *addr, unsigned int port)
{
	unsigned int count, i;
	avp_value_t val;

	count = table[PERM_MAX_SUBNETS].grp;

	i = 0;
	while ((i < count) && (table[i].grp < grp))
		i++;

	if (i == count) return -1;

	while ((i < count) && (table[i].grp == grp)) {
		if (((table[i].port == port) || (table[i].port == 0))
			&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)==0))
		{
			if (tag_avp.n && table[i].tag.s) {
				val.s = table[i].tag;
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}
			return 1;
		}
		i++;
	}

	return -1;
}


/* 
 * Check if an entry exists in subnet table that matches given ip_addr,
 * and port.  Port 0 in subnet table matches any port.  Return group of
 * first match or -1 if no match is found.
 */
int find_group_in_subnet_table(struct subnet* table,
		ip_addr_t *addr, unsigned int port)
{
	unsigned int count, i;
	avp_value_t val;

	count = table[PERM_MAX_SUBNETS].grp;

	i = 0;
	while (i < count) {
		if ( ((table[i].port == port) || (table[i].port == 0))
			&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)==0))
		{
			if (tag_avp.n && table[i].tag.s) {
				val.s = table[i].tag;
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}
			return table[i].grp;
		}
		i++;
	}

	return -1;
}


/* 
 * Print subnets stored in subnet table 
 */
int subnet_table_mi_print(struct subnet* table, struct mi_node* rpl)
{
	unsigned int count, i;

	count = table[PERM_MAX_SUBNETS].grp;

	for (i = 0; i < count; i++) {
		if (addf_mi_node_child(rpl, 0, 0, 0,
					"%4d <%u, %s, %u, %u> [%s]",
					i, table[i].grp, ip_addr2a(&table[i].subnet),
					table[i].mask, table[i].port,
					(table[i].tag.s==NULL)?"":table[i].tag.s) == 0) {
			return -1;
		}
	}
	return 0;
}

/*! \brief
 * RPC interface :: Print subnet entries stored in hash table 
 */
int subnet_table_rpc_print(struct subnet* table, rpc_t* rpc, void* c)
{
	int i;
	int count;
	void* th;
	void* ih;

	count = table[PERM_MAX_SUBNETS].grp;

	if (rpc->add(c, "{", &th) < 0)
	{
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for (i = 0; i < count; i++) {
		if(rpc->struct_add(th, "dd{", 
				"id", i,
				"group", table[i].grp,
				"item", &ih) < 0)
                {
                        rpc->fault(c, 500, "Internal error creating rpc ih");
                        return -1;
                }

		if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&table[i].subnet)) < 0)
		{
			rpc->fault(c, 500, "Internal error creating rpc data (subnet)");
			return -1;
		}
		if(rpc->struct_add(ih, "dds", "mask", table[i].mask,
					"port", table[i].port,
					"tag",  (table[i].tag.s==NULL)?"":table[i].tag.s) < 0)
		{
			rpc->fault(c, 500, "Internal error creating rpc data");
			return -1;
		}
	}
	return 0;
}


/* 
 * Empty contents of subnet table
 */
void empty_subnet_table(struct subnet *table)
{
	int i;
	table[PERM_MAX_SUBNETS].grp = 0;
	for(i=0; i<PERM_MAX_SUBNETS; i++)
	{
		if(table[i].tag.s!=NULL)
		{
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len =0;
		}
	}
}


/*
 * Release memory allocated for a subnet table
 */
void free_subnet_table(struct subnet* table)
{
	int i;
	if (!table)
		return;
	for(i=0; i<PERM_MAX_SUBNETS; i++)
	{
		if(table[i].tag.s!=NULL)
		{
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len =0;
		}
	}

	shm_free(table);
}

/*
 * Create and initialize a domain_name table
 */
struct domain_name_list** new_domain_name_table(void)
{
	struct domain_name_list** ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct domain_name_list **)shm_malloc
		(sizeof(struct domain_name_list*) * PERM_HASH_SIZE);
	if (!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct domain_name*) * PERM_HASH_SIZE);
	return ptr;
}


/* 
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_domain_name_table(struct domain_name_list **table)
{
	int i;
	struct domain_name_list *np, *next;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}

/*
 * Release all memory allocated for a hash table
 */
void free_domain_name_table(struct domain_name_list** table)
{
	if (!table)
		return;

	empty_domain_name_table(table);
	shm_free(table);
}


/* 
 * Check if an entry exists in hash table that has given group, domain_name, and
 * port.  Port 0 in hash table matches any port.
 */
int match_domain_name_table(struct domain_name_list** table, unsigned int group,
		str *domain_name, unsigned int port)
{
	struct domain_name_list *np;
	avp_value_t val;

	for (np = table[perm_hash(*domain_name)]; np != NULL; np = np->next) {
		if ( (np->grp == group)
				&& ((np->port == 0) || (np->port == port))
				&& np->domain.len == domain_name->len
				&& strncmp(np->domain.s, domain_name->s, domain_name->len)==0 ) {

			if (tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return 1;
		}
	}

	return -1;
}


/* 
 * Check if an domain_name/port entry exists in hash table in any group.
 * Returns first group in which ip_addr/port is found.
 * Port 0 in hash table matches any port. 
 */
int find_group_in_domain_name_table(struct domain_name_list** table,
		str *domain_name, unsigned int port)
{
	struct domain_name_list *np;

	for (np = table[perm_hash(*domain_name)]; np != NULL; np = np->next) {
		if ( ((np->port == 0) || (np->port == port))
				&& np->domain.len == domain_name->len
				&& strncmp(np->domain.s, domain_name->s, domain_name->len)==0 ) {
			return np->grp;
		}
	}

	return -1;
}


/* 
 * Add <grp, domain_name, port> into hash table
 */
int domain_name_table_insert(struct domain_name_list** table, unsigned int grp,
		str *domain_name, unsigned int port, char *tagv)
{
	struct domain_name_list *np;
	unsigned int hash_val;
	int len;

	len = sizeof(struct domain_name_list) + domain_name->len;
	if(tagv!=NULL)
		len += strlen(tagv) + 1;

	np = (struct domain_name_list *) shm_malloc(len);
	if (np == NULL) {
		LM_ERR("no shm memory for table entry\n");
		return -1;
	}

	memset(np, 0, len);

	np->grp = grp;
	np->domain.s = (char*)np + sizeof(struct domain_name_list);
	memcpy(np->domain.s, domain_name->s, domain_name->len);
	np->domain.len = domain_name->len;
	np->port = port;
	if(tagv!=NULL) {
		np->tag.s = (char*)np + sizeof(struct domain_name_list) + domain_name->len;
		np->tag.len = strlen(tagv);
		strcpy(np->tag.s, tagv);
	}

	LM_DBG("** Added domain name: %.*s\n", np->domain.len, np->domain.s);

	hash_val = perm_hash(*domain_name);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/*! \brief
 * RPC: Print addresses stored in hash table 
 */
int domain_name_table_rpc_print(struct domain_name_list** table, rpc_t* rpc, void* c)
{
	int i;
	void* th;
	void* ih;
	struct domain_name_list *np;


	if (rpc->add(c, "{", &th) < 0)
	{
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if(rpc->struct_add(th, "dd{", 
					"table", i,
					"group", np->grp,
					"item", &ih) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc ih");
				return -1;
			}

			if(rpc->struct_add(ih, "S", "domain_name", &np->domain) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "ds", "port",  np->port,
						"tag",  np->tag.len ? np->tag.s : "NULL") < 0) {
				rpc->fault(c, 500, "Internal error creating rpc data");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}

/*! \brief
 * MI: Print domain name stored in hash table 
 */
int domain_name_table_mi_print(struct domain_name_list** table, struct mi_node* rpl)
{
	int i;
	struct domain_name_list *np;

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if (addf_mi_node_child(rpl, 0, 0, 0,
						"%4d <%u, %.*s, %u> [%s]",
						i, np->grp, np->domain.len, np->domain.s,
						np->port, (np->tag.s==NULL)?"":np->tag.s) == 0)
				return -1;
			np = np->next;
		}
	}
	return 0;
}


