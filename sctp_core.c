/**
 * $Id$
 *
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

#include "sctp_core.h"

/**
 *
 */
static sctp_srapi_t _sctp_srapi = { 0 };
static int _sctp_srapi_set = 0;

/**
 *
 */
int sctp_core_init(void)
{
	if(_sctp_srapi_set==0) {
		LM_ERR("SCTP API not initialized\n");
		return -1;
	}

	return _sctp_srapi.init();
}

/**
 *
 */
void sctp_core_destroy(void)
{
}

/**
 *
 */
int sctp_core_init_sock(struct socket_info* sock_info)
{
	return 0;
}

/**
 *
 */
void sctp_core_init_options(void)
{
}

/**
 *
 */
int sctp_core_check_compiled_sockopts(char* buf, int size)
{
	return 0;
}

/**
 *
 */
int sctp_core_check_support(void)
{
	return 0;
}

/**
 *
 */
int sctp_core_rcv_loop(void)
{
	return 0;
}

/**
 *
 */
int sctp_core_msg_send(struct dest_info* dst, char* buf, unsigned len)
{
	return 0;
}

/**
 *
 */
int sctp_core_register_api(sctp_srapi_t *api)
{
	if(api==NULL || api->init==NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(_sctp_srapi_set==1) {
		LM_ERR("SCTP API already initialized\n");
		return -1;
	}
	_sctp_srapi_set = 1;
	memcpy(&_sctp_srapi, api, sizeof(sctp_srapi_t));
	return 0;
}
