/**
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-04-07: a structure for both hashes introduced (ramona) 
 * 2003-04-06: db connection closed in mod_init (janakj)
 */

/*
 * Prefix-Domains Translation - ser module
 * Ramona Modroiu <modroiu@fokus.fraunhofer.de>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../db/db_op.h"
#include "../../sr_module.h"
#include "../../parser/parse_fline.h"
#include "../../db/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../parser/parse_uri.h"
#include "../../parser/msg_parser.h"

#include "domains.h"

MODULE_VERSION


#define NR_KEYS			2
#define DB_KEY_NAME		"domain"
#define DB_KEY_CODE		"code"

int hs_two_pow = 1;

/** structure containing the both hashes */
double_hash_t *hash;

/** next code to be allocated */
code_t *next_code = NULL;

/** database connection */
db_con_t *db_con = NULL;


/** parameters */
char *db_url = "sql://root@127.0.0.1/pdt";
char *db_table = "domains";

/** pstn prefix */
char *prefix = NULL;
code_t prefix_len = 0;

code_t code_terminator = 0;

code_t start_range = 10;

gen_lock_t l;

static int prefix2domain(struct sip_msg*, char*, char*);
static int mod_init(void);
static void mod_destroy(void);
static int mod_child_init(int r);

static cmd_export_t cmds[]={
	{"prefix2domain", prefix2domain, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"db_url", STR_PARAM, &db_url},
	{"db_table", STR_PARAM, &db_table},
	{"prefix", STR_PARAM, &prefix},
	{"terminator", INT_PARAM, &code_terminator},
	{"start_range", INT_PARAM, &start_range},
	{"hsize_2pow", INT_PARAM, &hs_two_pow},
	{0, 0, 0}
};

struct module_exports exports = {
	"pdt",
	cmds,
	params,
	
	mod_init,		/* module initialization function */
	0,				/* response function */
	mod_destroy,	/* destroy function */
	0,				/* oncancel function */
	mod_child_init	/* per child init function */
};	


inline code_t code_valid(code_t code)
{
	code_t tmp;
	
	tmp = code;
	while(tmp)
	{
		if(tmp%10==code_terminator)
			return 0;
		tmp /= 10;
	}

	return 1;
}

/* the superior interval limit is kept to signal that
 * there are no more prefixes available */
code_t apply_correction(code_t code)
{
	code_t p, tmp, new_code;

	if(code==MAX_CODE)
		return MAX_CODE;
	
	p = 1;
	tmp = code;
	new_code = code;
	while(tmp)
	{
		if(tmp%10==code_terminator)
		{
			tmp += 1;

			if(MAX_CODE-p <= new_code)
				return MAX_CODE;

			new_code += p;
		}
		
		if(p>MAX_CODE_10)
			return MAX_CODE;
		p *= 10;
		tmp /= 10;
	}

	return new_code;
}

inline int prefix_valid()
{
	char *p;
	
	if(!prefix)
		return 1;

	p=prefix;
	prefix_len = 0;

	while(*p!='\0')
	{
		prefix_len++;
		if( *p<'0' || *p>'9' )
		{
			DBG("PDT: prefix_valid: supplied parameter as prefix is not valid\n");
			return 0;
		}
		p++;
	}		

	return 1;
}


/**
 * init module function
 */
static int mod_init(void)
{
	db_res_t* db_res = NULL;
	code_t i, code;
	dc_t* cell; 

		
	DBG("PDT: initializing...\n");
	
	if(hs_two_pow<0)
	{
		LOG(L_ERR, "PDT: mod_init: hash_size_two_pow must be"
					" positive and less than %d\n", MAX_HSIZE_TWO_POW);
		return -1;
	}
	
	if(code_terminator>9 || code_terminator<0)
	{
		LOG(L_ERR, "PDT: mod_init: code_terminator must be a digit\n");
		return -1;
	}

	if(!prefix_valid())
		return -1;

	next_code = (code_t*)shm_malloc(sizeof(code_t));
	if(!next_code)
	{
		LOG(L_ERR, "PDT: mod_init: cannot allocate next_code!\n");
		return -1;
	}
	if(lock_init(&l) == 0)
	{		
		shm_free(next_code);
		LOG(L_ERR, "PDT: mod_init: cannot init the lock\n");
		return -1;	
	}	
	
	if(register_fifo_cmd(get_domainprefix, "get_domainprefix", 0)<0)
	{
		LOG(L_ERR, "PDT: mod_init: cannot register fifo command 'get_domaincode'\n");
		goto error1;
	}	


	/* binding to mysql module */
	if(bind_dbmod())
	{
		LOG(L_ERR, "PDT: mod_init: Database module not found\n");
		goto error1;
	}

	/* open a connection with the database */
	db_con = db_init(db_url);
	if(!db_con)
	{
	
		LOG(L_ERR, "PDT: mod_init: Error while connecting to database\n");        
		goto error1;
	}
	else
	{
		db_use_table(db_con, db_table);
		DBG("PDT: mod_init: Database connection opened successfully\n");
	}
	
	/* init hashes in share memory */
	if( (hash = init_double_hash(hs_two_pow)) == NULL)
	{
		LOG(L_ERR, "PDT: mod_init: hash could not be allocated\n");	
		goto error2;
	}
	
	/* loading all information from database */
	*next_code = 0;
	if(db_query(db_con, NULL, NULL, NULL, NULL, 0, 0, "code", &db_res)==0)
	{
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			
			code = RES_ROWS(db_res)[i].values[0].val.int_val; 
			if (!code_valid(code))
			{
				LOG(L_ERR, "PDT: mod_init: existing code contains the terminator\n");
				goto error;
			}
			
			if (*next_code < code)
				*next_code = code;

			cell=new_cell(
			(char*)(RES_ROWS(db_res)[i].values[1].val.string_val), code);
			
			if(cell == NULL)
					goto error;
			
			if(add_to_double_hash(hash, cell)<0)
			{
				LOG(L_ERR, "PDT: mod_init: could not add information from database"
								" into shared-memory hashes\n");
				goto error;
			}
			
 		}
		// clear up here
		//print_hash(hash->dhash, hash->hash_size);
		//print_hash(hash->chash, hash->hash_size);

		(*next_code)++;
		if (*next_code < start_range)
				*next_code = start_range;
		*next_code = apply_correction(*next_code);

		DBG("PDT: mod_init: next_code:%d\n", *next_code);
		

		/* free up the space allocated for response */
		if(db_free_query(db_con, db_res)<0)
		{
			LOG(L_ERR, "PDT: mod_init: error when freeing"
				" up the response space\n");
		}
	}
	else
	{
		/* query to database failed */
		LOG(L_ERR, "PDT: mod_init: query to database failed\n");
		goto error;
	}

	db_close(db_con); /* janakj - close the connection */
	/* success code */
	return 0;

error:
	free_double_hash(hash);
error2:
	db_close(db_con);
error1:	
	shm_free(next_code);
	lock_destroy(&l);
	return -1;
}	

/* each child get a new connection to the database */
static int mod_child_init(int r)
{
	DBG("PDT: mod_child_init #%d / pid <%d>\n", r, getpid());

	db_con = db_init(db_url);
	if(!db_con)
	{
	  LOG(L_ERR,"PDT: child %d: Error while connecting database\n",r);
	  return -1;
	}
	else
	{
	  db_use_table(db_con, db_table);
	  DBG("PDT:child %d: Database connection opened successfuly\n",r);
	}
	return 0;
}


/* change the r-uri if it is a PSTN format */
static int prefix2domain(struct sip_msg* msg, char* str1, char* str2)
{
	char *host_port;
	code_t code=0, i;
	int digit;
	
	if(!msg)
		return -1;
	
	/* parse the uri, if not yet */
	if(msg->parsed_uri_ok==0)
		if(parse_sip_msg_uri(msg)<0)
		{
		  LOG(L_ERR,"PDT:prefix2domain: ERROR while parsing the R-URI\n");
		  return -1;
		}

	/* if the user part begin with the prefix for PSTN users, extract the code*/
	if (msg->parsed_uri.user.len<=0)
	{
		DBG("PDT: prefix2domain: user part of the message was empty\n");
		return 1;
	}
	
	if(prefix_len>0 && strncasecmp(prefix, msg->parsed_uri.user.s, prefix_len)!=0)
	{
		DBG("PDT: prefix2domain: PSTN prefix did not matched\n");
		return 1;
			
	}

	i=0;
	code=0;
	digit = msg->parsed_uri.user.s[prefix_len+i]-'0';
	while (digit != code_terminator)
	{
		if (digit<0 || digit>9)
		{		
			DBG("PDT: prefix2domain: domain_code not well formed\n");
			return -1;
		}
		

		if(MAX_CODE_10<code || (MAX_CODE_10==code && MAX_CODE-MAX_CODE_R<=digit))
		{
			DBG("PDT: prefix2domain: domain_code not well formed\n");
			return -1;	
		}

		code=code*10+digit;
		i++;
		digit = msg->parsed_uri.user.s[prefix_len+i]-'0';
	}
		
    
	/* find the domain that corresponds to that code */
	if(!(host_port=get_domain_from_hash(hash->chash, hash->hash_size, code)))
	{
		LOG(L_ERR, "PDT: get_domain_from_hash: required " 
					"code %d is not allocated yet\n", code);
		return -1;
	}
	
	/* update the new uri */
	if(update_new_uri(msg, prefix_len+i+1, host_port)<0)
	{
		DBG("PDT: prefix2domain: new_uri cannot be updated\n");
		return -1;
	}
	return 1;
}

/* change the uri according to translation of the prefix */
int update_new_uri(struct sip_msg *msg, int code_len, char* host_port)
{
	char *tmp;
	int uri_len = 0, user_len = 0; 
	
	/* flag to show that ruri is not parsed */
	msg->parsed_uri_ok = 0;

	/* compute the new uri length */
	uri_len = 4/*sip:*/ + msg->parsed_uri.user.len-code_len +
			( msg->parsed_uri.passwd.len ? msg->parsed_uri.passwd.len + 1:0 ) + 
			strlen(host_port) + 1/*@*/ +
			(msg->parsed_uri.params.len ? msg->parsed_uri.params.len + 1:0 ) +
			(msg->parsed_uri.headers.len ? msg->parsed_uri.headers.len + 1:0 );
	
	if (uri_len > MAX_URI_SIZE) 
	{
		LOG(L_ERR, "PDT: update_new_uri(): uri is too long\n");
		return -1;
	}

	/* space for the new uri */
	tmp = (char*)pkg_malloc(uri_len+1);
	if(tmp == NULL)	
	{
		LOG(L_ERR, "PDT: update_new_uri: error allocating space\n");
		return -1;
	}

	/* construct the new uri */
	strcpy(tmp, "sip:");
	
	/* add user part */
	user_len = msg->parsed_uri.user.len-code_len;
	strncat(tmp, msg->parsed_uri.user.s+code_len, user_len);

	/* add password, if that exists */
	if(msg->parsed_uri.passwd.s && msg->parsed_uri.passwd.len > 0)
	{
		strcat(tmp, ":");
		strncat(tmp, msg->parsed_uri.passwd.s, 
				msg->parsed_uri.passwd.len);
	}
	strcat(tmp,"@");
	
	/* add host(and port) part of the uri */
	strcat(tmp, host_port);

	if(msg->parsed_uri.params.s && msg->parsed_uri.params.len > 0)
	{
		strcat(tmp, ";");
		strncat(tmp, msg->parsed_uri.params.s, msg->parsed_uri.params.len);
	}
	
	if(msg->parsed_uri.headers.s && msg->parsed_uri.headers.len > 0)
	{
		strcat(tmp, "?");
		strncat(tmp, msg->parsed_uri.headers.s, msg->parsed_uri.headers.len);
	}
	
	/* free space of the old new_uri */
	if(msg->new_uri.s)
	{
		pkg_free(msg->new_uri.s);
		msg->new_uri.len = 0;
	}
	
	/* setup the new uri */
	msg->new_uri.s = tmp;
	msg->new_uri.len = uri_len;

	// here to clear	
	DBG("PDT: update_new_uri: len=%d uri=%.*s\n", msg->new_uri.len, 
			msg->new_uri.len, msg->new_uri.s);
	
	return 0;
}


static void mod_destroy(void)
{
    DBG("PDT: mod_destroy : Cleaning up\n");
    free_double_hash(hash);
    db_close(db_con);
    shm_free(next_code);
    lock_destroy(&l);
}


/*
 
	Fifo command example:
 
	":get_domaincode:[response_file]\n
	 domain_name\n
	 authorization_to_register_domains\n
	 \n
 	"
 
 */
int get_domainprefix(FILE *stream, char *response_file)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];
	db_op_t  db_ops[NR_KEYS] = {OP_EQ, OP_EQ};

	code_t code;
	dc_t* cell; 
	
	char domain_name[256];
	str sdomain;

	char authorization[10];
	str sauth;
	int authorized=0;
		
	/* read a line -the domain name parameter- from the fifo */
	sdomain.s = domain_name;
	if(!read_line(sdomain.s, 255, stream, &sdomain.len) || sdomain.len==0)	
	{
		LOG(L_ERR, "PDT: get_domaincode: could not read from fifo\n");
		fifo_reply(response_file, "400 |get_domaincode: could not " 
						"read from fifo\n");
		return 1;
	}
	domain_name[sdomain.len] = '\0';

	/* read a line -the authorization to register new domains- from the fifo */
	sauth.s = authorization;
	if(!read_line(sauth.s, 3, stream, &sauth.len) || sauth.len==0)
	{	
		LOG(L_ERR, "PDT: get_domaincode: could not read from fifo\n");
		fifo_reply(response_file, "400 |get_domaincode: could not "
						"read from fifo\n");
		return 1;
	}

	/* see what kind of user we have */
	authorized = sauth.s[0]-'0';

	lock_get(&l);

	/* search the domain in the hashtable */
	cell = get_code_from_hash(hash->dhash, hash->hash_size, domain_name);
	
	/* the domain is registered */
	if(cell)
	{

		lock_release(&l);
			
		/* domain already in the database */	
		fifo_reply(response_file, "201 |Domain name= %.*s"
				"Domain code= %d%d\n",
				sdomain.len, sdomain.s, cell->code, code_terminator);
		return 0;
		
	}
	
	/* domain not registered yet */
	/* user not authorized to register new domains */	
	if(!authorized)
	{
		lock_release(&l);
		fifo_reply(response_file, "203 |Domain name not registered yet\n");
		return 0;
	}

	code = *next_code;
	*next_code = apply_correction(code+1);
		

	/* prepare for insertion into database */
	db_keys[0] = DB_KEY_CODE;
	db_keys[1] = DB_KEY_NAME;

	db_vals[0].type = DB_INT;
	db_vals[0].nul = 0;
	db_vals[0].val.int_val = code;

	db_vals[1].type = DB_STR;
	db_vals[1].nul = 0;
	db_vals[1].val.str_val.s = sdomain.s;
	db_vals[1].val.str_val.len = sdomain.len;
	DBG("%d %.*s\n", code, sdomain.len, sdomain.s);
			
	/* insert a new domain into database */
	if(db_insert(db_con, db_keys, db_vals, NR_KEYS)<0)
	{
		/* next available code is still code */
		*next_code = code;
		lock_release(&l);
		LOG(L_ERR, "PDT: get_domaincode: error storing a"
				" new domain\n");
		fifo_reply(response_file, "204 |Cannot register the new domain in a"
					" consistent way\n");
		return -1;
	}
	
	/* insert the new domain into hashtables, too */
	cell = new_cell(sdomain.s, code);
	if(add_to_double_hash(hash, cell)<0)
		goto error;		

	lock_release(&l);

	/* user authorized to register new domains */
	fifo_reply(response_file, "202 |Domain name= %.*s"
		"	New domain code=  %d%d\n",
		sdomain.len, sdomain.s, code, code_terminator);

	return 0;

	
error:
	/* next available code is still code */
	*next_code = code;
	/* delete from database */
	if(db_delete(db_con, db_keys, db_ops, db_vals, NR_KEYS)<0)
		LOG(L_ERR,"PDT: get_domaincode: database/share-memory are inconsistent\n");
	lock_release(&l);
	
	return -1;
}


