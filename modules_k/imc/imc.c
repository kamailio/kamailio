/*
 * $Id$
 *
 * imc module - instant messaging conferencing implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../db/db.h"
#include "../../db/db_res.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../parser/contact/parse_contact.h"
#include "../../resolve.h"
#include "../../hash_func.h"
#include "../../fifo_server.h"
#include "../../mi/mi.h"

#include "../tm/tm_load.h"


#include "imc_mng.h"

MODULE_VERSION

#define IMC_HELP_MSG	"\r\n#create <room_name> - create new connference room\r\n\
#join [<room_name>] - join the conference room\r\n\
#invite <user_name> [<room_name>] - invite a user to join a conference room\r\n\
#accept - accept invitation to join a conference room\r\n\
#deny - deny invitation to join a conference room\r\n\
#remove <user_name> [<room_name>] - remove an user from the conference room\r\n\
#exit [<room_name>] - exit from a conference room\r\n\
#destroy [<room_name>] - destroy conference room\r\n"

#define IMC_HELP_MSG_LEN (sizeof(IMC_HELP_MSG)-1)

/** parameters */

str msg_type = { "MESSAGE", 7 };
db_con_t *imc_db = NULL;
db_func_t imc_dbf;
str db_url;

char* room_table = "imc_rooms";
char* member_table = "imc_members";

imc_hentry_p _imc_htable = NULL;
int imc_hash_size = 4;
char *imc_cmd_start_str= NULL ;
char imc_cmd_start_char;

/** module functions */
static int mod_init(void);
static int child_init(int);

static int imc_manager(struct sip_msg*, char *, char *);
static int imc_list_rooms(FILE *stream, char *response_file);
static int imc_list_members(FILE *stream, char *response_file);

static struct mi_root* imc_mi_list_rooms(struct mi_root* cmd, void* param);
static struct mi_root* imc_mi_list_members(struct mi_root* cmd, void* param);

void destroy(void);
int imc_list_randm(FILE *stream);
int imc_list_members2(FILE* stream, imc_room_p room);

/** TM bind */
struct tm_binds tmb;

/** TM callback function */
void inv_callback( struct cell *t, int type, struct tmcb_params *ps);

static cmd_export_t cmds[]={
	{"imc_manager",  imc_manager, 0, 0, REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"db_url",				STR_PARAM, &db_url.s},
	{"hash_size",			INT_PARAM, &imc_hash_size},
	{"imc_cmd_start_char",	STR_PARAM, &imc_cmd_start_str},	
	{0,0,0}
};

#ifdef STATISTICS
#include "../../statistics.h"

stat_var* imc_active_rooms;

stat_export_t imc_stats[] = {
	{"active_rooms" ,  0,  &imc_active_rooms  },
	{0,0,0}
};

#endif

static mi_export_t mi_cmds[] = {
	{ "imc_list_rooms",    imc_mi_list_rooms,    0,  0 },
	{ "imc_list_members",  imc_mi_list_members,  0,  0 },
	{ 0, 0, 0, 0}
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
	mod_init,   /* mod init */
	(response_function) 0,       /* response handler */
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
	db_res_t *r_res= NULL;
	db_res_t *m_res= NULL;
	db_row_t *m_row = NULL, *r_row = NULL;	
	db_val_t *m_row_vals, *r_row_vals = NULL;
	str name, domain;
	imc_room_p room = NULL;
	int er_ret = -1;
	
	rq_result_cols[0] ="name";		
	rq_result_cols[1] ="domain";	
	rq_result_cols[2] ="flag";

	mq_result_cols[0] ="user";		
	mq_result_cols[1] ="domain";
	mq_result_cols[2] ="flag";

	mquery_cols[0] = "room";
	mquery_vals[0].type = DB_STR;
	mquery_vals[0].nul = 0;


	if(imc_dbf.use_table(imc_db, room_table)< 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR in use table\n");
		return -1;
	}

	if(imc_dbf.query(imc_db,0, 0, 0, rq_result_cols,0, 3, 0,&r_res)< 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR while querrying table\n");
		return -1;
	}
	if(r_res && r_res->n<=0)
	{
		LOG(L_INFO, "imc:mod_init:the query returned no result\n");
		imc_dbf.free_result(imc_db, r_res);
		r_res = NULL;
		return 0;
	}

	DBG("imc:mod_init: found %d rooms\n", r_res->n);

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
			LOG(L_ERR, "imc:mod_init:ERROR while adding room\n ");
			goto error;		
		}	
	
		/* add members */
		if(imc_dbf.use_table(imc_db, member_table)< 0)
		{
			LOG(L_ERR, "imc:mod_init:ERROR in use table\n ");
			goto error;
		}

		mquery_vals[0].val.str_val= room->uri;
		
		if(imc_dbf.query(imc_db, mquery_cols, 0, mquery_vals, mq_result_cols, 
					1, 3, 0, &m_res)< 0)
		{
			LOG(L_ERR, "imc:mod_init:ERROR while querrying table\n");
			goto error;
		}

		if(m_res && m_res->n <=0)
		{
			LOG(L_INFO, "imc:mod_init:the query returned no result\n");
			er_ret = 0;
			goto error; /* each room must have at least one member*/
		}
		for(j =0; j< m_res->n; j++)
		{
			m_row = &m_res->rows[j];
			m_row_vals = ROW_VALUES(m_row);
			
			name.s =m_row_vals[0].val.str_val.s;
			name.len = strlen(name.s);
			
			domain.s = 	m_row_vals[1].val.str_val.s;
			domain.len = strlen(domain.s);
			
			flag = 	m_row_vals[2].val.int_val	;
			
			DBG("imc:mod_init: Adding memeber: [name]=%.*s [domain]=%.*s"
					" in [room]= %.*s\n",name.len, name.s, domain.len,domain.s,
					room->uri.len, room->uri.s);

			member = imc_add_member(room, &name, &domain, flag);
			if(member == NULL)
			{
				LOG(L_ERR, "imc:mod_init:ERROR while adding member\n ");
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

	if(imc_dbf.use_table(imc_db, member_table)< 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR in use table\n ");
		goto error;
	}

	if(imc_dbf.delete(imc_db, 0, 0 , 0, 0) < 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR while deleting information from db\n");
		goto error;
	}
	
	if(imc_dbf.use_table(imc_db, room_table)< 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR in use table\n ");
		goto error;
	}

	if(imc_dbf.delete(imc_db, 0, 0 , 0, 0) < 0)
	{
		LOG(L_ERR, "imc:mod_init:ERROR while deleting information from db\n");
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
	DBG("imc: initializing ...\n");

	if(imc_hash_size<=0)
	{
		LOG(L_ERR, "imc:mod_init: error - invalid hash size\n");
		return -1;
	}

	imc_hash_size = 1<<imc_hash_size;

	if(imc_htable_init()<0)
	{
		LOG(L_ERR, "IMC:mod_init: error- initializing hash table\n");
		return -1;
	}

	if(imc_cmd_start_str == NULL)
		imc_cmd_start_str = "#";

	/*  binding to mysql module */	
	
	if(	db_url.s == NULL)
	{
		LOG(L_ERR, "imc:mod_init: ERROR no db url found\n");
		return -1;
	}

	db_url.len = db_url.s ? strlen(db_url.s) : 0;
	DBG("imc:mod_init: db_url=%s/%d/%p\n", ZSW(db_url.s), db_url.len, db_url.s);
	
	if (bind_dbmod(db_url.s, &imc_dbf))
	{
		DBG("imc:mod_init: ERROR: Database module not found\n");
		return -1;
	}

	imc_db = imc_dbf.init(db_url.s);
	if (!imc_db)
	{
		LOG(L_ERR,"imc:mod_init: Error while connecting database\n");
		return -1;
	}
	/* read the informations stored in db */
	if(add_from_db() <0)
	{
		LOG(L_ERR, "IMC:mod_init: error while getting information from db\n");
		return -1;
	}
	imc_list_randm(stdout);
	
	/* incarcare TM API */
	if (load_tm_api(&tmb)!=0) {
		LOG(L_ERR, "ERROR:imc:mod_init: error - unable to load tm api\n");
		return -1;
	}

	if(register_fifo_cmd(imc_list_rooms, "imc_list_rooms", 0)<0)
	{
		LOG(L_ERR, "IMC:mod_init: error - unable to register fifo cmd" 
			" 'imc_list_rooms'\n");
		return -1;
	}

	if(register_fifo_cmd(imc_list_members, "imc_list_members", 0)<0)
	{
		LOG(L_ERR,"IMC:mod_init: error - unable to register fifo cmd"
			"'imc_list_members'\n");
		return -1;
	}

	if(imc_cmd_start_str)
		imc_cmd_start_char = imc_cmd_start_str[0];
	
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
	if (imc_dbf.init==0)
	{
		LOG(L_CRIT, "imc: child_init: database not bound\n");
		return -1;
	}
	imc_db = imc_dbf.init(db_url.s);
	if (!imc_db)
	{
		LOG(L_ERR,"imc: child %d: Error while connecting database\n",
				rank);
		return -1;
	}
	else
	{
		if (imc_dbf.use_table(imc_db, room_table) < 0)  
		{
			LOG(L_ERR, "imc: child %d: Error in use_table %s\n", rank, 
					room_table);
			return -1;
		}
		if (imc_dbf.use_table(imc_db, member_table) < 0)  
		{
			LOG(L_ERR, "imc: child %d: Error in use_table %s\n", rank, 
					member_table);
			return -1;
		}

		DBG("imc: child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}


static int imc_manager(struct sip_msg* msg, char *str1, char *str2)
{
	str body, body_invit, body_final;
	static char from_uri_buf[256];
	static char to_uri_buf[256];
	static char body_buf[256];
	str from_uri_s, to_uri_s;
	imc_cmd_t cmd, *p_cmd= NULL;
	imc_room_p room;
	imc_member_p member = NULL, member_inv=NULL, imp=NULL;
	struct sip_uri from_uri, inv_uri, *pto_uri, *pfrom_uri, *pinv_uri= NULL;
	struct to_body *pfrom;
	int flag_member=0, flag_room=0, deleted_user=0, room_deleted=0, room_released=0;
	int action = 0;
	int is_command = 0;
	str uri= {NULL, 0};
	del_member_t* cback_param = NULL;
	int size = 0, add_domain= 1, add_sip= 0, i= 0;

	room = NULL;
	body.s = get_body( msg );
	if (body.s==0) 
	{
		LOG(L_ERR,"imc:imc_manager: error - cannot extract body from msg\n");
		goto error;
	}
	
	/* lungimea corpului mesajului */
	if (!msg->content_length) 
	{
		LOG(L_ERR,"imc:imc_manager: error - no Content-Length\n");
		goto error;
	}
	body.len = get_content_length( msg );

	if(body.len <= 0)
	{
		DBG("imc:imc_manager: empty body!\n");
		goto error;
	}

	if(parse_sip_msg_uri(msg)<0)
	{
		LOG(L_ERR,"imc:imc_manager: error - parsing r-uri\n");
		goto error;
	}
	
	pto_uri=&msg->parsed_uri;
	
	if(parse_from_header(msg)<0)
	{
		LOG(L_ERR,"imc:imc_manager: error - parsing From header\n");
		goto error;
	}
	pfrom = (struct to_body*)msg->from->parsed;	
	if(parse_uri(pfrom->uri.s, pfrom->uri.len, &from_uri)<0){
		LOG(L_ERR,"imc:imc_manager: error - parsing From URI\n");
		goto error;
	}
	pfrom_uri=&from_uri;

	if(body.s[0]== imc_cmd_start_char)
	{
		DBG("imc:imc_manager: found command\n");
		is_command = 1;	
		if(imc_parse_cmd(body.s, body.len, &cmd)<0)
		{
			LOG(L_ERR,"imc:imc_manager: error parsing imc cmd!\n");
			goto error;
		}
		p_cmd=&cmd;
		if(cmd.name.len==(sizeof("create")-1)
				&& !strncasecmp(cmd.name.s, "create", cmd.name.len))
		{
			DBG("imc:imc_manager: Create\n");
			room= imc_get_room(&p_cmd->param[0], &pto_uri->host);
			if(room== NULL)
			{
				DBG("imc:imc_manager:  room did not previously exist\n");
				if(cmd.param[1].len==(sizeof("private")-1)
				&& !strncasecmp(cmd.param[1].s, "private", cmd.param[1].len))
				{
					flag_room |= IMC_ROOM_PRIV;					
					DBG("imc:imc_manager: found private parameter\n");
				}
				
				room= imc_add_room(&p_cmd->param[0], &pto_uri->host, flag_room);
				if(room == NULL)
				{
					LOG(L_ERR, "imc:imc_manager: ERROR while adding new room\n");
					goto error;
				}	
				DBG("imc:imc_manager:  added room uri= %.*s\n", room->uri.len, room->uri.s);
				flag_member |= IMC_MEMBER_OWNER;
				/* adding the owner as the forst member*/
				member= imc_add_member(room, &pfrom_uri->user,
						&pfrom_uri->host, flag_member);
				if(member == NULL)
				{
					LOG(L_ERR, "imc:imc_manager: ERROR while adding new room\n");
					goto error;
				}
				DBG("imc:imc_manager: added the owner as the first member "
						"[uri]= %.*s\n",member->uri.len, member->uri.s);
				
				goto build_inform;	
			}
			else
			{
				DBG("imc:imc_manager: room previously existed\n");
				if(!(room->flags & IMC_ROOM_PRIV)) 
				{
					DBG("imc:imc_manager: checking if the user is a member\n");
					member= imc_get_member(room, &pfrom_uri->user,
							&pfrom_uri->host);
					if(member== NULL)
					{					
						member= imc_add_member(room, &pfrom_uri->user,
								&pfrom_uri->host, flag_member);
						if(member == NULL)
						{
							LOG(L_ERR, "imc:imc_manager: ERROR while adding new room\n");
							goto error;
						}
						DBG("imc:imc_manager: adding the user as a member\n");
					}				
				}
			}
		} 
		else 
			if(cmd.name.len==(sizeof("join")-1)
				&& !strncasecmp(cmd.name.s, "join", cmd.name.len))
			{
				DBG("JOIN\n");
				room= imc_get_room((p_cmd->param[0].s)?&p_cmd->param[0]:
						&pto_uri->user,	&pto_uri->host);
				if(room== NULL)
				{			
					DBG("could not find room\nadd a new room\n");
					room= imc_add_room((p_cmd->param[0].s)?&p_cmd->param[0]:
							&pto_uri->user, &pto_uri->host, flag_room);
					if(room == NULL)
					{
						LOG(L_ERR, "imc:imc_manager: ERROR while adding new room\n");
						goto error;
					}
					DBG("imc:imc_manager: created a new room name= %.*s\n", room->name.len, room->name.s);

					flag_member |= IMC_MEMBER_OWNER;
					member= imc_add_member(room, &pfrom_uri->user,
							&pfrom_uri->host, flag_member);
					if(member == NULL)
					{
						LOG(L_ERR, "imc:imc_manager: ERROR while adding new member\n");
						goto error;
					}
					
				}
				else
				{	
					DBG("imc:imc_manager: found room\n");

					if(!(room->flags & IMC_ROOM_PRIV))
					{
						DBG("imc:imc_manager: room not private\n");
						member= imc_get_member(room, &pfrom_uri->user, 
								&pfrom_uri->host);
						DBG("imc:imc_manager: check if the user is a member\n");
						if(member== NULL)
						{					
							DBG("adding new member\n");
							member= imc_add_member(room, &pfrom_uri->user,
									&pfrom_uri->host, flag_member);	
							if(member == NULL)
							{
								LOG(L_ERR, "imc:imc_manager: ERROR while adding new room\n");
								goto error;
							}	
							goto build_inform;
						}
						else
						{	
							DBG("found member uri = %.*s\n", member->uri.len,member->uri.s );
							if(member_inv!=NULL)
								if(member_inv->flags & IMC_MEMBER_INVITED) 
								{
									member->flags ^= IMC_MEMBER_INVITED;
								}					
						}
				
					}
				}	
			}
			else 
				if(cmd.name.len==(sizeof("invite")-1)
				&& !strncasecmp(cmd.name.s, "invite", cmd.name.len))
				{
					DBG("INVITE\n");
					size= p_cmd->param[0].len+2 ;	
					
					while (i<size )
					{
						if(p_cmd->param[0].s[i]== '@')
						{	
							
							add_domain =0;
							DBG("found @\n");
							break;
						}
						i++;
					}

					if(add_domain)
						size+= pto_uri->host.len;
					if(strncmp(p_cmd->param[0].s, "sip:", 4)!= 0)
					{
						size+= 4;
						add_sip = 1;
						DBG("must add sip\n");
					}
					
				
					uri.s = (char*)pkg_malloc( size *sizeof(char));
					if(uri.s == NULL)
					{
						LOG(L_ERR, "imc:imc_manager: no more memory\n");
						goto error;
					}
					
					size= 0;
					if(add_sip)
					{	
						strcpy(uri.s, "sip:");
						size= 4;
					}
					
					memcpy(uri.s +size, p_cmd->param[0].s, p_cmd->param[0].len);
					size+= p_cmd->param[0].len;

					if(add_domain)
					{	
						uri.s[size] = '@';
						size++;
						memcpy(uri.s+ size, pto_uri->host.s, pto_uri->host.len);
						size+= pto_uri->host.len;
					}
					uri.len = size;

					parse_uri(uri.s, uri.len, &inv_uri);
					
					pinv_uri=&inv_uri;
				
					DBG("uri= %.*s  len = %d\n", uri.len, uri.s, uri.len); 
				
					room= imc_get_room((p_cmd->param[1].s)?&p_cmd->param[1]:
							&pto_uri->user, &pto_uri->host);				
					if(room== NULL)
					{
						LOG(L_ERR,"imc:imc_manager:The room does not exist!\n");
						goto error;
					}			
					member= imc_get_member(room, &pfrom_uri->user,
							&pfrom_uri->host);
					DBG("imc:imc_manager: verify if the user is a member of the room\n");
					if(member==NULL)
					{
						LOG(L_ERR,"imc:imc_manager: The user who sent" 
							" the invitation is not a member of the room!\n");
						goto error;
					}

					if(!(member->flags & IMC_MEMBER_OWNER) &&
							!(member->flags & IMC_MEMBER_ADMIN))
					{
						LOG(L_ERR,"imc:imc_manager: invite_user: The user does"
							" not have the right to invite other users!\n");
						goto error;
					}
                 	DBG("imc:imc_manager: verify if the invited user is already a member\n");
					member_inv= imc_get_member(room, &pinv_uri->user,
							&pinv_uri->host);
					if(member_inv!=NULL)
					{
						LOG(L_ERR,"imc:imc_manager: The invited user" 
								" is already a member of the room!\n");
						goto error;
					}
		
					flag_member |= IMC_MEMBER_INVITED;		
					member_inv= imc_add_member(room, &pinv_uri->user,
							&pinv_uri->host, flag_member);
					if(member_inv == NULL)
					{
					  LOG(L_ERR,"imc:imc_manager:ERROR while adding member\n");
					  goto error;	
					}	
					member_inv->inviting_muri = member->uri;
					DBG("succeded to add a new invited member\n");
					body_invit.s = body_buf;
					body_invit.len = 13 + member->uri.len - 4/* sip: */ + 28;	
					memcpy(body_invit.s, "INVITE from: ", 13);
					memcpy(body_invit.s+13, member->uri.s + 4,
							member->uri.len - 4);
					memcpy(body_invit.s+ 9 + member->uri.len,
							"(Type: '#accept' or '#deny')", 28);	
					body_invit.s[body_invit.len] = '\0';			
				
					to_uri_s.s = to_uri_buf;
					
					to_uri_s.len = member_inv->uri.len;
					memcpy(to_uri_s.s, member_inv->uri.s, member_inv->uri.len);

					from_uri_s.s = from_uri_buf;
					from_uri_s.len = room->uri.len;
					strncpy(from_uri_s.s, room->uri.s, room->uri.len);
					DBG("imc:imc_manager: sending invitation\n");
					DBG("[to]= %.*s\n[from]= %.*s\n [body_invite]= %.*s\n",
							to_uri_s.len,to_uri_s.s,from_uri_s.len, from_uri_s.s,
							body_invit.len, body_invit.s);
				
					cback_param= (del_member_t *)shm_malloc(sizeof(del_member_t));
					memset(cback_param, 0, size);
					cback_param->room_name = room->name;
					cback_param->room_domain = room->domain;
					cback_param->member_name = member_inv->user;
					cback_param->member_domain = member_inv->domain;
					cback_param->inv_uri = member->uri;
					tmb.t_request(&msg_type,  /* Tipul mesajului */
						NULL,      /* Request-URI */
						&to_uri_s,      /* To */
						&from_uri_s,        /* From */
						NULL,         /* Antet optional */
						&body_invit,            /* Message body */
						inv_callback,             /* functie callback */
						(void*)(cback_param)              /* parametru callback */
						);				
				
					if(uri.s)
					{
						pkg_free(uri.s);
						uri.s = NULL;
					}
					if(room!=NULL)
					{
						room_released = imc_release_room(room);
					}
									
				}
				else 
					if(cmd.name.len==(sizeof("accept")-1)
						&& !strncasecmp(cmd.name.s, "accept", cmd.name.len))
					{
					/* accepting the invitation */
						room=imc_get_room((p_cmd->param[0].s)?&p_cmd->param[0]:
								&pto_uri->user, &pto_uri->host);
						if(room== NULL)
						{
							LOG(L_ERR,"imc:imc_manager: accept :"
									" The room is not created!\n");
							goto error;
						}			
					/* if aready invited add as a member */
						member_inv=imc_get_member(room, &pfrom_uri->user,
							&pfrom_uri->host);
						if(member_inv==NULL || 
								!(member_inv->flags & IMC_MEMBER_INVITED))
						{
							LOG(L_ERR,"imc:imc_manager: accept:"
								" The user is not invited in the room!\n");
							goto error;
						}
			
						member_inv->flags ^= IMC_MEMBER_INVITED;
					
						member = member_inv;
						goto build_inform;
					}
					else 
						if(cmd.name.len==(sizeof("deny")-1)
							&& !strncasecmp(cmd.name.s, "deny", cmd.name.len))
						{
							/* denying an invitation */
							room= imc_get_room((p_cmd->param[0].s)?
									&p_cmd->param[0]:&pto_uri->user, 
									&pto_uri->host);
							if(room== NULL)
							{
								LOG(L_ERR,"imc:imc_manager: deny :"
									" The room does not exist!\n");
								goto error;
							}			
			/* If the user is an invited member, delete it froim the list */
							member_inv= imc_get_member(room,
									&pfrom_uri->user, &pfrom_uri->host);
							if(member_inv==NULL || !(member_inv->flags & 
										IMC_MEMBER_INVITED))
							{
								LOG(L_ERR,"imc:imc_manager: deny:The user is"
									" not invited in the room!\n");
								goto error;
							}		
			
							deleted_user= imc_del_member(room,
									&pfrom_uri->user, &pfrom_uri->host);
							body.s = body_buf;
							strcpy(body.s, "The user has denied the invitation");
							body.len = strlen(body.s);
							from_uri_s.s = from_uri_buf;
							from_uri_s.len = room->uri.len;
							memcpy(from_uri_s.s, room->uri.s, room->uri.len);
	
							to_uri_s.s = to_uri_buf;
							to_uri_s.len = member_inv->inviting_muri.len;
							strncpy(to_uri_s.s, member_inv->inviting_muri.s, member_inv->inviting_muri.len);				
								
							DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n",
									to_uri_s.len, to_uri_s.s , from_uri_s.len, from_uri_s.s,
									body.len, body.s);
							tmb.t_request(&msg_type,	/* Request method */
									NULL,				/* Request-URI */
									&to_uri_s,			/* To */
									&from_uri_s,		/* From */
									NULL,				/* Headers */
									&body,		/* Body */
									NULL,				/* callback function */
									NULL				/* callback parameter */
								);

						} 
						else
							if(cmd.name.len==(sizeof("remove")-1)
							&& !strncasecmp(cmd.name.s, "remove", cmd.name.len))
							{
								/*
								if(strncmp(p_cmd->param[0].s, "sip:", 4)!= 0)
								{
									DBG( "imc:imc_manager:autocomplete\n");
									uri.s = (char*)pkg_malloc(( 5+ p_cmd->param[0].len)*sizeof(char));
									strcpy(uri.s, "sip:");
									strncpy(uri.s +4, p_cmd->param[0].s, p_cmd->param[0].len);
									uri.len =  4+ p_cmd->param[0].len;
									parse_uri(uri.s, uri.len, &inv_uri);
								}
							else
								parse_uri(p_cmd->param[0].s,p_cmd->param[0].len, &inv_uri);
*/
					size= p_cmd->param[0].len+2 ;	
					
					while (i<size )
					{
						if(p_cmd->param[0].s[i]== '@')
						{	
							
							add_domain =0;
							DBG("found @\n");
							break;
						}
						i++;
					}

					if(add_domain)
						size+= pto_uri->host.len;
					if(strncmp(p_cmd->param[0].s, "sip:", 4)!= 0)
					{
						size+= 4;
						add_sip = 1;
						DBG("must add sip\n");
					}
					
				
					uri.s = (char*)pkg_malloc( size *sizeof(char));
					if(uri.s == NULL)
					{
						LOG(L_ERR, "imc:imc_manager: no more memory\n");
						goto error;
					}
					
					size= 0;
					if(add_sip)
					{	
						strcpy(uri.s, "sip:");
						size= 4;
					}
					
					memcpy(uri.s +size, p_cmd->param[0].s, p_cmd->param[0].len);
					size+= p_cmd->param[0].len;

					if(add_domain)
					{	
						uri.s[size] = '@';
						size++;
						memcpy(uri.s+ size, pto_uri->host.s, pto_uri->host.len);
						size+= pto_uri->host.len;
					}
					uri.len = size;

					parse_uri(uri.s, uri.len, &inv_uri);
					
					pinv_uri=&inv_uri;
				
					DBG("uri= %.*s  len = %d\n", uri.len, uri.s, uri.len); 

								pinv_uri= &inv_uri;
								room= imc_get_room((p_cmd->param[1].s)?
										&p_cmd->param[1]:&pto_uri->user,
										&pto_uri->host);
								if(room==NULL)
								{
									LOG(L_ERR,"imc:imc_manager: remove:"
										" The room does not exist!\n");
									goto error;
								}			
			/*verify if the user who sent the request is a member in the room
			 * and has the right to remove other users */
								member= imc_get_member(room, 
										&pfrom_uri->user, &pfrom_uri->host);

								if(member== NULL)
								{
									LOG(L_ERR,"imc:imc_manager: remove: The"
										" user who sent the request is not"
										" a member of the room!\n");
									goto error;
								}
								if(!(member->flags & IMC_MEMBER_OWNER) &&
										!(member->flags & IMC_MEMBER_ADMIN))
								{
									LOG(L_ERR,"imc:imc_manager: remove :The"
										" user does not have the right to"
										" remove other users!\n");
									goto error;
								}
		/* verify if the user that is to be removed is a member of the room */
								member_inv= imc_get_member(room,
										&pinv_uri->user, &pinv_uri->host);
								if(member_inv== NULL)
								{
									LOG(L_ERR,"imc:imc_manager: invite :The"
										"user is not a member of room!\n");
									goto error;
								}
				
								if(member_inv->flags & IMC_MEMBER_OWNER)
								{
									LOG(L_ERR,"imc:imc_manager: remove :The"
										" user is the owner of the room;"
										" he can not be removed!\n");
									goto error;
								}	
																	
								member = member_inv;
							    /* send message to the removed person */
							
								body.s = body_buf;
								strcpy(body.s, "You have been removed from this room");
								body.len = strlen(body.s);
								from_uri_s.s = from_uri_buf;
								from_uri_s.len = room->uri.len;
								memcpy(from_uri_s.s, room->uri.s, room->uri.len);

								DBG("send_remove_message: sending remove message\n");
	
								to_uri_s.s = to_uri_buf;
								to_uri_s.len = member_inv->uri.len;
								strncpy(to_uri_s.s, member_inv->uri.s, member_inv->uri.len);				

								DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n",
								to_uri_s.len, to_uri_s.s , from_uri_s.len, from_uri_s.s,
								body.len, body.s);
								tmb.t_request(&msg_type,	/* Request method */
										NULL,				/* Request-URI */
									&to_uri_s,			/* To */
									&from_uri_s,		/* From */
									NULL,				/* Headers */
									&body,		/* Body */
									NULL,				/* callback function */
									NULL				/* callback parameter */
								);

								member->flags |= IMC_MEMBER_DELETED;
								action = 1;
								goto build_inform;
								
							}
							else
								if(cmd.name.len==(sizeof("exit")-1)
							&& !strncasecmp(cmd.name.s, "exit", cmd.name.len))
								{
								/* the user wants to leave the room */
									room= imc_get_room((p_cmd->param[0].s)?
											&p_cmd->param[0]:&pto_uri->user,
											&pto_uri->host);
									if(room== NULL)
									{
										LOG(L_ERR,"imc:imc_manager: exit :"
											" The room does not exist!\n");
										goto error;
									}		
								/* verify if the user is a member of the room */
									member= imc_get_member(room, 
											&pfrom_uri->user, &pfrom_uri->host);

									if(member== NULL)
									{
										LOG(L_ERR,"imc:imc_manager:exit:The"	
										  " user is not a member of the room!\n");
										goto error;
									}
			
									if(member->flags & IMC_MEMBER_OWNER)
									{
				/*If the user is the owner of the room, the room is distroyed */
										flag_room ^=IMC_ROOM_DELETED;

										body_final.s = body_buf;
										strcpy(body_final.s, "The room has been destroyed");
										body_final.len = strlen(body_final.s);
										
										goto send_message;
									}	
									else
									{
										/* delete user */
										deleted_user= imc_del_member(room,
										   &pfrom_uri->user, &pfrom_uri->host);
										action = 1;
										goto build_inform;
									}
			
								}
								else 
									if(cmd.name.len==(sizeof("destroy")-1)
						&& !strncasecmp(cmd.name.s, "destroy", cmd.name.len))
									{
									/* distrugere camera */
										room= imc_get_room((p_cmd->param[0].s)?
											&p_cmd->param[0]:&pto_uri->user,
											&pto_uri->host);
										if(room== NULL)
										{
											LOG(L_ERR,"imc:imc_manager:destroy:"
												" The room does not exist!\n");
											goto error;
										}
						
								/* verify is the user is a member of the room*/
										member= imc_get_member(room, 
											&pfrom_uri->user, &pfrom_uri->host);

										if(member== NULL)
										{
											LOG(L_ERR,"imc:imc_manager:destroy"
												":The user is not a member of"
												" the room!\n");
											goto error;
										}
			
										if(!(member->flags & IMC_MEMBER_OWNER))
										{
											LOG(L_ERR,"imc:imc_manager:destroy"
												":The user is not the owner of"
											" the room, he can not destroy it!\n");
											goto error;
										}
										flag_room ^=IMC_ROOM_DELETED;
										body_final.s = body_buf;
										strcpy(body_final.s, "The room has been destroyed");
										body_final.len = strlen(body_final.s);
									
										goto send_message;
										
									}
									else
									{	
										if(cmd.name.len==(sizeof("help")-1)
							&& !strncasecmp(cmd.name.s, "help", cmd.name.len))
											goto send_help_msg;
									
										else
										{
										   DBG("imc:imc_manager:Unknown command"
										   "[%.*s]\n", cmd.name.len,cmd.name.s);
											goto error;
										}
									
									}
		goto done;
	}

	room= imc_get_room(&pto_uri->user, &pto_uri->host);		
	if(room== NULL)
	{
		LOG(L_ERR,"imc:imc_manager: send message: The room does not exist!\n");
		goto error;
	}
	else
		DBG("imc:imc_manager:room uri:%.*s\n", room->uri.len, room->uri.s);

	member= imc_get_member(room, &pfrom_uri->user, &pfrom_uri->host);
	if(member== NULL || (member->flags & IMC_MEMBER_INVITED))
	{
		LOG(L_ERR,"imc:imc_manager: send message:The user does not have the"
			" right to send messages!\n");
		goto error;
	}
    	
	goto build_message;


build_inform:
		body_final.s = body_buf;
		body_final.len = member->uri.len - 4 /* sip: part of URI */ + 20;
		memcpy(body_final.s, member->uri.s + 4, member->uri.len - 4);
		if ( action == 1)
		{
			memcpy(body_final.s+member->uri.len-4," left room.  ",20);
		}
		else
		{	
			memcpy(body_final.s+member->uri.len-4," joined room.",20);
		}
		goto send_message;



build_message:
		body_final.s = body_buf;
		body_final.len = body.len + member->uri.len - 4 + 4 /* < > */;
		memcpy(body_final.s, "<", 1);
		memcpy(body_final.s + 1, member->uri.s + 4, member->uri.len - 4);
		memcpy(body_final.s + 1 + member->uri.len - 4, ">: ", 3);		
		memcpy(body_final.s + 1 + member->uri.len - 4 +3, body.s, body.len);
		body_final.s[body_final.len] = '\0';


send_message:
	imp = room->members;
	from_uri_s.s = from_uri_buf;
	from_uri_s.len = room->uri.len;
	strncpy(from_uri_s.s, room->uri.s, room->uri.len);

	DBG("send_message: sending message\n");
	DBG("room->nr_of_members= %d\n",room->nr_of_members );
	if(imp)
		DBG("send_message: member uri = %.*s\n", imp->uri.len, imp->uri.s);

	while(imp)
	{
		to_uri_s.s = to_uri_buf;
		to_uri_s.len = imp->uri.len;
		strncpy(to_uri_s.s, imp->uri.s, imp->uri.len);
		if(!(imp->flags & IMC_MEMBER_INVITED))
		{
			if(is_command || (imp->user.len!=pfrom_uri->user.len
				|| imp->domain.len!=pfrom_uri->host.len
				|| strncasecmp(imp->user.s, pfrom_uri->user.s,
					pfrom_uri->user.len) 
				|| strncasecmp(imp->domain.s, pfrom_uri->host.s,
					pfrom_uri->host.len)))
					
			{
				DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n",
						to_uri_s.len, to_uri_s.s , from_uri_s.len, from_uri_s.s,
						body_final.len, body_final.s);

				tmb.t_request(&msg_type,	/* Request method */
						NULL,				/* Request-URI */
						&to_uri_s,			/* To */
						&from_uri_s,		/* From */
						NULL,				/* Headers */
						&body_final,		/* Body */
						NULL,				/* callback function */
						NULL				/* callback parameter */
				);
			}
		}	
		imp = imp->next;
	}

	if(member->flags& IMC_MEMBER_DELETED)
	{
		deleted_user= imc_del_member(room, 
					  &pinv_uri->user, &pinv_uri->host);
		if(deleted_user == 0)
		{
			LOG(L_ERR, "imc:send_message: ERROR while deleting user\n");
			goto error;
		}
	}
	if(flag_room &IMC_ROOM_DELETED)
	{
		DBG("imc:send_message: deleting room\n");
		room_deleted = imc_del_room((
					p_cmd->param[0].s)?&p_cmd->param[0]:
					&pto_uri->user, &pto_uri->host);
		if(room_deleted == 0)
		{
			LOG(L_ERR, "imc:send_message: ERROR while deleting room\n");
			goto error;
		}

	}
	goto done;

send_help_msg:
	
	DBG("send_message: sending help message\n");

	from_uri_s.s = from_uri_buf;
	
	from_uri_s.len = msg->first_line.u.request.uri.len;
	memcpy(from_uri_s.s,msg->first_line.u.request.uri.s ,msg->first_line.u.request.uri.len);
	to_uri_s.s = to_uri_buf;
	to_uri_s.len = pfrom->uri.len;
	memcpy(to_uri_s.s, pfrom->uri.s, pfrom->uri.len);
	
	body_final.s = IMC_HELP_MSG;
	body_final.len = IMC_HELP_MSG_LEN;

	DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n",
		to_uri_s.len, to_uri_s.s , from_uri_s.len, from_uri_s.s,
			body_final.len, body_final.s);
	tmb.t_request(&msg_type,	/* Request method */
					NULL,				/* Request-URI */
					&to_uri_s,			/* To */
					&from_uri_s,		/* From */
					NULL,				/* Headers */
					&body_final,		/* Body */
					NULL,				/* callback function */
					NULL				/* callback parameter */
				);

done:
	if(uri.s)
	{
		pkg_free(uri.s);
		uri.s = NULL;
	}

	if(room!=NULL)
	{
		room_released = imc_release_room(room);
	}
	return 1;

error:
	if(room!=NULL)
	{
		room_released = imc_release_room(room);
	}
	if(uri.s)
	{
		pkg_free(uri.s);
		uri.s = NULL;
	}

	return -1;	
}
/*
static void imc_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	return;
}
*/

/**
 * destroy module
 */
void destroy(void)
{
	imc_room_p irp = NULL;
	imc_member_p member = NULL;
	int i;
	db_key_t mq_cols[4];
	db_val_t mq_vals[4];
	db_key_t rq_cols[4];
	db_val_t rq_vals[4];

	DBG("imc: destroy module ...\n");
	
	mq_cols[0] ="user";
	mq_vals[0].type = DB_STR;
	mq_vals[0].nul = 0;
			
	mq_cols[1] ="domain";
	mq_vals[1].type = DB_STR;
	mq_vals[1].nul = 0;
	
	mq_cols[2] ="flag";
	mq_vals[2].type = DB_INT;
	mq_vals[2].nul = 0;

	mq_cols[3] ="room";
	mq_vals[3].type = DB_STR;
	mq_vals[3].nul = 0;


	rq_cols[0] ="name";
	rq_vals[0].type = DB_STR;
	rq_vals[0].nul = 0;
		
	rq_cols[1] ="domain";
	rq_vals[1].type = DB_STR;
	rq_vals[1].nul = 0;

	rq_cols[2] ="flag";
	rq_vals[2].type = DB_INT;
	rq_vals[2].nul = 0;

	for(i=0; i<imc_hash_size; i++) 
	{
		irp = _imc_htable[i].rooms;
		
		while(irp)
		{
			rq_vals[0].val.str_val = irp->name;
			rq_vals[1].val.str_val = irp->domain;
			rq_vals[2].val.int_val = irp->flags;

			if(imc_dbf.use_table(imc_db, room_table)< 0)
			{
				LOG(L_ERR, "imc:destroy: ERROR in use_table\n");
				return;
			}

			if(imc_dbf.insert(imc_db, rq_cols, rq_vals, 3)<0)
			{
				LOG(L_ERR, "imc:destroy: ERROR while inserting into table"
						" imc_rooms\n");
				return;
			}
			DBG("imc:destroy: room %d %.*s\n", i, irp->name.len, irp->name.s);
			member = irp->members;
			while(member)
			{
				mq_vals[0].val.str_val = member->user;
				mq_vals[1].val.str_val = member->domain;
				mq_vals[2].val.int_val = member->flags;
				mq_vals[3].val.str_val = irp->uri;

				if(imc_dbf.use_table(imc_db, member_table)< 0)
				{
					LOG(L_ERR, "imc:destroy: ERROR in use_table\n");
					return;
				}

				if(imc_dbf.insert(imc_db, mq_cols, mq_vals, 4)<0)
				{
					LOG(L_ERR, "imc:destroy: ERROR while inserting into table"
						" imc_rooms\n");
					return;
				}
				member = member->next;
			}

			irp = irp->next;

		}

	}

	imc_htable_destroy();
}

int imc_list_randm(FILE *stream)
{
	int i;
	imc_room_p irp = NULL;
	// irp_temp=NULL;
	
	for(i=0; i<imc_hash_size; i++) 
	{
		irp = _imc_htable[i].rooms;
			while(irp)
			{

				fprintf(stdout, "\n\nRoom: %.*s\nMembers: %d\nOwner: %.*s\n",
						irp->uri.len, irp->uri.s+4,
						irp->nr_of_members, irp->uri.len, irp->uri.s);
				imc_list_members2(stdout, irp);
				irp = irp->next;
			}
	}
	return 0;	

}
int imc_list_members2(FILE* stream, imc_room_p room)
{

	imc_member_p member= NULL;
	member = room->members;
	if(member ==  NULL)
	{
		DBG("imc:imc_list_members2: no members found\n");
		return -1;
	}

	while(member)
	{
		DBG("Member: name= %.*s\ndomain= %.*s\nflag= %d\n",member->user.len,
				member->user.s, member->domain.len, member->domain.s,
				member->flags);
		member = member->next;
	}

	return 0;

}

int imc_list_rooms(FILE *stream, char *response_file){
	int i;
	FILE *freply=NULL;
	imc_room_p irp = NULL;

	freply = open_reply_pipe(response_file);
	if(freply==NULL)
	{
		LOG(L_ERR, "IMC:imc_list_rooms: error -cannot open fifo reply '%s'\n",
				response_file);
		return -1;
	}

	for(i=0; i<imc_hash_size; i++) 
	{
		irp = _imc_htable[i].rooms;
			while(irp){
				fprintf(freply, "Room: %.*s\nMembers: %d\nOwner: %.*s\n",
						irp->uri.len, irp->uri.s+4,
						irp->nr_of_members, irp->uri.len, irp->uri.s);
				irp = irp->next;
			}
	}
		
	if(freply!=NULL)
		fclose(freply);

	return 0;
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

				attr= add_mi_attr(node, MI_DUP_VALUE, "OWNER", 5, irp->uri.s,
					irp->uri.len);
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
		LOG(L_ERR, "IMC:imc_mi_list_members: error - no room name!\n");
		return init_mi_tree( 404, "room name not found", 19);
	}
	rnbuf[room_name.len] = '\0';
	if(*room_name.s=='\0' || *room_name.s=='.')
	{
		LOG(L_INFO, "IMC:imc_mi_list_members: empty room name\n");
		return init_mi_tree( 400, "empty param", 11);
	}

	/* find room */
	parse_uri(room_name.s,room_name.len, &inv_uri);
	pinv_uri=&inv_uri;
	room=imc_get_room(&pinv_uri->user, &pinv_uri->host);

	if(room==NULL)
	{
		LOG(L_ERR,"IMC:imc_mi_list_members: no such room!\n");
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


/******************** FIFO ****************************/

int imc_list_members(FILE *stream, char *response_file){
	int i;
	FILE *freply=NULL;
	char rnbuf[256];
	str room_name;
	imc_room_p room;
	struct sip_uri inv_uri, *pinv_uri;
	imc_member_p imp=NULL;
	
	freply = open_reply_pipe(response_file);
	if(freply==NULL)
	{
		LOG(L_ERR, "IMC:imc_list_members: error - cannot open fifo" 
				" reply '%s'\n",response_file);
		return -1;
	}
	
	/* room name */
	room_name.s = rnbuf;
	if(!read_line(room_name.s, 255, stream, &room_name.len) ||
			room_name.len==0)	
	{
		LOG(L_ERR, "IMC:imc_list_members: error - no room name!\n");
		fifo_reply(response_file, "400 imc_list_members -room name not found\n");
		return 1;
	}
	rnbuf[room_name.len] = '\0';
	if(*room_name.s=='\0' || *room_name.s=='.')
	{
		LOG(L_INFO, "IMC:imc_list_members: empty room name\n");
		fifo_reply(response_file, "400 imc_list_members - empty param\n");
		return 1;
	}

	/* find room */
	parse_uri(room_name.s,room_name.len, &inv_uri);
	pinv_uri=&inv_uri;
	room=imc_get_room(&pinv_uri->user, &pinv_uri->host);

	if(room==NULL)
	{
		LOG(L_ERR,"IMC:imc_list_members: no such room!\n");
		fifo_reply(response_file, "404 imc_list_members - no such room\n");
		return 1;
	}

	fprintf(freply, "Room:  %.*s\nMembers:\n", room_name.len, room_name.s);
	imp = room->members;
	i=0;
	while(imp)
	{
		i++;
		fprintf(freply, "  #%d:  %.*s \n",i, 
						imp->uri.len, imp->uri.s+4);
		imp = imp->next;
	}
	
	fprintf(freply, "Number of members:  %d\n",i);
	
	imc_release_room(room);

	if(freply!=NULL)
		fclose(freply);

	return 0;

}

void inv_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	str body_final;
	char from_uri_buf[256];
	char to_uri_buf[256];
	char body_buf[256];
	str from_uri_s, to_uri_s;
	imc_member_p member= NULL;
	imc_room_p room = NULL;

	if(ps->param==NULL || *ps->param==NULL || 
			(del_member_t*)(*ps->param) == NULL)
	{
		DBG("imc inv_callback: member not received\n");
		return;
	}
	
	LOG(L_DBG, "imc:inv_callback: completed with status %d [member name domain:"
			"%p/%.*s/%.*s]\n",ps->code, ps->param, 
			((del_member_t *)(*ps->param))->member_name.len,
			((del_member_t *)(*ps->param))->member_name.s,
			((del_member_t *)(*ps->param))->member_domain.len, 
			((del_member_t *)(*ps->param))->member_domain.s);
	if(ps->code < 300)
		return;
	else
	{
		room= imc_get_room(&((del_member_t *)(*ps->param))->room_name,
						&((del_member_t *)(*ps->param))->room_domain );
		if(room==NULL)
		{
			LOG(L_ERR,"imc:imc_manager: remove:"
					" The room does not exist!\n");
			goto error;
		}			
		/*verify if the user who sent the request is a member in the room
		 * and has the right to remove other users */
		member= imc_get_member(room,
				&((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);

		if(member== NULL)
		{
			LOG(L_ERR,"imc:imc_manager: remove: The user"
					"is not a member of the room!\n");
			goto error;
		}
		imc_del_member(room,
				&((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);
		goto build_inform;

	}
	

build_inform:
		
		body_final.s = body_buf;
		body_final.len = member->uri.len - 4 /* sip: part of URI */ + 20;
		memcpy(body_final.s, member->uri.s + 4, member->uri.len - 4);
		memcpy(body_final.s+member->uri.len-4," is not registered.  ",21);
		
		goto send_message;

send_message:
	
	from_uri_s.s = from_uri_buf;
	from_uri_s.len = room->uri.len;
	strncpy(from_uri_s.s, room->uri.s, room->uri.len);

	DBG("send_message: sending message\n");
	
	to_uri_s.s = to_uri_buf;
	to_uri_s.len = ((del_member_t *)(*ps->param))->inv_uri.len;
	strncpy(to_uri_s.s,((del_member_t *)(*ps->param))->inv_uri.s ,
			((del_member_t *)(*ps->param))->inv_uri.len);

	DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n",
		to_uri_s.len, to_uri_s.s , from_uri_s.len, from_uri_s.s,
			body_final.len, body_final.s);
	tmb.t_request(&msg_type,	/* Request method */
					NULL,				/* Request-URI */
					&to_uri_s,			/* To */
					&from_uri_s,		/* From */
					NULL,				/* Headers */
					&body_final,		/* Body */
					NULL,				/* callback function */
					NULL				/* callback parameter */
				);
	if(room!=NULL)
	{
		imc_release_room(room);
	}

	if((del_member_t *)(*ps->param))
		shm_free(*ps->param);

	return;

error:
		if(room!=NULL)
		{
			imc_release_room(room);
		}
		if((del_member_t *)(*ps->param))
			shm_free(*ps->param);
	return; 
}

