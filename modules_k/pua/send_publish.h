/*
 * $Id$
 *
 * pua module - presence user agent module
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
 */


#ifndef _PU_SEND_PUBLISH_
#define _PU_SEND_PUBLISH_
#include <time.h>
#include "../tm/tm_load.h"
#include "../../str.h"
#include "hash.h"


typedef struct publ_info
{
	str id;
	str* pres_uri;
	str* body;
	int expires;
	int flag;
	int source_flag;

}publ_info_t;

typedef int (*send_publish_t)(publ_info_t* publ);
int send_publish( publ_info_t* publ );
void print_ua_pres(ua_pres_t* p);
void publ_cback_func(struct cell *t, int type, struct tmcb_params *ps);
str* publ_build_hdr(int expires, str* etag);

#endif
