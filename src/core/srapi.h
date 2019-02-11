/*
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#ifndef __SRAPI_H__
#define __SRAPI_H__

#include "str.h"
#include "parser/msg_parser.h"

typedef void (*sr_generate_callid_f)(str*);

int sr_register_callid_func(sr_generate_callid_f f);

sr_generate_callid_f sr_get_callid_func(void);

typedef int (*sr_cseq_update_f)(sip_msg_t*);
typedef struct sr_cfgenv {
	sr_cseq_update_f cb_cseq_update;
	str uac_cseq_auth;
	str uac_cseq_refresh;
} sr_cfgenv_t;

void sr_cfgenv_init(void);
sr_cfgenv_t* sr_cfgenv_get(void);

#endif
