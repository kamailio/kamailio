#include "dmq.h"
#include "bind_dmq.h"
#include "peer.h"
#include "dmq_funcs.h"

int bind_dmq(dmq_api_t* api) {
	api->register_dmq_peer = register_dmq_peer;
	api->send_message = dmq_send_message;
	api->bcast_message = bcast_dmq_message;
	return 0;
}

