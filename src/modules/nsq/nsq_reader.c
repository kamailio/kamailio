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
#include "nsq_reader.h"

char *eventData = NULL;

extern json_api_t json_api;
extern str nsq_event_key;
extern str nsq_event_sub_key;

int nsq_pv_get_event_payload(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	return eventData == NULL ? pv_get_null(msg, param, res)
							 : pv_get_strzval(msg, param, res, eventData);
}

int nsq_consumer_fire_event(char *routename)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("searching event_route[%s]\n", routename);
	rt = route_get(&event_rt, routename);
	if(rt < 0 || event_rt.rlist[rt] == NULL) {
		LM_DBG("route %s does not exist\n", routename);
		return -2;
	}
	LM_DBG("executing event_route[%s] (%d)\n", routename, rt);
	if(faked_msg_init() < 0) {
		return -2;
	}
	fmsg = faked_msg_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(rtb);

	return 0;
}

int nsq_consumer_event(char *payload, char *channel, char *topic)
{
	struct json_object *json_obj = NULL;
	int ret = 0;
	str ev_name = {0, 0}, ev_category = {0, 0};
	char *k = NULL;
	char buffer[512];
	char *p;

	eventData = payload;

	json_obj = json_api.json_parse(payload);
	if(json_obj == NULL) {
		return ret;
	}

	k = pkg_malloc(nsq_event_key.len + 1);
	memcpy(k, nsq_event_key.s, nsq_event_key.len);
	k[nsq_event_key.len] = '\0';
	json_api.extract_field(json_obj, k, &ev_category);
	pkg_free(k);

	k = pkg_malloc(nsq_event_sub_key.len + 1);
	memcpy(k, nsq_event_sub_key.s, nsq_event_sub_key.len);
	k[nsq_event_sub_key.len] = '\0';
	json_api.extract_field(json_obj, k, &ev_name);
	pkg_free(k);

	snprintf(buffer, 512, "nsq:consumer-event-%.*s-%.*s", ev_category.len,
			ev_category.s, ev_name.len, ev_name.s);
	for(p = buffer; *p; ++p)
		*p = tolower(*p);
	for(p = buffer; *p; ++p)
		if(*p == '_')
			*p = '-';
	if(nsq_consumer_fire_event(buffer) != 0) {
		snprintf(buffer, 512, "nsq:consumer-event-%.*s", ev_category.len,
				ev_category.s);
		for(p = buffer; *p; ++p)
			*p = tolower(*p);
		for(p = buffer; *p; ++p)
			if(*p == '_')
				*p = '-';
		if(nsq_consumer_fire_event(buffer) != 0) {
			snprintf(buffer, 512, "nsq:consumer-event-%.*s-%.*s",
					nsq_event_key.len, nsq_event_key.s, nsq_event_sub_key.len,
					nsq_event_sub_key.s);
			for(p = buffer; *p; ++p)
				*p = tolower(*p);
			for(p = buffer; *p; ++p)
				if(*p == '_')
					*p = '-';
			if(nsq_consumer_fire_event(buffer) != 0) {
				snprintf(buffer, 512, "nsq:consumer-event-%.*s",
						nsq_event_key.len, nsq_event_key.s);
				for(p = buffer; *p; ++p)
					*p = tolower(*p);
				for(p = buffer; *p; ++p)
					if(*p == '_')
						*p = '-';
				if(nsq_consumer_fire_event(buffer) != 0) {
					snprintf(buffer, 512, "nsq:consumer-event");
					if(nsq_consumer_fire_event(buffer) != 0) {
						LM_ERR("nsq:consumer-event not found");
					}
				}
			}
		}
	}

	if(json_obj) {
		json_object_put(json_obj);
	}

	eventData = NULL;

	return ret;
}

void nsq_message_handler(struct NSQReader *rdr, struct NSQDConnection *conn,
		struct NSQMessage *msg, void *ctx)
{
	int ret = 0;

	char *payload = (char *)shm_malloc(msg->body_length + 1);
	if(!payload) {
		LM_ERR("error allocating shared memory for payload");
	}
	strncpy(payload, msg->body, msg->body_length);
	payload[msg->body_length] = 0;

	ret = nsq_consumer_event(payload, rdr->channel, rdr->topic);

	buffer_reset(conn->command_buf);

	if(ret < 0) {
		nsq_requeue(conn->command_buf, msg->id, 100);
	} else {
		nsq_finish(conn->command_buf, msg->id);
	}
	buffered_socket_write_buffer(conn->bs, conn->command_buf);

	buffer_reset(conn->command_buf);
	nsq_ready(conn->command_buf, rdr->max_in_flight);
	buffered_socket_write_buffer(conn->bs, conn->command_buf);

	free_nsq_message(msg);
	shm_free(payload);
}
