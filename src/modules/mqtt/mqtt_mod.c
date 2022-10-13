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


#include "../../core/sr_module.h"
#include "../../core/fmsg.h"
#include "../../core/mod_fix.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/kemi.h"

#include "mqtt_dispatch.h"

MODULE_VERSION

static char *_mqtt_host               = NULL;
static int   _mqtt_port               = 1883;
static char *_mqtt_id                 = NULL;
static char *_mqtt_username           = NULL;
static char *_mqtt_password           = NULL;
static char *_mqtt_will               = NULL;
static char *_mqtt_willtopic          = NULL;
static int   _mqtt_keepalive          = 5;
static char *_mqtt_ca_file            = NULL;
static char *_mqtt_ca_path            = NULL;
static char *_mqtt_certificate        = NULL;
static char *_mqtt_private_key        = NULL;
static char *_mqtt_tls_method         = NULL;
static char *_mqtt_tls_alpn           = NULL;
static int   _mqtt_verify_certificate = 1;
static char *_mqtt_cipher_list        = NULL;

str _mqtt_event_callback = STR_NULL;
int _mqtt_dispatcher_pid = -1;

static int mod_init(void);
static int  child_init(int);

static int cmd_mqtt_publish(sip_msg_t* msg, char* topic, char* payload, char* qos);
static int fixup_mqtt_publish(void** param, int param_no);
static int ki_mqtt_publish(sip_msg_t* msg, str* topic, str* payload, int qos);

static int cmd_mqtt_subscribe(sip_msg_t* msg, char* topic, char* qos);
static int fixup_mqtt_subscribe(void** param, int param_no);
static int ki_mqtt_subscribe(sip_msg_t* msg, str* topic, int qos);

static int cmd_mqtt_unsubscribe(sip_msg_t* msg, char* topic);
static int ki_mqtt_unsubscribe(sip_msg_t* msg, str* topic);

static cmd_export_t cmds[]={
	{"mqtt_publish",			(cmd_function)cmd_mqtt_publish,		3, fixup_mqtt_publish,
		0, ANY_ROUTE},
	{"mqtt_subscribe",			(cmd_function)cmd_mqtt_subscribe,		2, fixup_mqtt_subscribe,
		0, ANY_ROUTE},
	{"mqtt_unsubscribe",			(cmd_function)cmd_mqtt_unsubscribe,		1, fixup_spve_all,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"host",               PARAM_STRING,   &_mqtt_host},
	{"port",               INT_PARAM,      &_mqtt_port},
	{"id",                 PARAM_STRING,   &_mqtt_id},
	{"username",           PARAM_STRING,   &_mqtt_username},
	{"password",           PARAM_STRING,   &_mqtt_password},
	{"will_topic",         PARAM_STRING,   &_mqtt_willtopic},
	{"will",               PARAM_STRING,   &_mqtt_will},
	{"keepalive",          INT_PARAM,      &_mqtt_keepalive},
	{"event_callback",     PARAM_STR,      &_mqtt_event_callback},
	{"tls_method",         PARAM_STRING,   &_mqtt_tls_method},
	{"tls_alpn",           PARAM_STRING,   &_mqtt_tls_alpn},
	{"ca_file",            PARAM_STRING,   &_mqtt_ca_file},
	{"ca_path",            PARAM_STRING,   &_mqtt_ca_path},
	{"certificate",        PARAM_STRING,   &_mqtt_certificate},
	{"private_key",        PARAM_STRING,   &_mqtt_private_key},
	{"verify_certificate", INT_PARAM,      &_mqtt_verify_certificate},
	{"cipher_list",        PARAM_STRING,   &_mqtt_cipher_list},
	{0,0,0}
};

static pv_export_t mod_pvs[] = {
	{ {"mqtt", (sizeof("mqtt")-1)}, PVT_OTHER, pv_get_mqtt,
		pv_set_mqtt, pv_parse_mqtt_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"mqtt",		 		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	mod_pvs,			/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	0        			/* module destroy function */
};

/*!
 * \brief Module initialization function
 * \return 0 on success, -1 on failure
 */
static int mod_init(void)
{
	if(faked_msg_init()<0) {
		LM_ERR("failed to init faked sip message\n");
		return -1;
	}

	if (_mqtt_host == NULL) {
		LM_ERR("MQTT host parameter not set\n");
		return -1;
	}

	/* add space for mqtt dispatcher */
	register_procs(1 );

	/* add child to update local config framework structures */
	cfg_register_child(1);

	/* prepare some things for all processes */
	mqtt_init_environment();

	return 0;
}


/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int pid;

	if (rank==PROC_INIT) {
		if(mqtt_init_notify_sockets()<0) {
			LM_ERR("failed to initialize notify sockets\n");
			return -1;
		}
		return 0;
	}

	if (rank!=PROC_MAIN) {
		if(_mqtt_dispatcher_pid!=getpid()) {
			mqtt_close_notify_sockets_parent();
		}
		return 0;
	}

	pid=fork_process(PROC_NOCHLDINIT, "mqtt dispatcher", 1);
	if (pid<0)
		return -1; /* error */
	if(pid==0) {
		/* child */
		_mqtt_dispatcher_pid = getpid();

		/* do child init to allow execution of rpc like functions */
		if(init_child(PROC_RPC) < 0) {
			LM_DBG("failed to do RPC child init for dispatcher\n");
			return -1;
		}
		/* initialize the config framework */
		if (cfg_child_init())
			return -1;
		/* main function for dispatcher */
		mqtt_close_notify_sockets_child();

		/* module parameter hand over to dispatcher */
		mqtt_dispatcher_cfg_t cfg;
		cfg.host               = _mqtt_host;
		cfg.port               = _mqtt_port;
		cfg.id                 = _mqtt_id;
		cfg.username           = _mqtt_username;
		cfg.password           = _mqtt_password;
		cfg.keepalive          = _mqtt_keepalive;
		cfg.will               = _mqtt_will;
		cfg.will_topic         = _mqtt_willtopic;
		cfg.ca_file            = _mqtt_ca_file;
		cfg.ca_path            = _mqtt_ca_path;
		cfg.certificate        = _mqtt_certificate;
		cfg.private_key        = _mqtt_private_key;
		cfg.tls_method         = _mqtt_tls_method;
		cfg.tls_alpn           = _mqtt_tls_alpn;
		cfg.verify_certificate = _mqtt_verify_certificate;
		cfg.cipher_list        = _mqtt_cipher_list;

		/* this process becomes the dispatcher, block now */
		return mqtt_run_dispatcher(&cfg);
	}

	return 0;
}

/**
 * Send out a message to a topic with a specified mqtt qos level (0, 1, 2).
 * Used in cfg script.
 */
static int cmd_mqtt_publish(sip_msg_t* msg, char* topic, char* payload, char* qos)
{
	str stopic;
	str spayload;
	unsigned int iqos=0;

	if(topic==0) {
		LM_ERR("invalid topic\n");
		return -1;
	}

	if(payload==0) {
		LM_ERR("invalid payload\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)topic, &stopic)!=0) {
		LM_ERR("unable to get topic\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)payload, &spayload)!=0) {
		LM_ERR("unable to get payload\n");
		return -1;
	}

	iqos = (unsigned int)(unsigned long)qos;

	// pass the request to the dispatcher
	if(mqtt_prepare_publish(&stopic, &spayload, iqos)<0) {
		LM_ERR("failed to prepare publish: [%.*s] -> [%.*s] (qos=%d)\n", stopic.len, stopic.s, spayload.len, spayload.s, iqos);
		return -1;
	}

	return 1;
}

/**
 * Send out a message to a topic with a specified mqtt qos level (0, 1, 2).
 * Used in kemi script.
 */
static int ki_mqtt_publish(sip_msg_t* msg, str* topic, str* payload, int qos)
{
	int ret;

	ret = mqtt_prepare_publish(topic, payload, qos);
	if (ret<0) return ret;

	return (ret+1);
}

/**
 * 
 */
static int fixup_mqtt_publish(void** param, int param_no)
{
	switch (param_no) {
		case 1:
			return fixup_spve_spve(param, param_no);
		case 2:
			return fixup_spve_spve(param, param_no);
		case 3:
			return fixup_uint_uint(param, param_no);
		default:
			return -1;
	}
}

/**
 * Subscribe to the given topic. 
 * Mqtt qos levels 0, 1 and 2 can be used.
 * Used in cfg script.
 */
static int cmd_mqtt_subscribe(sip_msg_t* msg, char* topic, char* qos)
{
	str stopic;
	unsigned int iqos=0;

	if(topic==0) {
		LM_ERR("invalid topic\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)topic, &stopic)!=0) {
		LM_ERR("unable to get topic\n");
		return -1;
	}
	
	iqos = (unsigned int)(unsigned long)qos;

	// pass the request to the dispatcher
	if(mqtt_prepare_subscribe(&stopic, iqos)<0) {
		LM_ERR("failed to prepare subscribe: [%.*s] (qos=%d)\n", stopic.len, stopic.s, iqos);
		return -1;
	}
	
	return 1;
}

/**
 * Subscribe to the given topic. 
 * Mqtt qos levels 0, 1 and 2 can be used.
 * Used in kemi script.
 */
static int ki_mqtt_subscribe(sip_msg_t* msg, str* topic, int qos)
{
	int ret;

	ret = mqtt_prepare_subscribe(topic, qos);

	if(ret<0) return ret;

	return (ret+1);
}

/**
 * 
 */
static int fixup_mqtt_subscribe(void** param, int param_no)
{
	switch (param_no) {
		case 1:
			return fixup_spve_spve(param, param_no);
		case 2:
			return fixup_uint_uint(param, param_no);
		default:
			return -1;
	}
}

/**
 * Unsubscribe to a previously subscribed topic.
 * Used in cfg script.
 */
static int cmd_mqtt_unsubscribe(sip_msg_t* msg, char* topic)
{
	str stopic;

	if(topic==0) {
		LM_ERR("invalid topic\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)topic, &stopic)!=0) {
		LM_ERR("unable to get topic\n");
		return -1;
	}
	// pass the request to the dispatcher
	if(mqtt_prepare_unsubscribe(&stopic)<0) {
		LM_ERR("failed to prepare unsubscribe: [%.*s]\n", stopic.len, stopic.s);
		return -1;
	}

	return 1;
}

/**
 * Unsubscribe to a previously subscribed topic.
 * Used in kemi script.
 */
static int ki_mqtt_unsubscribe(sip_msg_t* msg, str* topic)
{
	int ret;

	ret = mqtt_prepare_unsubscribe(topic);

	if(ret<0) return ret;

	return (ret+1);
}
/**
 * Define kemi compatible commands.
 */
/* clang-format off */
static sr_kemi_t sr_kemi_corex_exports[] = {
	{ str_init("mqtt"), str_init("publish"),
		SR_KEMIP_INT, ki_mqtt_publish,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqtt"), str_init("subscribe"),
		SR_KEMIP_INT, ki_mqtt_subscribe,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("mqtt"), str_init("unsubscribe"),
		SR_KEMIP_INT, ki_mqtt_unsubscribe,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 * Register in kemi framework
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_corex_exports);
	return 0;
}
