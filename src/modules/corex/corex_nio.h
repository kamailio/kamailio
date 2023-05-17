/**
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Extensible SIP Router, a free SIP server.
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

#ifndef _COREX_NIO_H_
#define _COREX_NIO_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/events.h"
#include "../../core/ut.h"

#include "../../core/tcp_options.h"
#include "../../core/msg_translator.h"

extern int nio_route_no;
extern int nio_min_msg_len;
extern int nio_is_incoming;

extern str nio_msg_avp_param;
extern int_str nio_msg_avp_name;
extern unsigned short nio_msg_avp_type;

int nio_msg_received(sr_event_param_t *evp);
int nio_msg_sent(sr_event_param_t *evp);

int nio_check_incoming(void);
char *nio_msg_update(sip_msg_t *msg, unsigned int *olen);

int nio_intercept_init(void);

#endif
