/*
 * $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../dprint.h"
#include "../../globals.h"
#include "../../pt.h"

#include "sruid.h"

/**
 *
 */
int sruid_init(sruid_t *sid, char sep, char *cid)
{
	int i;

	if(sid==NULL)
		return -1;
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
	LM_DBG("root for sruid is [%.*s] (%u / %d)\n", i+5, sid->uid.s,
			sid->counter, i+5);
	return 0;
}

/**
 *
 */
int sruid_next(sruid_t *sid)
{
	unsigned short digit;
	int i;
	unsigned int val;

	if(sid==NULL)
		return -1;

	sid->counter++;
	if(sid->counter==0)
		sid->counter=1;

	val = sid->counter;
	i = 0;
	while(val!=0)
	{
		digit =  val & 0x0f;
		sid->out[i++] = (digit >= 10) ? digit + 'a' - 10 : digit + '0';
		val >>= 4;
	}
	sid->out[i] = '\0';
	sid->uid.len = sid->out + i - sid->buf;
	LM_DBG("new sruid is [%.*s] (%u / %d)\n", sid->uid.len, sid->uid.s,
			sid->counter, sid->uid.len);
	return 0;
}

