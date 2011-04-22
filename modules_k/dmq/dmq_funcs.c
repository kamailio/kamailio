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

int build_from_str(str* username, struct sip_uri* uri, str* from) {
	/* sip:user@host:port */
	int from_len = username->len + uri->host.len + uri->port.len + 10;
	if(!uri->host.s || !uri->host.len) {
		LM_ERR("no host in uri\n");
		return -1;
	}
	if(!username->s || !username->len) {
		LM_ERR("no username given\n");
		return -1;
	}
	from->s = pkg_malloc(from_len);
	from->len = 0;
	
	memcpy(from->s, "sip:", 4);
	from->len += 4;
	
	memcpy(from->s + from->len, username->s, username->len);
	from->len += username->len;
	
	memcpy(from->s + from->len, "@", 1);
	from->len += 1;
	
	memcpy(from->s + from->len, uri->host.s, uri->host.len);
	from->len += uri->host.len;
	
	if(uri->port.s && uri->port.len) {
		memcpy(from->s + from->len, ":", 1);
		from->len += 1;
		memcpy(from->s + from->len, uri->port.s, uri->port.len);
		from->len += uri->port.len;
	}
	return 0;
}

int send_dmq_message(dmq_peer_t* peer, str* body, dmq_node_t* node) {
	uac_req_t uac_r;
	str str_hdr = {0, 0};
	/* TODO - do not hardcode these - just for tesing purposes */
	str from, to, req_uri;
	void *cb_param = NULL;
	int result;
	
	if(build_from_str(&peer->peer_id, &dmq_server_uri, &from) < 0) {
		LM_ERR("error building from string\n");
		return -1;
	}
	if(build_from_str(&peer->peer_id, &node->uri, &to) < 0) {
		LM_ERR("error building to string\n");
		return -1;
	}
	req_uri = to;
	
	set_uac_req(&uac_r, &dmq_request_method, &str_hdr, body, NULL, TMCB_LOCAL_COMPLETED,
			dmq_tm_callback, (void*)cb_param);
	result = tmb.t_request(&uac_r, &req_uri,
			       &to, &from,
			       NULL);
	if(result < 0)
	{
		LM_ERR("error in tmb.t_request_within\n");
		return -1;
	}
	return 0;
}