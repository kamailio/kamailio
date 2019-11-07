/*
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#include <stdio.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/dprint.h"
#include "../../core/cfg/cfg_struct.h"

#include "kz_amqp.h"
#include "kz_json.h"
#include "kz_fixup.h"
#include "kz_trans.h"
#include "kz_pua.h"

#define DBK_DEFAULT_NO_CONSUMERS 1
#define DBK_DEFAULT_NO_WORKERS 8

#define AMQP_WORKERS_RANKING PROC_XWORKER

static int mod_init(void);
static int  mod_child_init(int rank);
static int fire_init_event(int rank);
static void mod_destroy(void);

str dbk_node_hostname = { 0, 0 };
str dbk_reg_fs_path = { 0, 0 };

int kz_server_counter = 0;
int kz_zone_counter = 0;

int dbk_auth_wait_timeout = 3;
int dbk_reconn_retries = 8;
int dbk_presentity_phtable_size = 4096;
int dbk_command_table_size = 2048;
int dbk_use_federated_exchange = 0;
str dbk_federated_exchange = str_init("federation");
str dbk_primary_zone_name = str_init("local");

int dbk_create_empty_dialog = 1;

int dbk_channels = 50;

int dbk_consumer_processes = DBK_DEFAULT_NO_CONSUMERS;
int dbk_consumer_workers = DBK_DEFAULT_NO_WORKERS;

struct timeval kz_sock_tv = (struct timeval){0,100000};
struct timeval kz_amqp_tv = (struct timeval){0,100000};
struct timeval kz_qtimeout_tv = (struct timeval){2,0};
struct timeval kz_ack_tv = (struct timeval){0,100000};
struct timeval kz_timer_tv = (struct timeval){0,200000};
struct timeval kz_amqp_connect_timeout_tv = (struct timeval){0,200000};

int kz_timer_ms = 200;

str kz_json_escape_str = str_init("%");
char kz_json_escape_char = '%';

str dbk_consumer_event_key = str_init("Event-Category");
str dbk_consumer_event_subkey = str_init("Event-Name");

int dbk_internal_loop_count = 5;
int dbk_consumer_loop_count = 10;
int dbk_consumer_ack_loop_count = 20;
int dbk_include_entity = 1;
int dbk_use_full_entity = 0;
int dbk_pua_mode = 1;
db_locking_t kz_pua_lock_type = DB_LOCKING_WRITE;
int dbk_use_hearbeats = 0;
int dbk_single_consumer_on_reconnect = 1;
int dbk_consume_messages_on_reconnect = 1;

int startup_time = 0;

int *kz_worker_pipes_fds = NULL;
int *kz_worker_pipes = NULL;
int kz_cmd_pipe = 0;
int  kz_cmd_pipe_fds[2] = {-1,-1};

/* database connection */
db1_con_t *kz_pa_db = NULL;
db_func_t kz_pa_dbf;
str kz_presentity_table = str_init("presentity");
str kz_db_url = {0,0};

str kz_amqps_ca_cert = {0,0};
str kz_amqps_cert = {0,0};
str kz_amqps_key = {0,0};
int kz_amqps_verify_peer = 1;
int kz_amqps_verify_hostname = 1;

str kz_query_timeout_avp = {0,0};
pv_spec_t kz_query_timeout_spec;

str kz_query_result_avp = str_init("$avp(amqp_result)");
pv_spec_t kz_query_result_spec;

str kz_app_name = str_init(NAME);

MODULE_VERSION

static tr_export_t mod_trans[] = {
	{ {"kz", sizeof("kz")-1}, kz_tr_parse},
	{ { 0, 0 }, 0 }
};

static pv_export_t kz_mod_pvs[] = {
	{{"kzR", (sizeof("kzR")-1)}, PVT_OTHER, kz_pv_get_last_query_result, 0,	0, 0, 0, 0},
	{{"kzE", (sizeof("kzE")-1)}, PVT_OTHER, kz_pv_get_event_payload, 0,	0, 0, 0, 0},
	{{"kzRK", (sizeof("kzRK")-1)}, PVT_OTHER, kz_pv_get_event_routing_key, 0,	0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 *  database module interface
 */
static cmd_export_t cmds[] = {
    {"kazoo_publish", (cmd_function) kz_amqp_publish, 3, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_publish", (cmd_function) kz_amqp_publish_ex, 4, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_query", (cmd_function) kz_amqp_query, 4, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_query", (cmd_function) kz_amqp_query_ex, 3, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_pua_publish", (cmd_function) kz_pua_publish, 1, 0, 0, ANY_ROUTE},
    {"kazoo_pua_publish_mwi", (cmd_function) kz_pua_publish_mwi, 1, 0, 0, ANY_ROUTE},
    {"kazoo_pua_publish_presence", (cmd_function) kz_pua_publish_presence, 1, 0, 0, ANY_ROUTE},
    {"kazoo_pua_publish_dialoginfo", (cmd_function) kz_pua_publish_dialoginfo, 1, 0, 0, ANY_ROUTE},

    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe, 1, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe_simple, 4, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},


    {"kazoo_json", (cmd_function) kz_json_get_field, 3, fixup_kz_json, fixup_kz_json_free, ANY_ROUTE},
    {"kazoo_json_keys", (cmd_function) kz_json_get_keys, 3, fixup_kz_json, fixup_kz_json_free, ANY_ROUTE},
    {"kazoo_encode", (cmd_function) kz_amqp_encode, 2, fixup_kz_amqp_encode, fixup_kz_amqp_encode_free, ANY_ROUTE},

    {"kazoo_async_query", (cmd_function) kz_amqp_async_query, 5, fixup_kz_async_amqp, fixup_kz_async_amqp_free, ANY_ROUTE},
    {"kazoo_async_query", (cmd_function) kz_amqp_async_query_ex, 6, fixup_kz_async_amqp, fixup_kz_async_amqp_free, ANY_ROUTE},
    {"kazoo_query_async", (cmd_function) kz_amqp_async_query, 5, fixup_kz_async_amqp, fixup_kz_async_amqp_free, ANY_ROUTE},
    {"kazoo_query_async", (cmd_function) kz_amqp_async_query_ex, 6, fixup_kz_async_amqp, fixup_kz_async_amqp_free, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    {"node_hostname", PARAM_STR, &dbk_node_hostname},
    {"amqp_connection", PARAM_STRING|USE_FUNC_PARAM,(void*)kz_amqp_add_connection},
    {"amqp_max_channels", INT_PARAM, &dbk_channels},
    {"amqp_timmer_process_interval", INT_PARAM, &kz_timer_ms},
    {"amqp_consumer_ack_timeout_micro", INT_PARAM, &kz_ack_tv.tv_usec},
    {"amqp_consumer_ack_timeout_sec", INT_PARAM, &kz_ack_tv.tv_sec},
    {"amqp_interprocess_timeout_micro", INT_PARAM, &kz_sock_tv.tv_usec},
    {"amqp_interprocess_timeout_sec", INT_PARAM, &kz_sock_tv.tv_sec},
    {"amqp_waitframe_timeout_micro", INT_PARAM, &kz_amqp_tv.tv_usec},
    {"amqp_waitframe_timeout_sec", INT_PARAM, &kz_amqp_tv.tv_sec},
    {"amqp_consumer_processes", INT_PARAM, &dbk_consumer_processes},
    {"amqp_consumer_workers", INT_PARAM, &dbk_consumer_workers},
    {"amqp_consumer_event_key", PARAM_STR, &dbk_consumer_event_key},
    {"amqp_consumer_event_subkey", PARAM_STR, &dbk_consumer_event_subkey},
    {"amqp_query_timeout_micro", INT_PARAM, &kz_qtimeout_tv.tv_usec},
    {"amqp_query_timeout_sec", INT_PARAM, &kz_qtimeout_tv.tv_sec},
    {"amqp_internal_loop_count", INT_PARAM, &dbk_internal_loop_count},
    {"amqp_consumer_loop_count", INT_PARAM, &dbk_consumer_loop_count},
    {"amqp_consumer_ack_loop_count", INT_PARAM, &dbk_consumer_ack_loop_count},
    {"pua_include_entity", INT_PARAM, &dbk_include_entity},
	{"presence_use_full_entity", INT_PARAM, &dbk_use_full_entity},
    {"presentity_table", PARAM_STR, &kz_presentity_table},
	{"db_url", PARAM_STR, &kz_db_url},
    {"pua_mode", INT_PARAM, &dbk_pua_mode},
    {"single_consumer_on_reconnect", INT_PARAM, &dbk_single_consumer_on_reconnect},
    {"consume_messages_on_reconnect", INT_PARAM, &dbk_consume_messages_on_reconnect},
    {"amqp_query_timeout_avp", PARAM_STR, &kz_query_timeout_avp},
    {"json_escape_char", PARAM_STR, &kz_json_escape_str},
    {"app_name", PARAM_STR, &kz_app_name},
    {"use_federated_exchange", INT_PARAM, &dbk_use_federated_exchange},
    {"federated_exchange", PARAM_STR, &dbk_federated_exchange},
    {"amqp_heartbeats", INT_PARAM, &dbk_use_hearbeats},
    {"amqp_primary_zone", PARAM_STR, &dbk_primary_zone_name},
    {"amqp_command_hashtable_size", INT_PARAM, &dbk_command_table_size},
    {"amqp_result_avp", PARAM_STR, &kz_query_result_avp},
    {"amqps_ca_cert", PARAM_STR, &kz_amqps_ca_cert},
    {"amqps_cert", PARAM_STR, &kz_amqps_cert},
    {"amqps_key", PARAM_STR, &kz_amqps_key},
    {"amqps_verify_peer", INT_PARAM, &kz_amqps_verify_peer},
    {"amqps_verify_hostname", INT_PARAM, &kz_amqps_verify_hostname},
	{"pua_lock_type", INT_PARAM, &kz_pua_lock_type},
    {"amqp_connect_timeout_micro", INT_PARAM, &kz_amqp_connect_timeout_tv.tv_usec},
    {"amqp_connect_timeout_sec", INT_PARAM, &kz_amqp_connect_timeout_tv.tv_sec},
    {0, 0, 0}
};


struct module_exports exports = {
	"kazoo",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	kz_mod_pvs,      /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	mod_child_init,  /* per-child init function */
	mod_destroy      /* module destroy function */
};

inline static int kz_parse_avp( str *avp_spec, pv_spec_t *avp, char *txt)
{
	if (pv_parse_spec(avp_spec, avp)==NULL) {
		LM_ERR("malformed or non AVP %s AVP definition\n",txt);
		return -1;
	}
	return 0;
}

static int kz_init_avp(void) {
	if(kz_query_timeout_avp.s)
		kz_query_timeout_avp.len = strlen(kz_query_timeout_avp.s);

	if ( kz_query_timeout_avp.s ) {
		if ( kz_parse_avp(&kz_query_timeout_avp, &kz_query_timeout_spec, "amqp_query_timeout_avp") <0) {
			return -1;
		}
	} else {
		memset( &kz_query_timeout_spec, 0, sizeof(pv_spec_t));
	}

	if ( kz_parse_avp(&kz_query_result_avp, &kz_query_result_spec, "amqp_result_avp") <0) {
		return -1;
	}

	return 0;
}

static int mod_init(void) {
	int i;
    startup_time = (int) time(NULL);
    kz_json_escape_char = kz_json_escape_str.s[0];

    if (dbk_node_hostname.s == NULL) {
	LM_ERR("You must set the node_hostname parameter\n");
	return -1;
    }

   	if(kz_init_avp()) {
   		LM_ERR("Error in avp params\n");
   		return -1;
   	}

    if(!kz_amqp_init()) {
   		return -1;
    }

    if(kz_timer_ms > 0) {
    	kz_timer_tv.tv_usec = (kz_timer_ms % 1000) * 1000;
    	kz_timer_tv.tv_sec = kz_timer_ms / 1000;
    }
    
    if(dbk_pua_mode == 1) {
		kz_db_url.len = kz_db_url.s ? strlen(kz_db_url.s) : 0;
		LM_DBG("db_url=%s/%d/%p\n", ZSW(kz_db_url.s), kz_db_url.len,kz_db_url.s);
		kz_presentity_table.len = strlen(kz_presentity_table.s);

		if(kz_db_url.len > 0) {

			/* binding to database module  */
			if (db_bind_mod(&kz_db_url, &kz_pa_dbf))
			{
				LM_ERR("Database module not found\n");
				return -1;
			}


			if (!DB_CAPABILITY(kz_pa_dbf, DB_CAP_ALL))
			{
				LM_ERR("Database module does not implement all functions"
						" needed by kazoo module\n");
				return -1;
			}

			kz_pa_db = kz_pa_dbf.init(&kz_db_url);
			if (!kz_pa_db)
			{
				LM_ERR("Connection to database failed\n");
				return -1;
			}

			kz_pa_dbf.close(kz_pa_db);
			kz_pa_db = NULL;
		}
    }


    int total_workers = dbk_consumer_workers + (dbk_consumer_processes * kz_server_counter) + 2;

    register_procs(total_workers);
    cfg_register_child(total_workers);

	if (pipe(kz_cmd_pipe_fds) < 0) {
		LM_ERR("cmd pipe() failed\n");
		return -1;
	}

    kz_worker_pipes_fds = (int*) shm_malloc(sizeof(int) * (dbk_consumer_workers) * 2 );
    kz_worker_pipes = (int*) shm_malloc(sizeof(int) * dbk_consumer_workers);
    for(i=0; i < dbk_consumer_workers; i++) {
    	kz_worker_pipes_fds[i*2] = kz_worker_pipes_fds[i*2+1] = -1;
		if (pipe(&kz_worker_pipes_fds[i*2]) < 0) {
			LM_ERR("worker pipe(%d) failed\n", i);
			return -1;
		}
    }

	kz_cmd_pipe = kz_cmd_pipe_fds[1];
	for(i=0; i < dbk_consumer_workers; i++) {
		kz_worker_pipes[i] = kz_worker_pipes_fds[i*2+1];
	}

    return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(kz_tr_init_buffers()<0)
	{
		LM_ERR("failed to initialize transformations buffers\n");
		return -1;
	}
	return register_trans_mod(path, mod_trans);
}


static int mod_child_init(int rank)
{
	int pid;
	int i;
	kz_amqp_zone_ptr g;
	kz_amqp_server_ptr s;

	if (rank==PROC_INIT)
		fire_init_event(rank);

	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;

	if (rank==PROC_MAIN) {
		for(i=0; i < dbk_consumer_workers; i++) {
			pid=fork_process(AMQP_WORKERS_RANKING, "AMQP Consumer Worker", 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0){
				if (cfg_child_init()) return -1;
				close(kz_worker_pipes_fds[i*2+1]);
				cfg_update();
				return(kz_amqp_consumer_worker_proc(kz_worker_pipes_fds[i*2]));
			}
		}

		for (g = kz_amqp_get_zones(); g != NULL; g = g->next) {
			int w = (g == kz_amqp_get_primary_zone() ? dbk_consumer_processes : 1);
			for(i=0; i < w; i++) {
				for (s = g->servers->head; s != NULL; s = s->next) {
					pid=fork_process(PROC_NOCHLDINIT, "AMQP Consumer", 0);
					if (pid<0)
						return -1; /* error */
					if(pid==0){
						if (cfg_child_init()) return -1;
						cfg_update();
						return(kz_amqp_consumer_proc(s));
					}
				}
			}
		}

		pid=fork_process(PROC_NOCHLDINIT, "AMQP Publisher", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0){
			if (cfg_child_init()) return -1;
			close(kz_cmd_pipe_fds[1]);
			cfg_update();
			kz_amqp_publisher_proc(kz_cmd_pipe_fds[0]);
		}
		return 0;
	}

	if(rank == AMQP_WORKERS_RANKING && dbk_pua_mode == 1) {
		if (kz_pa_dbf.init==0)
		{
			LM_CRIT("child_init: database not bound\n");
			return -1;
		}
		kz_pa_db = kz_pa_dbf.init(&kz_db_url);
		if (!kz_pa_db)
		{
			LM_ERR("child %d: unsuccessful connecting to database\n", rank);
			return -1;
		}

		if (kz_pa_dbf.use_table(kz_pa_db, &kz_presentity_table) < 0)
		{
			LM_ERR( "child %d:unsuccessful use_table presentity_table\n", rank);
			return -1;
		}
		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}

static int fire_init_event(int rank)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("rank is (%d)\n", rank);
	if (rank!=PROC_INIT)
		return 0;

	rt = route_get(&event_rt, "kazoo:mod-init");
	if(rt>=0 && event_rt.rlist[rt]!=NULL) {
		LM_DBG("executing event_route[kazoo:mod-init] (%d)\n", rt);
		if(faked_msg_init()<0)
			return -1;
		fmsg = faked_msg_next();
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		if(ctx.run_flags&DROP_R_F)
		{
			LM_ERR("exit due to 'drop' in event route\n");
			return -1;
		}
		set_route_type(rtb);
	}

	return 0;
}


static void mod_destroy(void) {
	kz_amqp_destroy();
    if (kz_worker_pipes_fds) { shm_free(kz_worker_pipes_fds); }
    if (kz_worker_pipes) { shm_free(kz_worker_pipes); }
}


