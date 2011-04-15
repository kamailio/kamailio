#include "notification_peer.h"

int add_notification_peer() {
	int ret;
	dmq_notification_peer.callback = dmq_notification_callback;
	dmq_notification_peer.description.s = "dmqpeer";
	dmq_notification_peer.description.len = 7;
	dmq_notification_peer.peer_id.s = "dmqpeer";
	dmq_notification_peer.peer_id.len = 7;
	ret = register_dmq_peer(&dmq_notification_peer);
	return ret;
}

int dmq_notification_callback(struct sip_msg* msg) {
	/* received dmqnode list */
	dmq_node_list_t* rlist;
	LM_ERR("dmq triggered from dmq_notification_callback\n");
	rlist = extract_node_list(msg);
	if(!rlist) {
		LM_ERR("extract_node_list failed\n");
		return -1;
	}
	if(update_node_list(rlist) < 0) {
		LM_ERR("cannot update node_list\n");
		return -1;
	}
	return 0;
}