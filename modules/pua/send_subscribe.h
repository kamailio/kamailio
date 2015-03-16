/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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


#ifndef _PU_SEND_SUBSC_
#define _PU_SEND_SUBSC_

#include <time.h>

#include "../../modules/tm/tm_load.h"
#include "../../str.h"
#include "hash.h"

typedef struct subs_info
{
	str id;
	str* pres_uri;
	str* watcher_uri;
	str* contact;
	str* remote_target;
	str* outbound_proxy;
	int event;
	str* extra_headers;
	int expires;
	int source_flag;
	int flag;         /*  it can be : INSERT_TYPE or UPDATE_TYPE; not compulsory */
	void* cb_param;  /* the parameter for the function to be called on the callback 
						 for the received reply; it must be allocated in share memory;
						 a reference to it will be found in the cb_param filed of the ua_pres_structure
						 receied as a parameter for the registered function*/
	int internal_update_flag;
}subs_info_t;


typedef int (*send_subscribe_t)(subs_info_t* subs);
int send_subscribe(subs_info_t* subs);
void subs_cback_func(struct cell *t, int type, struct tmcb_params *ps);
str* subs_build_hdr(str* watcher_uri, int expires, int event, str* extra_headers);
dlg_t* pua_build_dlg_t(ua_pres_t* presentity);
ua_pres_t* subscribe_cbparam(subs_info_t* subs, int ua_flag);
ua_pres_t* subs_cbparam_indlg(ua_pres_t* subs, int expires, int ua_flag);

#endif
