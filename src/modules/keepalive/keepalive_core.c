/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/fmsg.h"
#include "../tm/tm_load.h"

#include "keepalive.h"
#include "api.h"

struct tm_binds tmb;

static void ka_run_route(sip_msg_t *msg, str *uri, char *route);
static void ka_options_callback(struct cell *t, int type,
		struct tmcb_params *ps);

extern str ka_ping_from;
/*! \brief
 * Callback run from timer,  for probing a destination
 *
 * This timer is regularly fired.
 */
ticks_t ka_check_timer(ticks_t ticks, struct timer_ln* tl, void* param)
{
	ka_dest_t *ka_dest;
	str ka_ping_method = str_init("OPTIONS");
	str ka_outbound_proxy = {0, 0};
	uac_req_t uac_r;

	ka_dest = (ka_dest_t *)param;

    LM_DBG("dest: %.*s\n", ka_dest->uri.len, ka_dest->uri.s);

    if(ka_counter_del > 0 && ka_dest->counter > ka_counter_del) {
        return (ticks_t)(0); /* stops the timer */
    }

	str *uuid = shm_malloc(sizeof(str));
	ka_str_copy(&(ka_dest->uuid), uuid, NULL);
    /* Send ping using TM-Module.
     * int request(str* m, str* ruri, str* to, str* from, str* h,
     *		str* b, str *oburi,
     *		transaction_cb cb, void* cbp); */
    set_uac_req(&uac_r, &ka_ping_method, 0, 0, 0, TMCB_LOCAL_COMPLETED,
            ka_options_callback, (void *)uuid);

    if(tmb.t_request(&uac_r, &ka_dest->uri, &ka_dest->uri, &ka_ping_from,
               &ka_outbound_proxy)
            < 0) {
        LM_ERR("unable to ping [%.*s]\n", ka_dest->uri.len, ka_dest->uri.s);
    }

    ka_dest->last_checked = time(NULL);

	return ka_dest->ping_interval; /* periodical, but based on dest->ping_interval, not on initial_timeout */
}

/*! \brief
 * Callback-Function for the OPTIONS-Request
 * This Function is called, as soon as the Transaction is finished
 * (e. g. a Response came in, the timeout was hit, ...)
 */
static void ka_options_callback(
		struct cell *t, int type, struct tmcb_params *ps)
{
	str uri = {0, 0};
	sip_msg_t *msg = NULL;
	ka_state state;

	char *state_routes[] = {"", "keepalive:dst-up", "keepalive:dst-down"};

	str *uuid = (str *)(*ps->param);

	LM_DBG("ka_options_callback with uuid: %.*s\n", uuid->len, uuid->s);

	// Retrieve ka_dest by uuid from destination list
	ka_lock_destination_list();
	ka_dest_t *ka_dest=0,*hollow=0;
	if (!ka_find_destination_by_uuid(*uuid, &ka_dest, &hollow)) {
		LM_ERR("Couldn't find destination \r\n");
		shm_free(uuid->s);
		shm_free(uuid);
		ka_unlock_destination_list();
		return;
	}
	lock_get(&ka_dest->lock); // Lock record so we prevent to be removed in the meantime
	shm_free(uuid->s);
	shm_free(uuid);
	ka_unlock_destination_list();

	uri.s = t->to_hdr.s + 5;
	uri.len = t->to_hdr.len - 8;
	LM_DBG("OPTIONS request was finished with code %d (to %.*s)\n", ps->code,
			ka_dest->uri.len, ka_dest->uri.s); //uri.len, uri.s);


	// accepting 2XX return codes
	if(ps->code >= 200 && ps->code <= 299) {
		state = KA_STATE_UP;
		ka_dest->last_down = time(NULL);
		ka_dest->counter=0;
	} else {
		state = KA_STATE_DOWN;
		ka_dest->last_up = time(NULL);
		ka_dest->counter++;
	}

	if(state != ka_dest->state) {
		ka_run_route(msg, &uri, state_routes[state]);

		if(ka_dest->statechanged_clb != NULL) {
			ka_dest->statechanged_clb(&ka_dest->uri, state, ka_dest->user_attr);
		}

		LM_DBG("new state is: %d\n", state);
		ka_dest->state = state;
	}
	if(ka_dest->response_clb != NULL) {
		ka_dest->response_clb(&ka_dest->uri, ps, ka_dest->user_attr);
	}
	lock_release(&ka_dest->lock);
}

/*
 * Execute kamailio script event routes
 *
 */
static void ka_run_route(sip_msg_t *msg, str *uri, char *route)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;

	if(route == NULL) {
		LM_ERR("bad route\n");
		return;
	}

	LM_DBG("run event_route[%s]\n", route);

	rt = route_get(&event_rt, route);
	if(rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_DBG("route *%s* does not exist", route);
		return;
	}

	fmsg = msg;
	if(fmsg == NULL) {
		if(faked_msg_init() < 0) {
			LM_ERR("faked_msg_init() failed\n");
			return;
		}
		fmsg = faked_msg_next();
		fmsg->parsed_orig_ruri_ok = 0;
		fmsg->new_uri = *uri;
	}

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);
}


/*
 * copy str into dynamically allocated shm memory
 */
int ka_str_copy(str *src, str *dest, char *prefix)
{
	int lp = prefix ? strlen(prefix) : 0;

	dest->s = (char *)shm_malloc((src->len + 1 + lp) * sizeof(char));
	if(dest->s == NULL) {
		LM_ERR("no more memory!\n");
		return -1;
	}

	if(prefix)
		strncpy(dest->s, prefix, lp);
	strncpy(dest->s + lp, src->s, src->len);
	dest->s[src->len + lp] = '\0';
	dest->len = src->len + lp;

	return 0;
}
