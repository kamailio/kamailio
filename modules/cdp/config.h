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

#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "utils.h"
#include <libxml/parser.h>

/** Peer configuration. */
typedef struct{
	str fqdn;	/**< FQDN of the peer */
	str realm;	/**< Realm of the peer */
	int port;	/**< TCP port of the peer; the Diameter uri is then aaa://fqdn:port. */
    str src_addr; /**< IP address used to connect to the peer */
} peer_config;


/** Acceptor socket configuration. */
typedef struct{
	int port;	/**< TCP port number to listen on */ 
	str bind;	/**< IP address to bind to (if null, then :: (0.0.0.0) - all) */
} acceptor_config;

typedef enum {
	DP_AUTHORIZATION,	/**< Authorization application */
	DP_ACCOUNTING		/**< Accounting application */
} app_type;

/** Application configuration. */
typedef struct {
	int id;			/**< integer id of the appication */
	int vendor;		/**< vendor id of the application */
	app_type type;			/**< type of the application */
} app_config;

/** Routing Table Entry */
typedef struct _routing_entry {
	str fqdn;				/**< FQDN of the server 				*/
	int metric;				/**< The metric of the route			*/
	struct _routing_entry *next;
} routing_entry;

/** Routing Table realm */
typedef struct _routing_realm {
	str realm;				/**< the realm to identify				*/
	routing_entry *routes;	/**< ordered list of routes				*/
	struct _routing_realm *next; /**< the next realm in the table	*/
} routing_realm;

/** Routing Table configuration */
typedef struct {
	routing_realm *realms;	/**< list of realms				 	*/
	routing_entry *routes;	/**< ordered list of default routes 	*/
} routing_table;

/** Full Diameter Peer configuration. */
typedef struct {
	str fqdn;					/**< own FQDN */
	str realm;					/**< own Realm */
	str identity;				/**< own diameter URI */
	int vendor_id;				/**< own vendorid */
	str product_name;			/**< own product name */
	int accept_unknown_peers;	/**< if to accept connections from peers that are not configured initially */
	int drop_unknown_peers;		/**< if to drop the peers that are not initially configured on disconnected;
									 usually, you want to do this, unless you want your list of peers to
									 grow and you want to try and connect back to everybody that connected 
									 to you before */
	int tc;						/**< Tc timer duration (30 seconds should be) */
	int workers;				/**< Number of worker-processes to fork */
	int queue_length;			/**< Length of the message queue; when it is filled, the server part will
									 block until workers will finish work on at least one item in the queue */
	int connect_timeout;		/**< Connect timeout for outbound connections */
	int transaction_timeout;	/**< Transaction timeout duration */
	
	int sessions_hash_size;		/**< Size of the sessions hash table */									 
	int default_auth_session_timeout; /** The default Authorization Session Timeout to use if none other indicated */ 
	int max_auth_session_timeout;	  /** The max Authorization Session Timeout limit */ 
	
	peer_config *peers;			/**< list of peers */
	int peers_cnt;				/**< size of the list of peers */
	
	acceptor_config *acceptors;	/**< list of acceptors */
	int acceptors_cnt;			/**< size of the list of acceptors */
	
	app_config *applications;	/**< list of supported applications */
	int applications_cnt;		/**< size of list of supported applications*/

	int *supported_vendors;		/**< list of supported vendor ids */
	int supported_vendors_cnt;	/**< size of list of supported vendor ids */
	
	routing_table *r_table;		/**< realm routing table */
} dp_config;


dp_config *new_dp_config();
routing_realm *new_routing_realm();
routing_entry *new_routing_entry();
void free_dp_config(dp_config *x);
void free_routing_realm(routing_realm *rr);
void free_routing_entry(routing_entry *re);
inline void log_dp_config(dp_config *x);

xmlDocPtr parse_dp_config_file(char* filename);
xmlDocPtr parse_dp_config_str(str config_str);

dp_config* parse_dp_config(xmlDocPtr);

#endif /*__CONFIG_H_*/
