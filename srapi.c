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


#include <stdio.h>
#include <stdlib.h>
#include "dprint.h"

#include "srapi.h"

static sr_generate_callid_f _sr_generate_callid_func = NULL;

/**
 *
 */
int sr_register_callid_func(sr_generate_callid_f f)
{
	if(_sr_generate_callid_func!=NULL) {
		LM_INFO("overwriting generate callid function\n");
	}
	_sr_generate_callid_func = f;
	return 0;
}

/**
 *
 */
sr_generate_callid_f sr_get_callid_func(void)
{
	return _sr_generate_callid_func;
}

/**
 *
 */
static sr_cfgenv_t _sr_cfgenv;

/**
 *
 */
void sr_cfgenv_init(void)
{
	memset(&_sr_cfgenv, 0, sizeof(sr_cfgenv_t));
	_sr_cfgenv.uac_cseq_auth.s = "P-K-CSeq-Auth";
	_sr_cfgenv.uac_cseq_auth.len = strlen(_sr_cfgenv.uac_cseq_auth.s);
	_sr_cfgenv.uac_cseq_refresh.s = "P-K-CSeq-Refresh";
	_sr_cfgenv.uac_cseq_refresh.len = strlen(_sr_cfgenv.uac_cseq_refresh.s);
}

/**
 *
 */
sr_cfgenv_t* sr_cfgenv_get(void)
{
	return &_sr_cfgenv;
}
