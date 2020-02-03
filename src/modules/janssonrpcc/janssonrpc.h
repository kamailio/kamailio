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

#include "../../core/sr_module.h"
#include "../jansson/jansson_utils.h"
extern jansson_to_val_f jsontoval;
extern pv_spec_t jsonrpc_result_pv;

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
extern int cmd_pipe;
extern str result_pv_str;
extern retry_range_t* global_retry_ranges;
extern const str null_str;

#define jsr_ms_to_tv(ms, tv) \
	do { \
		memset(&tv, 0, sizeof(struct timeval)); \
		tv.tv_sec = ms/1000; \
		tv.tv_usec = ((ms % 1000) * 1000); \
	} while(0)

#endif /* _JSONRPC_H_ */
