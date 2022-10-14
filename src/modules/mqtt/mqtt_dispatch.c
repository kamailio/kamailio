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

#include <sys/socket.h>

#include <mosquitto.h>
#include <ev.h>

#include "../../core/sr_module.h"
#include "../../core/fmsg.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/kemi.h"
#include "../../core/pt.h"

#include "mqtt_dispatch.h"

// a socket pair to send data from any sip worker to the dispatcher
static int _mqtt_notify_sockets[2];

// time in seconds for periodic mqtt housekeeping
const static float _mqtt_timer_freq = 1.0;

// in case of connectivity loss: how many ticks to wait until we retry
// actual reconnect time is _mqtt_timer_freq * _reconnect_wait_ticks
const static int _reconnect_wait_ticks = 3;

// libmosquitto handle
static struct mosquitto *_mosquitto;

// the dispatchers event loop
static struct ev_loop *loop;
// periodic timer for housekeeping and reconnects
static struct ev_timer timer_notify;
// select() on mosquitto socket to get a callback when data arrives
static struct ev_io socket_notify;

// the kemi callback name, see mqtt_mod.c
extern str _mqtt_event_callback;

void mqtt_socket_notify(struct ev_loop *loop, struct ev_io *watcher, int revents);
void mqtt_request_notify(struct ev_loop *loop, struct ev_io *watcher, int revents);
void mqtt_timer_notify(struct ev_loop *loop, ev_timer *timer, int revents);
void mqtt_on_connect(struct mosquitto *, void *, int);
void mqtt_on_disconnect(struct mosquitto *, void *, int);
void mqtt_on_message(struct mosquitto *, void *, const struct mosquitto_message *);
int  mqtt_run_cfg_route( int rt, str *rtname, sip_msg_t *fake_message);
int  mqtt_publish(str *topic, str *payload, int qos);
int  mqtt_subscribe(str *topic, int qos);
int  mqtt_unsubscribe(str *topic);

// pointers for event routes, initialized in mqtt_init_environment()
typedef struct _mqtt_evroutes {
	int connected;
	str connected_name;
	int disconnected;
	str disconnected_name;
	int msg_received;
	str msg_received_name;
} mqtt_evroutes_t;
static mqtt_evroutes_t _mqtt_rts;

/**
 * Prepare event route pointers.
 */
void mqtt_init_environment()
{
	memset(&_mqtt_rts, 0, sizeof(mqtt_evroutes_t));

	_mqtt_rts.connected_name.s = "mqtt:connected";
	_mqtt_rts.connected_name.len = strlen(_mqtt_rts.connected_name.s);
	_mqtt_rts.connected = route_lookup(&event_rt, "mqtt:connected");
	if (_mqtt_rts.connected < 0 || event_rt.rlist[_mqtt_rts.connected] == NULL)
		_mqtt_rts.connected = -1;

	_mqtt_rts.disconnected_name.s = "mqtt:disconnected";
	_mqtt_rts.disconnected_name.len = strlen(_mqtt_rts.disconnected_name.s);
	_mqtt_rts.disconnected = route_lookup(&event_rt, "mqtt:disconnected");
	if (_mqtt_rts.disconnected < 0 || event_rt.rlist[_mqtt_rts.disconnected] == NULL)
		_mqtt_rts.disconnected = -1;

	_mqtt_rts.msg_received_name.s = "mqtt:message";
	_mqtt_rts.msg_received_name.len = strlen(_mqtt_rts.msg_received_name.s);
	_mqtt_rts.msg_received = route_lookup(&event_rt, "mqtt:message");
	if (_mqtt_rts.msg_received < 0 || event_rt.rlist[_mqtt_rts.msg_received] == NULL)
		_mqtt_rts.msg_received = -1;
}

/**
 * Create the ipc socket pair for worker->dispatcher messaging.
 */
int mqtt_init_notify_sockets(void)
{
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, _mqtt_notify_sockets) < 0) {
		LM_ERR("opening notify stream socket pair\n");
		return -1;
	}
	LM_DBG("inter-process event notification sockets initialized: %d ~ %d\n",
			_mqtt_notify_sockets[0], _mqtt_notify_sockets[1]);
	return 0;
}

/**
 * Close the sending socket.
 * Done for dispatcher process.
 */
void mqtt_close_notify_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(_mqtt_notify_sockets[1]);
	_mqtt_notify_sockets[1] = -1;
}

/**
 * Close the receiving socket
 * Done in all non-dispatcher processes.
 */
void mqtt_close_notify_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(_mqtt_notify_sockets[0]);
	_mqtt_notify_sockets[0] = -1;
}

/**
 * Main loop of the dispatcher process (blocking)
 */
int mqtt_run_dispatcher(mqtt_dispatcher_cfg_t* cfg)
{
	int res, cert_req;
	struct ev_io request_notify;

	// prepare and init libmosquitto handle
	LM_DBG("starting mqtt dispatcher processing\n");
	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) {
		LM_ERR("failed to init libmosquitto\n");
		return -1;
	}

	_mosquitto = mosquitto_new(cfg->id, true, 0);
	if (_mosquitto == 0) {
		LM_ERR("failed to allocate mosquitto struct\n");
		return -1;
	}

	if (cfg->will != NULL && cfg->will_topic != NULL) {
		LM_DBG("setting will to [%s] -> [%s]\n", cfg->will_topic, cfg->will);
		res = mosquitto_will_set(_mosquitto, cfg->will_topic, strlen(cfg->will), cfg->will, 0, false);
		if (res != MOSQ_ERR_SUCCESS) {
			LM_DBG("unable to set will: code=[%d]\n", res);
			return -1;
		}
	}

	if (cfg->username != NULL && cfg->password != NULL) {
		res = mosquitto_username_pw_set(_mosquitto, cfg->username, cfg->password);
		if (res != MOSQ_ERR_SUCCESS) {
			LM_DBG("unable to set password: code=[%d]\n", res);
			return -1;
		}
	}

	// callback for arriving messages
	mosquitto_message_callback_set(_mosquitto, mqtt_on_message);
	// callback for outgoing connections
	mosquitto_connect_callback_set(_mosquitto, mqtt_on_connect);
	// callback for connection loss
	mosquitto_disconnect_callback_set(_mosquitto, mqtt_on_disconnect);

	// prepare event loop
	loop = ev_default_loop(0);
	if(loop==NULL) {
		LM_ERR("cannot get libev loop\n");
		return -1;
	}

	// listen for data on internal ipc socket 
	ev_io_init(&request_notify, mqtt_request_notify, _mqtt_notify_sockets[0], EV_READ);
	ev_io_start(loop, &request_notify);

	// periodic timer for mqtt keepalive
	ev_timer_init(&timer_notify, mqtt_timer_notify, _mqtt_timer_freq, 0.);
	ev_timer_start(loop, &timer_notify);


	// prepare tls configuration if at least a ca is configured
	if (cfg->ca_file != NULL || cfg->ca_path != NULL) {
		LM_DBG("Preparing TLS connection");
		if (cfg->verify_certificate == 0) {
			cert_req = 0;
		} else if (cfg->verify_certificate == 1) {
			cert_req = 1;
		} else {
			LM_ERR("invalid verify_certificate parameter\n");
			return -1;
		}
		res = mosquitto_tls_opts_set(_mosquitto, cert_req, cfg->tls_method, cfg->cipher_list);
		if (res != MOSQ_ERR_SUCCESS) {
			LM_ERR("invalid tls_method or cipher_list parameters\n");
			LM_ERR("mosquitto_tls_opts_set() failed: %d %s\n",errno, strerror(errno));
			return -1;
		}
		res = mosquitto_tls_set(_mosquitto, cfg->ca_file, cfg->ca_path, cfg->certificate, cfg->private_key, NULL);
		if (res != MOSQ_ERR_SUCCESS) {
			LM_ERR("invalid ca_file, ca_path, certificate or private_key parameters\n");
			LM_ERR("mosquitto_tls_set() failed: %d %s\n",errno, strerror(errno));
			return -1;
		}
        if (cfg->tls_alpn != NULL) {
#if LIBMOSQUITTO_VERSION_NUMBER >= 1006000
            res = mosquitto_string_option(_mosquitto, MOSQ_OPT_TLS_ALPN, cfg->tls_alpn);
            if (res != MOSQ_ERR_SUCCESS) {
                LM_ERR("mosquitto_string_option() failed setting TLS ALPN: %d %s\n",errno, strerror(errno));
                return -1;
            }
#else
		    LM_WARN("unable to set TLS ALPN due to outdated mosquitto library version, upgrade it to >= 1.6.0\n")
#endif
        }
	}

	res = mosquitto_connect(_mosquitto, cfg->host, cfg->port, cfg->keepalive);
	if (res == MOSQ_ERR_INVAL) {
		LM_ERR("invalid connect parameters\n");
		return -1;
	}
	if (res == MOSQ_ERR_ERRNO) {
		// it's not a problem if the initial connection failed, 
		// we will retry periodically to reconnect
		LM_DBG("mosquitto_connect() failed: %d %s\n",errno, strerror(errno));
	}

	// the actual main loop, it drives libev 
	while(1) {
		ev_loop (loop, 0);
	}

	return 0;
}

/**
 * libev notifies us because some data is waiting on the mosquitto socket.
 */
void mqtt_socket_notify(struct ev_loop *loop, struct ev_io *watcher, int revents)
{

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	// delegate mosquitto loop to read data from sockets
	mqtt_timer_notify(loop,0, 0);

}

/**
 * Periodic mqtt housekeeping.
 */
void mqtt_timer_notify(struct ev_loop *loop, ev_timer *timer, int revents)
{
	int res;
	int recres;
	static int wait_ticks = 0;

	// spend up to 30 secs in the mosquitto loop
	// the loop will delegate work to mosquitto_on_..... callbacks.
	res = mosquitto_loop(_mosquitto, 30, 1);
	switch (res) {
		case MOSQ_ERR_SUCCESS:
			break;
		case MOSQ_ERR_ERRNO:
			LM_ERR("mosquitto_loop() failed: %s\n", strerror(errno));
			break;
		case MOSQ_ERR_NO_CONN:
		case MOSQ_ERR_CONN_LOST:
			// is it time to reconnect?
			if (wait_ticks > 0) {
				// not yet...
				wait_ticks --;
				break;
			}
			LM_DBG("Reconnecting\n");
			recres = mosquitto_reconnect(_mosquitto);
			if (recres != MOSQ_ERR_SUCCESS) {
				LM_ERR("mosquitto_reconnect() failed: %d\n", recres);
				// let's wait again N ticks
				wait_ticks = _reconnect_wait_ticks;
			}
			break;
		case MOSQ_ERR_TLS:
			LM_ERR("mosquitto_loop() failed, tls error\n");
			break;
		default:
			LM_ERR("mosquitto_loop() failed: case %i\n", res);
			break;
	}

	if (timer != 0) {
		// be sure to keep the timer going.
		timer->repeat = _mqtt_timer_freq;
		ev_timer_again (loop, timer);
	}

}

/**
 * libmosquitto established a connection.
 */
void mqtt_on_connect(struct mosquitto *mosquitto, void *userdata, int rc)
{
	int mosquitto_fd;
	if (rc == 0) {
		LM_DBG("mqtt connected\n");

		// listen for incoming data on mqtt connection
		mosquitto_fd = mosquitto_socket(_mosquitto);
		// init/refresh the libev notifier
		ev_io_init(&socket_notify, mqtt_socket_notify, mosquitto_fd, EV_READ);
		ev_io_start(loop, &socket_notify);

		// tell the script about the connection
		mqtt_run_cfg_route(_mqtt_rts.connected, &_mqtt_rts.connected_name, 0);
	} else {
		LM_DBG("mqtt connect error [%i]\n", rc);
	}
}

/**
 * libmosquitto lost connection
 */
void mqtt_on_disconnect(struct mosquitto *mosquitto, void *userdata, int rc)
{
	LM_DBG("mqtt disconnected [rc %i]\n", rc);
	// the mosquitto read socket is invalid now, so detach libev
	ev_io_stop(loop, &socket_notify);
	// tell the script about the disconnection
	mqtt_run_cfg_route(_mqtt_rts.disconnected, &_mqtt_rts.disconnected_name, 0);
}

/**
 * libmosquitto received a messag
 */
void mqtt_on_message(struct mosquitto *mosquitto, void *userdata, const struct mosquitto_message *message)
{
	sip_msg_t *fmsg;
	sip_msg_t tmsg;

	str topic, payload;
	int qos;
	topic.s = message->topic;
	topic.len = strlen(message->topic);
	payload.s = (char*) message->payload;
	payload.len = message->payloadlen;
	qos = message->qos;
	LM_DBG("mqtt message [%s] -> [%s] (qos %d)\n", topic.s, payload.s, qos);

	cfg_update();

	fmsg = faked_msg_next();
	memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
	fmsg = &tmsg;
	// use hdr date as pointer for the mqtt-message, not used in faked msg
	fmsg->date=(hdr_field_t*)message;
	mqtt_run_cfg_route(_mqtt_rts.msg_received, &_mqtt_rts.msg_received_name, fmsg);
}

/**
 * Invoke a event route block
 */
int mqtt_run_cfg_route(int rt, str *rtname, sip_msg_t *fake_msg)
{
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;
	sr_kemi_eng_t *keng = NULL;

	// check for valid route pointer
	if((rt<0) && (_mqtt_event_callback.s==NULL || _mqtt_event_callback.len<=0))
		return 0;

	// create empty fake message, if needed
	if (fake_msg == NULL) {
		fmsg = faked_msg_next();
		memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
		fmsg = &tmsg;
	} else {
		fmsg = fake_msg;
	}
	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	LM_DBG("Run route [%.*s] [%s]\n", rtname->len, rtname->s, my_desc());
	if(rt>=0) {
		run_top_route(event_rt.rlist[rt], fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng!=NULL) {
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE,
						&_mqtt_event_callback, rtname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	set_route_type(backup_rt);
	return 0;
}

/**
 * prepare $mqtt pv call
 */
int pv_parse_mqtt_name(pv_spec_t *sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "msg", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "qos", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "topic", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV msrp name %.*s\n", in->len, in->s);
	return -1;
}

/**
 * Populate $mqtt pv
 */
int pv_get_mqtt(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	struct mosquitto_message* message;
	str topic, payload;
	int qos;

	if(param==NULL || res==NULL)
		return -1;


	// check fake message date hdr, it should point to a mosquitto message
	message = (struct mosquitto_message*)msg->date;

	if (message==NULL) {
		return pv_get_null(msg, param, res);
	} else {
		topic.s = message->topic;
		topic.len = strlen(message->topic);
		payload.s = (char*) message->payload;
		payload.len = message->payloadlen;
		qos = message->qos;
	}

	// populate value depeding on the param name
	// see pv_parse_mqtt_name()
	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			return pv_get_strval(msg, param, res, &topic);
		case 1:
			return pv_get_strval(msg, param, res, &payload);
		case 2:
			return pv_get_sintval(msg, param, res, qos);
		default:
			return pv_get_null(msg, param, res);
	}

	return 0;
}

/**
 * The pv $mqtt is read only, nothing to do here.
 */
int pv_set_mqtt(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val)
{
	return 0;
}

/**
 *
 */
int mqtt_prepare_publish(str *topic, str *payload, int qos)
{
	int len;
	mqtt_request_t *request;

	if(topic->s==NULL || topic->len == 0) {
		LM_ERR("invalid topic parameter\n");
		return -1;
	}
	if(payload->s==NULL || payload->len == 0) {
		LM_ERR("invalid payload parameter\n");
		return -1;
	}
	if(qos<0 || qos>2) {
		LM_ERR("invalid qos level\n");
		return -1;
	}

	LM_DBG("publishing [%.*s] -> [%.*s]\n",
			topic->len, topic->s, payload->len, payload->s);

	len = sizeof(mqtt_request_t);
	len += topic->len+16;
	len += payload->len+16;

	request = (mqtt_request_t*)shm_malloc(len);
	if(request==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(request, 0, len);
	request->type = PUBLISH;
	request->qos = qos;
	request->topic.s = (char*)request + sizeof(mqtt_request_t);
	request->topic.len = snprintf(request->topic.s, topic->len+16, 
				"%.*s",
				topic->len, topic->s);
	request->payload.s = request->topic.s + topic->len+16;
	request->payload.len = snprintf(request->payload.s, payload->len+16, 
				"%.*s",
				payload->len, payload->s);

	if(_mqtt_notify_sockets[1]!=-1) {
		len = write(_mqtt_notify_sockets[1], &request, sizeof(mqtt_request_t*));
		if(len<=0) {
			shm_free(request);
			LM_ERR("failed to pass the pointer to mqtt dispatcher\n");
			return -1;
		}
	} else {
		cfg_update();
		mqtt_publish(topic, payload, qos);
		shm_free(request);
	}

	return 0;
}

/**
 * 
 */
void mqtt_request_notify(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	mqtt_request_t *request = NULL;
	int rlen;

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	cfg_update();

	/* read message from client */
	rlen = read(watcher->fd, &request, sizeof(mqtt_request_t*));

	if(rlen != sizeof(mqtt_request_t*) || request==NULL) {
		LM_ERR("cannot read the sip worker message\n");
		return;
	}

	LM_DBG("received [%p] [%i] [%.*s]\n", request,
			request->type, request->topic.len, request->topic.s);
	switch(request->type) {
		case PUBLISH:
			mqtt_publish(&request->topic, &request->payload, request->qos);
			break;
		case SUBSCRIBE:
			mqtt_subscribe(&request->topic, request->qos);
			break;
		case UNSUBSCRIBE:
			mqtt_unsubscribe(&request->topic);
			break;
		default:
			LM_ERR("unknown request [%d] from sip worker\n", request->type);
	}
	shm_free(request);
}

/**
 * 
 */
int  mqtt_publish(str *topic, str *payload,int qos) {
	int res;

	LM_DBG("publish [%s] %s -> %s (%d)\n", my_desc(), topic->s, payload->s, payload->len);
	res = mosquitto_publish(_mosquitto, NULL, topic->s, payload->len, payload->s, qos, false);
	if (res != MOSQ_ERR_SUCCESS) {
		LM_WARN("unable to publish [%s] -> [%s], rc=%d\n", topic->s, payload->s, res);
		return -1;
	}
	return 0;
}

/**
 * 
 */
int mqtt_prepare_subscribe(str *topic, int qos)
{
	int len;
	mqtt_request_t *request;

	if(topic->s==NULL || topic->len == 0) {
		LM_ERR("invalid topic parameter\n");
		return -1;
	}

	if(qos<0 || qos>2) {
		LM_ERR("invalid qos level\n");
		return -1;
	}

	LM_DBG("prepare subscribe [%s] [%.*s]\n", my_desc(), topic->len, topic->s);
	len = sizeof(mqtt_request_t);
	len += topic->len+16;

	request = (mqtt_request_t*)shm_malloc(len);
	if(request==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(request, 0, len);
	request->type = SUBSCRIBE;
	request->qos = qos;
	request->topic.s = (char*)request + sizeof(mqtt_request_t);
	request->topic.len = snprintf(request->topic.s, topic->len+16, 
				"%.*s",
				topic->len, topic->s);

	if(_mqtt_notify_sockets[1]!=-1) {
		len = write(_mqtt_notify_sockets[1], &request, sizeof(mqtt_request_t*));
		if(len<=0) {
			shm_free(request);
			LM_ERR("failed to pass the pointer to mqtt dispatcher\n");
			return -1;
		}
	} else {
		cfg_update();
		mqtt_subscribe(topic, qos);
		shm_free(request);
	}

	return 0;

}

/**
 * 
 */
int mqtt_subscribe(str *topic, int qos) {
	int res;

	LM_DBG("subscribe [%s] %s\n", my_desc(), topic->s);
	res = mosquitto_subscribe(_mosquitto, NULL, topic->s,  qos);
	if (res != MOSQ_ERR_SUCCESS) {
		LM_WARN("unable to subscribe [%s], rc=%d\n", topic->s, res);
		return -1;
	}
	return 0;
}

/**
 * 
 */
int mqtt_prepare_unsubscribe(str *topic)
{
	int len;
	mqtt_request_t *request;

	if(topic->s==NULL || topic->len == 0) {
		LM_ERR("invalid topic parameter\n");
		return -1;
	}


	LM_DBG("prepare unsubscribe [%s] [%.*s]\n", my_desc(), topic->len, topic->s);
	len = sizeof(mqtt_request_t);
	len += topic->len+16;

	request = (mqtt_request_t*)shm_malloc(len);
	if(request==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(request, 0, len);
	request->type = UNSUBSCRIBE;
	request->topic.s = (char*)request + sizeof(mqtt_request_t);
	request->topic.len = snprintf(request->topic.s, topic->len+16, 
				"%.*s",
				topic->len, topic->s);

	if(_mqtt_notify_sockets[1]!=-1) {
		len = write(_mqtt_notify_sockets[1], &request, sizeof(mqtt_request_t*));
		if(len<=0) {
			shm_free(request);
			LM_ERR("failed to pass the pointer to mqtt dispatcher\n");
			return -1;
		}
	} else {
		cfg_update();
		mqtt_unsubscribe(topic);
		shm_free(request);
	}

	return 0;

}

/**
 * 
 */
int mqtt_unsubscribe(str *topic) {
	int res;

	LM_DBG("unsubscribe %s\n", topic->s);
	res = mosquitto_unsubscribe(_mosquitto, NULL, topic->s);
	if (res != MOSQ_ERR_SUCCESS) {
		LM_WARN("unable to subscribe [%s], rc=%d\n", topic->s, res);
		return -1;
	}
	return 0;
}
