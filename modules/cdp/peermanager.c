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

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "diameter.h"

#include "timer.h"
#include "peermanager.h"

#include "globals.h"
#include "peerstatemachine.h"

peer_list_t *peer_list=0;		/**< list of peers */
gen_lock_t *peer_list_lock=0;	/**< lock for the list of peers */

extern dp_config *config;		/**< Configuration for this diameter peer 	*/
extern char *dp_states[];
AAAMsgIdentifier *hopbyhop_id=0;/**< Current id for Hop-by-hop */
AAAMsgIdentifier *endtoend_id=0;/**< Current id for End-to-end */
gen_lock_t *msg_id_lock;		/**< lock for the message identifier changes */

/**
 * Initializes the Peer Manager.
 * The initial list of peers is taken from the configuration provided
 * @param config - configuration for initial peers
 * @returns 1
 */
int peer_manager_init(dp_config *config)
{
	int i;
	peer *p;
	LM_DBG("peer_manager_init(): Peer Manager initialization...\n");
	peer_list = shm_malloc(sizeof(peer_list_t));
	peer_list->head = 0;
	peer_list->tail = 0;
	peer_list_lock = lock_alloc();
	peer_list_lock = lock_init(peer_list_lock);

	hopbyhop_id = shm_malloc(sizeof(AAAMsgIdentifier));
	endtoend_id = shm_malloc(sizeof(AAAMsgIdentifier));
	msg_id_lock = lock_alloc();
	msg_id_lock = lock_init(msg_id_lock);

	srand((unsigned int)time(0));
	*hopbyhop_id = rand();
	*endtoend_id = (time(0)&0xFFF)<<20;
	*endtoend_id |= rand() & 0xFFFFF;

	for(i=0;i<config->peers_cnt;i++){
		p = new_peer(config->peers[i].fqdn,config->peers[i].realm,config->peers[i].port,config->peers[i].src_addr);
		if (!p) continue;
		p->is_dynamic = 0;
		add_peer(p);
	}

	add_timer(1,0,&peer_timer,0);

	return 1;
}

/**
 * Destroys the Peer Manager and disconnects all peer sockets.
 */
void peer_manager_destroy()
{
	peer *foo,*bar;
	lock_get(peer_list_lock);
	foo = peer_list->head;
	while(foo){
		if (foo->I_sock>0) close(foo->I_sock);
		if (foo->R_sock>0) close(foo->R_sock);
		bar = foo->next;
		free_peer(foo,1);
		foo = bar;
	}

/*	lock_get(msg_id_lock);	*/
	shm_free(hopbyhop_id);
	shm_free(endtoend_id);
	lock_destroy(msg_id_lock);
	lock_dealloc((void*)msg_id_lock);

	shm_free(peer_list);
	lock_destroy(peer_list_lock);
	lock_dealloc((void*)peer_list_lock);
	LM_DBG("peer_manager_init(): ...Peer Manager destroyed\n");
}

/**
 * Logs the list of peers
 * @param level - log level to print to
 */
void log_peer_list()
{
	/* must have lock on peer_list_lock when calling this!!! */
	peer *p;
	int i;

    LM_DBG("--- Peer List: ---\n");
	for(p = peer_list->head;p;p = p->next){
		LM_DBG(ANSI_GREEN" S["ANSI_YELLOW"%s"ANSI_GREEN"] "ANSI_BLUE"%.*s:%d"ANSI_GREEN" D["ANSI_RED"%c"ANSI_GREEN"]\n",dp_states[p->state],p->fqdn.len,p->fqdn.s,p->port,p->is_dynamic?'X':' ');
		for(i=0;i<p->applications_cnt;i++)
			LM_DBG(ANSI_YELLOW"\t [%d,%d]"ANSI_GREEN"\n",p->applications[i].id,p->applications[i].vendor);
	}
	LM_DBG("------------------\n");
}

/**
 * Adds a peer to the peer list
 * @param p - peer to add
 */
void add_peer(peer *p)
{
	if (!p) return;
	lock_get(peer_list_lock);
	p->next = 0;
	p->prev = peer_list->tail;
	if (!peer_list->head) peer_list->head = p;
	if (peer_list->tail) peer_list->tail->next = p;
	peer_list->tail = p;
	lock_release(peer_list_lock);
}

/**
 * Removes a peer from the peer list
 * @param p - the peer to remove
 */
void remove_peer(peer *p)
{
	peer *i;
	if (!p) return;
	i = peer_list->head;
	while(i&&i!=p) i = i->next;
	if (i){
		if (i->prev) i->prev->next = i->next;
		else peer_list->head = i->next;
		if (i->next) i->next->prev = i->prev;
		else peer_list->tail = i->prev;
	}
}

/**
 * Finds a peer based on the TCP socket.
 * @param sock - socket to look for
 * @returns the peer* or NULL if not found
 */
peer *get_peer_from_sock(int sock)
{
	peer *i;
	lock_get(peer_list_lock);

	i = peer_list->head;
	while(i&&i->I_sock!=sock&&i->R_sock!=sock) i = i->next;
	lock_release(peer_list_lock);
	return i;
}

/**
 * Finds a peer based on the FQDN and Realm.
 * @param fqdn - the FQDN to look for
 * @param realm - the Realm to look for
 * @returns the peer* or NULL if not found
 */
peer *get_peer_from_fqdn(str fqdn,str realm)
{
	peer *i;
	str dumb;

	lock_get(peer_list_lock);
	i = peer_list->head;
	while(i){
		if (fqdn.len == i->fqdn.len && strncasecmp(fqdn.s,i->fqdn.s,fqdn.len)==0)
			break;
		i = i->next;
	}
	lock_release(peer_list_lock);
	if (!i&&config->accept_unknown_peers){
		i = new_peer(fqdn,realm,3868,dumb);
		if (i){
			i->is_dynamic=1;
			touch_peer(i);
			add_peer(i);
		}
	}
	return i;
}

/**
 * Finds a peer based on the FQDN.
 * @param fqdn - the FQDN to look for
 * @returns the peer* or NULL if not found
 */
peer *get_peer_by_fqdn(str *fqdn)
{
	peer *i;
	lock_get(peer_list_lock);
	i = peer_list->head;
	while(i){
		if (fqdn->len == i->fqdn.len && strncasecmp(fqdn->s,i->fqdn.s,fqdn->len)==0)
			break;
		i = i->next;
	}
	lock_release(peer_list_lock);
	return i;
}

/**
 * Timer function for peer management.
 * This is registered as a timer by peer_manager_init() and gets called every
 * #PEER_MANAGER_TIMER seconds. Then it looks on what changed and triggers events.
 * @param now - time of call
 * @param ptr - generic pointer for timers - not used
 */
int peer_timer(time_t now,void *ptr)
{
	peer *p,*n;
	int i;
	LM_DBG("peer_timer(): taking care of peers...\n");
	lock_get(peer_list_lock);
	p = peer_list->head;
	while(p){
		lock_get(p->lock);
		n = p->next;

		if (p->disabled && (p->state != Closed || p->state != Closing)) {
			LM_DBG("Peer [%.*s] has been disabled - shutting down\n", p->fqdn.len, p->fqdn.s);
			if (p->state == I_Open) sm_process(p, Stop, 0, 1, p->I_sock);
			if (p->state == R_Open) sm_process(p, Stop, 0, 1, p->R_sock);
			lock_release(p->lock);
			p = n;
			continue;
		}

		if (p->activity+config->tc<=now){
			LM_INFO("peer_timer(): Peer %.*s \tState %d \n",p->fqdn.len,p->fqdn.s,p->state);
			switch (p->state){
				/* initiating connection */
				case Closed:
					if (p->is_dynamic && config->drop_unknown_peers){
						remove_peer(p);
						free_peer(p,1);
						break;
					}
					if (!p->disabled) {
						touch_peer(p);
						sm_process(p,Start,0,1,0);
					}
					break;
				/* timeouts */
				case Wait_Conn_Ack:
				case Wait_I_CEA:
				case Closing:
				case Wait_Returns:
				case Wait_Conn_Ack_Elect:
					touch_peer(p);
					sm_process(p,Timeout,0,1,0);
					break;
				/* inactivity detected */
				case I_Open:
				case R_Open:
					if (p->waitingDWA){
						p->waitingDWA = 0;
						if (p->state==I_Open) sm_process(p,I_Peer_Disc,0,1,p->I_sock);
						if (p->state==R_Open) sm_process(p,R_Peer_Disc,0,1,p->R_sock);
						LM_WARN("Inactivity on peer [%.*s] and no DWA, Closing peer...\n", p->fqdn.len, p->fqdn.s);
					} else {
						p->waitingDWA = 1;
						Snd_DWR(p);
						touch_peer(p);
						LM_WARN("Inactivity on peer [%.*s], sending DWR... - if we don't get a reply, the peer will be closed\n", p->fqdn.len, p->fqdn.s);
					}
					break;
				/* ignored states */
				/* unknown states */
				default:
					LM_ERR("peer_timer(): Peer %.*s inactive  in state %d\n",
						p->fqdn.len,p->fqdn.s,p->state);
			}
		}
		lock_release(p->lock);
		p = n;
	}
	lock_release(peer_list_lock);
	log_peer_list();
	i = config->tc/5;
	if (i<=0) i=1;
	return i;
}

/**
 * Generates the next Hop-by-hop identifier.
 * @returns the new identifier to be used in messages
 */
inline AAAMsgIdentifier next_hopbyhop()
{
	AAAMsgIdentifier x;
	lock_get(msg_id_lock);
	*hopbyhop_id = (*hopbyhop_id)+1;
	x = *hopbyhop_id;
	lock_release(msg_id_lock);
	return x;
}

/**
 * Generates the next End-to-end identifier.
 * @returns the new identifier to be used in messages
 */
inline AAAMsgIdentifier next_endtoend()
{
	AAAMsgIdentifier x;
	lock_get(msg_id_lock);
	*endtoend_id = (*endtoend_id)+1;
	x = *endtoend_id;
	lock_release(msg_id_lock);
	return x;
}
