/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
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

#include "peer.h"
#include "dmq.h"

/**
 * @brief init peer list
 */
dmq_peer_list_t* init_peer_list()
{
	dmq_peer_list_t* peer_list;
	peer_list = shm_malloc(sizeof(dmq_peer_list_t));
	if(peer_list==NULL) {
		LM_ERR("no more shm\n");
		return NULL;
	}
	memset(peer_list, 0, sizeof(dmq_peer_list_t));
	lock_init(&peer_list->lock);
	return peer_list;
}

/**
 * @brief search peer list
 */
dmq_peer_t* search_peer_list(dmq_peer_list_t* peer_list, dmq_peer_t* peer)
{
	dmq_peer_t* crt;

	crt = peer_list->peers;
	while(crt) {
		if (STR_EQ(crt->peer_id, peer->peer_id)) {
			return crt;
		}
		crt = crt->next;
	}
	return 0;
}

/**
 * @brief add peer
 */
dmq_peer_t* add_peer(dmq_peer_list_t* peer_list, dmq_peer_t* peer)
{
	dmq_peer_t* new_peer = NULL;

	new_peer = shm_malloc(sizeof(dmq_peer_t) + peer->peer_id.len + peer->description.len);
	if(new_peer==NULL) {
		LM_ERR("no more shm\n");
		return NULL;
	}
	*new_peer = *peer;
	
	/* copy the str's */
	new_peer->peer_id.s = (char*)new_peer + sizeof(dmq_peer_t);
	memcpy(new_peer->peer_id.s, peer->peer_id.s, peer->peer_id.len);

	new_peer->description.s = new_peer->peer_id.s + new_peer->peer_id.len;
	memcpy(new_peer->description.s, peer->description.s, peer->description.len);
	new_peer->next = peer_list->peers;
	peer_list->peers = new_peer;
	return new_peer;
}

/**
 * @brief find peer by id
 */
dmq_peer_t* find_peer(str peer_id)
{
	dmq_peer_t foo_peer;
	foo_peer.peer_id = peer_id;
	return search_peer_list(peer_list, &foo_peer);
}

/**
 * @empty callback
 */
int empty_peer_callback(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* dmq_node)
{
	return 0;
}

