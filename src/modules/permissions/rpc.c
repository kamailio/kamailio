/*
 * Permissions RPC functions
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


#include "../../core/dprint.h"
#include "address.h"
#include "trusted.h"
#include "hash.h"
#include "rpc.h"
#include "permissions.h"


/*! \brief
 * RPC function to reload trusted table
 */
void rpc_trusted_reload(rpc_t* rpc, void* c) {
	if (reload_trusted_table_cmd () != 1) {
		rpc->fault(c, 500, "Reload failed.");
		return;
	}

	rpc->rpl_printf(c, "Reload OK");
	return;
}


/*! \brief
 * RPC function to dump trusted table
 */
void rpc_trusted_dump(rpc_t* rpc, void* c) {

	if (hash_table==NULL) {
		rpc->fault(c, 500, "No trusted table");
		return;
	}

	if(hash_table_rpc_print(*hash_table, rpc, c) < 0) {
		LM_DBG("failed to print a hash_table dump\n");
		return;
	}

	return;
}


/*! \brief
 * RPC function to reload address table
 */
void rpc_address_reload(rpc_t* rpc, void* c) {
	if (reload_address_table_cmd () != 1) {
		rpc->fault(c, 500, "Reload failed.");
		return;
	}

	rpc->rpl_printf(c, "Reload OK");
	return;
}


/*! \brief
 * RPC function to dump address table
 */
void rpc_address_dump(rpc_t* rpc, void* c) {

	if(addr_hash_table==NULL) {
		rpc->fault(c, 500, "No address table");
		return;
	}
	if(addr_hash_table_rpc_print(*addr_hash_table, rpc, c) < 0 ) {
		LM_DBG("failed to print address table dump\n");
	}
	return;
}


/*! \brief
 * RPC function to dump subnet table
 */
void rpc_subnet_dump(rpc_t* rpc, void* c) {
	if(subnet_table==NULL) {
		rpc->fault(c, 500, "No subnet table");
		return;
	}
	if(subnet_table_rpc_print(*subnet_table, rpc, c) < 0) {
		LM_DBG("failed to print subnet table dump\n");
	}

	return;
}


/*! \brief
 * RPC function to dump domain name table
 */
void rpc_domain_name_dump(rpc_t* rpc, void* c) {

	if(domain_list_table==NULL) {
		rpc->fault(c, 500, "No domain list table");
		return;
	}
	if ( domain_name_table_rpc_print(*domain_list_table, rpc, c) < 0 ) {
		LM_DBG("failed to print domain table dump\n");
	}
	return;
}


#define MAX_FILE_LEN 128

/*! \brief
 * RPC function to make allow_uri query.
 */
void rpc_test_uri(rpc_t* rpc, void* c)
{
	str basenamep, urip, contactp;
	char basename[MAX_FILE_LEN + 1];
	char uri[MAX_URI_SIZE + 1], contact[MAX_URI_SIZE + 1];
	unsigned int allow_suffix_len;

	if (rpc->scan(c, "S", &basenamep) != 1) {
		rpc->fault(c, 500, "Not enough parameters (basename, URI and contact)");
		return;
	}
	if (rpc->scan(c, "S", &urip) != 1) {
		rpc->fault(c, 500, "Not enough parameters (basename, URI and contact)");
		return;
	}
	if (rpc->scan(c, "S", &contactp) != 1) {
		rpc->fault(c, 500, "Not enough parameters (basename, URI and contact)");
		return;
	}

	/* For some reason, rtp->scan doesn't set the length properly */
	if (contactp.len > MAX_URI_SIZE) {
		rpc->fault(c, 500, "Contact is too long");
		return;
	}
	allow_suffix_len = strlen(allow_suffix);
	if (basenamep.len + allow_suffix_len + 1 > MAX_FILE_LEN) {
		rpc->fault(c, 500, "Basename is too long");
		return;
	}

	memcpy(basename, basenamep.s, basenamep.len);
	memcpy(basename + basenamep.len, allow_suffix, allow_suffix_len);
	basename[basenamep.len + allow_suffix_len] = 0;
	memcpy(uri, urip.s, urip.len);
	memcpy(contact, contactp.s, contactp.len);
	contact[contactp.len] = 0;
	uri[urip.len] = 0;

	if (allow_test(basename, uri, contact) == 1) {
		rpc->rpl_printf(c, "Allowed");
		return;
	}
	rpc->rpl_printf(c, "Denied");
	return;
}
