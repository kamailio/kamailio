#ifndef BIND_DMQ_H
#define BIND_DMQ_H

#include "peer.h"
#include "dmqnode.h"
#include "dmq_funcs.h"

typedef int (*bcast_message_t)(dmq_peer_t* peer, str* body, dmq_node_t* except, dmq_resp_cback_t* resp_cback, int max_forwards);
typedef int (*send_message_t)(dmq_peer_t* peer, str* body, dmq_node_t* node, dmq_resp_cback_t* resp_cback, int max_forwards);

typedef struct dmq_api {
	register_dmq_peer_t register_dmq_peer;
	bcast_message_t bcast_message;
	send_message_t send_message;
} dmq_api_t;

typedef int (*bind_dmq_f)(dmq_api_t* api);

int bind_dmq(dmq_api_t* api);

#endif