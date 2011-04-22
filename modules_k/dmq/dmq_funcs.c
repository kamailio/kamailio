#include "dmq_funcs.h"

int register_dmq_peer(dmq_peer_t* peer) {
	lock_get(&peer_list->lock);
	if(search_peer_list(peer_list, peer)) {
		LM_ERR("peer already exists: %.*s %.*s\n", peer->peer_id.len, peer->peer_id.s,
		       peer->description.len, peer->description.s);
		return -1;
	}
	add_peer(peer_list, peer);
	lock_release(&peer_list->lock);
	return 0;
}

void dmq_tm_callback( struct cell *t, int type, struct tmcb_params *ps) {
	LM_ERR("callback\n");
}

int send_dmq_message(dmq_peer_t* peer, str* body) {
	uac_req_t uac_r;
	str str_hdr = {0, 0};
	/* TODO - do not hardcode these - just for tesing purposes */
	str from = {"sip:dmqnode@10.0.0.0:5060", 25};
	str req_uri = from;
	void *cb_param = NULL;
	int result;
	set_uac_req(&uac_r, &dmq_request_method, &str_hdr, body, NULL, TMCB_LOCAL_COMPLETED,
			dmq_tm_callback, (void*)cb_param);
	result = tmb.t_request(&uac_r, &req_uri,
			       &from, &from,
			       NULL);
	if(result < 0)
	{
		LM_ERR("error in tmb.t_request_within\n");
		return -1;
	}
	return 0;
}