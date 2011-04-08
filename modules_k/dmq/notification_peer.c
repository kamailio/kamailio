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
	LM_ERR("dmq triggered from dmq_notification_callback\n");
	return 0;
}