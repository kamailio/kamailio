#include "bind_dmq.h"
#include "peer.h"

int bind_dmq(dmq_api_t* api) {
	api->register_dmq_peer = register_dmq_peer;
	return 0;
}