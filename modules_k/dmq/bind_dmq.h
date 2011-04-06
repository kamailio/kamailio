#ifndef BIND_DMQ_H
#define BIND_DMQ_H

#include "peer.h"

typedef struct dmq_api {
	register_dmq_peer_t register_dmq_peer;
} dmq_api_t;

typedef int (*bind_dmq_f)(dmq_api_t* api);

int bind_dmq(dmq_api_t* api);

#endif