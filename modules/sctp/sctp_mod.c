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

#include "sctp_options.h"
#include "sctp_rpc.h"

MODULE_VERSION

static int mod_init(void);

static cmd_export_t cmds[]={
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
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


static int mod_init(void)
{
#ifdef USE_SCTP
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
