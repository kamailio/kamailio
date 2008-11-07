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
 */


#include "sctp_options.h"
#include "dprint.h"


struct sctp_cfg_options sctp_options;

void init_sctp_options()
{
#ifdef USE_SCTP
	sctp_options.sctp_autoclose=DEFAULT_SCTP_AUTOCLOSE; /* in seconds */
	sctp_options.sctp_send_ttl=DEFAULT_SCTP_SEND_TTL;   /* in milliseconds */
	sctp_options.sctp_send_retries=DEFAULT_SCTP_SEND_RETRIES;
#endif
}



#define W_OPT_NSCTP(option) \
	if (sctp_options.option){\
		WARN("sctp_options: " #option \
			" cannot be enabled (sctp support not compiled-in)\n"); \
			sctp_options.option=0; \
	}



void sctp_options_check()
{
#ifndef USE_SCTP
	W_OPT_NSCTP(sctp_autoclose);
	W_OPT_NSCTP(sctp_send_ttl);
	W_OPT_NSCTP(sctp_send_retries);
	if (sctp_options.sctp_send_retries>MAX_SCTP_SEND_RETRIES) {
		WARN("sctp: sctp_send_retries too high (%d), setting it to %d\n",
				sctp_option.sctp_send_retries, MAX_SCTP_SEND_RETRIES);
		sctp_options.sctp_send_retries=MAX_SCTP_SEND_RETRIES;
	}
#endif
}



void sctp_options_get(struct sctp_cfg_options *s)
{
	*s=sctp_options;
}
