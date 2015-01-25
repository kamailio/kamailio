/*
 * Copyright (C) 2007 iptelorg GmbH 
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

/**
 * @file
 * @brief Kamailio TLS support :: TLS hooks for modules
 * @ingroup tls
 * Module: @ref tls
 */


#include "tls_hooks.h"
#include "tls_hooks_init.h"
#include "globals.h"

#ifdef TLS_HOOKS

struct tls_hooks tls_hook= {0,0,0,0,0,0,0,0};

static int tls_hooks_loaded=0;

int register_tls_hooks(struct tls_hooks* h)
{
	if (!tls_disable){
		tls_hook=*h;
		tls_hooks_loaded++;
		return 0;
	}
	return -1;
}


int tls_init(struct socket_info* si)
{
	if (tls_hook.init_si)
		return tls_hook.init_si(si);
		return -1;
}

int tls_has_init_si()
{
	return (tls_hook.init_si!=0);
}

int init_tls()
{
	if (tls_hook.init)
		return tls_hook.init();
	return 0;
}

int pre_init_tls()
{
	if (tls_hook.pre_init)
		return tls_hook.pre_init();
	return 0;
}

void destroy_tls()
{
	if (tls_hook.destroy)
		tls_hook.destroy();
}

int tls_loaded()
{
	return tls_hooks_loaded;
}

#endif /* TLS_HOOKS */
