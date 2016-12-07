/*
 * xcap_client module - XCAP client for Kamailio
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

#ifndef XCAP_FUNC_H
#define XCAP_FUNC_H

#include "xcap_callbacks.h"

#define USERS_TYPE      1
#define GLOBAL_TYPE     2

#define IF_MATCH    	1
#define IF_NONE_MATCH    2

/* macros for the entities responsible for handling a record inserted
 * in xcap table*/

#define INTEGRATED_SERVER      0
#define XCAP_CL_MOD            1 /* xcap_client module responsibility */


typedef struct xcap_doc_sel
{
	str auid;
	int doc_type;
	int type; 
	str xid;
	str filename;
}xcap_doc_sel_t;

typedef struct ns_list
{
	int name;
	str value;
	struct ns_list* next;
}ns_list_t;

typedef struct step
{
	str val;
	struct step* next;
}step_t;

typedef struct xcap_node_sel
{
	step_t* steps;
	step_t* last_step;
	int size;
	ns_list_t* ns_list;
	ns_list_t* last_ns;
	int ns_no;

}xcap_node_sel_t;

typedef struct att_test
{
	str name;
	str value;
}attr_test_t;

typedef struct xcap_get_req
{
	char* xcap_root;
	unsigned int port;
	xcap_doc_sel_t doc_sel;
	xcap_node_sel_t* node_sel;
	char* etag;
	int match_type;
}xcap_get_req_t;

xcap_node_sel_t* xcapInitNodeSel(void);
typedef xcap_node_sel_t* (*xcap_nodeSel_init_t )(void);

xcap_node_sel_t* xcapNodeSelAddStep(xcap_node_sel_t* curr_sel, str* name,
		str* namespace, int pos, attr_test_t*  attr_test, str* extra_sel);

typedef xcap_node_sel_t* (*xcap_nodeSel_add_step_t)(xcap_node_sel_t* curr_sel,
	str* name,str* namespace,int pos,attr_test_t*  attr_test,str* extra_sel);

xcap_node_sel_t* xcapNodeSelAddTerminal(xcap_node_sel_t* curr_sel, 
		char* attr_sel, char* namespace_sel, char* extra_sel );

typedef xcap_node_sel_t* (*xcap_nodeSel_add_terminal_t)(xcap_node_sel_t* curr_sel, 
		char* attr_sel, char* namespace_sel, char* extra_sel );

/* generical function to get an element from an xcap server */
char* xcapGetElem(xcap_get_req_t req, char** etag);

typedef char* (*xcap_get_elem_t)(xcap_get_req_t req, char** etag);

void xcapFreeNodeSel(xcap_node_sel_t* node);

typedef void (*xcap_nodeSel_free_t)(xcap_node_sel_t* node);

/* specifical function to get a new document, not present in xcap table 
 * to be updated and handled by the xcap_client module*/
char* xcapGetNewDoc(xcap_get_req_t req, str user, str domain);
typedef char* (*xcapGetNewDoc_t)(xcap_get_req_t req, str user, str domain);

typedef struct xcap_api {
	xcap_get_elem_t get_elem;
	xcap_nodeSel_init_t int_node_sel;
	xcap_nodeSel_add_step_t add_step;
	xcap_nodeSel_add_terminal_t add_terminal;
	xcap_nodeSel_free_t free_node_sel;
	xcapGetNewDoc_t getNewDoc;
	register_xcapcb_t register_xcb;
}xcap_api_t;

int bind_xcap(xcap_api_t* api);

typedef int (*bind_xcap_t)(xcap_api_t* api);

char* send_http_get(char* path, unsigned int xcap_port, char* match_etag,
		int match_type, char** etag);
#endif
