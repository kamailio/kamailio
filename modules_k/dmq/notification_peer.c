#include "notification_peer.h"

static str notification_content_type = str_init("text/plain");
dmq_resp_cback_t notification_callback = {&notification_resp_callback_f, 0};

int add_notification_peer() {
	dmq_peer_t not_peer;
	not_peer.callback = dmq_notification_callback;
	not_peer.description.s = "notification_peer";
	not_peer.description.len = 17;
	not_peer.peer_id.s = "notification_peer";
	not_peer.peer_id.len = 17;
	dmq_notification_peer = register_dmq_peer(&not_peer);
	if(!dmq_notification_peer) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	}
	/* add itself to the node list */
	self_node = add_dmq_node(node_list, &dmq_server_address);
	if(!self_node) {
		LM_ERR("error adding self node\n");
		goto error;
	}
	/* local node - only for self */
	self_node->local = 1;
	return 0;
error:
	return -1;
}

dmq_node_t* add_server_and_notify(str* server_address) {
	/* add the notification server to the node list - if any */
	dmq_node_t* node = add_dmq_node(node_list, server_address);
	if(!node) {
		LM_ERR("error adding notification node\n");
		goto error;
	}
	/* request initial list from the notification server */
	if(request_nodelist(node, 2) < 0) {
		LM_ERR("error requesting initial nodelist\n");
		goto error;
	}
	return node;
error:
	return NULL;
}

/**
 * extract the node list from the body of a notification request SIP message
 * the SIP request will look something like:
 * 	KDMQ sip:10.0.0.0:5062
 * 	To: ...
 * 	From: ...
 * 	Max-Forwards: ...
 * 	Content-Length: 22
 * 	
 * 	sip:host1:port1;param1=value1
 * 	sip:host2:port2;param2=value2
 * 	...
 */
int extract_node_list(dmq_node_list_t* update_list, struct sip_msg* msg) {
	int content_length, total_nodes = 0;
	str body;
	str tmp_uri;
	dmq_node_t *cur = NULL;
	char *tmp, *end, *match;
	if(!msg->content_length) {
		LM_ERR("no content length header found\n");
		return -1;
	}
	content_length = get_content_length(msg);
	if(!content_length) {
		LM_DBG("content length is 0\n");
		return total_nodes;
	}
	body.s = get_body(msg);
	body.len = content_length;
	tmp = body.s;
	end = body.s + body.len;
	
	/* acquire big list lock */
	lock_get(&update_list->lock);
	while(tmp < end) {
		match = q_memchr(tmp, '\n', end - tmp);
		if(match) {
			match++;
		} else {
			/* for the last line - take all of it */
			match = end;
		}
		/* create the orig_uri from the parsed uri line and trim it */
		tmp_uri.s = tmp;
		tmp_uri.len = match - tmp - 1;
		tmp = match;
		/* trim the \r, \n and \0's */
		trim_r(tmp_uri);
		if(!find_dmq_node_uri(update_list, &tmp_uri)) {
			LM_DBG("found new node %.*s\n", STR_FMT(&tmp_uri));
			cur = build_dmq_node(&tmp_uri, 1);
			if(!cur) {
				LM_ERR("error creating new dmq node\n");
				goto error;
			}
			cur->next = update_list->nodes;
			update_list->nodes = cur;
			update_list->count++;
			total_nodes++;
		}
	}
	/* release big list lock */
	lock_release(&update_list->lock);
	return total_nodes;
error:
	lock_release(&update_list->lock);
	return -1;
}

int dmq_notification_callback(struct sip_msg* msg, peer_reponse_t* resp) {
	int nodes_recv;
	str* response_body = NULL;
	unsigned int maxforwards = 1;
	/* received dmqnode list */
	LM_DBG("dmq triggered from dmq_notification_callback\n");
	/* parse the message headers */
	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing message headers\n");
		goto error;
	}
	
	/* extract the maxforwards value, if any */
	if(msg->maxforwards) {
		LM_DBG("max forwards: %.*s\n", STR_FMT(&msg->maxforwards->body));
		str2int(&msg->maxforwards->body, &maxforwards);
	}
	maxforwards--;
	
	nodes_recv = extract_node_list(node_list, msg);
	LM_DBG("received %d new nodes\n", nodes_recv);
	response_body = build_notification_body();
	resp->content_type = notification_content_type;
	resp->reason = dmq_200_rpl;
	resp->body = *response_body;
	resp->resp_code = 200;
	
	/* if we received any new nodes tell about them to the others */
	if(nodes_recv > 0 && maxforwards > 0) {
		/* maxforwards is set to 0 so that the message is will not be in a spiral */
		bcast_dmq_message(dmq_notification_peer, response_body, 0, &notification_callback, maxforwards);
	}
	LM_DBG("broadcasted message\n");
	pkg_free(response_body);
	return 0;
error:
	return -1;
}

/**
 * builds the body of a notification message from the list of servers 
 * the result will look something like:
 * sip:host1:port1;param1=value1\r\n
 * sip:host2:port2;param2=value2\r\n
 * sip:host3:port3;param3=value3
 */
str* build_notification_body() {
	/* the length of the current line describing the server */
	int slen;
	/* the current length of the body */
	int clen = 0;
	dmq_node_t* cur_node = NULL;
	str* body;
	body = pkg_malloc(sizeof(str));
	memset(body, 0, sizeof(str));
	/* we allocate a chunk of data for the body */
	body->len = NBODY_LEN;
	body->s = pkg_malloc(body->len);
	/* we add each server to the body - each on a different line */
	lock_get(&node_list->lock);
	cur_node = node_list->nodes;
	while(cur_node) {
		LM_DBG("body_len = %d - clen = %d\n", body->len, clen);
		/* body->len - clen - 2 bytes left to write - including the \r\n */
		slen = build_node_str(cur_node, body->s + clen, body->len - clen - 2);
		if(slen < 0) {
			LM_ERR("cannot build_node_string\n");
			goto error;
		}
		clen += slen;
		body->s[clen++] = '\r';
		body->s[clen++] = '\n';
		cur_node = cur_node->next;
	}
	lock_release(&node_list->lock);
	body->len = clen;
	return body;
error:
	pkg_free(body->s);
	pkg_free(body);
	return NULL;
}

int request_nodelist(dmq_node_t* node, int forward) {
	str* body = build_notification_body();
	int ret;
	ret = dmq_send_message(dmq_notification_peer, body, node, &notification_callback, forward);
	pkg_free(body->s);
	pkg_free(body);
	return ret;
}

int notification_resp_callback_f(struct sip_msg* msg, int code, dmq_node_t* node, void* param) {
	int ret;
	LM_DBG("notification_callback_f triggered [%p %d %p]\n", msg, code, param);
	if(code == 408) {
		/* deleting node - the server did not respond */
		LM_ERR("deleting server %.*s because of failed request\n", STR_FMT(&node->orig_uri));
		ret = del_dmq_node(node_list, node);
		LM_DBG("del_dmq_node returned %d\n", ret);
	}
	return 0;
}

