/*
 *
 * sruid - unique id generator
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*!
* \file
* \brief core/utils :: Unique ID generator
* \ingroup core/utils
* Module: \ref core/utils
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/globals.h"
#include "../../core/pt.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/hashes.h"

#include "sruid.h"

/* starting polynomials */
#define SRUID_MASK_32 0xb4bcd35c
#define SRUID_MASK_31 0x7a5bc2e3

static unsigned int sruid_lfsr32 = 0;
static unsigned int sruid_lfsr31 = 0;

static int sruid_shift_lfsr(unsigned int *lfsr, unsigned int mask)
{
	int feedback;

	feedback = *lfsr & 0x1;
	*lfsr >>= 1;
	if (feedback == 1) {
		*lfsr ^= mask;
	}

	return *lfsr;
}

static void sruid_init_lfsr(void)
{
	sruid_lfsr32 = (unsigned int)time(NULL);
	sruid_lfsr31 = (unsigned int)getpid();
}

/**
 * returns a 32 bit random integer
 *
 */
static int sruid_get_random(){
	return (sruid_shift_lfsr(&sruid_lfsr32, SRUID_MASK_32)
				^ sruid_shift_lfsr(&sruid_lfsr31, SRUID_MASK_31)) & 0xffffffff;
}

/**
 *
 */
int sruid_init(sruid_t *sid, char sep, char *cid, int mode)
{
	int i;

	if(sid==NULL) {
		return -1;
	}
	memset(sid, 0, sizeof(sruid_t));
	memcpy(sid->buf, "srid", 4);
	if(cid!=NULL)
	{
		for(i=0; i<4 && cid[i]!='\0'; i++)
			sid->buf[i] = cid[i];
	}
	sid->buf[4] = sep;

	if(server_id!=0)
		i = snprintf(sid->buf+5, SRUID_SIZE - 5 /*so far*/ - 8 /* extra int */,
			"%x%c%x%c%x%c", (unsigned int)server_id, sep,
			(unsigned int)time(NULL), sep, (unsigned int)my_pid(), sep);
	else
		i = snprintf(sid->buf+5, SRUID_SIZE - 5 /*so far*/ - 8 /* extra int */,
			"%x%c%x%c",
			(unsigned int)time(NULL), sep, (unsigned int)my_pid(), sep);
	if(i<=0 || i>SRUID_SIZE-13)
	{
		LM_ERR("could not initialize sruid struct - output len: %d\n", i);
		return -1;
	}
	sid->out = sid->buf + i + 5;
	sid->uid.s = sid->buf;
	sid->mode = (sruid_mode_t)mode;
	if(sid->mode == SRUID_LFSR) {
		sruid_init_lfsr();
	}
	sid->pid = my_pid();
	LM_DBG("root for sruid is [%.*s] (%u / %d)\n", i+5, sid->uid.s,
			sid->counter, i+5);
	return 0;
}

/**
 *
 */
int sruid_reinit(sruid_t *sid, int mode)
{
	int i;
	char sep;

	if(sid==NULL) {
		return -1;
	}

	sep = sid->buf[4];
	sid->buf[5] = '\0';

	if(server_id!=0) {
		i = snprintf(sid->buf+5, SRUID_SIZE - 5 /*so far*/ - 8 /* extra int */,
			"%x%c%x%c%x%c", (unsigned int)server_id, sep,
			(unsigned int)time(NULL), sep, (unsigned int)my_pid(), sep);
	} else {
		i = snprintf(sid->buf+5, SRUID_SIZE - 5 /*so far*/ - 8 /* extra int */,
			"%x%c%x%c",
			(unsigned int)time(NULL), sep, (unsigned int)my_pid(), sep);
	}
	if(i<=0 || i>SRUID_SIZE-13)
	{
		LM_ERR("could not re-initialize sruid struct - output len: %d\n", i);
		return -1;
	}
	sid->out = sid->buf + i + 5;
	sid->uid.s = sid->buf;
	sid->mode = (sruid_mode_t)mode;
	if(sid->mode == SRUID_LFSR) {
		sruid_init_lfsr();
	}
	sid->pid = my_pid();
	LM_DBG("re-init root for sruid is [%.*s] (%u / %d)\n", i+5, sid->uid.s,
			sid->counter, i+5);
	return 0;
}

/**
 *
 */
int sruid_nextx(sruid_t *sid, str *x)
{
	unsigned short digit;
	int i;
	unsigned int val;

	if(sid==NULL) {
		return -1;
	}
	if(x!=NULL && x->len>0) {
		if(sid->out - sid->buf + 1 + x->len >= SRUID_SIZE) {
			LM_ERR("not enough space for x value\n");
			return -1;
		}
	}

	sid->counter++;
	if(sid->counter==0) {
		if(sid->mode == SRUID_INC) {
			/* counter overflow - re-init to have new timestamp */
			if(sruid_reinit(sid, SRUID_INC)<0)
				return -1;
		}
		sid->counter=1;
	}

	if(sid->mode == SRUID_LFSR) {
		val = sruid_get_random();
	} else {
		val = sid->counter;
	}
	i = 0;
	while(val!=0) {
		digit =  val & 0x0f;
		sid->out[i++] = (digit >= 10) ? digit + 'a' - 10 : digit + '0';
		val >>= 4;
	}
	if(x!=NULL && x->len>0) {
		sid->out[i++] = sid->buf[4]; /* sep */
		memcpy(sid->out + i, x->s, x->len);
		i += x->len;
	}
	sid->out[i] = '\0';
	sid->uid.len = sid->out + i - sid->buf;
	LM_DBG("new sruid is [%.*s] (%u / %d)\n", sid->uid.len, sid->uid.s,
			sid->counter, sid->uid.len);
	return 0;
}

/**
 *
 */
int sruid_nexthid(sruid_t *sid, str *sval)
{
	char buf_int[INT2STR_MAX_LEN];
	str hval = str_init("0");
	unsigned int hid = 0;

	if(sval==NULL || sval->s==NULL || sval->len<=0) {
		return sruid_nextx(sid, &hval);
	}
	hval = *sval;
	trim(&hval);
	hid = get_hash1_raw(hval.s, hval.len);
	hval.s = int2strbuf(hid, buf_int, INT2STR_MAX_LEN, &hval.len);
	return sruid_nextx(sid, &hval);
}

/**
 *
 */
int sruid_next(sruid_t *sid)
{
	return sruid_nextx(sid, NULL);
}

/**
 *
 */
int sruid_nextx_safe(sruid_t *sid, str *x)
{
	if(unlikely(sid->pid!=my_pid())) sruid_reinit(sid, sid->mode);
	return sruid_nextx(sid, x);
}

/**
 *
 */
int sruid_nexthid_safe(sruid_t *sid, str *sval)
{
	if(unlikely(sid->pid!=my_pid())) sruid_reinit(sid, sid->mode);
	return sruid_nexthid(sid, sval);
}

/**
 *
 */
int sruid_next_safe(sruid_t *sid)
{
	if(unlikely(sid->pid!=my_pid())) sruid_reinit(sid, sid->mode);
	return sruid_nextx(sid, NULL);
}
