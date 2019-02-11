/* 
 * Copyright (C) 2013 mariuszbi@gmail.com
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
 * \brief Kamailio core :: DNS wrapper functions
 * \author mariuszbi@gmail.com
 *      
 * \ingroup core 
 * Module: \ref core                    
 *  
 *
 */

#ifndef DNS_FUNC_H
#define DNS_FUNC_H

#include <sys/socket.h>

struct hostent;

typedef int (*res_init_t)(void);
typedef int (*res_search_t)(const char*, int, int, unsigned char*, int);
typedef struct hostent* (*gethostbyname_t)(const char*);
typedef struct hostent* (*gethostbyname2_t)(const char*, int);

struct dns_func_t {
	res_init_t sr_res_init;
	res_search_t sr_res_search;
	gethostbyname_t sr_gethostbyname;
	gethostbyname2_t sr_gethostbyname2;
};

/* 
 * initiate structure with system values
 */
//extern struct dns_func_t dns_func;

extern 
void load_dnsfunc(struct dns_func_t *d);


#endif
