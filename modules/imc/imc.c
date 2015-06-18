/*
 * $Id$
 *
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
 * History:
 * ---------
 *  2006-10-06  first version (anca)
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
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../str.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../resolve.h"
#include "../../hashes.h"
#include "../../lib/kmi/mi.h"

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

static int imc_manager(struct sip_msg*, char *, char *);

static struct mi_root* imc_mi_list_rooms(struct mi_root* cmd, void* param);
static struct mi_root* imc_mi_list_members(struct mi_root* cmd, void* param);

static void destroy(void);

/** TM bind */
struct tm_binds tmb;

/** TM callback function */
void inv_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"imc_manager",  (cmd_function)imc_manager, 0, 0, 0, REQUEST_ROUTE},
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
#include "../../lib/kcore/statistics.h"

stat_var* imc_active_rooms;

stat_export_t imc_stats[] = {
	{"active_rooms" ,  0,  &imc_active_rooms  },
	{0,0,0}
};

#endif

static mi_export_t mi_cmds[] = {
	{ "imc_list_rooms",    imc_mi_list_rooms,    MI_NO_INPUT_FLAG,  0,  0 },
	{ "imc_list_members",  imc_mi_list_members,  0,                 0,  0 },
	{ 0, 0, 0, 0, 0}
};



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
	mi_cmds,    /* exported MI functions */
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

		if(m_res && m_res->n <=0)
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
	if(m_res)
	{	
		imc_dbf.free_result(imc_db, m_res);
		m_res = NULL;
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
	
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
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


static int imc_manager(struct sip_msg* msg, char *str1, char *str2)
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


/************************* MI ***********************/
static struct mi_root* imc_mi_list_rooms(struct mi_root* cmd_tree, void* param)
{
	int i, len;
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl= NULL;
	struct mi_node* node= NULL;
	struct mi_attr* attr= NULL;
	imc_room_p irp = NULL;
	char* p = NULL;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		return 0;
	rpl = &rpl_tree->node;

	for(i=0; i<imc_hash_size; i++) 
	{
		lock_get(&_imc_htable[i].lock);
		irp = _imc_htable[i].rooms;
			while(irp){
				node = add_mi_node_child(rpl, 0, "ROOM", 4, 0, 0);
				if( node == NULL)
					goto error;

				attr= add_mi_attr(node, MI_DUP_VALUE, "URI", 3, irp->uri.s,
					irp->uri.len);
				if(attr == NULL)
					goto error;

				p = int2str(irp->nr_of_members, &len);
				attr= add_mi_attr(node, 0, "MEMBERS", 7,p, len );
				if(attr == NULL)
					goto error;

				attr= add_mi_attr(node, MI_DUP_VALUE, "OWNER", 5, 
						irp->members->uri.s, irp->members->uri.len);
				if(attr == NULL)
					goto error;
					
				irp = irp->next;
			}
		lock_release(&_imc_htable[i].lock);
	}

	return rpl_tree;

error:
	lock_release(&_imc_htable[i].lock);
	free_mi_tree(rpl_tree);
	return 0;

}


static struct mi_root* imc_mi_list_members(struct mi_root* cmd_tree,
																void* param)
{
	int i, len;
	struct mi_root* rpl_tree = NULL;
	struct mi_node* node= NULL;
	struct mi_node* node_r= NULL;
	struct mi_attr* attr= NULL;
	char rnbuf[256];
	str room_name;
	imc_room_p room;
	struct sip_uri inv_uri, *pinv_uri;
	imc_member_p imp=NULL;
	char* p = NULL;

	node= cmd_tree->node.kids;
	if(node == NULL|| node->next!=NULL)
		return 0;
	
	/* room name */
	room_name.s = rnbuf;
	room_name.len= node->value.len;
	memcpy(room_name.s, node->value.s, node->value.len);
	if(room_name.s == NULL || room_name.len == 0)
	{
		LM_ERR(" no room name!\n");
		return init_mi_tree( 404, "room name not found", 19);
	}
	rnbuf[room_name.len] = '\0';
	if(*room_name.s=='\0' || *room_name.s=='.')
	{
		LM_INFO("empty room name\n");
		return init_mi_tree( 400, "empty param", 11);
	}

	/* find room */
	parse_uri(room_name.s,room_name.len, &inv_uri);
	pinv_uri=&inv_uri;
	room=imc_get_room(&pinv_uri->user, &pinv_uri->host);

	if(room==NULL)
	{
		LM_ERR("no such room!\n");
		return init_mi_tree( 404, "no such room", 14);
	}

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		return 0;

	node_r = add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE, "ROOM", 4,
		room_name.s, room_name.len);
	if(node_r == NULL)
		goto error;
	

	imp = room->members;
	i=0;
	while(imp)
	{
		i++;
		node = add_mi_node_child(node_r, MI_DUP_VALUE, "MEMBER",6, imp->uri.s,
			imp->uri.len);
		if(node == NULL)
			goto error;
		imp = imp->next;
	}
	
	p = int2str(i, &len);
	attr= add_mi_attr(node_r, MI_DUP_VALUE, "NR_OF_MEMBERS", 13, p, len);
	if(attr == 0)
		goto error;

	imc_release_room(room);

	return rpl_tree;

error:
	imc_release_room(room);
	free_mi_tree(rpl_tree);
	return 0;
}
