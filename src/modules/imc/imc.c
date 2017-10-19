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
#include "../../lib/srdb1/db_res.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/timer.h"
#include "../../core/str.h"
#include "../../core/mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
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
str imc_hdr_ctype = str_init("Content-Type: text/plain\r\n");
char hdr_buf[1024];
str all_hdrs;

/** parameters */

db1_con_t *imc_db = NULL;
db_func_t imc_dbf;
static str db_url  = str_init(DEFAULT_DB_URL);
str outbound_proxy = {NULL, 0};

static str rooms_table   = str_init("imc_rooms");
static str members_table = str_init("imc_members");

static str imc_col_username = str_init("username");
static str imc_col_domain   = str_init("domain");
static str imc_col_flag     = str_init("flag");
static str imc_col_room     = str_init("room");
static str imc_col_name     = str_init("name");

imc_hentry_p _imc_htable = NULL;
int imc_hash_size = 4;
str imc_cmd_start_str = str_init(IMC_CMD_START_STR);
char imc_cmd_start_char;
str extra_hdrs = {NULL, 0};

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
	{"hash_size",			INT_PARAM, &imc_hash_size},
	{"imc_cmd_start_char",	PARAM_STR, &imc_cmd_start_str},
	{"rooms_table",			PARAM_STR, &rooms_table},
	{"members_table",		PARAM_STR, &members_table},
	{"outbound_proxy",		PARAM_STR, &outbound_proxy},
	{"extra_hdrs",        PARAM_STR, &extra_hdrs},
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
#ifdef STATISTICS
	imc_stats,
#else
	0,          /* exported statistics */
#endif
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* mod init */
	0,          /* response handler */
	(destroy_function) destroy,  /* destroy function */
	child_init  /* child init */
};

/**
 * the initiating function
 */
int add_from_db(void)
{
	imc_member_p member = NULL;
	int i, j, flag;
	db_key_t mq_result_cols[4], mquery_cols[2];
	db_key_t rq_result_cols[4];
	db_val_t mquery_vals[2];
	db1_res_t *r_res= NULL;
	db1_res_t *m_res= NULL;
	db_row_t *m_row = NULL, *r_row = NULL;
	db_val_t *m_row_vals, *r_row_vals = NULL;
	str name, domain;
	imc_room_p room = NULL;
	int er_ret = -1;

	rq_result_cols[0] = &imc_col_name;
	rq_result_cols[1] = &imc_col_domain;
	rq_result_cols[2] = &imc_col_flag;

	mq_result_cols[0] = &imc_col_username;
	mq_result_cols[1] = &imc_col_domain;
	mq_result_cols[2] = &imc_col_flag;

	mquery_cols[0] = &imc_col_room;
	mquery_vals[0].type = DB1_STR;
	mquery_vals[0].nul = 0;

	if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
	{
		LM_ERR("use_table failed\n");
		return -1;
	}

	if(imc_dbf.query(imc_db,0, 0, 0, rq_result_cols,0, 3, 0,&r_res)< 0)
	{
		LM_ERR("failed to querry table\n");
		return -1;
	}
	if(r_res==NULL || r_res->n<=0)
	{
		LM_INFO("the query returned no result\n");
		if(r_res) imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
		return 0;
	}

	LM_DBG("found %d rooms\n", r_res->n);

	for(i =0 ; i< r_res->n ; i++)
	{
		/*add rooms*/
		r_row = &r_res->rows[i];
		r_row_vals = ROW_VALUES(r_row);

		name.s = 	r_row_vals[0].val.str_val.s;
		name.len = strlen(name.s);

		domain.s = 	r_row_vals[1].val.str_val.s;
		domain.len = strlen(domain.s);

		flag = 	r_row_vals[2].val.int_val;

		room = imc_add_room(&name, &domain, flag);
		if(room == NULL)
		{
			LM_ERR("failed to add room\n ");
			goto error;
		}

		/* add members */
		if(imc_dbf.use_table(imc_db, &members_table)< 0)
		{
			LM_ERR("use_table failed\n ");
			goto error;
		}

		mquery_vals[0].val.str_val= room->uri;

		if(imc_dbf.query(imc_db, mquery_cols, 0, mquery_vals, mq_result_cols,
					1, 3, 0, &m_res)< 0)
		{
			LM_ERR("failed to querry table\n");
			goto error;
		}

		if(m_res==NULL || m_res->n<=0)
		{
			LM_INFO("the query returned no result\n");
			er_ret = 0;
			goto error; /* each room must have at least one member*/
		}
		for(j =0; j< m_res->n; j++)
		{
			m_row = &m_res->rows[j];
			m_row_vals = ROW_VALUES(m_row);

			name.s = m_row_vals[0].val.str_val.s;
			name.len = strlen(name.s);

			domain.s = m_row_vals[1].val.str_val.s;
			domain.len = strlen(domain.s);

			flag = m_row_vals[2].val.int_val;

			LM_DBG("adding memeber: [name]=%.*s [domain]=%.*s"
					" in [room]= %.*s\n",name.len, name.s, domain.len,domain.s,
					room->uri.len, room->uri.s);

			member = imc_add_member(room, &name, &domain, flag);
			if(member == NULL)
			{
				LM_ERR("failed to adding member\n ");
				goto error;
			}
			imc_release_room(room);
		}

		if(m_res)
		{
			imc_dbf.free_result(imc_db, m_res);
			m_res = NULL;
		}
	}

	if(imc_dbf.use_table(imc_db, &members_table)< 0)
	{
		LM_ERR("use table failed\n ");
		goto error;
	}

	if(imc_dbf.delete(imc_db, 0, 0 , 0, 0) < 0)
	{
		LM_ERR("failed to delete information from db\n");
		goto error;
	}

	if(imc_dbf.use_table(imc_db, &rooms_table)< 0)
	{
		LM_ERR("use table failed\n ");
		goto error;
	}

	if(imc_dbf.delete(imc_db, 0, 0 , 0, 0) < 0)
	{
		LM_ERR("failed to delete information from db\n");
		goto error;
	}

	if(r_res)
	{
		imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
	}

	return 0;

error:
	if(r_res)
	{
		imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
	}
	if(m_res)
	{
		imc_dbf.free_result(imc_db, m_res);
		m_res = NULL;
	}
	if(room)
		imc_release_room(room);
	return er_ret;

}


static int mod_init(void)
{
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, imc_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if(imc_rpc_init()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(imc_hash_size <= 0)
	{
		LM_ERR("invalid hash size\n");
		return -1;
	}

	imc_hash_size = 1 << imc_hash_size;

	if(imc_htable_init() < 0)
	{
		LM_ERR("initializing hash table\n");
		return -1;
	}

	if (extra_hdrs.s) {
		if (extra_hdrs.len + imc_hdr_ctype.len > 1024) {
			LM_ERR("extra_hdrs too long\n");
			return -1;
		}
		all_hdrs.s = &(hdr_buf[0]);
		memcpy(all_hdrs.s, imc_hdr_ctype.s, imc_hdr_ctype.len);
		memcpy(all_hdrs.s + imc_hdr_ctype.len, extra_hdrs.s,
				extra_hdrs.len);
		all_hdrs.len = extra_hdrs.len + imc_hdr_ctype.len;
	} else {
		all_hdrs = imc_hdr_ctype;
	}

	/*  binding to mysql module */
	LM_DBG("db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);

	if (db_bind_mod(&db_url, &imc_dbf))
	{
		LM_DBG("database module not found\n");
		return -1;
	}

	imc_db = imc_dbf.init(&db_url);
	if (!imc_db)
	{
		LM_ERR("failed to connect to the database\n");
		return -1;
	}
	/* read the informations stored in db */
	if(add_from_db() <0)
	{
		LM_ERR("failed to get information from db\n");
		return -1;
	}

	/* load TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_ERR("unable to load tm api\n");
		return -1;
	}

	imc_cmd_start_char = imc_cmd_start_str.s[0];

	if(imc_db)
		imc_dbf.close(imc_db);
	imc_db = NULL;

	return 0;
}

/**
 * child init
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (imc_dbf.init==0)
	{
		LM_ERR("database not bound\n");
		return -1;
	}
	imc_db = imc_dbf.init(&db_url);
	if (!imc_db)
	{
		LM_ERR("child %d: Error while connecting database\n", rank);
		return -1;
	}
	else
	{
		if (imc_dbf.use_table(imc_db, &rooms_table) < 0)
		{
			LM_ERR("child %d: Error in use_table '%.*s'\n", rank, rooms_table.len, rooms_table.s);
			return -1;
		}
		if (imc_dbf.use_table(imc_db, &members_table) < 0)
		{
			LM_ERR("child %d: Error in use_table '%.*s'\n", rank, members_table.len, members_table.s);
			return -1;
		}

		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}


static int ki_imc_manager(struct sip_msg* msg)
{
	imc_cmd_t cmd;
	str body;
	struct sip_uri from_uri, *pto_uri=NULL, *pfrom_uri=NULL;
	struct to_body *pfrom;
	int ret = -1;

	body.s = get_body( msg );
	if (body.s==0)
	{
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

	if(parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse r-uri\n");
		goto error;
	}

	pto_uri=&msg->parsed_uri;

	if(parse_from_header(msg)<0)
	{
		LM_ERR("failed to parse  From header\n");
		goto error;
	}
	pfrom = (struct to_body*)msg->from->parsed;
	if(parse_uri(pfrom->uri.s, pfrom->uri.len, &from_uri)<0){
		LM_ERR("failed to parse From URI\n");
		goto error;
	}
	pfrom_uri=&from_uri;

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
			if(imc_handle_create(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'create'\n");
				ret = -30;
				goto error;
			}
		break;
		case IMC_CMDID_JOIN:
			if(imc_handle_join(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'join'\n");
				ret = -40;
				goto error;
			}
		break;
		case IMC_CMDID_INVITE:
			if(imc_handle_invite(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'invite'\n");
				ret = -50;
				goto error;
			}
		break;
		case IMC_CMDID_ACCEPT:
			if(imc_handle_accept(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'accept'\n");
				ret = -60;
				goto error;
			}
		break;
		case IMC_CMDID_DENY:
			if(imc_handle_deny(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'deny'\n");
				ret = -70;
				goto error;
			}
		break;
		case IMC_CMDID_REMOVE:
			if(imc_handle_remove(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'remove'\n");
				ret = -80;
				goto error;
			}
		break;
		case IMC_CMDID_EXIT:
			if(imc_handle_exit(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'exit'\n");
				ret = -90;
				goto error;
			}
		break;
		case IMC_CMDID_LIST:
			if(imc_handle_list(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'list'\n");
				ret = -100;
				goto error;
			}
		break;
		case IMC_CMDID_DESTROY:
			if(imc_handle_destroy(msg, &cmd, pfrom_uri, pto_uri)<0)
			{
				LM_ERR("failed to handle 'destroy'\n");
				ret = -110;
				goto error;
			}
		break;
		case IMC_CMDID_HELP:
			if(imc_handle_help(msg, &cmd, &pfrom->uri,
			(msg->new_uri.s)?&msg->new_uri:&msg->first_line.u.request.uri)<0)
			{
				LM_ERR("failed to handle 'help'\n");
				ret = -120;
				goto error;
			}
		break;
		default:
			if(imc_handle_unknown(msg, &cmd, &pfrom->uri,
			(msg->new_uri.s)?&msg->new_uri:&msg->first_line.u.request.uri)<0)
			{
				LM_ERR("failed to handle 'unknown'\n");
				ret = -130;
				goto error;
			}
		}

		goto done;
	}

	if(imc_handle_message(msg, &body, pfrom_uri, pto_uri)<0)
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

			if(imc_dbf.insert(imc_db, rq_cols, rq_vals, 3)<0)
			{
				LM_ERR("failed to insert into table imc_rooms\n");
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

				if(imc_dbf.insert(imc_db, mq_cols, mq_vals, 4)<0)
				{
					LM_ERR("failed to insert  into table imc_rooms\n");
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
					"owner", &irp->members->uri);

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
