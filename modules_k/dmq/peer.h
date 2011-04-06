#ifndef PEER_H
#define PEER_H

#include <string.h>
#include <stdlib.h>
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"

typedef int(*peer_callback_t)(struct sip_msg*);

typedef struct dmq_peer {
	str peer_id;
	str description;
	peer_callback_t callback;
	struct dmq_peer* next;
} dmq_peer_t;

typedef struct dmq_peer_list {
	dmq_peer_t* peers;
	int count;
} dmq_peer_list_t;

extern dmq_peer_list_t* peer_list;

dmq_peer_list_t* init_peer_list();
dmq_peer_t* search_peer_list(dmq_peer_list_t* peer_list, dmq_peer_t* peer);
typedef int (*register_dmq_peer_t)(dmq_peer_t*);

int register_dmq_peer(dmq_peer_t* peer);
dmq_peer_t* find_peer(str peer_id);


#endif