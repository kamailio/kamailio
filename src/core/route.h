/*
 * Copyright (C) 2001-2003 FhG Fokus
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
* \brief Kamailio core :: Message routing
* \ingroup core
* Module: \ref core
*/


#ifndef route_h
#define route_h

#include <sys/types.h>
#include <regex.h>
#include <netdb.h>

#include "config.h"
#include "error.h"
#include "route_struct.h"
#include "action.h"
#include "parser/msg_parser.h"
#include "str_hash.h"

/*#include "cfg_parser.h" */

/* Various types of route sections, make sure that the values defined in the
 * macros below occupy disjunct bits so that they can also be used as flags
 */
#define REQUEST_ROUTE (1 << 0)
#define FAILURE_ROUTE (1 << 1)
#define TM_ONREPLY_ROUTE (1 << 2)
#define BRANCH_ROUTE  (1 << 3)
#define ONSEND_ROUTE  (1 << 4)
#define ERROR_ROUTE   (1 << 5)
#define LOCAL_ROUTE   (1 << 6)
#define CORE_ONREPLY_ROUTE (1 << 7)
#define BRANCH_FAILURE_ROUTE (1 << 8)
#define ONREPLY_ROUTE (TM_ONREPLY_ROUTE|CORE_ONREPLY_ROUTE)
#define EVENT_ROUTE   REQUEST_ROUTE
#define ANY_ROUTE     (0xFFFFFFFF)

/* The value of this variable is one of the route types defined above and it
 * determines the type of the route being executed, module functions can use
 * this value to determine the type of the route they are being executed in
 */
extern int route_type;

#define set_route_type(type) \
	do {					 \
		route_type = (type); \
	} while(0)

#define get_route_type()	route_type

#define is_route_type(type) (route_type & (type))

struct route_list{
	struct action** rlist;
	int idx; /* first empty entry */ 
	int entries; /* total number of entries */
	struct str_hash_table names; /* name to route index mappings */
};


/* main "script table" */
extern struct route_list main_rt;
/* main reply route table */
extern struct route_list onreply_rt;
extern struct route_list failure_rt;
extern struct route_list branch_rt;
extern struct route_list onsend_rt;
extern struct route_list event_rt;

/* script optimization level */
extern int scr_opt_lev;

int init_routes(void);
void destroy_routes(void);
int route_get(struct route_list* rt, char* name);
int route_lookup(struct route_list* rt, char* name);

void push(struct action* a, struct action** head);
int add_actions(struct action* a, struct action** head);
void print_rls(void);
int fix_rls(void);

int eval_expr(struct run_act_ctx* h, struct expr* e, struct sip_msg* msg);


/* fixup functions*/
int fix_actions(struct action* a);
int fix_expr(struct expr* exp);



#endif
