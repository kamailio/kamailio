/*
 * $Id$
 *
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

#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "../../cfg/cfg_struct.h"

#include "kz_amqp.h"
#include "kz_json.h"
#include "kz_fixup.h"
#include "kz_trans.h"
#include "kz_pua.h"

#define DBK_DEFAULT_NO_CONSUMERS 8

static int mod_init(void);
static int  mod_child_init(int rank);
static int fire_init_event(int rank);
static void mod_destroy(void);
static void  mod_consumer_proc(int rank);

str dbk_node_hostname = { 0, 0 };
str dbk_reg_fs_path = { 0, 0 };

int dbk_auth_wait_timeout = 3;
int dbk_reconn_retries = 8;

int dbk_presentity_phtable_size = 4096;

//int dbk_dialog_expires = 30;
//int dbk_presence_expires = 3600;
//int dbk_mwi_expires = 3600;
int dbk_create_empty_dialog = 1;

int dbk_channels = 50;

int dbk_consumer_processes = DBK_DEFAULT_NO_CONSUMERS;

struct timeval kz_sock_tv = (struct timeval){0,100000};
struct timeval kz_amqp_tv = (struct timeval){0,100000};
struct timeval kz_qtimeout_tv = (struct timeval){2,0};
struct timeval kz_ack_tv = (struct timeval){0,100000};
struct timeval kz_timer_tv = (struct timeval){0,200000};
int kz_timer_ms = 200;


str dbk_consumer_event_key = str_init("Event-Category");
str dbk_consumer_event_subkey = str_init("Event-Name");

int dbk_internal_loop_count = 5;
int dbk_consumer_loop_count = 10;
int dbk_consumer_ack_loop_count = 20;
int dbk_include_entity = 1;
int dbk_pua_mode = 1;

int dbk_single_consumer_on_reconnect = 1;
int dbk_consume_messages_on_reconnect = 1;

int startup_time = 0;

int *kz_pipe_fds = NULL;

db1_con_t * shared_db1 = NULL;

/* database connection */
db1_con_t *kz_pa_db = NULL;
db_func_t kz_pa_dbf;
str kz_presentity_table = str_init("presentity");
str kz_db_url = {0,0};

str kz_query_timeout_avp = {0,0};
pv_spec_t kz_query_timeout_spec;

MODULE_VERSION

static tr_export_t mod_trans[] = {
	{ {"kz", sizeof("kz")-1}, kz_tr_parse},
	{ { 0, 0 }, 0 }
};

static pv_export_t kz_mod_pvs[] = {
	{{"kzR", (sizeof("kzR")-1)}, PVT_OTHER, kz_pv_get_last_query_result, 0,	0, 0, 0, 0},
	{{"kzE", (sizeof("kzE")-1)}, PVT_OTHER, kz_pv_get_event_payload, 0,	0, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 *  database module interface
 */
static cmd_export_t cmds[] = {
    {"kazoo_publish", (cmd_function) kz_amqp_publish, 3, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_query", (cmd_function) kz_amqp_query, 4, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_query", (cmd_function) kz_amqp_query_ex, 3, fixup_kz_amqp, fixup_kz_amqp_free, ANY_ROUTE},
    {"kazoo_pua_publish", (cmd_function) kz_pua_publish, 1, 0, 0, ANY_ROUTE},
    /*
    {"kazoo_pua_flush", (cmd_function) w_mi_dbk_presentity_flush0, 0, 0, 0, ANY_ROUTE},
    {"kazoo_pua_flush", (cmd_function) w_mi_dbk_presentity_flush1, 1, 0, 0, ANY_ROUTE},
    {"kazoo_pua_flush", (cmd_function) w_mi_dbk_presentity_flush2, 2, 0, 0, ANY_ROUTE},
    {"kazoo_pua_flush", (cmd_function) w_mi_dbk_presentity_flush3, 3, 0, 0, ANY_ROUTE},
    */

/*
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe_1, 1, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe_2, 2, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe_3, 3, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},
*/
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe, 1, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},
    {"kazoo_subscribe", (cmd_function) kz_amqp_subscribe_simple, 4, fixup_kz_amqp4, fixup_kz_amqp4_free, ANY_ROUTE},


    {"kazoo_json", (cmd_function) kz_json_get_field, 3, fixup_kz_json, fixup_kz_json_free, ANY_ROUTE},
    {"kazoo_encode", (cmd_function) kz_amqp_encode, 2, fixup_kz_amqp_encode, fixup_kz_amqp_encode_free, ANY_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    {"node_hostname", STR_PARAM, &dbk_node_hostname.s},
  //  {"dialog_expires", INT_PARAM, &dbk_dialog_expires},
  //  {"presence_expires", INT_PARAM, &dbk_presence_expires},
  //  {"mwi_expires", INT_PARAM, &dbk_mwi_expires},
    {"amqp_connection", STR_PARAM|USE_FUNC_PARAM,(void*)kz_amqp_add_connection},
    {"amqp_max_channels", INT_PARAM, &dbk_channels},
    {"amqp_timmer_process_interval", INT_PARAM, &kz_timer_ms},
    {"amqp_consumer_ack_timeout_micro", INT_PARAM, &kz_ack_tv.tv_usec},
    {"amqp_consumer_ack_timeout_sec", INT_PARAM, &kz_ack_tv.tv_sec},
    {"amqp_interprocess_timeout_micro", INT_PARAM, &kz_sock_tv.tv_usec},
    {"amqp_interprocess_timeout_sec", INT_PARAM, &kz_sock_tv.tv_sec},
    {"amqp_waitframe_timeout_micro", INT_PARAM, &kz_amqp_tv.tv_usec},
    {"amqp_waitframe_timeout_sec", INT_PARAM, &kz_amqp_tv.tv_sec},
    {"amqp_consumer_processes", INT_PARAM, &dbk_consumer_processes},
    {"amqp_consumer_event_key", STR_PARAM, &dbk_consumer_event_key.s},
    {"amqp_consumer_event_subkey", STR_PARAM, &dbk_consumer_event_subkey.s},
    {"amqp_query_timeout_micro", INT_PARAM, &kz_qtimeout_tv.tv_usec},
    {"amqp_query_timeout_sec", INT_PARAM, &kz_qtimeout_tv.tv_sec},
    {"amqp_internal_loop_count", INT_PARAM, &dbk_internal_loop_count},
    {"amqp_consumer_loop_count", INT_PARAM, &dbk_consumer_loop_count},
    {"amqp_consumer_ack_loop_count", INT_PARAM, &dbk_consumer_ack_loop_count},
    {"pua_include_entity", INT_PARAM, &dbk_include_entity},
    {"presentity_table", STR_PARAM, &kz_presentity_table.s},
	{"db_url", STR_PARAM, &kz_db_url.s},
    {"pua_mode", INT_PARAM, &dbk_pua_mode},
    {"single_consumer_on_reconnect", INT_PARAM, &dbk_single_consumer_on_reconnect},
    {"consume_messages_on_reconnect", INT_PARAM, &dbk_consume_messages_on_reconnect},
    {"amqp_query_timeout_avp", STR_PARAM, &kz_query_timeout_avp.s},
    {0, 0, 0}
};


struct module_exports exports = {
    "kazoo",
    DEFAULT_DLFLAGS,		/* dlopen flags */
    cmds,
    params,			/* module parameters */
    0,				/* exported statistics */
    0,			/* exported MI functions */
    kz_mod_pvs,				/* exported pseudo-variables */
    0,				/* extra processes */
    mod_init,			/* module initialization function */
    0,				/* response function */
    mod_destroy,		/* destroy function */
    mod_child_init				/* per-child init function */
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

	return 0;
}

static int mod_init(void) {
	int i;
    startup_time = (int) time(NULL);


    if (dbk_node_hostname.s == NULL) {
	LM_ERR("You must set the node_hostname parameter\n");
	return -1;
    }
    dbk_node_hostname.len = strlen(dbk_node_hostname.s);

    dbk_consumer_event_key.len = strlen(dbk_consumer_event_key.s);
   	dbk_consumer_event_subkey.len = strlen(dbk_consumer_event_subkey.s);


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


    int total_workers = dbk_consumer_processes + 3;
    int total_pipes = total_workers;
    kz_pipe_fds = (int*) shm_malloc(sizeof(int) * (total_pipes) * 2 );

    for(i=0; i < total_pipes; i++) {
    	kz_pipe_fds[i*2] = kz_pipe_fds[i*2+1] = -1;
		if (pipe(&kz_pipe_fds[i*2]) < 0) {
			LM_ERR("pipe(%d) failed\n", i);
			return -1;
		}
    }

    register_procs(total_workers);
    cfg_register_child(total_workers);

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

	fire_init_event(rank);

	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;


	if (rank==PROC_MAIN) {
		pid=fork_process(2, "AMQP Consumer", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0){
			kz_amqp_consumer_proc(1);
		}
		else {
			pid=fork_process(1, "AMQP Publisher", 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0){
				kz_amqp_publisher_proc(0);
			}
			else {
				pid=fork_process(3, "AMQP Timer", 1);
				if (pid<0)
					return -1; /* error */
				if(pid==0){
					kz_amqp_timeout_proc(2);
				}
				else {
					for(i=0; i < dbk_consumer_processes; i++) {
						pid=fork_process(i+4, "AMQP Consumer Worker", 1);
						if (pid<0)
							return -1; /* error */
						if(pid==0){
							mod_consumer_proc(i+3);
						}
					}
				}
			}
		}
		return 0;
	}

	if(dbk_pua_mode == 1) {
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

static void  mod_consumer_proc(int rank)
{
	kz_amqp_consumer_loop(rank);
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
    shm_free(kz_pipe_fds);
}


