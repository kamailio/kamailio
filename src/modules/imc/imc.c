/*
 * imc module - instant messaging conferencing implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "../../lib/srdb1/db.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/timer.h"
#include "../../core/str.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/resolve.h"
#include "../../core/hashes.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "../../modules/tm/tm_load.h"


#include "imc_mng.h"
#include "imc_cmd.h"

MODULE_VERSION

/** header variables */
str imc_hdrs = str_init("Supported: kamailio/imc\r\n");
char hdr_buf[1024];
str all_hdrs;

/** parameters */
db1_con_t *imc_db = NULL;
db_func_t imc_dbf;

static str db_url  = str_init(DEFAULT_DB_URL);
int db_mode = 0;

str rooms_table   = str_init("imc_rooms");
str members_table = str_init("imc_members");

str imc_col_username = str_init("username");
str imc_col_domain   = str_init("domain");
str imc_col_flag     = str_init("flag");
str imc_col_room     = str_init("room");
str imc_col_name     = str_init("name");

str outbound_proxy = {NULL, 0};

imc_hentry_p _imc_htable = NULL;
int imc_hash_size = 4;
str imc_cmd_start_str = str_init(IMC_CMD_START_STR);
char imc_cmd_start_char;
str extra_hdrs = {NULL, 0};
int imc_create_on_join = 1;
int imc_check_on_create = 0;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int w_imc_manager(struct sip_msg*, char *, char *);

static int imc_rpc_init(void);

static void destroy(void);

/** TM bind */
struct tm_binds tmb;

/** TM callback function */
void inv_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"imc_manager",  (cmd_function)w_imc_manager, 0, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"db_url",				PARAM_STR, &db_url},
	{"db_mode", 			INT_PARAM, &db_mode},
	{"hash_size",			INT_PARAM, &imc_hash_size},
	{"imc_cmd_start_char",	PARAM_STR, &imc_cmd_start_str},
	{"rooms_table",			PARAM_STR, &rooms_table},
	{"members_table",		PARAM_STR, &members_table},
	{"outbound_proxy",		PARAM_STR, &outbound_proxy},	
	{"extra_hdrs",        PARAM_STR, &extra_hdrs},
	{"create_on_join", INT_PARAM, &imc_create_on_join},
	{"check_on_create", INT_PARAM, &imc_check_on_create},
	{0,0,0}
};

#ifdef STATISTICS
#include "../../core/counters.h"

stat_var* imc_active_rooms;

stat_export_t imc_stats[] = {
	{"active_rooms" ,  0,  &imc_active_rooms  },
	{0,0,0}
};

#endif


/** module exports */
struct module_exports exports= {
	"imc",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported commands */
	params,     /* exported parameters */
	0,          /* exported rpc functions */
	0,          /* exported pseudo-variables */
	0,          /* response handling function */
	mod_init,   /* module init function */
	child_init, /* child init function */
	destroy     /* module destroy function */
};

static int mod_init(void)
{
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, imc_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if(imc_rpc_init()<0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(imc_hash_size <= 0) {
		LM_ERR("invalid hash size\n");
		return -1;
	}

	imc_hash_size = 1 << imc_hash_size;

	if(imc_htable_init() < 0) {
		LM_ERR("initializing hash table\n");
		return -1;
	}

	if (extra_hdrs.s) {
		if (extra_hdrs.len + imc_hdrs.len > 1024) {
			LM_ERR("extra_hdrs too long\n");
			return -1;
		}
		all_hdrs.s = &(hdr_buf[0]);
		memcpy(all_hdrs.s, imc_hdrs.s, imc_hdrs.len);
		memcpy(all_hdrs.s + imc_hdrs.len, extra_hdrs.s,
				extra_hdrs.len);
		all_hdrs.len = extra_hdrs.len + imc_hdrs.len;
	} else {
		all_hdrs = imc_hdrs;
	}	

	if(db_mode == 2) {
		/*  binding to mysql module */
		LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);

		if (db_bind_mod(&db_url, &imc_dbf)) {
			LM_DBG("database module not found\n");
			return -1;
		}

		imc_db = imc_dbf.init(&db_url);
		if (!imc_db) {
			LM_ERR("failed to connect to the database\n");
			return -1;
		}

		/* read the informations stored in db */
		if (load_rooms_from_db() < 0) {
			LM_ERR("failed to get information from db\n");
			return -1;
		}

		if(imc_db)
			imc_dbf.close(imc_db);

		imc_db = NULL;
	}	

	/* load TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("unable to load tm api\n");
		return -1;
	}

	imc_cmd_start_char = imc_cmd_start_str.s[0];	

	return 0;
}

/**
 * child init
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (db_mode == 2) {
		if (imc_dbf.init == 0) {
			LM_ERR("database not bound\n");
			return -1;
		}
		
		imc_db = imc_dbf.init(&db_url);		
		if (!imc_db) {
			LM_ERR("child %d: Error while connecting database\n", rank);
			return -1;
		}
		else {
			if (imc_dbf.use_table(imc_db, &rooms_table) < 0) {
				LM_ERR("child %d: Error in use_table '%.*s'\n", rank, STR_FMT(&rooms_table));
				return -1;
			}
			if (imc_dbf.use_table(imc_db, &members_table) < 0) {
				LM_ERR("child %d: Error in use_table '%.*s'\n", rank, STR_FMT(&members_table));
				return -1;
			}

			LM_DBG("child %d: Database connection opened successfully\n", rank);
		}
	}	

	return 0;
}


static int ki_imc_manager(struct sip_msg* msg)
{
	imc_cmd_t cmd;
	str body;
	struct imc_uri src, dst;
	int ret = -1;

	body.s = get_body( msg );
	if (body.s==0) {
		LM_ERR("cannot extract body from msg\n");
		goto error;
	}

	/* lungimea corpului mesajului */
	if (!msg->content_length)
	{
		LM_ERR("no Content-Length\n");
		goto error;
	}
	body.len = get_content_length( msg );

	if(body.len <= 0)
	{
		LM_DBG("empty body!\n");
		goto error;
	}

	dst.uri = *GET_RURI(msg);
	if(parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse r-uri\n");
		goto error;
	}
	dst.parsed = msg->parsed_uri;

	if(parse_from_header(msg)<0)
	{
		LM_ERR("failed to parse From header\n");
		goto error;
	}
	src.uri = ((struct to_body*)msg->from->parsed)->uri;
	if (parse_uri(src.uri.s, src.uri.len, &src.parsed)<0){
		LM_ERR("failed to parse From URI\n");
		goto error;
	}

	if(body.s[0]== imc_cmd_start_char)
	{
		LM_DBG("found command\n");
		if(imc_parse_cmd(body.s, body.len, &cmd)<0)
		{
			LM_ERR("failed to parse imc cmd!\n");
			ret = -20;
			goto error;
		}

		switch(cmd.type)
		{
		case IMC_CMDID_CREATE:
			if(imc_handle_create(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'create'\n");
				ret = -30;
				goto error;
			}			
		break;
		case IMC_CMDID_JOIN:
			if(imc_handle_join(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'join'\n");
				ret = -40;
				goto error;
			}
		break;
		case IMC_CMDID_INVITE:
			if(imc_handle_invite(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'invite'\n");
				ret = -50;
				goto error;
			}
		break;
		case IMC_CMDID_ADD:
			if(imc_handle_add(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'add'\n");
				ret = -50;
				goto error;
			}			
		break;
		case IMC_CMDID_ACCEPT:
			if(imc_handle_accept(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'accept'\n");
				ret = -60;
				goto error;
			}
		break;
		case IMC_CMDID_REJECT:
			if(imc_handle_reject(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'reject'\n");
				ret = -70;
				goto error;
			}
		break;
		case IMC_CMDID_REMOVE:
			if(imc_handle_remove(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'remove'\n");
				ret = -80;
				goto error;
			}			
		break;
		case IMC_CMDID_LEAVE:
			if(imc_handle_leave(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'leave'\n");
				ret = -90;
				goto error;
			}
		break;
		case IMC_CMDID_MEMBERS:
			if(imc_handle_members(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'members'\n");
				ret = -100;
				goto error;
			}
		break;
		case IMC_CMDID_ROOMS:
			if(imc_handle_rooms(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'rooms'\n");
				ret = -100;
				goto error;
			}
		break;
		case IMC_CMDID_DESTROY:
			if(imc_handle_destroy(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'destroy'\n");
				ret = -110;
				goto error;
			}
		break;
		case IMC_CMDID_MODIFY:
			if(imc_handle_modify(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'modify'\n");
				ret = -120;
				goto error;
			}
		break;
		case IMC_CMDID_HELP:
			if(imc_handle_help(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'help'\n");
				ret = -120;
				goto error;
			}
		break;
		default:
			if(imc_handle_unknown(msg, &cmd, &src, &dst)<0)
			{
				LM_ERR("failed to handle 'unknown'\n");
				ret = -130;
				goto error;
			}
		}

		goto done;
	}	

	if(imc_handle_message(msg, &body, &src, &dst)<0)
	{
		LM_ERR("failed to handle 'message'\n");
		ret = -200;
		goto error;
	}

done:
	return 1;

error:
	return ret;
}

static int w_imc_manager(struct sip_msg* msg, char *str1, char *str2)
{
	return ki_imc_manager(msg);
}

/**
 * destroy module
 */
static void destroy(void)
{
	imc_room_p irp = NULL;
	imc_member_p member = NULL;
	int i;
	db_key_t mq_cols[4];
	db_val_t mq_vals[4];
	db_key_t rq_cols[4];
	db_val_t rq_vals[4];

	if (db_mode == 0)
		goto done;

	if(imc_db==NULL)
		goto done;

	mq_cols[0] = &imc_col_username;
	mq_vals[0].type = DB1_STR;
	mq_vals[0].nul = 0;

	mq_cols[1] = &imc_col_domain;
	mq_vals[1].type = DB1_STR;
	mq_vals[1].nul = 0;

	mq_cols[2] = &imc_col_flag;
	mq_vals[2].type = DB1_INT;
	mq_vals[2].nul = 0;

	mq_cols[3] = &imc_col_room;
	mq_vals[3].type = DB1_STR;
	mq_vals[3].nul = 0;


	rq_cols[0] = &imc_col_name;
	rq_vals[0].type = DB1_STR;
	rq_vals[0].nul = 0;

	rq_cols[1] = &imc_col_domain;
	rq_vals[1].type = DB1_STR;
	rq_vals[1].nul = 0;

	rq_cols[2] = &imc_col_flag;
	rq_vals[2].type = DB1_INT;
	rq_vals[2].nul = 0;

	for(i=0; i<imc_hash_size; i++)
	{
		irp = _imc_htable[i].rooms;

		while(irp)
		{
			rq_vals[0].val.str_val = irp->name;
			rq_vals[1].val.str_val = irp->domain;
			rq_vals[2].val.int_val = irp->flags;

			if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
			{
				LM_ERR("use_table failed\n");
				return;
			}

			if(imc_dbf.replace(imc_db, rq_cols, rq_vals, 3, 2, 0)<0)
			{
				LM_ERR("failed to replace into table imc_rooms\n");
				return;
			}
			LM_DBG("room %d %.*s\n", i, irp->name.len, irp->name.s);
			member = irp->members;
			while(member)
			{
				mq_vals[0].val.str_val = member->user;
				mq_vals[1].val.str_val = member->domain;
				mq_vals[2].val.int_val = member->flags;
				mq_vals[3].val.str_val = irp->uri;

				if(imc_dbf.use_table(imc_db, &members_table)< 0)
				{
					LM_ERR("use_table failed\n");
					return;
				}

				if(imc_dbf.replace(imc_db, mq_cols, mq_vals, 4, 2, 0)<0)
				{
					LM_ERR("failed to replace  into table imc_rooms\n");
					return;
				}
				member = member->next;
			}
			irp = irp->next;
		}
	}

done:
	imc_htable_destroy();
}


/************************* RPC ***********************/
static void  imc_rpc_list_rooms(rpc_t* rpc, void* ctx)
{
	int i;
	imc_room_p irp = NULL;
	void *vh;
	static str unknown = STR_STATIC_INIT("");

	for(i=0; i<imc_hash_size; i++)
	{
		lock_get(&_imc_htable[i].lock);
		irp = _imc_htable[i].rooms;
		while(irp){
			if (rpc->add(ctx, "{", &vh) < 0) {
				lock_release(&_imc_htable[i].lock);
				rpc->fault(ctx, 500, "Server error");
				return;
			}
			rpc->struct_add(vh, "SdS",
					"room", &irp->uri,
					"members", irp->nr_of_members,
					"owner", (irp->nr_of_members > 0) ? &irp->members->uri : &unknown);

			irp = irp->next;
		}
		lock_release(&_imc_htable[i].lock);
	}

}

static void  imc_rpc_list_members(rpc_t* rpc, void* ctx)
{
	imc_room_p room = NULL;
	void *vh;
	void *ih;
	struct sip_uri inv_uri, *pinv_uri;
	imc_member_p imp=NULL;
	str room_name;

	if (rpc->scan(ctx, "S", &room_name) < 1) {
		rpc->fault(ctx, 500, "No room name");
		return;
	}
	if(room_name.s == NULL || room_name.len == 0
			|| *room_name.s=='\0' || *room_name.s=='.') {
		LM_ERR("empty room name!\n");
		rpc->fault(ctx, 500, "Empty room name");
		return;
	}
	/* find room */
	if(parse_uri(room_name.s,room_name.len, &inv_uri)<0) {
		LM_ERR("invalid room name!\n");
		rpc->fault(ctx, 500, "Invalid room name");
		return;
	}
	pinv_uri=&inv_uri;
	room=imc_get_room(&pinv_uri->user, &pinv_uri->host);

	if(room==NULL) {
		LM_ERR("no such room!\n");
		rpc->fault(ctx, 500, "Room not found");
		return;
	}
	if (rpc->add(ctx, "{", &vh) < 0) {
		imc_release_room(room);
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "S[d",
			"room", &room->uri,
			"members", &ih,
			"count", room->nr_of_members);

	imp = room->members;
	while(imp) {
		rpc->array_add(ih, "S", &imp->uri);
		imp = imp->next;
	}
	imc_release_room(room);
}

static const char* imc_rpc_list_rooms_doc[2] = {
	"List imc rooms.",
	0
};

static const char* imc_rpc_list_members_doc[2] = {
	"List members in an imc room.",
	0
};

rpc_export_t imc_rpc[] = {
	{"imc.list_rooms", imc_rpc_list_rooms, imc_rpc_list_rooms_doc, RET_ARRAY},
	{"imc.list_members", imc_rpc_list_members, imc_rpc_list_members_doc, 0},
	{0, 0, 0, 0}
};

static int imc_rpc_init(void)
{
	if (rpc_register_array(imc_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_imc_exports[] = {
	{ str_init("imc"), str_init("imc_manager"),
		SR_KEMIP_INT, ki_imc_manager,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_imc_exports);
	return 0;
}
