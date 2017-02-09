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

#ifndef __NSQ_MOD_H_
#define __NSQ_MOD_H_

#include <sys/types.h>
#include <sys/wait.h>

#include "../../core/cfg/cfg_struct.h"
#include "../../lib/srdb1/db.h"
#include "nsq_reader.h"
#include "nsq_trans.h"
#include "nsq_pua.h"

#define DBN_DEFAULT_NO_WORKERS 4
#define LOOKUPD_ADDRESS "127.0.0.1"
#define CONSUMER_EVENT_KEY "Event-Category"
#define CONSUMER_EVENT_SUB_KEY "Event-Name"
#define DEFAULT_CHANNEL "Kamailio-Channel"
#define DEFAULT_TOPIC "Kamailio-Topic"
#define NSQD_ADDRESS "127.0.0.1"
#define PRESENTITY_TABLE "presentity"

typedef struct nsq_topic_channel
{
	char *topic;
	char *channel;
	struct nsq_topic_channel *next;
} nsq_topic_channel_t;


int nsq_workers = 1;
int nsq_max_in_flight = 1;
int consumer_use_nsqd = 0;
str nsq_lookupd_address = str_init(LOOKUPD_ADDRESS);
int lookupd_port = 4161;
str nsq_event_key = str_init(CONSUMER_EVENT_KEY);
str nsq_event_sub_key = str_init(CONSUMER_EVENT_SUB_KEY);
str nsqd_address = str_init(NSQD_ADDRESS);
int nsqd_port = 4150;
int dbn_pua_mode = 1;
int dbn_include_entity = 1;

nsq_topic_channel_t *tc_list = NULL;
str nsq_json_escape_str = str_init("%");
char nsq_json_escape_char = '%';

int nsq_topic_channel_counter = 0;
int dbn_consumer_workers = DBN_DEFAULT_NO_WORKERS;
int startup_time = 0;

/* database connection */
db1_con_t *nsq_pa_db = NULL;
db_func_t nsq_pa_dbf;
str nsq_presentity_table = str_init(PRESENTITY_TABLE);
str nsq_db_url = {NULL, 0};
int db_table_lock_type = 1;
db_locking_t db_table_lock = DB_LOCKING_WRITE;

static int mod_init(void);
static int mod_child_init(int);
static int nsq_add_topic_channel(modparam_t type, void* val);
static void free_tc_list(nsq_topic_channel_t *tc_list);
static void mod_destroy(void);

int nsq_pv_get_event_payload(struct sip_msg*, pv_param_t*, pv_value_t*);

#endif
