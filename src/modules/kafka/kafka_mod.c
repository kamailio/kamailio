/*
 * Copyright (C) 2019 Vicente Hernando (Sonoc https://www.sonoc.io)
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
 *
 */

/**
 * \file
 * \brief Kafka :: Module Core
 * \ingroup kfk
 *
 * - Module: \ref kfk
 */

/**
 * \defgroup kfk Kafka :: Kafka module for Kamailio
 *
 * This module contains functions related to Apache Kafka initialization and closing,
 * as well as the module interface.
 * It uses librdkafka library.
 * Currently it only provides producer capabilites.
 */

/* Headers */
#include <inttypes.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "kfk.h"

MODULE_VERSION

/* Declaration of static variables and functions. */

static rpc_export_t rpc_cmds[];
static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);
static int fixup_kafka_send(void **param, int param_no);
static int w_kafka_send(struct sip_msg *msg, char *ptopic, char *pmessage);
static int w_kafka_send_key(
		struct sip_msg *msg, char *ptopic, char *pmessage, char *pkey);

/*
 * Variables and functions to deal with module parameters.
 */
char *brokers_param = NULL; /**< List of brokers. */
static int kafka_conf_param(modparam_t type, void *val);
static int kafka_topic_param(modparam_t type, void *val);

/**
 * \brief Module commands
 */
static cmd_export_t cmds[] = {{"kafka_send", (cmd_function)w_kafka_send, 2,
									  fixup_kafka_send, 0, ANY_ROUTE},
		{"kafka_send_key", (cmd_function)w_kafka_send_key, 3, fixup_kafka_send,
				0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

/**
 * \brief Structure for module parameters.
 */
static param_export_t params[] = {{"brokers", PARAM_STRING, &brokers_param},
		{"configuration", PARAM_STRING | USE_FUNC_PARAM,
				(void *)kafka_conf_param},
		{"topic", PARAM_STRING | USE_FUNC_PARAM, (void *)kafka_topic_param},
		{0, 0, 0}};

/**
 * \brief Kafka :: Module interface
 */
struct module_exports exports = {
		"kafka", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, params, 0,		  /* exported RPC methods */
		0,						  /* exported pseudo-variables */
		0,						  /* response function */
		mod_init,				  /* module initialization function */
		child_init,				  /* per child init function */
		mod_destroy				  /* destroy function */
};

static int mod_init(void)
{
	/* Register RPC commands. */
	if(rpc_register_array(rpc_cmds) != 0) {
		LM_ERR("Failed to register RPC commands\n");
		return -1;
	}

	/* Initialize statistics. */
	if(kfk_stats_init()) {
		LM_ERR("Failed to initialize statistics\n");
		return -1;
	}

	return 0;
}

static int child_init(int rank)
{
	/* skip child init for non-worker process ranks */
	/* if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN) */
	/* We execute kfk_init in PROC_MAIN so it cleans messages, etc right 
	   when destroying the module. */
	if(rank == PROC_INIT || rank == PROC_TCP_MAIN)
		return 0;

	if(kfk_init(brokers_param)) {
		LM_ERR("Failed to initialize Kafka\n");
		return -1;
	}
	return 0;
}

static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");

	kfk_close();

	kfk_stats_close();
}

/**
 * \brief Parse configuration parameter.
 */
static int kafka_conf_param(modparam_t type, void *val)
{
	return kfk_conf_parse((char *)val);
}

/**
 * \brief Parse topic parameter.
 */
static int kafka_topic_param(modparam_t type, void *val)
{
	return kfk_topic_parse((char *)val);
}

static int fixup_kafka_send(void **param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/**
 * \brief Send a message via Kafka
 */
static int w_kafka_send(struct sip_msg *msg, char *ptopic, char *pmessage)
{
	str s_topic;

	if(ptopic == NULL) {
		LM_ERR("Invalid topic parameter\n");
		return -1;
	}

	if(get_str_fparam(&s_topic, msg, (gparam_t *)ptopic) != 0) {
		LM_ERR("No topic\n");
		return -1;
	}
	if(s_topic.s == NULL || s_topic.len == 0) {
		LM_ERR("Invalid topic string\n");
		return -1;
	}

	str s_message;

	if(pmessage == NULL) {
		LM_ERR("Invalid message parameter\n");
		return -1;
	}

	if(get_str_fparam(&s_message, msg, (gparam_t *)pmessage) != 0) {
		LM_ERR("No message\n");
		return -1;
	}
	if(s_message.s == NULL || s_message.len == 0) {
		LM_ERR("Invalid message string\n");
		return -1;
	}

	if(kfk_message_send(&s_topic, &s_message, NULL)) {
		LM_ERR("Cannot send kafka (topic: %.*s) message: %.*s\n", s_topic.len,
				s_topic.s, s_message.len, s_message.s);
		return -1;
	}

	LM_DBG("Message sent (Topic: %.*s) : %.*s\n", s_topic.len, s_topic.s,
			s_message.len, s_message.s);
	return 1;
}

/**
 * \brief Send a message via Kafka plus key parameter.
 */
static int w_kafka_send_key(
		struct sip_msg *msg, char *ptopic, char *pmessage, char *pkey)
{
	str s_topic;

	if(ptopic == NULL) {
		LM_ERR("Invalid topic parameter\n");
		return -1;
	}

	if(get_str_fparam(&s_topic, msg, (gparam_t *)ptopic) != 0) {
		LM_ERR("No topic\n");
		return -1;
	}
	if(s_topic.s == NULL || s_topic.len == 0) {
		LM_ERR("Invalid topic string\n");
		return -1;
	}

	str s_message;

	if(pmessage == NULL) {
		LM_ERR("Invalid message parameter\n");
		return -1;
	}

	if(get_str_fparam(&s_message, msg, (gparam_t *)pmessage) != 0) {
		LM_ERR("No message\n");
		return -1;
	}
	if(s_message.s == NULL || s_message.len == 0) {
		LM_ERR("Invalid message string\n");
		return -1;
	}

	str s_key;

	if(pkey == NULL) {
		LM_ERR("Invalid key parameter\n");
		return -1;
	}

	if(get_str_fparam(&s_key, msg, (gparam_t *)pkey) != 0) {
		LM_ERR("No key\n");
		return -1;
	}
	if(s_key.s == NULL || s_key.len == 0) {
		LM_ERR("Invalid key string\n");
		return -1;
	}

	if(kfk_message_send(&s_topic, &s_message, &s_key)) {
		LM_ERR("Cannot send kafka (topic: %.*s) (key: %.*s) message: %.*s\n",
				s_topic.len, s_topic.s, s_key.len, s_key.s, s_message.len,
				s_message.s);
		return -1;
	}

	LM_DBG("Message key sent (Topic: %.*s) (key: %.*s) : %.*s\n", s_topic.len,
			s_topic.s, s_key.len, s_key.s, s_message.len, s_message.s);
	return 1;
}

/**
 * \brief KEMI function to send a Kafka message.
 */
static int ki_kafka_send(struct sip_msg *msg, str *s_topic, str *s_message)
{
	if(s_topic == NULL || s_topic->s == NULL || s_topic->len == 0) {
		LM_ERR("Invalid topic string\n");
		return -1;
	}

	if(s_message == NULL || s_message->s == NULL || s_message->len == 0) {
		LM_ERR("Invalid message string\n");
		return -1;
	}

	if(kfk_message_send(s_topic, s_message, NULL)) {
		LM_ERR("Cannot send kafka (topic: %.*s) message: %.*s\n", s_topic->len,
				s_topic->s, s_message->len, s_message->s);
		return -1;
	}

	LM_DBG("Message sent (Topic: %.*s) : %.*s\n", s_topic->len, s_topic->s,
			s_message->len, s_message->s);
	return 1;
}

/**
 * \brief KEMI function to send a Kafka message plus key.
 */
static int ki_kafka_send_key(
		struct sip_msg *msg, str *s_topic, str *s_message, str *s_key)
{
	if(s_topic == NULL || s_topic->s == NULL || s_topic->len == 0) {
		LM_ERR("Invalid topic string\n");
		return -1;
	}

	if(s_message == NULL || s_message->s == NULL || s_message->len == 0) {
		LM_ERR("Invalid message string\n");
		return -1;
	}

	if(s_key == NULL || s_key->s == NULL || s_key->len == 0) {
		LM_ERR("Invalid key string\n");
		return -1;
	}

	if(kfk_message_send(s_topic, s_message, s_key)) {
		LM_ERR("Cannot send kafka (topic: %.*s) (key: %.*s) message: %.*s\n",
				s_topic->len, s_topic->s, s_key->len, s_key->s, s_message->len,
				s_message->s);
		return -1;
	}

	LM_DBG("Message sent (Topic: %.*s) (key: %.*s) : %.*s\n", s_topic->len,
			s_topic->s, s_key->len, s_key->s, s_message->len, s_message->s);
	return 1;
}

/**
 * \brief Kafka :: Array with KEMI functions
 */
/* clang-format off */
static sr_kemi_t sr_kemi_kafka_exports[] = {
	{ str_init("kafka"), str_init("send"),
	  SR_KEMIP_INT, ki_kafka_send,
	  { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
		SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kafka"), str_init("send_key"),
	  SR_KEMIP_INT, ki_kafka_send_key,
	  { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
		SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 * \brief Kafka :: register Kafka module
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_kafka_exports);
	return 0;
}

static void rpc_kafka_stats(rpc_t *rpc, void *ctx)
{
	uint64_t msg_total = 0;
	uint64_t msg_error = 0;

	if(kfk_stats_get(&msg_total, &msg_error)) {
		LM_ERR("Failed to get total statistics\n");
		rpc->fault(ctx, 500, "Failed to get total statistics");
		return;
	}

	LM_DBG("Total messages: %" PRIu64 "  Errors: %" PRIu64 "\n", msg_total,
			msg_error);
	if(rpc->rpl_printf(ctx, "Total messages: %" PRIu64 "  Errors: %" PRIu64,
			   msg_total, msg_error)
			< 0) {
		rpc->fault(ctx, 500, "Internal error showing total statistics");
		return;
	}
}

static void rpc_kafka_stats_topic(rpc_t *rpc, void *ctx)
{
	str s_topic;

	if(rpc->scan(ctx, "S", &s_topic) < 1) {
		rpc->fault(ctx, 400, "required topic string");
		return;
	}

	if(s_topic.len == 0 || s_topic.s == NULL) {
		LM_ERR("Bad topic name\n");
		rpc->fault(ctx, 400, "Bad topic name");
		return;
	}

	uint64_t msg_total = 0;
	uint64_t msg_error = 0;

	if(kfk_stats_topic_get(&s_topic, &msg_total, &msg_error)) {
		LM_ERR("Failed to get statistics for topic: %.*s\n", s_topic.len,
				s_topic.s);
		rpc->fault(ctx, 500, "Failed to get per topic statistics");
		return;
	}

	LM_DBG("Topic: %.*s   messages: %" PRIu64 "  Errors: %" PRIu64 "\n",
			s_topic.len, s_topic.s, msg_total, msg_error);
	if(rpc->rpl_printf(ctx,
			   "Topic: %.*s  Total messages: %" PRIu64 "  Errors: %" PRIu64,
			   s_topic.len, s_topic.s, msg_total, msg_error)
			< 0) {
		rpc->fault(ctx, 500,
				"Internal error showing statistics for topic: %.*s",
				s_topic.len, s_topic.s);
		return;
	}
}

static const char *rpc_kafka_stats_doc[2] = {
		"Print general topic independent statistics", 0};

static const char *rpc_kafka_stats_topic_doc[2] = {
		"Print statistics based on topic", 0};

static rpc_export_t rpc_cmds[] = {
		{"kafka.stats", rpc_kafka_stats, rpc_kafka_stats_doc, 0},
		{"kafka.stats_topic", rpc_kafka_stats_topic, rpc_kafka_stats_topic_doc,
				0},
		{0, 0, 0, 0}};
