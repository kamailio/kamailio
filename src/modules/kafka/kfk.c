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
 * \brief Kafka :: Apache Kafka functions via librdkafka
 * \ingroup kfk
 *
 * - Module: \ref kfk
 */

#include <syslog.h> /* For log levels. */
#include <librdkafka/rdkafka.h>

#include "../../core/dprint.h"
#include "../../core/parser/parse_param.h"
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"

/**
 * \brief data type for a configuration property.
 */
typedef struct kfk_conf_node_s {
	str *sname; /**< name of property */
	str *svalue; /**< value of property */
	struct kfk_conf_node_s *next; /**< next property in list */
} kfk_conf_node_t;

/**
 * \brief list of configuration properties.
 */
typedef struct kfk_conf_s {
	param_t *attrs; /**< parsed attributes from configuration parameter. */
	char *spec; /**< original string of configuration. */
	kfk_conf_node_t *property; /**< list of configuration properties. */
} kfk_conf_t;

/**
 * \brief data type for a topic.
 *
 * This is an element in a topic list.
 */
typedef struct kfk_topic_s {
	str *topic_name; /**< Name of the topic. */
	rd_kafka_topic_t *rd_topic; /**< rd kafkfa topic structure. */
	param_t *attrs; /**< parsed attributes for topic configuration. */
	char *spec; /**< original string for topic configuration. */
	kfk_conf_node_t *property; /**< list of configuration properties for a topic. */
	struct kfk_topic_s *next; /**< Next element in topic list. */
} kfk_topic_t;

/**
 * \brief stats about a topic.
 */
typedef struct kfk_stats_s {
	str *topic_name; /**< Name of the topic, or NULL for general statistics. */
	uint64_t total; /**< Total number of messages sent. */
	uint64_t error; /**< Number of failed messages to sent. */
	struct kfk_stats_s *next; /**< Next element in stats list. */
} kfk_stats_t;

/* Static variables. */
static rd_kafka_conf_t *rk_conf = NULL;  /* Configuration object */
static rd_kafka_t *rk = NULL; /* Producer instance handle */
static kfk_conf_t *kfk_conf = NULL; /* List for Kafka configuration properties. */
static kfk_topic_t *kfk_topic = NULL; /* List for Kafka topics. */

#define ERRSTR_LEN 512 /**< Length of internal buffer for errors. */
static char errstr[ERRSTR_LEN]; /* librdkafka API error reporting buffer */
gen_lock_t *stats_lock = NULL; /**< Lock to protect shared statistics data. */

/**
 * \brief Total statistics
 *
 * First node (mandatory) is the general one with NULL topic.
 * Next nodes are topic dependant ones and are optional.
 * This way because general node is created in kfk_stats_init in mod_init is
 * shared among every Kamailio process.
 */
static kfk_stats_t *stats_general;

/* Static functions. */
static void kfk_conf_free(kfk_conf_t *kconf);
static void kfk_topic_free(kfk_topic_t *ktopic);
static int kfk_conf_configure();
static int kfk_topic_list_configure();
static int kfk_topic_exist(str *topic_name);
static rd_kafka_topic_t* kfk_topic_get(str *topic_name);
static int kfk_stats_add(const char *topic, rd_kafka_resp_err_t err);
static void kfk_stats_topic_free(kfk_stats_t *st_topic);

/**
 * \brief Kafka logger callback
 */
static void kfk_logger (const rd_kafka_t *rk, int level,
		    const char *fac, const char *buf) {

	switch(level) {
		case LOG_EMERG:
			LM_NPRL("RDKAFKA fac: %s : %s : %s\n",
					fac, rk ? rd_kafka_name(rk) : NULL,
					buf);
			break;
			
		case LOG_ALERT:
			LM_ALERT("RDKAFKA fac: %s : %s : %s\n",
					 fac, rk ? rd_kafka_name(rk) : NULL,
					 buf);
			break;
			
		case LOG_CRIT:
			LM_CRIT("RDKAFKA fac: %s : %s : %s\n",
					fac, rk ? rd_kafka_name(rk) : NULL,
					buf);
			break;

		case LOG_ERR:
			LM_ERR("RDKAFKA fac: %s : %s : %s\n",
				   fac, rk ? rd_kafka_name(rk) : NULL,
				   buf);
			break;

		case LOG_WARNING:
			LM_WARN("RDKAFKA fac: %s : %s : %s\n",
					fac, rk ? rd_kafka_name(rk) : NULL,
					buf);
			break;

		case LOG_NOTICE:
			LM_NOTICE("RDKAFKA fac: %s : %s : %s\n",
					fac, rk ? rd_kafka_name(rk) : NULL,
					buf);
			break;
			
		case LOG_INFO:
			LM_INFO("RDKAFKA fac: %s : %s : %s\n",
					fac, rk ? rd_kafka_name(rk) : NULL,
					buf);
			break;

		case LOG_DEBUG:
			LM_DBG("RDKAFKA fac: %s : %s : %s\n",
				   fac, rk ? rd_kafka_name(rk) : NULL,
				   buf);
			break;

		default:
			LM_ERR("Unsupported kafka log level: %d\n", level);
			break;
	}
}

/**
 * \brief Message delivery report callback using the richer rd_kafka_message_t object.
 */
static void kfk_msg_delivered (rd_kafka_t *rk,
							   const rd_kafka_message_t *rkmessage, void *opaque) {

	LM_DBG("Message delivered callback\n");
	
	const char *topic_name = NULL;
	topic_name = rd_kafka_topic_name(rkmessage->rkt);
	if (!topic_name) {
		LM_ERR("Cannot get topic name for delivered message\n");
		return;
	}
	
	kfk_stats_add(topic_name, rkmessage->err);
	
	if (rkmessage->err) {
		LM_ERR("RDKAFKA Message delivery failed: %s\n",
			   rd_kafka_err2str(rkmessage->err));
	} else {
		LM_DBG("RDKAFKA Message delivered (%zd bytes, offset %"PRId64", "
			   "partition %"PRId32"): %.*s\n",
			   rkmessage->len, rkmessage->offset,
			   rkmessage->partition,
			   (int)rkmessage->len, (const char *)rkmessage->payload);
	}
}

/**
 * \brief Initialize kafka functionality.
 *
 * \param brokers brokers to add.
 * \return 0 on success.
 */
int kfk_init(char *brokers)
{
	LM_DBG("Initializing Kafka\n");

	if (brokers == NULL) {
		LM_ERR("brokers parameter not set\n");
		return -1;
	}
	
	/*
	 * Create Kafka client configuration place-holder
	 */
	rk_conf = rd_kafka_conf_new();

	/* Set logger */
	rd_kafka_conf_set_log_cb(rk_conf, kfk_logger);

	/* Set message delivery callback. */
	rd_kafka_conf_set_dr_msg_cb(rk_conf, kfk_msg_delivered);

	/* Configure properties: */
	if (kfk_conf_configure()) {
		LM_ERR("Failed to configure general properties\n");
		return -1;
	}

	/*
	 * Create producer instance.
	 *
	 * NOTE: rd_kafka_new() takes ownership of the conf object
	 *       and the application must not reference it again after
	 *       this call.
	 */
	rk = rd_kafka_new(RD_KAFKA_PRODUCER, rk_conf, errstr, sizeof(errstr));
	if (!rk) {
		LM_ERR("Failed to create new producer: %s\n", errstr);
		return -1;
	}
	rk_conf = NULL; /* Now owned by producer. */
	LM_DBG("Producer handle created\n");

	LM_DBG("Adding broker: %s\n", brokers);
	/* Add brokers */
	if (rd_kafka_brokers_add(rk, brokers) == 0) {
		LM_ERR("No valid brokers specified: %s\n", brokers);
		return -1;
	}
	LM_DBG("Added broker: %s\n", brokers);

	/* Topic creation and configuration. */
	if (kfk_topic_list_configure()) {
		LM_ERR("Failed to configure topics\n");
		return -1;
	}

	return 0;
}

/**
 * \brief Close kafka related functionality.
 */
void kfk_close()
{
	rd_kafka_resp_err_t err;
	
	LM_DBG("Closing Kafka\n");

    /* Destroy the producer instance */
	if (rk) {
		/* Flushing messages. */
		LM_DBG("Flushing messages\n");
		err = rd_kafka_flush(rk, 0);
		if (err) {
			LM_ERR("Failed to flush messages: %s\n", rd_kafka_err2str(err));
		}

		/* Destroy producer. */
		LM_DBG("Destroying instance of Kafka producer\n");
		rd_kafka_destroy(rk);
	}

	/* Destroy configuration if not freed by rd_kafka_destroy. */
	if (rk_conf) {
		LM_DBG("Destroying instance of Kafka configuration\n");
		rd_kafka_conf_destroy(rk_conf);
	}

	/* Free list of configuration properties. */
	if (kfk_conf) {
		kfk_conf_free(kfk_conf);
	}

	/* Free list of topics. */
	while (kfk_topic) {
		kfk_topic_t *next = kfk_topic->next;
		kfk_topic_free(kfk_topic);
		kfk_topic = next;
	}
}

/**
 * \brief Free a general configuration object.
 */
static void kfk_conf_free(kfk_conf_t *kconf)
{
	if (kconf == NULL) {
		/* Nothing to free. */
		return;
	}

	kfk_conf_node_t *knode = kconf->property;
	while (knode) {
		kfk_conf_node_t *next = knode->next;
		pkg_free(knode);
		knode = next;
	}

	free_params(kconf->attrs);
	pkg_free(kconf);
}

/**
 * \brief Parse general configuration properties for Kafka.
 */
int kfk_conf_parse(char *spec)
{
	param_t *pit = NULL;
	param_hooks_t phooks;
	kfk_conf_t *kconf = NULL;

	if (kfk_conf != NULL) {
		LM_ERR("Configuration already set\n");
		goto error;
	}
	
	str s;
	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len-1]==';') {
		s.len--;
	}
	if (parse_params(&s, CLASS_ANY, &phooks, &pit) < 0) {
		LM_ERR("Failed parsing params value\n");
		goto error;
	}

	kconf = (kfk_conf_t*)pkg_malloc(sizeof(kfk_conf_t));
	if (kconf == NULL) {
		PKG_MEM_ERROR;
		goto error;
	}
	memset(kconf, 0, sizeof(kfk_conf_t));
	kconf->attrs = pit;
	kconf->spec = spec;
	for (pit = kconf->attrs; pit; pit=pit->next)
	{
		/* Parse a property. */
		kfk_conf_node_t *knode = NULL;
		knode = (kfk_conf_node_t*)pkg_malloc(sizeof(kfk_conf_node_t));
		if (knode == NULL) {
			PKG_MEM_ERROR;
			goto error;
		}
		memset(knode, 0, sizeof(kfk_conf_node_t));
		
		knode->sname = &pit->name;
		knode->svalue = &pit->body;
		if (knode->sname && knode->svalue) {
			LM_DBG("Parsed property: %.*s -> %.*s\n",
				   knode->sname->len, knode->sname->s,
				   knode->svalue->len, knode->svalue->s);
		}

		/* Place node at beginning of knode list. */
	    knode->next = kconf->property;
		kconf->property = knode;
	} /* for pit */

	kfk_conf = kconf;
	return 0;

error:
	if(pit!=NULL) {
		free_params(pit);
	}

	if(kconf != NULL) {
		kfk_conf_free(kconf);
	}
	return -1;
}

/**
 * \brief Configure Kafka properties.
 *
 * \return 0 on success. 
 */
static int kfk_conf_configure()
{
	if (kfk_conf == NULL) {
		/* Nothing to configure. */
		LM_DBG("No properties to configure\n");
		return 0;
	}

	LM_DBG("Configuring properties\n");
	
	kfk_conf_node_t *knode = kfk_conf->property;
	while (knode) {
		kfk_conf_node_t *next = knode->next;
		str *sname = knode->sname;
		str *svalue = knode->svalue;
		knode = next;
		
		if (sname == NULL || sname->len == 0 || sname->s == NULL) {
			LM_ERR("Bad name in configuration property\n");
			continue;
		}

		if (svalue == NULL || svalue->len == 0 || svalue->s == NULL) {
			LM_ERR("Bad value in configuration property\n");
			continue;
		}

		/* We temporarily convert to zstring. */
		char cname = sname->s[sname->len];
		sname->s[sname->len] = '\0';
		char cvalue = svalue->s[svalue->len];
		svalue->s[svalue->len] = '\0';
		
		LM_DBG("Setting property: %s -> %s\n", sname->s, svalue->s);
		
		if (rd_kafka_conf_set(rk_conf, sname->s, svalue->s,
							  errstr, sizeof(errstr)) !=
			RD_KAFKA_CONF_OK) {
			LM_ERR("Configuration failed: %s\n", errstr);

			/* We restore zstrings back to str */
			sname->s[sname->len] = cname;
			svalue->s[svalue->len] = cvalue;
			return -1;
		}

		/* We restore zstrings back to str */
		sname->s[sname->len] = cname;
		svalue->s[svalue->len] = cvalue;

	} /* while knode */
	
	return 0;
}

/**
 * \brief Free a topic object.
 */
static void kfk_topic_free(kfk_topic_t *ktopic)
{
	if (ktopic == NULL) {
		/* Nothing to free. */
		return;
	}

	kfk_conf_node_t *knode = ktopic->property;
	while (knode) {
		kfk_conf_node_t *next = knode->next;
		pkg_free(knode);
		knode = next;
	}

	/* Destroy rd Kafka topic. */
	if (ktopic->rd_topic) {
		rd_kafka_topic_destroy(ktopic->rd_topic);
	}
	
	free_params(ktopic->attrs);
	pkg_free(ktopic);
}

/**
 * \brief Parse topic properties for Kafka.
 */
int kfk_topic_parse(char *spec)
{
	param_t *pit = NULL;
	param_hooks_t phooks;
	kfk_topic_t *ktopic = NULL;

	str s;
	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len-1]==';') {
		s.len--;
	}
	if (parse_params(&s, CLASS_ANY, &phooks, &pit) < 0) {
		LM_ERR("Failed parsing params value\n");
		goto error;
	}

	ktopic = (kfk_topic_t*)pkg_malloc(sizeof(kfk_topic_t));
	if (ktopic == NULL) {
		PKG_MEM_ERROR;
		goto error;
	}
	memset(ktopic, 0, sizeof(kfk_topic_t));
	ktopic->attrs = pit;
	ktopic->spec = spec;
	for (pit = ktopic->attrs; pit; pit=pit->next)
	{
		/* Check for topic name. */
		if (pit->name.len==4 && strncmp(pit->name.s, "name", 4)==0) {
			if (ktopic->topic_name != NULL) {
				LM_ERR("Topic name already set\n");
				goto error;
			}
			ktopic->topic_name = &pit->body;
			LM_DBG("Topic name: %.*s\n", pit->body.len, pit->body.s);
			
		} else {

			/* Parse a property. */
			kfk_conf_node_t *knode = NULL;
			knode = (kfk_conf_node_t*)pkg_malloc(sizeof(kfk_conf_node_t));
			if (knode == NULL) {
				PKG_MEM_ERROR;
				goto error;
			}
			memset(knode, 0, sizeof(kfk_conf_node_t));
			
			knode->sname = &pit->name;
			knode->svalue = &pit->body;
			if (knode->sname && knode->svalue) {
				LM_DBG("Topic parsed property: %.*s -> %.*s\n",
					   knode->sname->len, knode->sname->s,
					   knode->svalue->len, knode->svalue->s);
			}
			
			/* Place node at beginning of ktopic list. */
			knode->next = ktopic->property;
			ktopic->property = knode;
		} /* if pit->name.len == 4 */
	} /* for pit */

	/* Topic name is mandatory. */
	if(ktopic->topic_name == NULL)
	{
		LM_ERR("No topic name\n");
		goto error;
	}

	/* Place topic at beginning of topic list. */
	ktopic->next = kfk_topic;
	kfk_topic = ktopic;
	return 0;

error:
	if(pit!=NULL) {
		free_params(pit);
	}

	if(ktopic != NULL) {
		kfk_topic_free(ktopic);
	}
	return -1;
}

/**
 * \brief Create and configure a topic.
 *
 * \return 0 on success.
 */
static int kfk_topic_configure(kfk_topic_t *ktopic)
{
	rd_kafka_topic_conf_t *topic_conf = NULL;
	rd_kafka_topic_t *rkt = NULL;
	
	if (ktopic == NULL) {
		LM_ERR("No topic to create\n");
		goto error;
	}
	
	/* Check topic name. */
	if (!ktopic->topic_name || !ktopic->topic_name->s || ktopic->topic_name->len == 0) {
		LM_ERR("Bad topic name\n");
		goto error;
	}

	int topic_found = kfk_topic_exist(ktopic->topic_name);
	if (topic_found == -1) {
		LM_ERR("Failed to search for topic %.*s in cluster\n",
			   ktopic->topic_name->len, ktopic->topic_name->s);
		goto error;
	} else if (topic_found == 0) {
		LM_ERR("Topic not found %.*s in cluster\n",
			   ktopic->topic_name->len, ktopic->topic_name->s);
		goto error;
	}
	
	LM_DBG("Creating topic: %.*s\n",
		   ktopic->topic_name->len, ktopic->topic_name->s);

	/* Topic configuration */

	topic_conf = rd_kafka_topic_conf_new();

	kfk_conf_node_t *knode = kfk_topic->property;
	while (knode) {
		kfk_conf_node_t *next = knode->next;
		str *sname = knode->sname;
		str *svalue = knode->svalue;
		knode = next;
		
		if (sname == NULL || sname->len == 0 || sname->s == NULL) {
			LM_ERR("Bad name in topic configuration property\n");
			continue;
		}

		if (svalue == NULL || svalue->len == 0 || svalue->s == NULL) {
			LM_ERR("Bad value in topic configuration property\n");
			continue;
		}

		/* We temporarily convert to zstring. */
		char cname = sname->s[sname->len];
		sname->s[sname->len] = '\0';
		char cvalue = svalue->s[svalue->len];
		svalue->s[svalue->len] = '\0';
		
		LM_DBG("Setting topic property: %s -> %s\n", sname->s, svalue->s);

		rd_kafka_conf_res_t res;
		res = rd_kafka_topic_conf_set(topic_conf, sname->s, svalue->s,
									  errstr, sizeof(errstr));
		if (res != RD_KAFKA_CONF_OK) {
			LM_ERR("Failed to set topic configuration: %s -> %s\n",
				   sname->s, svalue->s);

			/* We restore zstrings back to str */
			sname->s[sname->len] = cname;
			svalue->s[svalue->len] = cvalue;

			goto error;
		}

		/* We restore zstrings back to str */
		sname->s[sname->len] = cname;
		svalue->s[svalue->len] = cvalue;

	} /* while knode */

	/* We temporarily convert to zstring. */
	char c_topic_name = ktopic->topic_name->s[ktopic->topic_name->len];
	ktopic->topic_name->s[ktopic->topic_name->len] = '\0';

	rkt = rd_kafka_topic_new(rk, ktopic->topic_name->s, topic_conf);
	if (!rkt) {
		LM_ERR("Failed to create topic (%s): %s\n",
			   ktopic->topic_name->s,
			   rd_kafka_err2str(rd_kafka_last_error()));

		/* We restore zstrings back to str */
		ktopic->topic_name->s[ktopic->topic_name->len] = c_topic_name;

		goto error;
	}
	topic_conf = NULL; /* Now owned by topic */
	LM_DBG("Topic created: %s\n", ktopic->topic_name->s);
	
	/* We restore zstrings back to str */
	ktopic->topic_name->s[ktopic->topic_name->len] = c_topic_name;

	/* Everything went fine. */
	ktopic->rd_topic = rkt;
	return 0;

error:

	/* Destroy topic configuration. */
	if (topic_conf) {
		rd_kafka_topic_conf_destroy(topic_conf);
	}

	/* Destroy topic */
	if (rkt) {
		LM_DBG("Destroying topic\n");
		rd_kafka_topic_destroy(rkt);
	}

	return -1;
}
	
/**
 * \brief Create and configure a list of topics.
 *
 * \return 0 on success.
 */
static int kfk_topic_list_configure()
{
	kfk_topic_t *ktopic = kfk_topic;

	while (ktopic) {
		kfk_topic_t *next = ktopic->next;
		/* Create current topic. */
		if (kfk_topic_configure(ktopic)) {
			LM_ERR("Failed to create topic: %.*s\n",
				   ktopic->topic_name->len, ktopic->topic_name->s);
			return -1;
		}
		ktopic = next;
	}

	return 0;
}

/* -1 means RD_POLL_INFINITE */
/* 100000 means 100 seconds */
#define METADATA_TIMEOUT 100000 /**< Timeout when asking for metadata in milliseconds. */

/**
 * \brief check that a topic exists in cluster.
 *
 * \return 0 if topic does not exist.
 * \return 1 if topic does exist.
 * \return -1 on error.
 */
static int kfk_topic_exist(str *topic_name)
{
	/* Where to receive metadata. */
	const struct rd_kafka_metadata *metadatap = NULL;
	int i;
	int topic_found = 0; /* Topic not found by default. */

	if (!topic_name || topic_name->len == 0 || topic_name->s == NULL) {
		LM_ERR("Bad topic name\n");
		goto error;
	}

	/* Get metadata for all topics. */
	rd_kafka_resp_err_t res;
	res = rd_kafka_metadata(rk, 1, NULL, &metadatap, METADATA_TIMEOUT);
	if (res != RD_KAFKA_RESP_ERR_NO_ERROR) {
		LM_ERR("Failed to get metadata: %s\n", rd_kafka_err2str(res));
		goto error;
	}

	/* List topics */
	for (i=0; i<metadatap->topic_cnt; i++) {
		rd_kafka_metadata_topic_t *t = &metadatap->topics[i];
		if (t->topic) {
			LM_DBG("Metadata Topic: %s\n", t->topic);
			if (strncmp(topic_name->s, t->topic, topic_name->len) == 0) {
				topic_found = 1;
				LM_DBG("Metadata Topic (%s) found!\n", t->topic);
				break;
			}
		}
	} // for (i=0; i<m->topic_cnt; i++)

	/* Destroy metadata. */
	rd_kafka_metadata_destroy(metadatap);

	if (topic_found == 0) {
		LM_DBG("Topic not found: %.*s\n", topic_name->len, topic_name->s);
		return 0;
	}

	LM_DBG("Topic found: %.*s\n", topic_name->len, topic_name->s);
	return 1;
	
error:

	/* Destroy metadata. */
	if (metadatap) {
		rd_kafka_metadata_destroy(metadatap);
	}
	
	return -1;
}

/**
 * \brief get a topic based on its name.
 *
 * \return topic if it founds a matching one, NULL otherwise.
 */
static rd_kafka_topic_t* kfk_topic_get(str *topic_name)
{
	rd_kafka_topic_t *result = NULL;

	if (!topic_name || topic_name->len == 0 || topic_name->s == NULL) {
		LM_ERR("Bad topic name\n");
		goto clean;
	}

	kfk_topic_t *ktopic = kfk_topic;
	while (ktopic) {
		kfk_topic_t *next = ktopic->next;

		if (topic_name->len == ktopic->topic_name->len &&
			strncmp(topic_name->s, ktopic->topic_name->s, topic_name->len) == 0) {
			LM_DBG("Topic name match: %.*s\n",
				   ktopic->topic_name->len,
				   ktopic->topic_name->s);
			result = ktopic->rd_topic;
			break;
		}

		ktopic = next;
	}
	
clean:
	
	return result;
}

/**
 * \brief send a message to a topic.
 *
 * \param topic_name name of the topic
 * \param message message to send.
 * \param key to send.
 *
 * \return 0 on success.
 */
int kfk_message_send(str *topic_name, str *message, str *key)
{
    /* Get topic from name. */
	rd_kafka_topic_t *rkt = kfk_topic_get(topic_name);

	if (!rkt) {
		LM_ERR("Topic not found: %.*s\n", topic_name->len, topic_name->s);
		return -1;
	}

	/* Default key values (No key) */
	void *keyp = NULL;
	size_t key_len = 0;
	if (key != NULL && key->len > 0 && key->s != NULL) {
		keyp = key->s;
		key_len = key->len;
		LM_DBG("Key: %.*s\n", (int)key_len, (char*)keyp);
	}
	
	/* Send a message. */
	if (rd_kafka_produce(
			rkt,
			RD_KAFKA_PARTITION_UA,
			RD_KAFKA_MSG_F_COPY,
			/* Payload and length */
			message->s,
			message->len,
			/* Optional key and its length */
			keyp, key_len,
			/* Message opaque, provided in
			 * delivery report callback as
			 * msg_opaque. */
			NULL) == -1) {
		rd_kafka_resp_err_t err = rd_kafka_last_error();
		LM_ERR("Error sending message: %s\n", rd_kafka_err2str(err));

		return -1;
	}

	LM_DBG("Message sent\n");

	/* Poll to handle delivery reports */
	rd_kafka_poll(rk, 0);
	LM_DBG("Message polled\n");

	return 0;
}

/**
 * \brief Initialize statistics.
 *
 * \return 0 on success.
 */
int kfk_stats_init()
{
	LM_DBG("Initializing statistics\n");

	stats_lock = lock_alloc();
	if (!stats_lock) {
		LM_ERR("Cannot allocate stats lock\n");
		return -1;
	}

	if(lock_init(stats_lock) == NULL) {
		LM_ERR("cannot init stats lock\n");
		lock_dealloc(stats_lock);
		stats_lock = NULL;
		return -1;
	}

	stats_general = shm_malloc(sizeof(kfk_stats_t));
	if (!stats_general) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(stats_general, 0, sizeof(kfk_stats_t));

	return 0;
}

/**
 * \brief Close statistics.
 */
void kfk_stats_close()
{
	LM_DBG("Closing statistics\n");

	if (stats_lock) {
		LM_DBG("Freeing lock\n");
		lock_destroy(stats_lock);
		lock_dealloc(stats_lock);
		stats_lock = NULL;
	}

	kfk_stats_t *current_topic = stats_general;
	while (current_topic) {
		kfk_stats_t *next = current_topic->next;
		kfk_stats_topic_free(current_topic);
		current_topic = next;
	}
}

/**
 * \brief free a kfk_stats_t structure.
 */
static void kfk_stats_topic_free(kfk_stats_t *st_topic)
{
	if (!st_topic) {
		/* Nothing to free. */
		return;
	}

	/* Free topic_name str. */
	if (st_topic->topic_name) {
		if (st_topic->topic_name->s) {
			shm_free(st_topic->topic_name->s);
		}
		shm_free(st_topic->topic_name);
	}

	shm_free(st_topic);
}

/**
 * \brief create a new stats_topic node.
 *
 * \return the new kfk_stats_t on success.
 * \return NULL on error.
 */
static kfk_stats_t* kfk_stats_topic_new(const char *topic, rd_kafka_resp_err_t err)
{
	kfk_stats_t *st = NULL;

	if (!topic) {
		LM_ERR("No topic\n");
		goto error;
	}
	int topic_len = strlen(topic);
	if (topic_len == 0) {
		LM_ERR("Void topic\n");
		goto error;
	}
	
	st = shm_malloc(sizeof(kfk_stats_t));
	if (!st) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(st, 0, sizeof(kfk_stats_t));

	st->topic_name = shm_malloc(sizeof(str));
	if (!st->topic_name) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(st->topic_name, 0, sizeof(str));

	st->topic_name->s = shm_malloc(topic_len + 1);
	if (!st->topic_name->s) {
		SHM_MEM_ERROR;
		goto error;
	}
	memcpy(st->topic_name->s, topic, topic_len);
	st->topic_name->s[topic_len] = '\0';
	st->topic_name->len = topic_len;
	
	st->total++;
	if (err) {
		st->error++;
	}

	return st;
	
error:

	if (st) {
		kfk_stats_topic_free(st);
	}
	
	return NULL;
}

/**
 * \brief add a new message delivery to statistics.
 *
 * \return 0 on success.
 */
static int kfk_stats_add(const char *topic, rd_kafka_resp_err_t err)
{
	LM_DBG("Adding stats: (topic: %s) (error: %d)\n",
		   topic, err);

	if (topic == NULL || *topic == '\0') {
		LM_ERR("No topic to add to statistics\n");
		return -1;
	}
	int topic_len = strlen(topic);
	
	lock_get(stats_lock);

	stats_general->total++;

	if (err) {
		stats_general->error++;
	}

	LM_DBG("General stats: total = %" PRIu64 "  error = %" PRIu64 "\n",
		   stats_general->total, stats_general->error);

	kfk_stats_t **stats_pre = &(stats_general->next);
	while (*stats_pre != NULL) {
		LM_DBG("Topic search: %.*s\n", (*stats_pre)->topic_name->len,
				   (*stats_pre)->topic_name->s);
		if ((*stats_pre)->topic_name->len == topic_len &&
			strncmp(topic, (*stats_pre)->topic_name->s, (*stats_pre)->topic_name->len) == 0) {
			/* Topic match. */
			LM_DBG("Topic match: %.*s\n", (*stats_pre)->topic_name->len,
				   (*stats_pre)->topic_name->s);
			break;
		}

		stats_pre = &((*stats_pre)->next);
	}

	if (*stats_pre == NULL) {
		/* Topic not found. */
		LM_DBG("Topic: %s not found\n", topic);
		
		/* Add a new stats topic. */
		kfk_stats_t *new_topic = NULL;
		new_topic = kfk_stats_topic_new(topic, err);
		if (!new_topic) {
			LM_ERR("Failed to create stats for topic: %s\n", topic);
			goto error;
		}

		*stats_pre = new_topic;
		LM_DBG("Created Topic stats (%s): total = %" PRIu64 "  error = %" PRIu64 "\n",
			   topic, new_topic->total, new_topic->error);

		goto clean;
	}

	/* Topic found. Increase statistics. */
	kfk_stats_t *current = *stats_pre;
	current->total++;
	if (err) {
		current->error++;
	}

	LM_DBG("Topic stats (%s): total = %" PRIu64 "  error = %" PRIu64 "\n",
		   topic, current->total, current->error);

clean:
	lock_release(stats_lock);
	
	return 0;

error:
	lock_release(stats_lock);

	return -1;
}

/**
 * \brief Get total statistics.
 *
 * \param msg_total return total number of messages by reference.
 * \param msg_error return total number of errors by reference.
 *
 * \return 0 on success.
 */
int kfk_stats_get(uint64_t *msg_total, uint64_t *msg_error)
{
	lock_get(stats_lock);

	*msg_total = stats_general->total;
	*msg_error = stats_general->error;

	lock_release(stats_lock);

	return 0;
}

/**
 * \brief Get statistics for a specified topic.
 *
 * \param s_topic string with topic name.
 * \param msg_total return total number of messages by reference.
 * \param msg_error return total number of errors by reference.
 *
 * \return 0 on success.
 */
int kfk_stats_topic_get(str *s_topic, uint64_t *msg_total, uint64_t *msg_error)
{
	/* Default return values. */
	*msg_total = 0;
	*msg_error = 0;
	
	lock_get(stats_lock);

	kfk_stats_t *st = stats_general->next;
	while (st) {
		LM_DBG("Topic show search: %.*s\n", st->topic_name->len, st->topic_name->s);

		if (st->topic_name->len == s_topic->len &&
			strncmp(s_topic->s, st->topic_name->s, s_topic->len) == 0) {
			/* Topic match. */
			LM_DBG("Topic show match: %.*s\n", st->topic_name->len, st->topic_name->s);
			break;
		}

		st = st->next;
	}

	if (!st) {
		LM_ERR("Topic not found. Showing default 0 values\n");
		goto clean;
	}

	*msg_total = st->total;
	*msg_error = st->error;

clean:
	
	lock_release(stats_lock);

	return 0;
}
