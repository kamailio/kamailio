/*
 * Copyright (C) 2017 plivo (plivo.com)
 * Author : Surendra Tiwari (surendratiwari3@gmail.com)
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



#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/strutils.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_query.h"
#include "redisdb_connection.h"
#include "redisdb_dbase.h"
#include "time.h"



/*!
 * \struct structure for redis result
 */
typedef struct db_redis_result {
	redisReply *reply;
} db_redis_result_t;

#define RES_RESULT(db_res)     (((db_redis_result_t*)((db_res)->ptr))->reply)

/*!
 * \brief used for creating the new result set
 * \return pointer to result object of new result
 */
db1_res_t* db_redis_new_result(void)
{
	db1_res_t* obj;

	obj = db_new_result();
	if (!obj)
		return NULL;
	RES_PTR(obj) = pkg_malloc(sizeof(db_redis_result_t));
	if (!RES_PTR(obj)) {
		db_free_result(obj);
		return NULL;
	}
	memset(RES_PTR(obj), 0, sizeof(db_redis_result_t));
	return obj;
}

/*!
 * \brief Copy the string from one pointer to another 
 * \param s source string to be copied 
 * \return pointer to copy of string on success
 */

char *pkg_strdup (char *s) {
	char *d = pkg_malloc(strlen (s) + 1);   // Allocate memory
	if (d != NULL) strcpy (d,s);         // Copy string if okay
	return d;                            // Return new memory
}

/*!
 * \brief allocate memory for row and column and store the column name into result set _r
 * \param _r database result set
 * \param _c keys
 * \param _column number of column
 * \param _row number of row of redis 
 * \return 0 on success, negative on failure
 */
int db_redis_get_columns(db1_res_t* _r, const db_key_t* _c,const int _column,const int _row)
{
	int col;
	LM_INFO("IN db_redis_get_columns");

	if (!_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	RES_COL_N(_r) = _column;
	if (!RES_COL_N(_r)) 
	{
		LM_ERR("no columns returned from the query\n");
		return -2;
	} 
	else 
	{  
		LM_INFO("%d columns returned from the query\n", RES_COL_N(_r));
	}
	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		RES_COL_N(_r) = 0;
		LM_ERR("could not allocate columns\n");
		return -3;
	}
	RES_ROW_N(_r) = _row;
	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows\n");
		RES_ROW_N(_r) = 0;
		return -2;
	}
	/* defining the column name and column type, allocating the value to every column in result set fo srdb*/
	for(col = 0; col < RES_COL_N(_r); col++) {
		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		RES_NAMES(_r)[col]->s = _c[col]->s;
		RES_NAMES(_r)[col]->len = strlen(_c[col]->s);	
	}	
	return 0;
}
/*!
 * \brief allocate memory for row with row_number
 * \param _r database result set
 * \param reply redis reply set
 * \param _row_number of row of redis 
 * \return 0 on success, negative on failure
 */
int db_redis_get_rows(redisReply *reply, db1_res_t* _r,int row_number)
{
	int col;
	static str dummy_string = {"",0}; /* dummy_string used when null is returned from redis*/
	db_val_t* dval; /* result value for dbapi result set*/
	char string_time[80]; /* used to store the time into string format*/
	if ((!reply) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (db_allocate_row(_r, &(RES_ROWS(_r)[row_number])) != 0) 
	{
		LM_ERR("could not allocate row: %d\n", row_number);
		return -2;
	}

	/* defining the column name and column type, allocating the value to every column in result set fo srdb*/
	for(col = 0; col < RES_COL_N(_r); col++) {
		dval = &(ROW_VALUES(&(RES_ROWS(_r)[row_number]))[col]);
		if((strncmp(RES_NAMES(_r)[col]->s,"contact",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"callid",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"user_agent",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"received",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"path",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"socket",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"ruid",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"instance",RES_NAMES(_r)[col]->len)==0))
		{
			RES_TYPES(_r)[col] = DB1_STR;
			if((reply->element[col]->str == NULL) || (strcmp(reply->element[col]->str,"") == 0))
			{
				VAL_STR(dval) = dummy_string;
				VAL_NULL(dval) = 1;
				LM_INFO("dval value: %s",VAL_STR(dval).s);
			}		
			else
			{
				int len = strlen(reply->element[col]->str);
				LM_INFO("len on string : %d",len);

				VAL_STR(dval).s = pkg_malloc(len+1);
				if (VAL_STR(dval).s == NULL) {
					LM_ERR("no private memory left\n");
					return -7;
				}
				strncpy(VAL_STR(dval).s, reply->element[col]->str, len);
				((char*)VAL_STR(dval).s)[len] = '\0';
				VAL_FREE(dval) = 1;
				VAL_STR(dval).len = len;
			}
		}
		else if((strncmp(RES_NAMES(_r)[col]->s,"expires",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"last_modified",RES_NAMES(_r)[col]->len)==0))
		{
			RES_TYPES(_r)[col] = DB1_DATETIME;
			if((reply->element[col]->str == NULL) || (strcmp(reply->element[col]->str,"") == 0))
			{
				reply->element[col]->str = "0";
			}
			time_t tt = atoi(reply->element[col]->str);
			struct tm * ptm = localtime(&tt);
			strftime (string_time, 30, "%Y-%m-%d %H:%M:%S",  ptm);
			db_str2time(string_time, &VAL_TIME(dval));
		}
		else if((strncmp(RES_NAMES(_r)[col]->s,"q",RES_NAMES(_r)[col]->len)==0))
		{
			RES_TYPES(_r)[col] = DB1_DOUBLE;
			if((reply->element[col]->str == NULL) || (strcmp(reply->element[col]->str,"") == 0))
			{
				VAL_DOUBLE(dval) = 0;
				VAL_NULL(dval) = 1;
			}
			else
			{
				db_str2double(reply->element[col]->str, &VAL_DOUBLE(dval));
			}
		}
		else if((strncmp(RES_NAMES(_r)[col]->s,"cseq",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"flags",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"cflags",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"methods",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"reg_id",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"server_id",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"connection_id",RES_NAMES(_r)[col]->len)==0) 
				|| (strncmp(RES_NAMES(_r)[col]->s,"table_version",RES_NAMES(_r)[col]->len)==0)
				|| (strncmp(RES_NAMES(_r)[col]->s,"keepalive",RES_NAMES(_r)[col]->len)==0))
		{
			RES_TYPES(_r)[col] = DB1_INT;
			if((reply->element[col]->str == NULL) || (strcmp(reply->element[col]->str,"") == 0)){
				VAL_INT(dval) = 0;
				VAL_NULL(dval) = 1;
			}
			else
			{
				VAL_INT(dval) = atoi(reply->element[col]->str);
			}
			LM_INFO("type: DB1_INT");
		}
		else
		{
			LM_ERR("No matching column");
			return -1;	
		}
		VAL_TYPE(dval) = RES_TYPES(_r)[col];
	}	
	return 0;
}

/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* db_redis_init(const str* _url)
{
	LM_INFO("in db_redis_init");
	return db_do_init(_url, (void *)db_redis_new_connection);
}


/*
 * Just like insert, but replace the row if it exists
 */
int db_redis_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m)
{
	LM_INFO("in db_redis_replace");
	return -1;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_redis_use_table(db1_con_t* _h, const str* _t)
{
	LM_DBG("in db_redis_use_table");
	return db_use_table(_h, _t);
}

/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_redis_delete(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const int _n)
{
	int i;
	int field_count;
	const db_val_t *tval; /* used for accessing the value structure */
	int vtype;  /* used for storing the value type */    
	int return_code;/* used to store function return code */
	char *table_name; /* used to store the table name passed by srdb1 */
	char field_temp_storage_value[255]; /* initialize the loop variables */
	const char* redis_command_argv[_n*2]; /*used to store the redis command as an array */
	//redisReply* replyruid = NULL; /* used to store redis reply */
	redisReply* reply_redis = NULL; /* used to store pipeline command result */
	redisReply* redis_out = NULL; /* used to store pipeline command result */
	redisReply* reply_expire_redis= NULL; /* used to store the expire key based result */
	km_redis_con_t* _context; /* defining the redis connection context */
	_context = REDIS_CON(_h); /*get the redis context */
	struct timeval tv; /*setting the timeout for the context */
	int retry  =0;
	memset(field_temp_storage_value, '\0', sizeof(field_temp_storage_value));
	//getting the table name
	table_name = (char*)pkg_malloc((CON_TABLE(_h)->len+1) * sizeof(char));
	return_code = snprintf(table_name,(CON_TABLE(_h)->len+1) * sizeof(char),"%s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if(return_code<0)
	{
		LM_ERR("no private memory left\n");
		goto error;
	}

	LM_INFO("DELETE Query: %s",table_name);
	tv.tv_sec = 0;
    	tv.tv_usec = 100000;
    	redisSetTimeout(_context->con,tv);
	//delete the location enteries
	if(strcmp(table_name,"location")==0)
	{      
		if( _k != NULL)
		{
			for(i=0;i<_n;i++)
			{
				tval = _v + i;
				vtype = VAL_TYPE(tval);
				LM_INFO("db_redis_delete key name : %s",_k[i]->s);
				/*check weather delete is based on expires value*/
				if((strcmp(_k[i]->s,"expires")==0) && (i==0))
				{
					redis_command_argv[0] = pkg_strdup("ZRANGEBYSCORE");
					redis_command_argv[1] = pkg_strdup("kamailio:location:expires");
					//redis_command_argv[2] = pkg_strdup("0");
				}
				switch(vtype)
				{       
					case DB1_INT:   
						sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
						break;
					case DB1_BIGINT:
						sprintf(field_temp_storage_value,"%lld",VAL_BIGINT(tval));
						//strcat(redis_query,  (char *)VAL_BIGINT(tval));
						break;
					case DB1_DOUBLE:
						sprintf(field_temp_storage_value,"%lf",VAL_DOUBLE(tval));
						break;
					case DB1_STRING:
						//strncpy(field_temp_storage_value,VAL_STRING(tval),strlen(VAL_STRING(tval)));
						sprintf(field_temp_storage_value,"%s",VAL_STRING(tval));
						LM_INFO("its STRING");
						break;
					case DB1_STR:   
						strncpy(field_temp_storage_value,VAL_STR(tval).s,VAL_STR(tval).len);
						LM_INFO("its STR");
						break;
					case DB1_DATETIME:
						sprintf(field_temp_storage_value,"%ld",VAL_TIME(tval));
						break;
					case DB1_BLOB:  
						strncpy(field_temp_storage_value,VAL_BLOB(tval).s, VAL_BLOB(tval).len);
						break;
					case DB1_BITMAP:
						sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
						break;
					default:
						LM_ERR("val type [%d] not supported\n", vtype);
						goto error;
				}
				LM_INFO("db_redis_delete attribute: %s value: %s",_k[i]->s,field_temp_storage_value);
				if(strcmp(_k[i]->s,"expires")==0)
                                {
					redis_command_argv[_n*2-(i+1)] = pkg_strdup(field_temp_storage_value);	
				}
				//username based key deletion from redis
				if(strcmp(_k[i]->s,"username")==0)
				{
					//ruid related code commented
					/*
					replyruid = redisCommand(_context->con,"HMGET kamailio:location:%s ruid",field_temp_storage_value);
					if (!replyruid )
					{
						freeReplyObject(replyruid);
						goto error;
					}
					if(!(replyruid->element[0]->str))
					{
						LM_INFO("element null");
						//freeReplyObject(replyruid);
						goto error;
					}
					*/
					for(retry=0;retry<2;retry++)
					{
						freeReplyObject(redisCommand(_context->con,"MULTI"));
						//freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",replyruid->element[0]->str));
						freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",field_temp_storage_value));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:partition %s",field_temp_storage_value));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:expires %s",field_temp_storage_value));
						reply_redis = (redisCommand(_context->con,"EXEC"));
						if (!reply_redis )
						{
							if (redisReconnect(_context->con))
							{       
							      LM_INFO("Redis: Failed to reconnect server\n");
							      freeReplyObject(reply_redis);
							      goto error;
							 }
						}
						else
						{
							if(reply_redis){
								freeReplyObject(reply_redis);
							}
							goto done;
						}
					}
				}
				//ruid based key deletion from redis -ruid related codes commented
				/*
				if(strcmp(_k[i]->s,"ruid")==0)
				{
					for(retry=0;retry<2;retry++)
                                        {
						//getting the username using ruid
						replyruid = redisCommand(_context->con,"HGETALL kamailio:location:%s",field_temp_storage_value);
						if (!replyruid )
						{
							if (redisReconnect(_context->con))
                                                	{        
                                                    		LM_INFO("Redis: Failed to reconnect server\n");
                                                		freeReplyObject(replyruid);
                                                		goto error;
							}
						}
						else
						{
							retry =2;
						}
					}
					for(retry=0;retry<2;retry++)
                        		{
						freeReplyObject(redisCommand(_context->con,"MULTI"));
						freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",field_temp_storage_value));
						freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",replyruid->element[1]->str));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:partition %s",replyruid->element[1]->str));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:expires %s",replyruid->element[1]->str));
						reply_redis = (redisCommand(_context->con,"EXEC"));
						if (!reply_redis )
						{
							if (redisReconnect(_context->con))
                                              		{        
                                                 		LM_INFO("Redis: Failed to reconnect server\n");
								freeReplyObject(reply_redis);
                                                		goto error;
                                               		}
						}
						goto done;
					}
				}
				*/
			}		
			for(retry=0;retry<2;retry++)
                        {
				reply_expire_redis = redisCommandArgv(_context->con,_n*2,&(redis_command_argv[0]), NULL);
				if (!reply_expire_redis )
				{
					if (redisReconnect(_context->con))
                                	{        
                                		LM_INFO("Redis: Failed to reconnect server\n");
						freeReplyObject(reply_expire_redis);
                                		goto error;
                              		}
				}
				else
				{
					retry = 2;
				}
			}
			//deleting the keys based of expire values
			for (field_count=0; field_count<reply_expire_redis->elements; ++field_count )
			{
				if(!(reply_expire_redis->element[field_count]->str))
				{
					LM_INFO("redis [result] connection error");
					//freeReplyObject(replyruid);
					goto error;
				}
				else{
					LM_INFO("delete get the value from expire %s",reply_expire_redis->element[field_count]->str);
					//ruid related code commented
					/*
					replyruid = redisCommand(_context->con,"HMGET kamailio:location:%s ruid",reply_expire_redis->element[field_count]->str);
					LM_INFO("reply ruid: %s",replyruid->element[0]->str);
					if (!replyruid )
					{
						LM_INFO("Freeing redis object");
						freeReplyObject(replyruid);
						goto error;
					}
					else{
						if(replyruid->element[0]->str){
					*/
					for(retry=0;retry<2;retry++)
                        		{
						//LM_INFO("Executing pipeline: %s",replyruid->element[0]->str);
						freeReplyObject(redisCommand(_context->con,"MULTI"));
						//freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",replyruid->element[0]->str));
						freeReplyObject(redisCommand(_context->con,"DEL kamailio:location:%s",reply_expire_redis->element[field_count]->str));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:partition %s",reply_expire_redis->element[field_count]->str));
						freeReplyObject(redisCommand(_context->con,"ZREM kamailio:location:expires %s",reply_expire_redis->element[field_count]->str));
						redis_out = (redisCommand(_context->con,"EXEC"));
						LM_INFO("Pipeline Executed");
						if (!redis_out )
						{
							freeReplyObject(redis_out);
							if (redisReconnect(_context->con))
                                                        {        
                                                        	LM_INFO("Redis: Failed to reconnect server\n");
                                                               	goto error;
                                                        }
						}
						else
						{
							retry = 2;
						}
						if(redis_out){
							freeReplyObject(redis_out);
						}
					}
				}
			}
			//free the memory of redis_command_argv
			for(i=0;i<_n*2;i++)
			{
				if(redis_command_argv[i])
					pkg_free((char *)redis_command_argv[i]);
			}
			goto done;
		}
		else
		{
			//return error because no key from the query
			goto error;
		}

	}
	else
	{       
		//other table then location and version
	}
	//Successfull
done:
	pkg_free(table_name);
	/*if(reply_redis){
		freeReplyObject(reply_redis);
	}
	*/
	/*if(redis_out){
		freeReplyObject(redis_out);
	}
	*/
	/* if(replyruid){
		freeReplyObject(replyruid);	
	}
	*/
	if(reply_expire_redis){
                freeReplyObject(reply_expire_redis);
        }
	return 0;
	//Error
error:
	if(table_name)
	{
		pkg_free(table_name);
	}
	if(reply_expire_redis){
                freeReplyObject(reply_expire_redis);
        }
	return -1;
}


void db_redis_close(db1_con_t* _h)
{
	LM_INFO("in db_redis_close");
	db_do_close(_h, db_redis_free_connection);
}


/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_redis_update(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const db_key_t* _uk,
		const db_val_t* _uv, const int _n, const int _un)
{
	int i;
	int ret;
	char table_name[50];
	//char redis_input [600]="";
	km_redis_con_t* _context;
	_context = REDIS_CON(_h);
	int vtype;
	const db_val_t *tval;
	char field_temp_storage_value[255];
	char redis_query [600]="";
	const char* command_argv[_un*2+1];
	char username[30];
	int retry = 0;
	redisReply* reply_redis = NULL; /*used to get the redis reply */
	//redisReply* reply_redis = NULL; /*used to get the redis reply */
	struct timeval tv; /*setting the timeout for the context */
	memset(field_temp_storage_value, '\0', sizeof(field_temp_storage_value));

	LM_INFO("in db_redis_update table name  %s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	ret = snprintf(table_name,50,"%s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if(ret<0) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	tv.tv_sec = 0;
        tv.tv_usec = 100000;
        redisSetTimeout(_context->con,tv);
	if(strcmp(table_name,"location")==0) {
		if( _uk != NULL){
			//LM_INFO("key name : %s number of columns to update: %d number of pairs %d ",_uk[0]->s,_un,_n);
			tval = _v+0;
			memset(username, '\0', sizeof(username));
			strncpy(username,VAL_STR(tval).s,VAL_STR(tval).len);

			//ruid related code commented
			/*
			sprintf(redis_input,"HMGET kamailio:location:%s username",ruid);
			reply_redis = redisCommand(_context->con, redis_input);	
			if(!reply_redis) {
				LM_INFO("redis query failure %s",redis_query);
				return -1;
			}
			if(!(reply_redis->element[0]->str)) {
				LM_INFO("redis [result] connection error");
				return -1;
			}
			LM_INFO("username: %s",reply_redis->element[0]->str);
			*/
			sprintf(redis_query,"kamailio:location:%s",username);
			command_argv[0] = "HMSET";
			command_argv[1] = redis_query;
			//freeReplyObject(reply_redis);
			for(i=0;i<_un;i++){
				tval = _uv + i;
				vtype = VAL_TYPE(tval);
				//LM_INFO("key name: %s",_uk[i]->s);
				int x = (i+1)*2;
				command_argv[x] = _uk[i]->s;
				if(VAL_NULL(tval)){
					memset(field_temp_storage_value, '\0', sizeof(field_temp_storage_value));
				}
				else{
					switch(vtype)
					{
						case DB1_INT:
							sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
							break;
						case DB1_BIGINT:
							sprintf(field_temp_storage_value,"%lld",VAL_BIGINT(tval));
							break;
						case DB1_DOUBLE:
							sprintf(field_temp_storage_value,"%lf",VAL_DOUBLE(tval));
							break;
						case DB1_STRING:
							sprintf(field_temp_storage_value,"%s",VAL_STRING(tval));
							break;
						case DB1_STR:
							strncpy(field_temp_storage_value,VAL_STR(tval).s,VAL_STR(tval).len);
							break;
						case DB1_DATETIME:
							sprintf(field_temp_storage_value,"%ld",VAL_TIME(tval));
							break;
						case DB1_BLOB:
							strncpy(field_temp_storage_value,VAL_BLOB(tval).s, VAL_BLOB(tval).len);
							break;
						case DB1_BITMAP:
							sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
							break;
						default:
							LM_ERR("val type [%d] not supported\n", vtype);
							return -1;
					}
				}
				command_argv[x+1] = pkg_strdup (field_temp_storage_value);
				//LM_INFO("argv: %s",command_argv[x+1]);
				memset(field_temp_storage_value, '\0', sizeof(field_temp_storage_value));
			}
			for(retry=0;retry<2;retry++)
                        {
				reply_redis = redisCommandArgv(_context->con,_un*2+2,&(command_argv[0]), NULL);	
				if(!reply_redis) {
                                	LM_INFO("redis query failure %s",redis_query);
                                	if (redisReconnect(_context->con))
                                	{
                                        	LM_INFO("Redis: Failed to reconnect server\n");
                                        	freeReplyObject(reply_redis);
                                        	return -2;
                                	}
                        	}
				else
				{
					retry = 2;
				}
			}

		}
	}

	return 0;
}

/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compar
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_redis_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	int i;
	int j;
	int k;
	const db_val_t *tval; /* used for accessing the value structure */
	int return_code; /* used as checking the return code for the function call */
	char redis_query [600]=""; /* used to store the redis query*/
	char *table_name; /*store table name called from the dbapi */
	char redis_field[600]="";/* store the redis field names */
	char key[255]=""; /* used to store the redis key*/
	km_redis_con_t* _context; /* used to store the redis context */
	_context = REDIS_CON(_h); /* get the redis connection */
	redisReply* reply_redis = NULL; /*used to get the redis reply */
	redisReply* reply_expire_redis = NULL; /*used to get the redis reply */
	int retry =0 ;
	memset(redis_field, '\0', sizeof(redis_field));
	struct timeval tv; /*setting the timeout for the context */
	//checking the result parameter
	if(!_r) 
	{
		LM_ERR("invalid result parameter\n");
		goto error;
	}
	tv.tv_sec = 0;
        tv.tv_usec = 100000;
        redisSetTimeout(_context->con,tv);
	LM_DBG("in db_redis_query table name  %s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	//getting the table name
	table_name = (char*)pkg_malloc((CON_TABLE(_h)->len+1) * sizeof(char));
	return_code = snprintf(table_name,(CON_TABLE(_h)->len+1) * sizeof(char),"%s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if(return_code<0)
	{
		LM_ERR("no private memory left\n");
		goto error;
	}
	LM_DBG("SELECT Query: %s",table_name);
	//allocating the result set
	*_r = db_redis_new_result();
	if (!*_r)  {
		LM_ERR("no memory left for result \n");
		goto error;
	}
	//getting the verion for the queried table
	if(strcmp(table_name,"version")==0)
	{
		tval = _v+0;	
		LM_INFO("number:: %d k: %s value:: %s",_n,_k[0]->s,VAL_STRING(tval));
		sprintf(redis_query,"HMGET kamailio:table_version %s",VAL_STRING(tval));
		if(db_redis_get_columns(*_r,_c,_nc,1)<0) {
			LM_ERR("failed to set the rows in result\n");
			goto error;
		}
		LM_INFO("redis command: %s\n",redis_query);
		reply_redis = redisCommand(_context->con, redis_query);
		if(!reply_redis) {
			LM_INFO("redis query failure %s",redis_query);
			freeReplyObject(reply_redis);
			goto error;
		}
		if(db_redis_get_rows(reply_redis,*_r,0)<0) {
			LM_ERR("failed to set the rows in result\n");
			goto error;
		}
	}
	else if(strcmp(table_name,"location")==0)
	{
		//location related  username
		if(strcmp(_k[0]->s,"username") ==0)
		{
			tval = _v+0;	
			LM_INFO("number:: %d k: %s value:: %s",_n,_k[0]->s,VAL_STR(tval).s);
			sprintf(redis_query,"HMGET kamailio:location:%.*s",VAL_STR(tval).len,VAL_STR(tval).s);
			strcat(redis_query,key);
			strcat(redis_query," ");
			for (i=0; i<_nc; i++) 
			{
				strcat(redis_query,_c[i]->s);
				strcat(redis_query," ");
			}
			LM_INFO("redis command: %s\n",redis_query);
			for(retry=0;retry<2;retry++)
                        {
				reply_redis = redisCommand(_context->con, redis_query);
				if(!reply_redis) {
					LM_INFO("redis query failure %s",redis_query);
                                	if (redisReconnect(_context->con))
                                	{
                                        	LM_INFO("Redis: Failed to reconnect server\n");
                                		freeReplyObject(reply_redis);
                                		goto error;
					}
				}
				else
				{
					retry =2;
				}
                        }
			if((reply_redis->element[0]->str == NULL) || (strcmp(reply_redis->element[0]->str,"") == 0))
			{
				LM_INFO("no element returned");
				(*_r)->col.n = 0;
				(*_r)->n = 0;
				goto done;
			}

			if(db_redis_get_columns(*_r,_c,_nc,1)<0) {
				LM_ERR("failed to set the rows in result\n");
				return -1;
			}
			//only one row that's the 0th row
			db_redis_get_rows(reply_redis,*_r,0);
		}
		//expires based query
		if(strcmp(_k[0]->s,"expires") ==0)
		{
			tval = _v + 1;
			sprintf(redis_query,"ZRANGEBYSCORE kamailio:location:partition %d %d",VAL_INT(tval),VAL_INT(tval));
			LM_DBG("in expires section. query: %s",redis_query);
			for(retry=0;retry<2;retry++)
                        {
				reply_redis = redisCommand(_context->con, redis_query);
				if(!reply_redis) 
				{
					LM_INFO("redis query failure %s",redis_query);
					if (redisReconnect(_context->con))
                                	{        
                                		LM_INFO("Redis: Failed to reconnect server\n");
						freeReplyObject(reply_redis);
                                		goto error;
                                	}
				}
				else
				{
					retry = 2;
				}
			}
			if(reply_redis->elements == 0){
				//no elements returned
				//LM_INFO("NO REDIS RESULT for PARTITION");
				(*_r)->col.n = 0;
				(*_r)->n = 0;
				goto done;
			}
			else{	
				LM_INFO("FOUND REDIS RESULT for PARTITION");
				for(j=0;j<_nc;j++){
					strcat(redis_field,_c[j]->s);
					strcat(redis_field," ");
				}
				//convert the redis result to srdb1 format
				if(db_redis_get_columns(*_r,_c,_nc,reply_redis->elements)<0) {
					LM_ERR("failed to set the rows in result\n");
					return -1;
				}	
				for( k=0;k<reply_redis->elements;k++){	
					sprintf(redis_query,"HMGET kamailio:location:%s %s",reply_redis->element[k]->str,redis_field);
					LM_INFO("Redis query: %s",redis_query);
					for(retry=0;retry<2;retry++)
     					{ 
						reply_expire_redis = redisCommand(_context->con, redis_query);
						if(!reply_expire_redis) {
							LM_INFO("redis query failure %s",redis_query);
							if (redisReconnect(_context->con))
                                                	{        
                                                		LM_INFO("Redis: Failed to reconnect server\n");
								freeReplyObject(reply_expire_redis);
                                                		goto error;
                                                	}
						}
						else
						{
							db_redis_get_rows(reply_expire_redis,*_r,k);
							retry = 2;
						}
					}
				freeReplyObject(reply_expire_redis);
				}
			}
		}
	}
	else
	{
		LM_ERR("Query Not Supported");
		goto error;
	}

done:
	if(table_name)
	{
		pkg_free(table_name);
	}
	if(reply_redis)
	{
		freeReplyObject(reply_redis);
	}
	return 0;
error:
	return -1;
}

/*!
 * \brief Free the query and the result memory in the core
 * \param _con database connection
 * \param _r result set
 * \return 0 on success, -1 on failure
 */
int db_redis_free_result(db1_con_t* _h, db1_res_t* _r)
{
	//const db_val_t *dval;
	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	//freeReplyObject(RES_RESULT(_r));
	if(RES_RESULT(_r)){
		pkg_free(RES_RESULT(_r));
	}
	RES_RESULT(_r) = 0; 
	pkg_free(RES_PTR(_r));
	if (db_free_result(_r) < 0) {
		LM_ERR("unable to free result structure\n");
		return -1;
	}
	LM_DBG("in free result");
	return 0;
}


/*
 * \param _r pointer to a structure representing the result
 * \param nrows number of fetched rows
 * \return return zero on success, negative value on failure
 */
int db_redis_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	LM_INFO("in db_redis_fetch_result");
	return -1;
}


/*
 * Execute a raw SQL query
 */
int db_redis_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	LM_INFO("in db_redis_raw_query");
	return -1;
}

/*
 * Insert a key into redis 
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_redis_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{           

	int i;
	const db_val_t *tval; /* used for accessing the value structure */
	int expires_num_index=0; /* used to store expires value */
	int partition_num_index=0;/* used to store partition_num */
	int return_code=0; /* used as checking the return code for the function call */
	int keepalive=0; /*used to keep track weather keepalive is required or not */
	int vtype;  /*used for storing the value type */    
	//int ruid_index=0; /* used to store the index number in redis_command_argv for ruid */
	char *table_name; /*name of the table name with respect to srdb1 */
	char *username; /*used to store the username */
	const char *redis_command_argv[_n*2+1]; /*used to store the redis command as an array */
	char *redis_location_key_prefix="kamailio:location"; /*prefix for redis keys to store */
	km_redis_con_t* _context; /* defining the redis connection context */
	_context = REDIS_CON(_h); /* get the redis context */
	redisReply* reply_redis = NULL; /*used to get the redis reply */
	char field_temp_storage_value[255]; /*initialize the loop variables */
	char command_temp_storage[255];
	struct timeval tv; /*setting the timeout for the context */
	int retry =0;
	memset(command_temp_storage, '\0', sizeof(command_temp_storage));
	memset(field_temp_storage_value, '\0', sizeof(field_temp_storage_value));

	//getting the table name
	table_name = (char*)pkg_malloc((CON_TABLE(_h)->len+1) * sizeof(char));
	return_code = snprintf(table_name,(CON_TABLE(_h)->len+1) * sizeof(char),"%s%.*s%s",CON_TQUOTESZ(_h), CON_TABLE(_h)->len, CON_TABLE(_h)->s, CON_TQUOTESZ(_h));
	if(return_code<0)
	{
		LM_ERR("no private memory left\n");
		goto error;
	}

	LM_INFO("INSERT Query: %s",table_name);
	tv.tv_sec = 0;
        tv.tv_usec = 100000;
        redisSetTimeout(_context->con,tv);
	//insert to location keys into redis
	if(strcmp(table_name,"location")==0)
	{      
		//redis_command_argv[0] = (char*)pkg_malloc((strlen("HMSET")+1) * sizeof(char));
		return_code = snprintf(command_temp_storage,(strlen("HMSET")+1) * sizeof(char),"%s","HMSET");
		redis_command_argv[0] = pkg_strdup(command_temp_storage);
		if(return_code<0)
		{
			LM_ERR("no private memory left\n");
			goto error;
		}
		//check if key list is there or not
		if( _k != NULL)
		{
			//accessing the value of key 
			tval = _v+0; 

			//size for the redis_location_user_key_size
			int redis_location_user_key_size=strlen(redis_location_key_prefix)+1+VAL_STR(tval).len+1*sizeof(char);
			//allocate the memory for redis_location_user_key
			//redis_command_argv[1] = (char*)pkg_malloc(redis_location_user_key_size);
			//get the redis key for hmset
			memset(command_temp_storage, '\0', sizeof(command_temp_storage));
			snprintf(command_temp_storage,redis_location_user_key_size,"%s:%s",redis_location_key_prefix,VAL_STR(tval).s);
			redis_command_argv[1] = pkg_strdup(command_temp_storage);
			memset(command_temp_storage, '\0', sizeof(command_temp_storage));
			//get the username 
			//username = redis_command_argv[1];
			int username_size=VAL_STR(tval).len+1*sizeof(char);
			username = (char*)pkg_malloc(username_size);
			snprintf(username,username_size,"%s",VAL_STR(tval).s);
			// 	for loop get the attribute value pair for redis in commandArgv 
			//	here we are starting loop from i=1 because i=0 contains the username used into redis key 
			for(i=1;i<_n;i++)
			{
				tval = _v + i;
				vtype = VAL_TYPE(tval); /* getting the type of value */
				int x = i*2; /* getting the even i as a key and odd i as a value */
				redis_command_argv[x] = pkg_strdup(_k[i]->s); /* assiging the key to redis command array */
				//getting the value of specific type and convert it into string before insert into redis
				switch(vtype)
				{
					case DB1_INT:
						sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
						break;
					case DB1_BIGINT:
						sprintf(field_temp_storage_value,"%lld",VAL_BIGINT(tval));
						//strcat(redis_query,  (char *)VAL_BIGINT(tval));
						break;
					case DB1_DOUBLE:
						sprintf(field_temp_storage_value,"%lf",VAL_DOUBLE(tval));
						break;
					case DB1_STRING:
						sprintf(field_temp_storage_value,"%s",VAL_STRING(tval));
						break;
					case DB1_STR:
						strncpy(field_temp_storage_value,VAL_STR(tval).s,VAL_STR(tval).len);
						break;
					case DB1_DATETIME:
						sprintf(field_temp_storage_value,"%ld",VAL_TIME(tval));
						break;
					case DB1_BLOB:
						strncpy(field_temp_storage_value,VAL_BLOB(tval).s, VAL_BLOB(tval).len);
						break;
					case DB1_BITMAP:
						sprintf(field_temp_storage_value,"%d",VAL_INT(tval));
						break;
					default:
						LM_ERR("val type [%d] not supported\n", vtype);
						goto error;
				}//end of switch
				redis_command_argv[x+1] = pkg_strdup(field_temp_storage_value);
				//field_temp_storage_value="";
				memset(field_temp_storage_value,0, sizeof(field_temp_storage_value));
				//add the partition into zset
				if(strcmp(_k[i]->s,"partition")==0)
				{
					partition_num_index = x+1;
				}
				//add the expires value for user into zset
				if(strcmp(_k[i]->s,"expires")==0)
				{
					expires_num_index = x+1;
				}
				if(strcmp(_k[i]->s,"keepalive")==0)
				{
					keepalive = VAL_INT(tval);
				}
				//ruid related code commented

			     /* if(strcmp(_k[i]->s,"ruid")==0)
				{
					ruid_index = x+1;
				}
			     */
			}//end of for loop

		}//if condition
		else
		{
			//key values are null
			goto error;
		}
		//creating Query for location related   
	}
	else
	{       
		//other table then location and version
	}
	for(retry=0;retry<2;retry++)
	{
		//creating a pipeline to add the key into redis
		freeReplyObject(redisCommand(_context->con,"MULTI"));
		//freeReplyObject(redisCommand(_context->con,"HMSET %s:%s username %s",redis_location_key_prefix,redis_command_argv[ruid_index],username)); /*insert the ruid */
		//freeReplyObject(redisCommand(_context->con,"EXPIREAT %s:%s %d",redis_location_key_prefix,redis_command_argv[ruid_index],atoi(redis_command_argv[expires_num_index])+5)); /*insert the ruid */
		freeReplyObject(redisCommand(_context->con,"ZADD %s:expires %s %s",redis_location_key_prefix,redis_command_argv[expires_num_index],username));
		freeReplyObject(redisCommandArgv(_context->con,_n*2,&(redis_command_argv[0]), NULL));
		if(keepalive == 1)
		{
			freeReplyObject(redisCommand(_context->con,"ZADD %s:partition %s %s",redis_location_key_prefix,redis_command_argv[partition_num_index],username));	
		}
		reply_redis = (redisCommand(_context->con,"EXEC"));
		if (!reply_redis )
		{
			if (redisReconnect(_context->con))
                	{        
                		LM_INFO("Redis: Failed to reconnect server\n");
				freeReplyObject(reply_redis);
                		goto error;
               		}	
		}
		else
		{
			goto done;
		}
	}

	//called on when successfully completed
done: 
	//free the memory 
	pkg_free(table_name);
	if(reply_redis){
		freeReplyObject(reply_redis);
	}
	
	for(i=0;i<_n*2;i++)
	{
		if(redis_command_argv[i])
			pkg_free((char*)redis_command_argv[i]);
	}
	if(username){
		pkg_free(username);
	}
	return 0;

	//called on error
error:
	return -1;
}    
