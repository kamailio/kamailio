/**
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../shm_init.h"
#include "../../sctp_core.h"

#include "sctp_options.h"
#include "sctp_server.h"
#include "sctp_rpc.h"

MODULE_VERSION

static int mod_init(void);
#ifdef USE_SCTP
static int sctp_mod_pre_init(void);
#endif


static cmd_export_t cmds[]={
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"sctp_socket_rcvbuf",     PARAM_INT, &sctp_default_cfg.so_rcvbuf},
	{"sctp_socket_sndbuf",     PARAM_INT, &sctp_default_cfg.so_sndbuf},
	{"sctp_autoclose",         PARAM_INT, &sctp_default_cfg.autoclose},
	{"sctp_send_ttl",          PARAM_INT, &sctp_default_cfg.send_ttl},
	{"sctp_send_retries",      PARAM_INT, &sctp_default_cfg.send_retries},
	{"sctp_assoc_tracking",    PARAM_INT, &sctp_default_cfg.assoc_tracking},
	{"sctp_assoc_reuse",       PARAM_INT, &sctp_default_cfg.assoc_reuse},
	{"sctp_max_assocs",        PARAM_INT, &sctp_default_cfg.max_assocs},
	{"sctp_srto_initial",      PARAM_INT, &sctp_default_cfg.srto_initial},
	{"sctp_srto_max",          PARAM_INT, &sctp_default_cfg.srto_max},
	{"sctp_srto_min",          PARAM_INT, &sctp_default_cfg.srto_min},
	{"sctp_asocmaxrxt",        PARAM_INT, &sctp_default_cfg.asocmaxrxt},
	{"sctp_init_max_attempts", PARAM_INT, &sctp_default_cfg.init_max_attempts},
	{"sctp_init_max_timeo",    PARAM_INT, &sctp_default_cfg.init_max_timeo},
	{"sctp_hbinterval",        PARAM_INT, &sctp_default_cfg.hbinterval},
	{"sctp_pathmaxrxt",        PARAM_INT, &sctp_default_cfg.pathmaxrxt},
	{"sctp_sack_delay",        PARAM_INT, &sctp_default_cfg.sack_delay},
	{"sctp_sack_freq",         PARAM_INT, &sctp_default_cfg.sack_freq},
	{"sctp_max_burst",         PARAM_INT, &sctp_default_cfg.max_burst},

	{0, 0, 0}
};

struct module_exports exports = {
	"sctp",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	0,              /* destroy function */
	0               /* per child init function */
};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(!shm_initialized() && init_shm()<0)
		return -1;

#ifdef USE_SCTP
	/* shm is used, be sure it is initialized */
	if(sctp_mod_pre_init()<0)
		return -1;
	return 0;
#else
	LOG(L_CRIT, "sctp core support not enabled\n");
	return -1;
#endif
}

static int mod_init(void)
{
#ifdef USE_SCTP
	char tmp[256];
	if (sctp_check_compiled_sockopts(tmp, 256)!=0){
		LM_WARN("sctp unsupported socket options: %s\n", tmp);
	}

	if (sctp_register_cfg()){
		LOG(L_CRIT, "could not register the sctp configuration\n");
		return -1;
	}
	if (sctp_register_rpc()){
		LOG(L_CRIT, "could not register the sctp rpc commands\n");
		return -1;
	}
	return 0;
#else /* USE_SCTP */
	LOG(L_CRIT, "sctp core support not enabled\n");
	return -1;
#endif /* USE_SCTP */
}

#ifdef USE_SCTP
static int sctp_mod_pre_init(void)
{
	sctp_srapi_t api;

	/* set defaults before the config mod params */
	init_sctp_options();

	memset(&api, 0, sizeof(sctp_srapi_t));
	api.init                    = init_sctp;
	api.destroy                 = destroy_sctp;
	api.init_sock               = sctp_init_sock;
	api.check_support           = sctp_check_support;
	api.rcv_loop                = sctp_rcv_loop;
	api.msg_send                = sctp_msg_send;

	if(sctp_core_register_api(&api)<0) {
		LM_ERR("cannot regiser sctp core api\n");
		return -1;
	}
	return 0;
}
#endif
