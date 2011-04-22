#include "notification_peer.h"
#include "dmq_funcs.h"

int add_notification_peer() {
	dmq_node_t self_node;
	self_node.orig_uri = dmq_server_address;
	
	dmq_notification_peer.callback = dmq_notification_callback;
	dmq_notification_peer.description.s = "dmqpeer";
	dmq_notification_peer.description.len = 7;
	dmq_notification_peer.peer_id.s = "dmqpeer";
	dmq_notification_peer.peer_id.len = 7;
	if(register_dmq_peer(&dmq_notification_peer) < 0) {
		LM_ERR("error in register_dmq_peer\n");
		goto error;
	}
	/* add itself to the node list */
	if(add_dmq_node(node_list, &self_node) < 0) {
		LM_ERR("error adding self node\n");
		goto error;
	}
	if(request_initial_nodelist() < 0) {
		LM_ERR("error in request_initial_notification\n");
		goto error;
	}
	return 0;
error:
	return -1;
}

int extract_node_list(dmq_node_list_t* update_list, struct sip_msg* msg) {
	int content_length, total_nodes = 0;
	str body;
	str tmp_uri;
	dmq_node_t *cur = NULL, *prev = NULL, *first = NULL;
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
	
	while(tmp < end) {
		total_nodes++;
		cur = shm_malloc(sizeof(dmq_node_t));
		memset(cur, 0, sizeof(*cur));
		/* keep the list tail in first */
		if(!first) {
			first = cur;
		}
		cur->next = prev;
		
		match = q_memchr(tmp, '\n', end - tmp);
		if (match){
			match++;
		}else {
			/* for the last line - take all of it */
			match = end;
		}
		/* create the orig_uri from the parsed uri line and trim it */
		tmp_uri.s = tmp;
		tmp_uri.len = match - tmp;
		shm_str_dup(&cur->orig_uri, &tmp_uri);
		trim_r(cur->orig_uri);
		
		tmp = match;
		
		prev = cur;
	}
	lock_get(&update_list->lock);
	first->next = update_list->nodes;
	update_list->nodes = cur;
	lock_release(&update_list->lock);
	return total_nodes;
}

int dmq_notification_callback(struct sip_msg* msg, peer_reponse_t* resp) {
	int nodes_recv;
	/* received dmqnode list */
	LM_ERR("dmq triggered from dmq_notification_callback\n");
	nodes_recv = extract_node_list(node_list, msg);
	LM_DBG("received %d nodes\n", nodes_recv);
	return 0;
}

int build_node_str(dmq_node_t* node, char* buf, int buflen) {
	if(buflen < node->orig_uri.len) {
		LM_ERR("no more space left for node string\n");
		return -1;
	}
	memcpy(buf, node->orig_uri.s, node->orig_uri.len);
	return node->orig_uri.len;
}

/* builds the body of a notification message from the list of servers 
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

int request_initial_nodelist() {
	str* body = build_notification_body();
	int ret;
	ret = send_dmq_message(&dmq_notification_peer, body);
	pkg_free(body->s);
	pkg_free(body);
	return ret;
}