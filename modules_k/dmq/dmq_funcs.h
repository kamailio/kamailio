#ifndef DMQ_FUNCS_H
#define DMQ_FUNCS_H

#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "dmq.h"
#include "peer.h"
#include "worker.h"
#include "../../str.h"

int register_dmq_peer(dmq_peer_t* peer);
int send_dmq_message(dmq_peer_t* peer, str* body, dmq_node_t* node);

#endif