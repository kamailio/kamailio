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

#ifndef __PEER_H
#define __PEER_H


#include "utils.h"
#include "config.h"
#include "diameter.h"
#include <sys/types.h>


/** Peer states definition */
typedef enum {
	Closed 				= 0,	/**< Not connected */
	Wait_Conn_Ack		= 1,	/**< Connecting - waiting for Ack */
	Wait_I_CEA 			= 2,	/**< Connecting - waiting for Capabilities Exchange Answer */
	Wait_Conn_Ack_Elect	= 3,	/**< Connecting - Acknolegded and going for Election */
	Wait_Returns  		= 4,	/**< Connecting - done */
	R_Open 				= 5,	/**< Connected as receiver */
	I_Open 				= 6,	/**< Connected as initiator */
	Closing 			= 7		/**< Closing the connection */
} peer_state_t;


/** Peer events definition */
typedef enum {
	Start			= 101,	/**< Start connection attempt */
	Stop			= 102,	/**< Stop */
	Timeout			= 103,	/**< Time-out */
	Win_Election	= 104,	/**< Winning the election */
	R_Conn_CER		= 105,	/**< Receiver - Received connection Capabilities Exchange Request */
	I_Rcv_Conn_Ack 	= 106,	/**< Initiator - Received connection Ack */
	I_Rcv_Conn_NAck	= 107,	/**< Initiator - Received connection NAck */
	I_Rcv_CER		= 108,	/**< Initiator - Receiver Capabilities Exchange Request */
	I_Rcv_CEA		= 109,	/**< Initiator - Receiver Capabilities Exchange Answer */
	R_Rcv_CER		= 110,	/**< Receiver - Receiver Capabilities Exchange Request */
	R_Rcv_CEA		= 111,	/**< Receiver - Receiver Capabilities Exchange Answer */
	I_Rcv_Non_CEA	= 112,	/**< Initiator - Received non-Capabilities Exchange Answer */
	I_Rcv_DPR		= 113,	/**< Initiator - Received Disconnect Peer Request */
	I_Rcv_DPA		= 114,	/**< Initiator - Received Disconnect Peer Answer */
	R_Rcv_DPR		= 115,	/**< Receiver - Received Disconnect Peer Request */
	R_Rcv_DPA		= 116,	/**< Receiver - Received Disconnect Peer Answer */
	I_Rcv_DWR		= 117,	/**< Initiator - Received Diameter Watch-dog Request */
	I_Rcv_DWA		= 118,	/**< Initiator - Received Diameter Watch-dog Answer */
	R_Rcv_DWR		= 119,	/**< Receiver - Received Diameter Watch-dog Request */
	R_Rcv_DWA		= 120,	/**< Receiver - Received Diameter Watch-dog Answer */
	Send_Message	= 121,	/**< Send a message */
	I_Rcv_Message	= 122,	/**< Initiator - Received a message */
	R_Rcv_Message	= 123,	/**< Receiver - Received a message */
	I_Peer_Disc		= 124,	/**< Initiator - Peer disconnected */
	R_Peer_Disc		= 125	/**< Receiver - Peer disconnected */
} peer_event_t;

/** Peer data structure */
typedef struct _peer_t{
	str fqdn;				/**< FQDN of the peer */
	str realm;				/**< Realm of the peer */
	int port;				/**< TCP Port of the peer */
	str src_addr;			/**< IP Address used to connect to the peer */

	app_config *applications;/**< list of supported applications */
	int applications_cnt;	/**< size of list of supporter applications*/
	
	gen_lock_t *lock;		/**< lock for operations with this peer */
	
	peer_state_t state;		/**< state of the peer */
	int I_sock;				/**< socket used as initiator */
	int R_sock;				/**< socket used as receiver */
	
	time_t activity;		/**< timestamp of last activity */
	time_t last_selected;	/**< timestamp this peer was last selected for routing - used in least recently used load balancing across metric */
	int is_dynamic;			/**< whether this peer was accepted although it was not initially configured */
	int disabled;			/**< administratively enable/disable a peer - ie remove/re-add from service dynamically */
	int waitingDWA;			/**< if a Diameter Watch-dog Request was sent out and waiting for an answer */
	
	str send_pipe_name;		/**< pipe to signal messages to be sent out*/
	
	int fd_exchange_pipe_local;	/**< pipe to communicate with the receiver process and exchange a file descriptor - local end, to read from */
	int fd_exchange_pipe;	/**< pipe to communicate with the receiver process and exchange a file descriptor */

	AAAMessage *r_cer;      /**< the cer received from R-connection */
	
	struct _peer_t *next;	/**< next peer in the peer list */
	struct _peer_t *prev;	/**< previous peer in the peer list */
} peer;

peer* new_peer(str fqdn,str realm,int port,str src_addr);
void free_peer(peer *x,int locked);

inline void touch_peer(peer *p);

#endif
