#ifndef DMQ_H
#define DMQ_H

#include "../../dprint.h"
#include "../../error.h"
#include "../../sr_module.h"
#include "bind_dmq.h"
#include "peer.h"
#include "worker.h"

#define DEFAULT_NUM_WORKERS	2

extern int num_workers;
extern dmq_worker_t* workers;

static inline int dmq_load_api(dmq_api_t* api) {
	bind_dmq_f binddmq;
	binddmq = (bind_dmq_f)find_export("bind_dmq", 0, 0);
	if ( binddmq == 0) {
		LM_ERR("cannot find bind_dmq\n");
		return -1;
	}
	if (binddmq(api) < 0)
	{
		LM_ERR("cannot bind dmq api\n");
		return -1;
	}
	return 0;
}

int handle_dmq_message(struct sip_msg* msg, char* str1 ,char* str2);

#endif