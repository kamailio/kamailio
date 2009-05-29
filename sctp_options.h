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

#ifndef _sctp_options_h
#define _sctp_options_h

#define DEFAULT_SCTP_AUTOCLOSE 180 /* seconds */
#define DEFAULT_SCTP_SEND_TTL  32000 /* in ms (32s)  */
#define DEFAULT_SCTP_SEND_RETRIES 0
#define MAX_SCTP_SEND_RETRIES 9


struct cfg_group_sctp{
	int so_rcvbuf;
	int so_sndbuf;
	unsigned int autoclose; /* in seconds */
	unsigned int send_ttl; /* in milliseconds */
	unsigned int send_retries;
	unsigned int srto_initial;
	unsigned int srto_max;
	unsigned int srto_min;
};

extern struct cfg_group_sctp sctp_default_cfg;

/* sctp config handle */
extern void* sctp_cfg;

void init_sctp_options();
void sctp_options_check();
int sctp_register_cfg();
void sctp_options_get(struct cfg_group_sctp *s);
int sctp_get_os_defaults(struct cfg_group_sctp *s);

#endif /* _sctp_options_h */
