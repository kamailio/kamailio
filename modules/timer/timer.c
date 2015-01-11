/*
 * Copyright (C) 2006 iptelorg GmbH
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "../../timer.h"
#include "../../timer_ticks.h"
#include "../../route.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../../error.h"
#include "../../config.h"
#include "../../trim.h"
#include "../../select.h"
#include "../../ut.h"
#include "../../select_buf.h"

#include "../../receive.h"
#include "../../ip_addr.h"

#include "../../receive.h"
#include "../../globals.h"
#include "../../route.h"
#include "../../parser/msg_parser.h"
#include "../../action.h"
#include "../../script_cb.h"
#include "../../dset.h"
#include "../../usr_avp.h"


MODULE_VERSION

#define MODULE_NAME "timer"

struct timer_action {
	char *timer_name;
	int route_no;
	int interval;
	int enable_on_start;
	int disable_itself;
	unsigned short flags; /* slow / fast */
	struct timer_ln *link;

	struct timer_action* next;
};

/* list of all operations */
static struct timer_action* timer_actions = 0;
static struct timer_action* pkg_timer_actions = 0;
static struct receive_info rcv_info;
static struct timer_action* timer_executed = 0;

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}

#define eat_alphanum(_p) \
	while ( (*(_p) >= 'a' && *(_p) <= 'z') || (*(_p) >= 'A' && *(_p) <= 'Z') || (*(_p) >= '0' && *(_p) <= '9') || (*(_p) == '_') ) {\
		(_p)++;\
	}

static struct timer_action* find_action_by_name(struct timer_action* timer_actions, char *name, int len) {
	struct timer_action *a;
	if (len == -1) len = strlen(name);
	for (a=timer_actions; a; a = a->next) {		
		if (a->timer_name && strlen(a->timer_name)==len && strncmp(name, a->timer_name, len) == 0)
			return a;
	}
	return NULL;
}

static int sel_root(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_timer(str* res, select_t* s, struct sip_msg* msg) {
	struct timer_action* a;
	if (!msg) { /* select fixup */
		a = find_action_by_name(timer_actions /* called after mod_init */, s->params[2].v.s.s, s->params[2].v.s.len);
		if (!a) {
			ERR(MODULE_NAME": timer_enable_fixup: timer '%.*s' not declared\n", s->params[2].v.s.len, s->params[2].v.s.s);
			return E_CFG;
		}
		s->params[2].v.p = a;
	}
	return 0;
}

static int sel_enabled(str* res, select_t* s, struct sip_msg* msg) {
	static char buf[2] = "01";
	if (!msg) return sel_timer(res, s, msg);
	res->len = 1;
	res->s = &buf[(((struct timer_action*) s->params[2].v.p)->link->flags & F_TIMER_ACTIVE) != 0];
	return 0;
}

static int sel_executed(str* res, select_t* s, struct sip_msg* msg) {
	if (!timer_executed) return 1;
	res->s = timer_executed->timer_name;
	res->len = strlen(res->s);
	return 0;
}

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME), sel_root, SEL_PARAM_EXPECTED},
	{ sel_root, SEL_PARAM_STR, STR_STATIC_INIT("timer"), sel_timer, SEL_PARAM_EXPECTED|CONSUME_NEXT_STR|FIXUP_CALL},
	{ sel_timer, SEL_PARAM_STR, STR_STATIC_INIT("enabled"), sel_enabled, 0},
	{ sel_root, SEL_PARAM_STR, STR_STATIC_INIT("executed"), sel_executed, 0},
	
	{ NULL, SEL_PARAM_STR, STR_NULL, NULL, 0}
};

static unsigned int timer_msg_no = 0;

static ticks_t timer_handler(ticks_t ticks, struct timer_ln* tl, void* data) {
	/*?min length of first line of message is 16 char!?*/
	#define MSG "GET /timer HTTP/0.9\n\n"
	struct sip_msg* msg;
	struct timer_action *a;
	struct run_act_ctx ra_ctx;

	a = data;
	if (!a->disable_itself) {

		DEBUG(MODULE_NAME": handler: called at %d ticks, timer: '%s', pid:%d\n", ticks, a->timer_name, getpid());

		if (a->route_no >= main_rt.idx) {
			BUG(MODULE_NAME": invalid routing table number #%d of %d\n", a->route_no, main_rt.idx);
			goto err2;
		}
		if (!main_rt.rlist[a->route_no]) {
			WARN(MODULE_NAME": route not declared (hash:%d)\n", a->route_no);
			goto err2;
		}
		msg=pkg_malloc(sizeof(struct sip_msg));
		if (msg==0) {
			ERR(MODULE_NAME": handler: no mem for sip_msg\n");
			goto err2;
		}
		timer_msg_no++;
		memset(msg, 0, sizeof(struct sip_msg)); /* init everything to 0 */

		msg->buf=MSG;
		msg->len=sizeof(MSG)-1;

		msg->rcv= rcv_info;
		msg->id=timer_msg_no;
		msg->set_global_address=default_global_address;
		msg->set_global_port=default_global_port;

		if (parse_msg(msg->buf, msg->len, msg)!=0){
			ERR(MODULE_NAME": handler: parse_msg failed\n");
			goto err;
		}
		/* ... clear branches from previous message */
		clear_branches();
		reset_static_buffer();
		if (exec_pre_script_cb(msg, REQUEST_CB_TYPE)==0 )
			goto end; /* drop the request */
		/* exec the routing script */
		timer_executed = a;
		init_run_actions_ctx(&ra_ctx);
		run_actions(&ra_ctx, main_rt.rlist[a->route_no], msg);
		timer_executed = 0;
		/* execute post request-script callbacks */
		exec_post_script_cb(msg, REQUEST_CB_TYPE);
	end:
		reset_avps();
		DEBUG(MODULE_NAME": handler: cleaning up\n");
	err:
		free_sip_msg(msg);
		pkg_free(msg);
	err2:	;
	}
        /* begin critical section */
	if (a->disable_itself) {

		timer_allow_del();
		timer_del(a->link);
		timer_reinit(a->link);
		a->disable_itself = 0;
	        /* end critical section */
		return 0;   /* do no call more */
	}
	else
        	return (ticks_t)(-1); /* periodical */
}

static int timer_enable_fixup(void** param, int param_no) {
	struct timer_action* a;
 	int /*res, */n;
	switch (param_no) {
		case 1:
			a = find_action_by_name(timer_actions /* called after mod_init*/, (char*) *param, -1);
			if (!a) {
				ERR(MODULE_NAME": timer_enable_fixup: timer '%s' not declared\n", (char*) *param);
				return E_CFG;
			}
			*param = a;
			break;
		case 2:
		/*	res = fixup_int_12(param, param_no);
			if (res < 0) return res; */
			n=atoi((char *)*param);
			*param = (void*)(long)(/*(int) *param*/n != 0);
			break;
		default: ;
	}
	return 0;
}

static int timer_enable_func(struct sip_msg* m, char* timer_act, char* enable) {
	struct timer_action* a;
	int en;
	a = (void*) timer_act;
	en = (int)(long) enable;
	/* timer is not deleted immediately but is removed from handler by itself because timer_del may be slow blocking procedure
	 * Disable and enable in sequence may be tricky
	 */
        /* begin critical section */
	if ((a->link->flags & F_TIMER_ACTIVE) == 0) {
		if (en) {
			timer_reinit(a->link);
			timer_add(a->link, MS_TO_TICKS(a->interval));
			a->disable_itself = 0;
		}
	}
	else {
		if (en && a->disable_itself) {
			a->disable_itself = 0;	/* it's not 100% reliable! */
		}
		else if (!en) {
			a->disable_itself++;
		}
	}
        /* end critical section */
	return 1;
}

static int get_next_part(char** s, str* part, char delim) {
	char *c, *c2;
	c = c2 = *s;
	eat_spaces(c);
	while (*c2!=delim && *c2!=0) c2++;

	if (*c2) {
		*s = c2+1;
	}
	else {
		*s = c2;
	}
	eat_spaces(*s);
	c2--;
	/* rtrim */
	while ( c2 >= c && ((*c2 == ' ')||(*c2 == '\t')) ) c2--;
	part->s = c;
	part->len = c2-c+1;
	return part->len;
}

/* timer_id=route_no,interval_ms[,"slow"|"fast"[,"enable"]] */
static int declare_timer(modparam_t type, char* param) {
	int n;
	unsigned int route_no, interval, enabled, flags;
	struct timer_action *pa;
	char *p, *save_p, c, *timer_name;
	str s;

	timer_name = 0;
	save_p = p = param;
	eat_alphanum(p);
	if (*p != '=' || p == save_p) goto err;
	*p = '\0';
	timer_name = save_p;
	p++;
	if (find_action_by_name(pkg_timer_actions, timer_name, -1) != NULL) {
		ERR(MODULE_NAME": declare_timer: timer '%s' already exists\n", timer_name);
		return E_CFG;
	}

	save_p = p;
	if (!get_next_part(&p, &s, ',')) goto err;

	c = s.s[s.len];
	s.s[s.len] = '\0';
	n = route_get(&main_rt, s.s);
	s.s[s.len] = c;
	if (n == -1) goto err;
	route_no = n;

	save_p = p;
	if (!get_next_part(&p, &s, ',')) goto err;
	if (str2int(&s, &interval) < 0) goto err;

	save_p = p;
	flags = 0;
	if (get_next_part(&p, &s, ',')) {
		if (s.len == 4 && strncasecmp(s.s, "FAST", 4)==0)
			flags = F_TIMER_FAST;
		else if (s.len == 4 && strncasecmp(s.s, "SLOW", 4)==0)
			;
		else goto err;
	}

	save_p = p;
	enabled = 0;
	if (get_next_part(&p, &s, ',')) {
		if (s.len == 6 && strncasecmp(s.s, "ENABLE", 6)==0)
			enabled = 1;
		else goto err;
	}

	
	pa = pkg_malloc(sizeof(*pa));   /* cannot use shmmem here! */
	if (!pa) {
		ERR(MODULE_NAME": cannot allocate timer data\n");
		return E_OUT_OF_MEM;
	}
	memset(pa, 0, sizeof(*pa));
	pa->timer_name = timer_name;
	pa->route_no = route_no;
	pa->interval = interval;
	pa->enable_on_start = enabled;
	pa->flags = flags;
	pa->next = pkg_timer_actions;
	pkg_timer_actions = pa;

	return 0;
err:
	ERR(MODULE_NAME": declare_timer: timer_name: '%s', error near '%s'\n", timer_name, save_p);
	return E_CFG;
}

static int mod_init() {
	struct timer_action *a, **pa;

	DEBUG(MODULE_NAME": init: initializing, pid=%d\n", getpid());

	/* copy from pkg to shm memory */
	for (pa=&timer_actions; pkg_timer_actions; pa=&(*pa)->next) {
		a = pkg_timer_actions;
		*pa = shm_malloc(sizeof(**pa));
		if (!*pa) {
			ERR(MODULE_NAME": cannot allocate timer data\n");
			return E_OUT_OF_MEM;
		}
		memcpy(*pa, a, sizeof(**pa));
		(*pa)->next = 0;
		pkg_timer_actions = a->next;
		pkg_free(a);
	}

	for (a=timer_actions; a; a=a->next) {
		a->link = timer_alloc();
		if (!a->link) {
			ERR(MODULE_NAME": init: cannot allocate timer\n");
			return E_OUT_OF_MEM;
		}
		timer_init(a->link, timer_handler, a, a->flags);
		if (!a->link) {
			ERR(MODULE_NAME": init: cannot initialize timer\n");
			return E_CFG;
		}
	}

	memset(&rcv_info, 0, sizeof(rcv_info));
	register_select_table(sel_declaration);
	return 0;
}

static int child_init(int rank) {
	struct timer_action* a;
	/* may I start timer in mod_init ?? */
	if (rank!=PROC_TIMER) return 0;
	for (a=timer_actions; a; a=a->next) {
		if (a->enable_on_start) {
			timer_add(a->link, MS_TO_TICKS(a->interval));
		}
	}
	return 0;
}

static void destroy_mod(void) {
	struct timer_action* a;
	DEBUG(MODULE_NAME": destroy: destroying, pid=%d\n", getpid());
	while (timer_actions) {
		a = timer_actions;
		if (a->link) {
			timer_del(a->link);
			timer_free(a->link);
		}
		timer_actions = a->next;
		shm_free(a);
	}
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{MODULE_NAME"_enable", timer_enable_func, 2, timer_enable_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
	{0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"declare_timer", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_timer},
	{0, 0, 0}
};


struct module_exports exports = {
	MODULE_NAME,
	cmds,        /* Exported commands */
	0,	     /* RPC */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	destroy_mod, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};
