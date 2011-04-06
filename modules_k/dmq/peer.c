#include "peer.h"

dmq_peer_list_t* init_peer_list() {
	dmq_peer_list_t* peer_list = shm_malloc(sizeof(dmq_peer_list_t));
	memset(peer_list, 0, sizeof(dmq_peer_list_t));
	return peer_list;
}

dmq_peer_t* search_peer_list(dmq_peer_list_t* peer_list, dmq_peer_t* peer) {
	dmq_peer_t* cur = peer_list->peers;
	int len;
	while(cur) {
		/* len - the minimum length of the two strings */
		len = cur->peer_id.len < peer->peer_id.len ? cur->peer_id.len:peer->peer_id.len;
		if(strncasecmp(cur->peer_id.s, peer->peer_id.s, len) == 0) {
			return cur;
		}
	}
	return 0;
}

void add_peer(dmq_peer_list_t* peer_list, dmq_peer_t* peer) {
	dmq_peer_t* new_peer = shm_malloc(sizeof(dmq_peer_t));
	*new_peer = *peer;
	new_peer->next = peer_list->peers;
	peer_list->peers = new_peer;
}

int register_dmq_peer(dmq_peer_t* peer) {
	if(search_peer_list(peer_list, peer)) {
		LM_ERR("peer already exists: %.*s %.*s\n", peer->peer_id.len, peer->peer_id.s,
		       peer->description.len, peer->description.s);
		return -1;
	}
	add_peer(peer_list, peer);
	return 0;
}

dmq_peer_t* find_peer(str peer_id) {
	dmq_peer_t foo_peer;
	foo_peer.peer_id = peer_id;
	return search_peer_list(peer_list, &foo_peer);
}