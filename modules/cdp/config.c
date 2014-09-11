/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "config.h"

/**
 * Create a new dp_config.
 */
inline dp_config *new_dp_config()
{
	dp_config *x;
	x = shm_malloc(sizeof(dp_config));
	if (!x) {
		LOG_NO_MEM("shm",sizeof(dp_config));
		goto error;
	}
	memset(x,0,sizeof(dp_config));
	return x;
error:
	LM_ERR("%s(): failed to create new dp_config.\n",__FUNCTION__);
	return 0;
}

/**
 * Create a new dp_config.
 */
inline routing_realm *new_routing_realm()
{
	routing_realm *x;
	x = shm_malloc(sizeof(routing_realm));
	if (!x) {
		LOG_NO_MEM("shm",sizeof(routing_realm));
		goto error;
	}
	memset(x,0,sizeof(routing_realm));
	return x;
error:
	LM_ERR("%s(): failed to create new routing_realm.\n",__FUNCTION__);
	return 0;
}

/**
 * Create a new dp_config.
 */
inline routing_entry *new_routing_entry()
{
	routing_entry *x;
	x = shm_malloc(sizeof(routing_entry));
	if (!x) {
		LOG_NO_MEM("shm",sizeof(routing_entry));
		goto error;
	}
	memset(x,0,sizeof(routing_entry));
	return x;
error:
	LM_ERR("%s(): failed to create new routing_entry.\n",__FUNCTION__);
	return 0;
}


/**
 * Free the space claimed by a routing entry
 */
inline void free_routing_entry(routing_entry *re)
{
	if (!re) return;
	if (re->fqdn.s) shm_free(re->fqdn.s);
	shm_free(re);
}

/**
 * Free the space claimed by a routing realm
 */
inline void free_routing_realm(routing_realm *rr)
{
	routing_entry *re,*ren;
	if (!rr) return;
	if (rr->realm.s) shm_free(rr->realm.s);
	for(re=rr->routes;re;re=ren){
		ren = re->next;
		free_routing_entry(re);
	}
	shm_free(rr);
}




/**
 * Frees the memory held by a dp_config.
 */
inline void free_dp_config(dp_config *x)
{
	int i;
	if (!x) return;
	if (x->fqdn.s) shm_free(x->fqdn.s);
	if (x->identity.s) shm_free(x->identity.s);
	if (x->realm.s) shm_free(x->realm.s);
	if (x->product_name.s) shm_free(x->product_name.s);
	if (x->peers) {
		for(i=0;i<x->peers_cnt;i++){
			if (x->peers[i].fqdn.s) shm_free(x->peers[i].fqdn.s);
			if (x->peers[i].realm.s) shm_free(x->peers[i].realm.s);
		}
		shm_free(x->peers);
	}
	if (x->acceptors) {
		for(i=0;i<x->acceptors_cnt;i++){
			if (x->acceptors[i].bind.s) shm_free(x->acceptors[i].bind.s);
		}
		shm_free(x->acceptors);
	}
	if (x->applications) shm_free(x->applications);

	if (x->supported_vendors) shm_free(x->supported_vendors);

	if (x->r_table) {
		routing_realm *rr,*rrn;
		routing_entry *re,*ren;
		for(rr=x->r_table->realms;rr;rr=rrn){
			rrn = rr->next;
			free_routing_realm(rr);
		}
		for(re=x->r_table->routes;re;re=ren){
			ren = re->next;
			free_routing_entry(re);
		}
		shm_free(x->r_table);
	}
	shm_free(x);
}

/**
 * Log the dp_config to output, for debug purposes.
 */
inline void log_dp_config(dp_config *x)
{
	int i;
	LM_DBG("Diameter Peer Config:\n");
	LM_DBG("\tFQDN    : %.*s\n",x->fqdn.len,x->fqdn.s);
	LM_DBG("\tRealm   : %.*s\n",x->realm.len,x->realm.s);
	LM_DBG("\tVendorID: %d\n",x->vendor_id);
	LM_DBG("\tProdName: %.*s\n",x->product_name.len,x->product_name.s);
	LM_DBG("\tAcceptUn: [%c]\n",x->accept_unknown_peers?'X':' ');
	LM_DBG("\tDropUnkn: [%c]\n",x->drop_unknown_peers?'X':' ');
	LM_DBG("\tTc      : %d\n",x->tc);
	LM_DBG("\tWorkers : %d\n",x->workers);
	LM_DBG("\tQueueLen: %d\n",x->queue_length);
	LM_DBG("\tConnTime: %d\n",x->connect_timeout);
	LM_DBG("\tTranTime: %d\n",x->transaction_timeout);
	LM_DBG("\tSessHash: %d\n",x->sessions_hash_size);
	LM_DBG("\tDefAuthT: %d\n",x->default_auth_session_timeout);
	LM_DBG("\tMaxAuthT: %d\n",x->max_auth_session_timeout);
	LM_DBG("\tPeers   : %d\n",x->peers_cnt);
	for(i=0;i<x->peers_cnt;i++)
		LM_DBG("\t\tFQDN:  %.*s \t Realm: %.*s \t Port: %d\n",
			x->peers[i].fqdn.len,x->peers[i].fqdn.s,
			x->peers[i].realm.len,x->peers[i].realm.s,
			x->peers[i].port);
	LM_DBG("\tAcceptors : %d\n",x->acceptors_cnt);
	for(i=0;i<x->acceptors_cnt;i++)
		LM_DBG("\t\tPort:  %d \t Bind: %.*s \n",
			x->acceptors[i].port,
			x->acceptors[i].bind.len,x->acceptors[i].bind.s);
	LM_DBG("\tApplications : %d\n",x->applications_cnt);
	for(i=0;i<x->applications_cnt;i++)
		LM_DBG("\t\t%s ID:  %d \t Vendor: %d \n",
			(x->applications[i].type==DP_AUTHORIZATION)?"Auth":"Acct",
			x->applications[i].id,
			x->applications[i].vendor);
	LM_DBG("\tSupported Vendors : %d\n",x->supported_vendors_cnt);
	for(i=0;i<x->supported_vendors_cnt;i++)
		LM_DBG("\t\t Vendor: %d \n",
			x->supported_vendors[i]);
	if (x->r_table){
		routing_realm *rr;
		routing_entry *re;
		LM_DBG("\tRouting Table : \n");
		for(rr=x->r_table->realms;rr;rr=rr->next){
			LM_DBG("\t\tRealm: %.*s\n",
				rr->realm.len,rr->realm.s);
			for(re=rr->routes;re;re=re->next)
				LM_DBG("\t\t\tRoute: [%4d] %.*s\n",
					re->metric,re->fqdn.len,re->fqdn.s);
		}
		for(re=x->r_table->routes;re;re=re->next)
			LM_DBG("\t\tDefaultRoute: [%4d] %.*s\n",
				re->metric,re->fqdn.len,re->fqdn.s);
	}

}
