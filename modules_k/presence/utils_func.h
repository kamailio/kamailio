/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */


#ifndef UTILS_FUNC_H
#define UTILS_FUNC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"

static inline int uandd_to_uri(str user,  str domain, str *out)
{
	int size;

	if(out==0)
		return -1;

	size = user.len + domain.len+7;

	out->s = (char*)pkg_malloc(size*sizeof(char));
	if(out->s == NULL)
	{
		LOG(L_ERR, "PRESENCE: uandd_to_uri: Error while allocating memory\n");
		return -1;
	}
	out->len = 0;
	strcpy(out->s,"sip:");
	out->len = 4;
	strncpy(out->s+out->len, user.s, user.len);
	out->len += user.len;
	out->s[out->len] = '@';
	out->len+= 1;
	strncpy(out->s + out->len, domain.s, domain.len);
	out->len += domain.len;

	out->s[out->len] = 0;
	DBG("presence:uandd_to_uri: uri=%.*s\n", out->len, out->s);
	
	return 0;
}

//str* int_to_str(long int n);

int a_to_i (char *s,int len);

void to64frombits(unsigned char *out, const unsigned char *in, int inlen);
int reply_bad_event(struct sip_msg * msg);

#endif

