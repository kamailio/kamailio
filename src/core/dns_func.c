
/* 
 * Copyright (C) 2013  mariuszbi@gmail.com
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
/*!
 * \file 
 * \brief Kamailio core :: DNS wrappers
 * \author mariuszbi@gmail.com
 * \ingroup core
 * Module: \ref core
 */

#include "dns_func.h"

#include <netinet/in.h>
#include <resolv.h>
#include <sys/types.h>
#include <netdb.h>

struct hostent;

struct dns_func_t dns_func = {
	res_init,
	res_search,
	gethostbyname,
#ifdef HAVE_GETHOSTBYNAME2
	gethostbyname2
#else
	NULL
#endif
};

 
void load_dnsfunc(struct dns_func_t *d) {
	dns_func.sr_res_init = d->sr_res_init;
	dns_func.sr_res_search = d->sr_res_search;
	dns_func.sr_gethostbyname = d->sr_gethostbyname;
	dns_func.sr_gethostbyname2 = d->sr_gethostbyname2;
} 

