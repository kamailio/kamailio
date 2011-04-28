#ifndef DMQ_FUNCS_H
#define DMQ_FUNCS_H

#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "../../config.h"
#include "dmq.h"
#include "peer.h"
#include "worker.h"

void ping_servers(unsigned int ticks,void *param);

typedef struct dmq_resp_cback {
	int (*f)(struct sip_msg* msg, int code, dmq_node_t* node, void* param);
	void* param;
} dmq_resp_cback_t;

typedef struct dmq_cback_param {
	dmq_resp_cback_t resp_cback;
	dmq_node_t* node;
} dmq_cback_param_t;

dmq_peer_t* register_dmq_peer(dmq_peer_t* peer);
int send_dmq_message(dmq_peer_t* peer, str* body, dmq_node_t* node, dmq_resp_cback_t* resp_cback, int max_forwards);
int bcast_dmq_message(dmq_peer_t* peer, str* body, dmq_node_t* except, dmq_resp_cback_t* resp_cback, int max_forwards);

#endif