/**
 * Copyright (C) 2019 Thomas Weber, pascom.net
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _MQTT_DISPATCH_
#define _MQTT_DISPATCH_

#include "../../core/pvar.h"

void mqtt_init_environment();

typedef struct mqtt_dispatcher_cfg {
	char *host;
	int   port;
	char *id;
	char *username;
	char *password;
	int   keepalive;
	char *will_topic;
	char *will;
	char *ca_file;
	char *ca_path;
	char *certificate;
	char *private_key;
	char *tls_method;
	char *tls_alpn;
	int   verify_certificate;
	char *cipher_list;
} mqtt_dispatcher_cfg_t;

int mqtt_init_notify_sockets(void);

void mqtt_close_notify_sockets_child(void);

void mqtt_close_notify_sockets_parent(void);

int mqtt_run_dispatcher(mqtt_dispatcher_cfg_t* cfg);

int pv_parse_mqtt_name(pv_spec_t *sp, str *in);
int pv_get_mqtt(sip_msg_t *msg,  pv_param_t *param, pv_value_t *res);
int pv_set_mqtt(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val);

int mqtt_prepare_publish(str *topic, str *payload, int qos);
int mqtt_prepare_subscribe(str *topic, int qos);
int mqtt_prepare_unsubscribe(str *topic);

enum mqtt_request_type {
	PUBLISH,
	SUBSCRIBE,
	UNSUBSCRIBE
};

typedef struct _mqtt_request {
	enum mqtt_request_type type;
	str topic;
	str payload;
	int qos;
} mqtt_request_t;

#endif
