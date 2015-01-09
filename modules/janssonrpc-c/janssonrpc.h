/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _JANSSONRPC_H_
#define _JANSSONRPC_H_

#ifdef TEST

#include "unit_tests/test.h"

#else

#include "../../sr_module.h"
#include "../jansson/jansson_utils.h"
jansson_to_val_f jsontoval;
pv_spec_t jsonrpc_result_pv;

#endif

#define JSONRPC_VERSION "2.0"

#define JSONRPC_INTERNAL_SERVER_ERROR -32603
#define JSONRPC_ERROR_NO_MEMORY -1;

/* DEFAULTS */
/* time (in ms) after which the error route is called */
#define JSONRPC_DEFAULT_TIMEOUT     500
#define JSONRPC_RESULT_STR "$var(jsrpc_result)"
#define JSONRPC_DEFAULT_RETRY       0

/* helpful macros */
#define CHECK_MALLOC_VOID(p)  if(!(p)) {ERR("Out of memory!\n"); return;}
#define CHECK_MALLOC(p)  if(!(p)) {ERR("Out of memory!\n"); return JSONRPC_ERROR_NO_MEMORY;}
#define CHECK_MALLOC_NULL(p)  if(!(p)) {ERR("Out of memory!\n"); return NULL;}
#define CHECK_MALLOC_GOTO(p,loc)  if(!(p)) {ERR("Out of memory!\n"); goto loc;}
#define CHECK_AND_FREE(p) if((p)!=NULL) shm_free(p)
#define CHECK_AND_FREE_EV(p) \
	if((p) && event_initialized((p))) {\
		event_del(p); \
		event_free(p); \
		p = NULL; \
	}

#define STR(ss) (ss).len, (ss).s
/* The lack of parens is intentional; this is intended to be used in a list
 * of multiple arguments.
 *
 * Usage: printf("my str %.*s", STR(mystr))
 *
 * Expands to: printf("my str %.*s", (mystr).len, (mystr).s)
 * */


#define PIT_MATCHES(param) \
	(pit->name.len == sizeof((param))-1 && \
		strncmp(pit->name.s, (param), sizeof((param))-1)==0)

#include <jansson.h>
#include <event.h>

typedef void (*libev_cb_f)(int sock, short flags, void *arg);

typedef struct retry_range {
	int start;
	int end;
	struct retry_range* next;
} retry_range_t;

/* globals */
int cmd_pipe;
extern const str null_str;
str result_pv_str;
retry_range_t* global_retry_ranges;

static inline str pkg_strdup(str src)
{
	str res;

	if (!src.s) {
		res.s = NULL;
		res.len = 0;
	} else if (!(res.s = (char *) pkg_malloc(src.len + 1))) {
		res.len = 0;
	} else {
		strncpy(res.s, src.s, src.len);
		res.s[src.len] = 0;
		res.len = src.len;
	}
	return res;
}

static inline str shm_strdup(str src)
{
	str res;

	if (!src.s) {
		res.s = NULL;
		res.len = 0;
	} else if (!(res.s = (char *) shm_malloc(src.len + 1))) {
		res.len = 0;
	} else {
		strncpy(res.s, src.s, src.len);
		res.s[src.len] = 0;
		res.len = src.len;
	}
	return res;
}

static inline struct timeval ms_to_tv(unsigned int time)
{
	struct timeval tv = {0,0};
	tv.tv_sec = time/1000;
	tv.tv_usec = ((time % 1000) * 1000);
	return tv;
}


#endif /* _JSONRPC_H_ */
