/*
 * NSQ module interface
 *
 * Copyright (C) 2016 Weave Communications
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This module was based on the Kazoo module created by 2600hz.
 * Thank you to 2600hz and their brilliant VoIP developers.
 *
 */

#include "../json/api.h"
#include "nsq_mod.h"

MODULE_VERSION

static pv_export_t nsq_mod_pvs[] = {
		{{"nsqE", (sizeof("nsqE") - 1)}, PVT_OTHER, nsq_pv_get_event_payload, 0,
				0, 0, 0, 0},
		{{0, 0}, 0, 0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"consumer_workers", INT_PARAM, &nsq_consumer_workers},
		{"max_in_flight", INT_PARAM, &nsq_max_in_flight},
		{"lookupd_address", PARAM_STR, &nsq_lookupd_address},
		{"lookupd_port", INT_PARAM, &lookupd_port},
		{"consumer_use_nsqd", INT_PARAM,
				&consumer_use_nsqd}, // consume messages from nsqd instead of lookupd
		{"topic_channel", PARAM_STRING | USE_FUNC_PARAM,
				(void *)nsq_add_topic_channel},
		{"nsqd_address", PARAM_STR, &nsqd_address},
		{"nsqd_port", INT_PARAM, &nsqd_port},
		{"consumer_event_key", PARAM_STR, &nsq_event_key},
		{"consumer_event_subkey", PARAM_STR, &nsq_event_sub_key}, {0, 0, 0}};

static void free_tc_list(nsq_topic_channel_t *tcl)
{
	nsq_topic_channel_t *tc, *tc0;
	tc = tcl;
	while(tc) {
		tc0 = tc->next;
		free(tc->topic);
		free(tc->channel);
		pkg_free(tc);
		tc = tc0;
	}
	tcl = NULL;
}

static int nsq_add_topic_channel(modparam_t type, void *val)
{
	nsq_topic_channel_t *tc;
	size_t size;
	char *channel = (char *)val;
	char *topic;
	char *sep = NULL;

	sep = strchr(channel, ':');
	if(!sep) {
		topic = (char *)val;
		channel = DEFAULT_CHANNEL;
		LM_ERR("delimiter (\":\") not found inside topic_channel param, using "
			   "default channel [%s]\n",
				channel);
	} else {
		topic = strsep(&channel, ":");
	}
	size = sizeof(nsq_topic_channel_t);
	tc = (nsq_topic_channel_t *)pkg_malloc(size);
	if(tc == NULL) {
		LM_ERR("memory error!\n");
		free_tc_list(tc_list);
		return -1;
	}
	memset(tc, 0, size);
	tc->topic = strdup(topic);
	tc->channel = strdup(channel);
	++nsq_topic_channel_counter;

	tc->next = tc_list;
	tc_list = tc;

	return 0;
}

struct module_exports exports = {
		"nsq", DEFAULT_DLFLAGS, /* dlopen flags */
		0,						/* Exported functions */
		params,					/* Exported parameters */
		0,						/* exported MI functions */
		nsq_mod_pvs,			/* exported pseudo-variables */
		0,						/* response function*/
		mod_init,				/* module initialization function */
		mod_child_init,			/* per-child init function */
		mod_destroy				/* destroy function */
};

static int fire_init_event(int rank)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("rank is (%d)\n", rank);
	if(rank != PROC_INIT)
		return 0;

	rt = route_get(&event_rt, "nsq:mod-init");
	if(rt >= 0 && event_rt.rlist[rt] != NULL) {
		LM_DBG("executing event_route[nsq:mod-init] (%d)\n", rt);
		if(faked_msg_init() < 0)
			return -1;
		fmsg = faked_msg_next();
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		if(ctx.run_flags & DROP_R_F) {
			LM_ERR("exit due to 'drop' in event route\n");
			return -1;
		}
		set_route_type(rtb);
	}

	return 0;
}

static int mod_init(void)
{
	if(json_load_api(&json_api) < 0) {
		LM_ERR("cannot bind to JSON API\n");
		return -1;
	}
	LM_DBG("NSQ Workers per Topic/Channel: %d\n", nsq_consumer_workers);
	if(!nsq_topic_channel_counter) {
		nsq_topic_channel_counter = 1;
	}
	LM_DBG("NSQ Total Topic/Channel: %d\n", nsq_topic_channel_counter);
	nsq_consumer_workers = nsq_consumer_workers * nsq_topic_channel_counter;
	LM_DBG("NSQ Total Workers: %d\n", nsq_consumer_workers);
	int total_workers = nsq_consumer_workers + 2;

	register_procs(total_workers);
	cfg_register_child(total_workers);

	return 0;
}

/**
 *
 */
void nsq_consumer_worker_proc(char *topic, char *channel, int max_in_flight)
{
	struct ev_loop *loop;
	loop = ev_default_loop(0);
	struct NSQReader *rdr;
	void *ctx = NULL; //(void *)(new TestNsqMsgContext());
	static char address[128];

	if(loop == NULL) {
		LM_ERR("cannot get libev loop\n");
	}

	LM_DBG("NSQ Worker connecting to NSQ Topic [%s] and NSQ Channel [%s]\n",
			topic, channel);
	// setup the reader
	rdr = new_nsq_reader(loop, topic, channel, (void *)ctx, NULL, NULL, NULL,
			nsq_message_handler);
	rdr->max_in_flight = max_in_flight;

	if(consumer_use_nsqd == 0) {
		snprintf(address, 128, "%.*s", nsq_lookupd_address.len,
				nsq_lookupd_address.s);
		nsq_reader_add_nsqlookupd_endpoint(rdr, address, lookupd_port);
	} else {
		snprintf(address, 128, "%.*s", nsqd_address.len, nsqd_address.s);
		nsq_reader_connect_to_nsqd(rdr, address, nsqd_port);
	}

	nsq_run(loop);
}

/**
 * @brief Initialize async module children
 */
static int mod_child_init(int rank)
{
	int pid;
	int i;
	int workers = nsq_consumer_workers / nsq_topic_channel_counter;
	int max_in_flight = 1;

	if(nsq_max_in_flight > 1) {
		max_in_flight = nsq_max_in_flight;
	}

	fire_init_event(rank);

	if(rank == PROC_INIT || rank == PROC_TCP_MAIN)
		return 0;

	if(rank == PROC_MAIN) {
		nsq_topic_channel_t *tc;

		tc = tc_list;
		if(tc == NULL) {
			LM_ERR("topic and channel not set, using defaults\n");
			for(i = 0; i < workers; i++) {
				pid = fork_process(PROC_XWORKER, "NSQ Consumer Worker", 1);
				if(pid < 0)
					return -1; /* error */
				if(pid == 0) {
					if(cfg_child_init())
						return -1;
					nsq_consumer_worker_proc(
							DEFAULT_TOPIC, DEFAULT_CHANNEL, max_in_flight);
					LM_CRIT("nsq_consumer_worker_proc():: worker_process "
							"finished without exit!\n");
					exit(-1);
				}
			}
		} else {
			while(tc) {
				for(i = 0; i < workers; i++) {
					pid = fork_process(PROC_XWORKER, "NSQ Consumer Worker", 1);
					if(pid < 0)
						return -1; /* error */
					if(pid == 0) {
						if(cfg_child_init())
							return -1;
						nsq_consumer_worker_proc(
								tc->topic, tc->channel, max_in_flight);
						LM_CRIT("nsq_consumer_worker_proc():: worker_process "
								"finished without exit!\n");
						exit(-1);
					}
				}
				tc = tc->next;
			}
		}

		return 0;
	}

	return 0;
}


/**
 * destroy module function
 */
static void mod_destroy(void)
{
	free_tc_list(tc_list);
}
