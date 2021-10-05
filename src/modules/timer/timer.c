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
#include "../../core/timer.h"
#include "../../core/timer_ticks.h"
#include "../../core/route.h"
#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/str.h"
#include "../../core/error.h"
#include "../../core/config.h"
#include "../../core/trim.h"
#include "../../core/select.h"
#include "../../core/ut.h"
#include "../../core/select_buf.h"

#include "../../core/receive.h"
#include "../../core/ip_addr.h"

#include "../../core/receive.h"
#include "../../core/globals.h"
#include "../../core/route.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/action.h"
#include "../../core/script_cb.h"
#include "../../core/dset.h"
#include "../../core/usr_avp.h"
#include "../../core/kemi.h"


MODULE_VERSION

#define MODULE_NAME "timer"

#define TIMER_ROUTE_NAME_SIZE 64

typedef struct timer_action {
	char *timer_name;
	char route_name_buf[TIMER_ROUTE_NAME_SIZE];
	str route_name;
	int route_no;
	int interval;
	int enable_on_start;
	int disable_itself;
	unsigned short flags; /* slow / fast */
	struct timer_ln *link;

	struct timer_action* next;
} timer_action_t;

/* list of all operations */
static timer_action_t* timer_actions = 0;
static timer_action_t* pkg_timer_actions = 0;
static receive_info_t rcv_info;
static timer_action_t* timer_executed = 0;

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
		(_p)++;}

#define eat_alphanum(_p) \
	while ( (*(_p) >= 'a' && *(_p) <= 'z') || (*(_p) >= 'A'\
				&& *(_p) <= 'Z') || (*(_p) >= '0' && *(_p) <= '9')\
				|| (*(_p) == '_') ) {\
		(_p)++;\
	}

static timer_action_t* find_action_by_name(timer_action_t* timer_actions,
		char *name, int len)
{
	timer_action_t *a;
	if (len == -1) len = strlen(name);
	for (a=timer_actions; a; a = a->next) {
		if (a->timer_name && strlen(a->timer_name)==len
				&& strncmp(name, a->timer_name, len) == 0)
			return a;
	}
	return NULL;
}

static int sel_root(str* res, select_t* s, sip_msg_t* msg)
{
	/* dummy */
	return 0;
}

static int sel_timer(str* res, select_t* s, sip_msg_t* msg)
{
	struct timer_action* a;
	if (!msg) { /* select fixup */
		a = find_action_by_name(timer_actions /* called after mod_init */,
				s->params[2].v.s.s, s->params[2].v.s.len);
		if (!a) {
			LM_ERR("timer '%.*s' not declared\n",
					s->params[2].v.s.len, s->params[2].v.s.s);
			return E_CFG;
		}
		s->params[2].v.p = a;
	}
	return 0;
}

static int sel_enabled(str* res, select_t* s, sip_msg_t* msg)
{
	static char buf[2] = "01";
	if (!msg)
		return sel_timer(res, s, msg);
	res->len = 1;
	res->s = &buf[(((struct timer_action*) s->params[2].v.p)->link->flags
					& F_TIMER_ACTIVE) != 0];
	return 0;
}

static int sel_executed(str* res, select_t* s, sip_msg_t* msg)
{
	if (!timer_executed)
		return 1;
	res->s = timer_executed->timer_name;
	res->len = strlen(res->s);
	return 0;
}

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME), sel_root,
		SEL_PARAM_EXPECTED},
	{ sel_root, SEL_PARAM_STR, STR_STATIC_INIT("timer"), sel_timer,
		SEL_PARAM_EXPECTED|CONSUME_NEXT_STR|FIXUP_CALL},
	{ sel_timer, SEL_PARAM_STR, STR_STATIC_INIT("enabled"), sel_enabled, 0},
	{ sel_root, SEL_PARAM_STR, STR_STATIC_INIT("executed"), sel_executed, 0},

	{ NULL, SEL_PARAM_STR, STR_NULL, NULL, 0}
};

static unsigned int timer_msg_no = 0;

static ticks_t timer_handler(ticks_t ticks, struct timer_ln* tl, void* data)
{
	/*?min length of first line of message is 16 char!?*/
#define MSG "GET /timer HTTP/0.9\r\nUser-Agent: internal\r\n\r\n"
	sip_msg_t* msg;
	timer_action_t *a;
	run_act_ctx_t ra_ctx;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("timer");

	a = data;
	if (!a->disable_itself) {

		LM_DBG("handler called at %d ticks, timer: '%s', pid:%d\n",
				ticks, a->timer_name, getpid());

		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			if (a->route_no >= main_rt.idx) {
				LM_BUG("invalid routing table number #%d of %d\n",
						a->route_no, main_rt.idx);
				goto err2;
			}
			if (!main_rt.rlist[a->route_no]) {
				LM_WARN("route not declared (hash:%d)\n", a->route_no);
				goto err2;
			}
		}
		msg=pkg_malloc(sizeof(sip_msg_t));
		if (msg==0) {
			LM_ERR("no pkg mem for sip msg\n");
			goto err2;
		}
		timer_msg_no++;
		memset(msg, 0, sizeof(sip_msg_t)); /* init everything to 0 */

		msg->buf=MSG;
		msg->len=sizeof(MSG)-1;

		msg->rcv= rcv_info;
		msg->id=timer_msg_no;
		msg->set_global_address=default_global_address;
		msg->set_global_port=default_global_port;

		if (parse_msg(msg->buf, msg->len, msg)!=0){
			LM_ERR("parse msg failed\n");
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
		if(keng==NULL) {
			run_actions(&ra_ctx, main_rt.rlist[a->route_no], msg);
		} else {
			if(sr_kemi_route(keng, msg, EVENT_ROUTE, &a->route_name, &evname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
		timer_executed = 0;
		/* execute post request-script callbacks */
		exec_post_script_cb(msg, REQUEST_CB_TYPE);
end:
		ksr_msg_env_reset();
		LM_DBG("cleaning up\n");
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
	return (ticks_t)(-1); /* periodical */
}

static int timer_enable_fixup(void** param, int param_no)
{
	timer_action_t* a;
	int /*res, */n;
	switch (param_no) {
		case 1:
			a = find_action_by_name(timer_actions /* called after mod_init*/,
					(char*) *param, -1);
			if (!a) {
				LM_ERR("timer '%s' not declared\n", (char*) *param);
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

static int timer_enable_helper(sip_msg_t* m, timer_action_t* a, int en)
{
	/* timer is not deleted immediately but is removed from handler
	 * by itself because timer_del may be slow blocking procedure
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

static int timer_enable_func(sip_msg_t* m, char* timer_act, char* enable)
{
	timer_action_t* a;
	int en;
	a = (void*) timer_act;
	en = (int)(long) enable;

	return  timer_enable_helper(m, a, en);
}

static int get_next_part(char** s, str* part, char delim)
{
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
static int declare_timer(modparam_t type, char* param)
{
	int n;
	unsigned int route_no, interval, enabled, flags;
	timer_action_t *pa;
	char *p, *save_p, c, *timer_name;
	str s;
	str route_name = STR_NULL;

	timer_name = 0;
	save_p = p = param;
	eat_alphanum(p);
	if (*p != '=' || p == save_p)
		goto err;
	*p = '\0';
	timer_name = save_p;
	p++;
	if (find_action_by_name(pkg_timer_actions, timer_name, -1) != NULL) {
		LM_ERR("timer '%s' already exists\n", timer_name);
		return E_CFG;
	}

	save_p = p;
	if (!get_next_part(&p, &s, ',')) goto err;

	if(s.len>=TIMER_ROUTE_NAME_SIZE-1) {
		LM_ERR("route name is too long [%.*s] (%d)\n", s.len, s.s, s.len);
		return E_CFG;
	}
	c = s.s[s.len];
	s.s[s.len] = '\0';
	n = route_get(&main_rt, s.s);
	s.s[s.len] = c;
	if (n == -1) goto err;
	route_no = n;
	route_name = s;

	save_p = p;
	if (!get_next_part(&p, &s, ','))
		goto err;
	if (str2int(&s, &interval) < 0)
		goto err;

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
		LM_ERR("cannot allocate timer data\n");
		return E_OUT_OF_MEM;
	}
	memset(pa, 0, sizeof(*pa));
	pa->timer_name = timer_name;
	memcpy(pa->route_name_buf, route_name.s, route_name.len);
	pa->route_name_buf[route_name.len] = '\0';
	pa->route_name.s = pa->route_name_buf;
	pa->route_name.len = route_name.len;
	pa->route_no = route_no;
	pa->interval = interval;
	pa->enable_on_start = enabled;
	pa->flags = flags;
	pa->next = pkg_timer_actions;
	pkg_timer_actions = pa;

	return 0;
err:
	LM_ERR("timer name '%s', error near '%s'\n", timer_name, save_p);
	return E_CFG;
}

static int mod_init()
{
	struct timer_action *a, **pa;

	LM_DBG("initializing, pid=%d\n", getpid());

	/* copy from pkg to shm memory */
	for (pa=&timer_actions; pkg_timer_actions; pa=&(*pa)->next) {
		a = pkg_timer_actions;
		*pa = shm_malloc(sizeof(**pa));
		if (!*pa) {
			LM_ERR("cannot allocate timer data\n");
			return E_OUT_OF_MEM;
		}
		memcpy(*pa, a, sizeof(**pa));
		(*pa)->route_name.s = (*pa)->route_name_buf;
		(*pa)->next = 0;
		pkg_timer_actions = a->next;
		pkg_free(a);
	}

	for (a=timer_actions; a; a=a->next) {
		a->link = timer_alloc();
		if (!a->link) {
			LM_ERR("cannot allocate timer\n");
			return E_OUT_OF_MEM;
		}
		timer_init(a->link, timer_handler, a, a->flags);
		if (!a->link) {
			LM_ERR("cannot initialize timer\n");
			return E_CFG;
		}
	}

	memset(&rcv_info, 0, sizeof(rcv_info));
	register_select_table(sel_declaration);
	return 0;
}

static int child_init(int rank)
{
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

static void destroy_mod(void)
{
	struct timer_action* a;
	LM_DBG("destroying, pid=%d\n", getpid());
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
	{MODULE_NAME"_enable", timer_enable_func, 2, timer_enable_fixup, 0,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"declare_timer", PARAM_STRING|PARAM_USE_FUNC, (void*) declare_timer},
	{0, 0, 0}
};


struct module_exports exports = {
	MODULE_NAME,     /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported commands */
	params,          /* Exported parameters */
	0,               /* Exported RPC functions */
	0,               /* pseudo-variables exports */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per-child init function */
	destroy_mod      /* destroy function */
};
