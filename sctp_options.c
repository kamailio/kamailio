/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
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
/* 
 * sctp options
 */
/*
 * History:
 * --------
 *  2008-08-07  initial version (andrei)
 *  2009-05-26  runtime cfg support (andrei)
 */

#include <string.h>

#include "sctp_options.h"
#include "dprint.h"
#include "cfg/cfg.h"

struct cfg_group_sctp sctp_default_cfg;



#ifdef USE_SCTP
/** cfg_group_sctp description (for the config framework). */
static cfg_def_t sctp_cfg_def[] = {
	/*   name        , type |input type| chg type, min, max, fixup, proc. cbk.
	      description */
	{ "socket_rcvbuf", CFG_VAR_INT| CFG_READONLY, 512, 102400, 0, 0,
		"socket receive buffer size (read-only)" },
	{ "socket_sndbuf", CFG_VAR_INT| CFG_READONLY, 512, 102400, 0, 0,
		"socket send buffer size (read-only)" },
	{ "autoclose", CFG_VAR_INT| CFG_READONLY, 1, 1<<30, 0, 0,
		"seconds before closing and idle connection (must be non-zero)" },
	{ "send_ttl", CFG_VAR_INT| CFG_ATOMIC, 0, 1<<30, 0, 0,
		"milliseconds before aborting a send" },
	{ "send_retries", CFG_VAR_INT| CFG_ATOMIC, 0, MAX_SCTP_SEND_RETRIES, 0, 0,
		"re-send attempts on failure" },
	{0, 0, 0, 0, 0, 0, 0}
};



void* sctp_cfg; /* sctp config handle */

#endif /* USE_SCTP */

void init_sctp_options()
{
#ifdef USE_SCTP
	sctp_default_cfg.so_rcvbuf=0; /* do nothing, use the kernel default */
	sctp_default_cfg.so_sndbuf=0; /* do nothing, use the kernel default */
	sctp_default_cfg.autoclose=DEFAULT_SCTP_AUTOCLOSE; /* in seconds */
	sctp_default_cfg.send_ttl=DEFAULT_SCTP_SEND_TTL;   /* in milliseconds */
	sctp_default_cfg.send_retries=DEFAULT_SCTP_SEND_RETRIES;
#endif
}



#define W_OPT_NSCTP(option) \
	if (sctp_default_cfg.option){\
		WARN("sctp_options: " #option \
			" cannot be enabled (sctp support not compiled-in)\n"); \
			sctp_default_cfg.option=0; \
	}



void sctp_options_check()
{
#ifndef USE_SCTP
	W_OPT_NSCTP(autoclose);
	W_OPT_NSCTP(send_ttl);
	W_OPT_NSCTP(send_retries);
#else
	if (sctp_default_cfg.send_retries>MAX_SCTP_SEND_RETRIES) {
		WARN("sctp: sctp_send_retries too high (%d), setting it to %d\n",
				sctp_default_cfg.send_retries, MAX_SCTP_SEND_RETRIES);
		sctp_default_cfg.send_retries=MAX_SCTP_SEND_RETRIES;
	}
#endif
}



void sctp_options_get(struct cfg_group_sctp *s)
{
#ifdef USE_SCTP
	*s=*(struct cfg_group_sctp*)sctp_cfg;
#else
	memset(s, 0, sizeof(*s));
#endif /* USE_SCTP */
}



#ifdef USE_SCTP
/** register sctp config into the configuration framework.
 * @return 0 on success, -1 on error */
int sctp_register_cfg()
{
	if (cfg_declare("sctp", sctp_cfg_def, &sctp_default_cfg, cfg_sizeof(sctp),
				&sctp_cfg))
		return -1;
	if (sctp_cfg==0){
		BUG("null sctp cfg");
		return -1;
	}
	return 0;
}
#endif /* USE_SCTP */
