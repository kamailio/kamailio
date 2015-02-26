/*
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *
 */

#ifndef _RLS_NOTIFY_H
#define _RLS_NOTIFY_H

#include <libxml/parser.h>
#include "../../str.h"
#include "../presence/subscribe.h"

#define BOUNDARY_STRING_LEN    24
#define BUF_REALLOC_SIZE       2048
#define MAX_HEADERS_LENGTH     (104+ 255+ 1)
#define RLS_HDR_LEN      1024
#define MAX_FORWARD 70

#define REALLOC_BUF\
		size+= BUF_REALLOC_SIZE;\
		buf= (char*)pkg_realloc(buf, size);\
		if(buf== NULL) \
		{	ERR_MEM("constr_multipart_body");}

int send_full_notify(subs_t* subs, xmlNodePtr rl_node, 
                     str* rl_uri, unsigned int hash_code);

typedef int (*list_func_t)(char* uri, void* param); 

int process_list_and_exec(xmlNodePtr list, str username, str domain,
		list_func_t function, void* param);
char* generate_string(int length);
char* generate_cid(char* uri, int uri_len);
char* get_auth_string(int flag);
int agg_body_sendn_update(str* rl_uri, char* boundary_string, str* rlmi_body,
		str* multipart_body, subs_t* subs, unsigned int hash_code);
int rls_send_notify(subs_t* subs,str* body,char* start_cid,char* boundary_string);
int create_empty_rlmi_doc(xmlDocPtr *rlmi_doc, xmlNodePtr *list_node, str *uri, int version, int full_state);

extern char *instance_id;
#endif
