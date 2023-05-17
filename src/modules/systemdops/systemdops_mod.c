/**
 * Copyright (C) 2019 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-daemon.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/events.h"
#include "../../core/globals.h"


MODULE_VERSION

struct module_exports exports = {
		"systemdops",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		0,				 /* cmd (cfg function) exports */
		0,				 /* param exports */
		0,				 /* RPC method exports */
		0,				 /* pseudo-variables exports */
		0,				 /* response handling function */
		0,				 /* module init function */
		0,				 /* per-child init function */
		0				 /* module destroy function */
};

/**
 *
 */
void ksr_sd_app_ready(void)
{
	sd_notifyf(0, "READY=1\nMAINPID=%lu", (unsigned long)creator_pid);
}

/**
 *
 */
void ksr_sd_app_shutdown(void)
{
	sd_notify(0, "STOPPING=1");
}

/**
 * module registration function
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_corecb_t *cbp = sr_corecb_get();
	if(cbp == NULL) {
		return -1;
	}
	cbp->app_ready = ksr_sd_app_ready;
	cbp->app_shutdown = ksr_sd_app_shutdown;
	return 0;
}
