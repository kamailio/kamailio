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

#include <time.h>

#include "peer.h"
#include "diameter.h"

/**
 * Create a new peer.
 * - All the memory from parameters is duplicated.
 * @param fqdn - FQDN of the peer
 * @param realm - Realm of the peer
 * @param port - port of the peer to connect to
 * @returns the new peer* if ok, NULL on error
 */
peer* new_peer(str fqdn,str realm,int port,str src_addr)
{
	peer *x;
	x = shm_malloc(sizeof(peer));
	if (!x){
		LOG_NO_MEM("shm",sizeof(peer));
		goto error;
	}
	memset(x,0,sizeof(peer));
	shm_str_dup_macro(x->fqdn,fqdn);
	if (!x->fqdn.s) goto error;	
	shm_str_dup_macro(x->realm,realm);
	if (!x->realm.s) goto error;	
	shm_str_dup_macro(x->src_addr,src_addr);
	if (!x->src_addr.s) goto error;
	x->port = port;
	x->lock = lock_alloc();
	x->lock = lock_init(x->lock);
		
	x->state = Closed;

	x->I_sock = -1;
	x->R_sock = -1;

	x->activity = time(0)-500;	
	
	x->next = 0;
	x->prev = 0;
	
	return x;
error:
	return 0;
}

/**
 * Frees the memory taken by a peer structure.
 * @param x - the peer to free
 * @param locked - if the caller of this function already acquired the lock on this peer
 */
void free_peer(peer *x,int locked)
{
	if (!x) return;
	if (!locked) lock_get(x->lock);
	if (x->fqdn.s) shm_free(x->fqdn.s);
	if (x->realm.s) shm_free(x->realm.s);	
	if (x->src_addr.s) shm_free(x->src_addr.s);
	lock_destroy(x->lock);
	lock_dealloc((void*)x->lock);
	shm_free(x);
}

/**
 * "Touches" the peer by updating the last activity time to the current time.
 * @param p - which peer to touch
 */
inline void touch_peer(peer *p)
{
	p->activity = time(0);
}
