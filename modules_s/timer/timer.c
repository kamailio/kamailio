/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


struct timer_action {
	int timer_no;
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

#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}

static int sel_timer(str* res, select_t* s, struct sip_msg* msg) {
	struct timer_action* a;
	int n;
	if (!msg) { /* select fixup */
		for (a = timer_actions, n = s->params[1].v.i; a && n!=0; a=a->next, n--);
		if (!a) {
			LOG(L_ERR, "ERROR: timer: timer_enable_fixup: timer #%d not declared\n", s->params[1].v.i);
			return E_CFG;
		}
		s->params[1].v.p = a;
	}
	return 0;
}

static int sel_enabled(str* res, select_t* s, struct sip_msg* msg) {
	static char buf[2] = "01";
	if (!msg) return sel_timer(res, s, msg);
	res->len = 1;
	res->s = &buf[(((struct timer_action*) s->params[1].v.p)->link->flags & F_TIMER_ACTIVE) != 0];
	return 0;
}

select_row_t sel_declaration[] = {
        { NULL, SEL_PARAM_STR, STR_STATIC_INIT("timer"), sel_timer, SEL_PARAM_EXPECTED|CONSUME_NEXT_INT|FIXUP_CALL},
        { sel_timer, SEL_PARAM_STR, STR_STATIC_INIT("enabled"), sel_enabled, 0},
        { NULL, SEL_PARAM_STR, STR_NULL, NULL, 0}
};

static unsigned int timer_msg_no = 0;

static ticks_t timer_handler(ticks_t ticks, struct timer_ln* tl, void* data) {
	/*?min length of first line of message is 16 char!?*/
	#define MSG "GET /timer HTTP/0.9\n\n"
	struct sip_msg* msg;
	struct timer_action *a;

	a = data;
	if (!a->disable_itself) {

		DBG("timer: handler: called at %d ticks, timer_no is #%d, pid:%d\n", ticks, a->timer_no, getpid());

		if (a->route_no >= main_rt.idx) {
			BUG("invalid routing table number #%d of %d\n", a->route_no, main_rt.idx);
			goto err2;
		}
		if (!main_rt.rlist[a->route_no]) {
			LOG(L_WARN, "WARN: timer: route not declared (hash:%d)\n", a->route_no);
			goto err2;
		}
		msg=pkg_malloc(sizeof(struct sip_msg));
		if (msg==0) {
			LOG(L_ERR, "ERROR: timer: handler: no mem for sip_msg\n");
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
			LOG(L_ERR, "ERROR: timer: handler: parse_msg failed\n");
			goto err;
		}
		/* ... clear branches from previous message */
		clear_branches();
		reset_static_buffer();
		if (exec_pre_req_cb(msg)==0 )
			goto end; /* drop the request */
		/* exec the routing script */
		run_actions(main_rt.rlist[a->route_no], msg);
		run_flags &= ~RETURN_R_F; /* absorb returns */

		/* execute post request-script callbacks */
		exec_post_req_cb(msg);
	end:
		reset_avps();
		DBG("timer: handler: cleaning up\n");
	err:
		free_sip_msg(msg);
		pkg_free(msg);
	err2:	;
	}
        /* begin critical section */
	if (a->disable_itself) {

		timer_allow_del(a->link);
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
	int /* res, */ n;
/*	res = fixup_int_12(param, param_no);
	if (res < 0) return res; */
	n=atoi((char *)*param);
	switch (param_no) {
		case 1:
			for (a = timer_actions /*, n=(int) *param*/; a && n!=0; a=a->next, n--);
			if (!a) {
				LOG(L_ERR, "ERROR: timer: timer_enable_fixup: timer #%ld not declared\n", (long) *param);
				return E_CFG;
			}
			*param = a;
			break;
		case 2:
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

/* route_no,interval_ms[,"slow"|"fast"[,"start"]] */
static int declare_timer(modparam_t type, char* param) {
	int n;
	unsigned int route_no, interval, enabled, flags;
	struct timer_action **pa;
	char *p, *save_p, c;
	str s;

	save_p = p = param;
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

	for (pa=&pkg_timer_actions, n=0; *pa; pa=&(*pa)->next, n++);
	*pa = pkg_malloc(sizeof(**pa));   /* cannot use shmmem here! */
	if (!*pa) {
		LOG(L_ERR, "ERROR: cannot allocate timer data\n");
		return E_OUT_OF_MEM;
	}
	memset(*pa, 0, sizeof(**pa));
	(*pa)->timer_no = n;
	(*pa)->route_no = route_no;
	(*pa)->interval = interval;
	(*pa)->enable_on_start = enabled;
	(*pa)->flags = flags;

	return 0;
err:
	LOG(L_ERR, "ERROR: declare_timer: error near '%s'", save_p);
	return E_CFG;
}

static int mod_init() {
	struct timer_action *a, **pa;

	DBG("DEBUG: timer: init: initializing, pid=%d\n", getpid());

	/* copy from pkg to shm memory */
	for (pa=&timer_actions; pkg_timer_actions; pa=&(*pa)->next) {
		a = pkg_timer_actions;
		*pa = shm_malloc(sizeof(**pa));
		if (!*pa) {
			LOG(L_ERR, "ERROR: cannot allocate timer data\n");
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
			LOG(L_ERR, "ERROR: timer: init: cannot allocate timer\n");
			return E_OUT_OF_MEM;
		}
		timer_init(a->link, timer_handler, a, a->flags);
		if (!a->link) {
			LOG(L_ERR, "ERROR: timer: init: cannot initialize timer\n");
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
printf("TIMER CHILDINIT: rank:%d , pid:%d\n", rank, getpid());
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
	DBG("DEBUG: timer: destroy: destroying, pid=%d\n", getpid());
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
	{"timer_enable", timer_enable_func, 2, timer_enable_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | ONSEND_ROUTE},
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
	"timer",
	cmds,        /* Exported commands */
	0,	     /* RPC */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	destroy_mod, /* destroy function */
	0,           /* oncancel function */
	child_init   /* per-child init function */
};
