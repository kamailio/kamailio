/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * History:
 * --------
 *  2003-10-21  file created (bogdan)
 *  2004-06-06  init_db_fifo added, DB api updated (andrei)
 */



#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "../mem/mem.h"
#include "../fifo_server.h"
#include "../dprint.h"
#include "../str.h"
#include "db.h"
#include "db_fifo.h"

#define MAX_SIZE_LINE 512
#define MAX_ARRAY     32

#define SELECT_CMD  1
#define DELETE_CMD  2
#define INSERT_CMD  3
#define UPDATE_CMD  4
#define RAWQUERY_CMD     5
#define RAWQUERYRES_CMD  6

#define SELECT_STR          "select"
#define SELECT_STR_LEN      (sizeof(SELECT_STR)-1)
#define DELETE_STR          "delete"
#define DELETE_STR_LEN      (sizeof(DELETE_STR)-1)
#define INSERT_STR          "insert"
#define INSERT_STR_LEN      (sizeof(INSERT_STR)-1)
#define UPDATE_STR          "update"
#define UPDATE_STR_LEN      (sizeof(UPDATE_STR)-1)
#define RAWQUERY_STR        "raw_query"
#define RAWQUERY_STR_LEN    (sizeof(RAWQUERY_STR)-1)
#define RAWQUERYRES_STR     "raw_query_response"
#define RAWQUERYRES_STR_LEN (sizeof(RAWQUERYRES_STR)-1)
#define END_CHR    '.'

#define INT_TYPE         "int"
#define INT_TYPE_LEN     (sizeof(INT_TYPE)-1)
#define DOUBLE_TYPE      "double"
#define DOUBLE_TYPE_LEN  (sizeof(DOUBLE_TYPE)-1)
#define STRING_TYPE      "string"
#define STRING_TYPE_LEN  (sizeof(STRING_TYPE)-1)
#define DATE_TYPE        "date"
#define DATE_TYPE_LEN    (sizeof(DATE_TYPE)-1)
#define BLOB_TYPE        "blob"
#define BLOB_TYPE_LEN    (sizeof(BLOB_TYPE)-1)
#define BITMAP_TYPE      "bitmap"
#define BITMAP_TYPE_LEN  (sizeof(BITMAP_TYPE)-1)

#define NULL_VAL         "null"
#define NULL_VAL_LEN    (sizeof(NULL_VAL)-1)


#define trim_spaces(str) \
	do { \
		for(;(str).s[0]==' ';(str).s++,((str).len)--);\
		for(;(str).s[(str).len-1]==' ';(str).s[--((str).len)]='\0');\
	}while(0)

#define double_log( _str_ ) \
	do { \
		fprintf( rpl, "ERROR: %s\n",_str_); \
		LOG( L_ERR, "ERROR:(%s:%d): %s\n",__FILE__,__LINE__,_str_); \
	}while(0)

#define semidouble_log( _str_ ) \
	do { \
		fprintf( rpl, "ERROR: Internal Server Error\n"); \
		LOG( L_ERR, "ERROR:(%s:%d): %s\n",__FILE__,__LINE__,_str_); \
	}while(0)

#define get_int(_p_,_end_,_res_,_n_,_err_s_,_err_) \
	do { \
		_res_ = 0;\
		for( _n_=0 ; (_p_)<(_end_) && isdigit(*(_p_)) ; (_p_)++,(_n_)++)\
			(_res_)=(_res_)*10+(*(_p_)-'0');\
		if ((_n_)==0) {\
			double_log( _err_s_ );\
			goto _err_;\
		}\
	}while(0);




static char   buf[MAX_SIZE_LINE];
static char   tbl_buf[MAX_SIZE_LINE]; /* current 'table name' buffer */
static FILE*  rpl;
static db_con_t*     fifo_db_con=0;
static db_func_t fifo_dbf;




static inline int sgn_str2float(str* _s, float* _r, db_type_t* _type )
{
	int i, dot = 0;
	int ngv = 0;
	float order = 0.1;

	*_r = 0;
	*_type = DB_INT;
	i = 0;
	if (_s->len==0) return -3;
	if ( (_s->s[0]=='-' && (ngv=1)==1) || (_s->s[0]=='+') )
		i++;
	for( ; i < _s->len; i++) {
		if (_s->s[i] == '.') {
			if (dot) return -1;
			dot = 1;
			*_type = DB_DOUBLE;
			continue;
		}
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			if (dot) {
				*_r += (_s->s[i] - '0') * order;
				order /= 10;
			} else {
				*_r *= 10;
				*_r += _s->s[i] - '0';
			}
		} else {
			return -2;
		}
	}
	if (ngv) *_r = -(*_r);
	return 0;
}



static inline int parse_db_value( str *s, db_val_t *val, str **ret_s)
{
	db_type_t type;
	db_type_t nr_type;
	int cast;
	struct tm td;
	char *p;
	char *end;
	int n;
	float nr;

	cast = 0;
	*ret_s = 0;
	p = 0;
	type = DB_STR;

	/* some safety checks */
	if (s->len==0 || s->s==0) {
		semidouble_log("BUG -> parse_db_value gets a len/s=0 string");
		goto error;
	}

	/* is there some data cast operator? */
	if (s->s[0]=='[') {
		/* get the end of the cast operator */
		for(p=s->s,end=s->s+s->len ; p<end && *p!=']' ; p++);
		if (p>=end-1) {
			double_log("Bad cast operator format (expected attr=[type]val)");
			goto error;
		}
		n = p - s->s - 1;
		p = s->s + 1;
		DBG("---><%.*s>\n",n,p);
		/*identify the cast type*/
		if (n==INT_TYPE_LEN && !strncasecmp(p,INT_TYPE,n)) {
			type = DB_INT;
		} else if (n==DOUBLE_TYPE_LEN && !strncasecmp(p,DOUBLE_TYPE,n)) {
			type = DB_DOUBLE;
		} else if (n==STRING_TYPE_LEN && !strncasecmp(p,STRING_TYPE,n)) {
			type = DB_STR;
		} else if (n==BLOB_TYPE_LEN && !strncasecmp(p,BLOB_TYPE,n)) {
			type = DB_BLOB;
		} else if (n==DATE_TYPE_LEN && !strncasecmp(p,DATE_TYPE,n)) {
			type = DB_DATETIME;
		} else if (n==BITMAP_TYPE_LEN && !strncasecmp(p,BITMAP_TYPE,n)) {
			type = DB_BITMAP;
		} else {
			double_log("Unknown cast type");
			goto error;
		}
		cast = 1;
		s->s += n+2;
		s->len -= n+2;
	}

	/* string has at least one character */
	DBG("DEBUG:parse_db_value: value id <%.*s>\n",s->len,s->s);
	if ( s->s[0]=='\"' && s->s[s->len-1]=='\"' && s->len!=1) {
		/* can be DB_STR, DB_STRING, DB_BLOB */
		/* get rid of the quoting */
		s->s++;
		s->len -= 2;
		/* if casted, check if is valid */
		if (cast) {
			if (type!=DB_STR && type!=DB_BLOB) {
				double_log("Invalid cast for quoted value");
				goto error;
			}
		} else {
			type = DB_STR;
		}
		/* fill in the val struct */
		memset( val, 0, sizeof(db_val_t));
		val->type = type;
		if (type==DB_STR) {
			val->val.str_val = *s;
			*ret_s = &val->val.str_val;
		} else if (type==DB_BLOB) {
			val->val.blob_val = *s;
			*ret_s = &val->val.blob_val;
		} else {
			semidouble_log("BUG -> type is not STR or BLOB");
			goto error;
		}
	} else if ( s->s[0]=='<' && s->s[s->len-1]=='>' && s->len!=1) {
		/* can be only date+time type DB_DATETIME*/
		/* if casted, check if is valid */
		if (cast && type!=DB_DATETIME) {
			double_log("Invalid cast for quoted value");
			goto error;
		}
		/* get rid of the quoting */
		s->s++;
		s->len -= 2;
		/* start parsing */
		p = s->s;
		end = s->s + s->len;
		td.tm_wday = 0;
		td.tm_yday = 0;
		/* get year */
		get_int( p, end, td.tm_year, n, "Missing year in date format",error);
		td.tm_year -= 1900; /* correction */
		if (*(p++)!='-') goto date_error;
		/* get month */
		get_int( p, end, td.tm_mon, n, "Missing month in date format",error);
		td.tm_mon --; /* correction */
		if (*(p++)!='-') goto date_error;
		/* get day */
		get_int( p, end, td.tm_mday, n, "Missing day in date format",error);
		if (*(p++)!=' ') goto date_error;
		/* get hour */
		get_int( p, end, td.tm_hour, n, "Missing hour in date format",error);
		if (*(p++)!=':') goto date_error;
		/* get minutes */
		get_int( p, end, td.tm_min, n, "Missing minutes in date format",error);
		if (*(p++)!=':') goto date_error;
		/* get seconds */
		get_int( p, end, td.tm_sec, n,"Missing seconds in date format",error);
		if (p!=end) goto date_error;
		td.tm_isdst = -1 ; /*daylight*/
		/* fill the val struct */
		val->type = DB_DATETIME;
		val->val.time_val = mktime( &td );
		/*DBG("DBG: <%.*s> is %s\n",s->len,s->s,ctime(&val->val.time_val));*/
	} else if ( (*(p=s->s)=='+') || (*p=='-') || isdigit(*p) ) {
		/* can be a DB_INT / DB_DOUBLE / DB_BITMAP value */
		if (sgn_str2float( s, &nr, &nr_type)!=0) {
			double_log("Bad int/float value format (expected [+/-]nr[.nr])");
			goto error;
		}
		/* if casted, check if valid */
		if (cast) {
			switch (type) {
				case DB_BITMAP:
					if ( nr_type!=DB_INT || nr<0 ) {
						double_log("Invalid value for BITMAP type");
						goto error;
					}
					break;
				case DB_INT:
				case DB_DOUBLE:
					if (type==DB_INT && nr_type==DB_DOUBLE ) {
						double_log("Invalid cast to INT for a DOUBLE value");
						goto error;
					}
					break;
				default:
					double_log("Invalid cast for numerical value");
					goto error;
			}
		} else {
			type = nr_type;
		}
		/* fill the val struct */
		val->type = type;
		switch (type) {
			case DB_INT:
				val->val.int_val = (int)nr; break;
			case DB_DOUBLE:
				val->val.double_val = nr; break;
			case DB_BITMAP:
				val->val.bitmap_val = (int)nr; break;
			default:
				semidouble_log("BUG -> unknown type when filling num. val");
				goto error;
		}
	} else if (s->len==NULL_VAL_LEN && !strncasecmp(s->s,NULL_VAL,s->len) ) {
		/* it's a NULL val */
		if (!cast) {
			double_log("NULL values requires type casting");
			goto error;
		}
		val->type = type;
		val->nul = 1;
	} else {
		double_log("Unable to recognize value type");
		goto error;
	}

	return 0;
date_error:
	double_log("Bad <date time> format (expected <YYYY-MM-DD hh:mm:ss>)\n");
error:
	return -1;
}



/* returns : -1 error
 *            0 success */
static inline int get_avps( FILE *fifo , db_key_t *keys, db_op_t *ops,
										db_val_t *vals, int *nr, int max_nr)
{
	str  line;
	str  key,op,val;
	char *c;
	str  *p_val;
	int  sp_found;

	*nr = 0;

	while(1) {
		/* read a new line */
		line.s = buf;
		if (read_line( line.s, MAX_SIZE_LINE, fifo, &line.len)!=1) {
			double_log("Command end when reading AVPs - missing . at after "
				"AVP list?");
			goto error;
		}
		trim_spaces(line);
		/* is this the separter/end char? */
		if (line.len==1 && *line.s==END_CHR)
			return 0;
		/* we have a new avp */
		if (*nr<max_nr) {
			/* parse the line key|op|val */
			c = line.s;
			/* parse the key name */
			for( key.s=c ; *c && (isalnum(*c)||*c=='_') ; c++ );
			if (!*c) goto parse_error;
			key.len = c-key.s;
			if (key.len==0) goto parse_error;
			/* possible spaces? */
			for( sp_found=0 ; *c && isspace(*c) ; c++,sp_found=1 );
			if (!*c) goto parse_error;
			/* parse the operator */
			op.s = c;
			switch (*c) {
				case '<':
				case '>':
					if (*(c+1)=='=') c++;
				case '=':
					c++;
					if (!*c) goto parse_error;
					break;
				default:
					/* at least one space must be before unknown ops */
					if(!sp_found) goto parse_error;
					/* eat everything to first space */
					for( ; *c && !isspace(*c) ; c++ );
					if (!*c || c==op.s) goto parse_error; /* 0 length */
					/* include into operator str. one space before and after*/
					op.s--;
					c++;
			}
			op.len = c - op.s;
			/* possible spaces? */
			for( ; *c && isspace(*c) ; c++ );
			if (!*c) goto parse_error;
			/* get value */
			val.s = c;
			val.len = line.len - (c-line.s);
			if (val.len==0) goto parse_error;
			if (parse_db_value( &val, &vals[*nr], &p_val)!=0)
				goto error;
			/* duplicate the avp -> make all null terminated */
			c = (char*)pkg_malloc(key.len+op.len+2+(p_val?p_val->len+1:0));
			if (c==0) {
				semidouble_log("no more pkg memory");
				goto error;
			}
			/*copy the key */
			keys[*nr] = c;
			memcpy( c, key.s, key.len);
			c[key.len] = 0;
			c += key.len + 1;
			/*copy the op */
			ops[*nr] = c;
			memcpy( c, op.s, op.len);
			c[op.len] = 0;
			c += op.len + 1;
			/*copy the val */
			if (p_val) {
				memcpy( c, p_val->s, p_val->len);
				c[p_val->len] = 0;
				p_val->s = c;
			}
			/* done */
			(*nr)++;
		} else {
			LOG(L_WARN,"WARNING:get_avps: too many avps (max=%d), ignoring "
				"\"%.*s\"\n",max_nr,line.len,line.s);
		}
	}
parse_error:
	LOG(L_ERR,"ERROR:get_avps: parse error in \"%.*s\" at char [%d][%c] "
		"offset %d\n",line.len,line.s,*c,*c, (unsigned)(c-line.s));
	double_log("Broken AVP(attr|op|val) in DB command");
error:
	for(;*nr;(*nr)--)
		pkg_free( (void*)keys[(*nr)-1] );
	return -1;
}



/* returns : -1 error
 *            0 success */
static inline int get_keys( FILE *fifo , db_key_t *keys, int *nr, int max_nr)
{
	str line;
	char *key;

	*nr = 0;

	while(1) {
		/* read a new line */
		line.s = buf;
		if (!read_line( line.s, MAX_SIZE_LINE, fifo, &line.len) || !line.len) {
			double_log("Bad key list in SELECT DB command "
				"(missing '.' at the end?)");
			goto error;
		}
		trim_spaces(line);
		/* is this the separter/end char? */
		if (line.len==1 && *line.s==END_CHR)
			return 0;
		/* we have a new key */
		if (*nr<max_nr) {
			/* duplicate the key -> null terminated */
			key = (char*)pkg_malloc(line.len+1);
			if (key==0) {
				semidouble_log("no more pkg memory");
				goto error;
			}
			memcpy( key, line.s, line.len);
			key[line.len] = 0;
			keys[*nr] = key;
			(*nr)++;
		} else {
			LOG(L_WARN,"WARNING:get_keys: too many keys (max=%d), ignoring "
				"\"%.*s\"\n",max_nr,line.len,line.s);
		}
	}

error:
	for(;*nr;(*nr)--)
		pkg_free( (void*)keys[(*nr)-1] );
	return -1;
}



static inline void print_res(db_res_t* res, FILE *rpl)
{
	int i, j;

	for(i = 0; i < RES_COL_N(res); i++) {
		fprintf(rpl, "%s ", RES_NAMES(res)[i]);
	}
	fprintf(rpl,"\n");

	for(i = 0; i < RES_ROW_N(res); i++) {
		for(j = 0; j < RES_COL_N(res); j++) {
			if (RES_ROWS(res)[i].values[j].nul) {
				fprintf(rpl,"NULL ");
				continue;
			}
			switch(RES_ROWS(res)[i].values[j].type) {
				case DB_INT:
					fprintf(rpl,"%d ",
						RES_ROWS(res)[i].values[j].val.int_val);
					break;
				case DB_DOUBLE:
					fprintf(rpl,"%f ",
						RES_ROWS(res)[i].values[j].val.double_val);
				break;
				case DB_DATETIME:
					fprintf(rpl,"%s ",
						ctime(&(RES_ROWS(res)[i].values[j].val.time_val)));
					break;
				case DB_STRING:
					fprintf(rpl,"%s ",
						RES_ROWS(res)[i].values[j].val.string_val);
					break;
				case DB_STR:
					fprintf(rpl,"%.*s ", 
						RES_ROWS(res)[i].values[j].val.str_val.len,
						RES_ROWS(res)[i].values[j].val.str_val.s);
					break;
				case DB_BLOB:
					fprintf(rpl,"%.*s ",
						RES_ROWS(res)[i].values[j].val.blob_val.len,
						RES_ROWS(res)[i].values[j].val.blob_val.s);
					break;
				case DB_BITMAP:
					fprintf(rpl,"%d ",
						RES_ROWS(res)[i].values[j].val.bitmap_val);
					break;
			}
		}
		fprintf(rpl,"\n");
	}
}



/* binds the database module, initializes the database and 
 * registers the db fifo cmd
 * returns 0 on success, -1 on error */
int init_db_fifo(char* fifo_db_url)
{
	if ( bind_dbmod(fifo_db_url, &fifo_dbf)==0 ) {
		if (!DB_CAPABILITY(fifo_dbf, DB_CAP_ALL | DB_CAP_RAW_QUERY)) {
			LOG(L_ERR, "ERROR: init_db_fifo: Database module does "
			    "not implement all function needed by AVP code\n");
			return -1;
		}

		if ( (fifo_db_con=fifo_dbf.init( fifo_db_url ))==0) {
			/* connection failed */
			LOG(L_ERR,"ERROR: init_db_fifo: unable to connect to database -> "
				"fifo DB commands disabled!\n");
		}else if (register_fifo_cmd(db_fifo_cmd, FIFO_DB, 0)<0) {
			LOG(L_ERR, "ERROR: init_db_fifo: unable to register '%s'"
					" FIFO cmd\n", FIFO_DB);
		} else {
			return 0; /* success */
		}
	}else{
		LOG(L_WARN, "WARNING: init_db_fifo: unable to find any db module - "
			"fifo DB commands disabled!\n");
	}
	return -1; /* error */
}





int db_fifo( FILE *fifo, char *response_file )
{
	static db_key_t keys1[MAX_ARRAY];
	static db_op_t  ops1[MAX_ARRAY];
	static db_val_t vals1[MAX_ARRAY];
	static db_key_t keys2[MAX_ARRAY];
	static db_op_t  ops2[MAX_ARRAY];
	static db_val_t vals2[MAX_ARRAY];
	static db_res_t *select_res;
	str   line;
	int   db_cmd;
	int   nr1, nr2;
	int   ret;
	int   n;

	ret = -1; /* default is error */
	rpl =  0;

	if (fifo_db_con==0) /* disabled due to database init/binding errors */
		goto error;
	/* first check the response file */
	rpl = open_reply_pipe( response_file );
	if (rpl==0)
		goto error;

	/* first name must be the real name of the DB operation */
	line.s = buf;
	if (!read_line( line.s, MAX_SIZE_LINE, fifo, &line.len) || line.len==0) {
		double_log("DB command name expected");
		goto error;
	}
	trim_spaces(line);

	/* check the name of the command */
	if (line.len==SELECT_STR_LEN
	&& !strncasecmp( line.s, SELECT_STR, line.len)) {
		db_cmd = SELECT_CMD;
	} else if (line.len==DELETE_STR_LEN
	&& !strncasecmp( line.s, DELETE_STR, line.len)) {
		db_cmd = DELETE_CMD;
	} else if (line.len==INSERT_STR_LEN
	&& !strncasecmp( line.s, INSERT_STR, line.len)) {
		db_cmd = INSERT_CMD;
	} else if (line.len==UPDATE_STR_LEN
	&& !strncasecmp( line.s, UPDATE_STR, line.len)) {
		db_cmd = UPDATE_CMD;
	} else if (line.len==RAWQUERY_STR_LEN
	&& !strncasecmp( line.s, RAWQUERY_STR, line.len)) {
		db_cmd = RAWQUERY_CMD;
	} else if (line.len==RAWQUERYRES_STR_LEN
	&& !strncasecmp( line.s, RAWQUERYRES_STR, line.len)) {
		db_cmd = RAWQUERYRES_CMD;
	} else {
		double_log("Unknown DB command name");
		goto error;
	}
	DBG("DEBUG:db_fifo: cmd \"%.*s\" received\n",line.len,line.s);

	nr1 = 0;
	nr2 = 0;

	if (db_cmd==SELECT_CMD) {
		/* read the columns to be fetched */
		if ( get_keys( fifo, keys1, &nr1, MAX_ARRAY)!=0 )
			goto error;
	} else if (db_cmd==UPDATE_CMD) {
		/* read the col=val pairs to be updated */
		if (get_avps( fifo , keys1, ops1, vals1, &nr1, MAX_ARRAY)!=0 )
			goto error;
		/* must be at least one AVP in an update command */
		if (nr1==0) {
			double_log("UPDATE command must have at least one"
				" field to update");
			goto error;
		}
		/* all the operators must be '=' */
		for(n=0;n<nr1;n++) {
			if (ops1[n][0]!='=' || ops1[n][1]!='\0') {
				double_log("Invalid operator in updated fields (expected = )");
				goto error1;
			}
		}/*end for*/
	} else if (db_cmd==RAWQUERY_CMD || db_cmd==RAWQUERYRES_CMD) {
		/* read the raw db command  */
		line.s = buf;
		if (!read_line( line.s, MAX_SIZE_LINE-1,fifo,&line.len) || !line.len) {
			double_log("Raw db command expected");
			goto error;
		}
		trim_spaces(line);
		/* run the command */
		if (db_cmd==RAWQUERY_CMD)
			n = fifo_dbf.raw_query( fifo_db_con, line.s, 0);
		else
			n = fifo_dbf.raw_query( fifo_db_con, line.s, &select_res);
		if (n!=0) {
			double_log("Internal Server error - DB query failed");
			goto error;
		}
		/* any results? */
		if (db_cmd==RAWQUERYRES_CMD) {
			/* get all response and write them into reply fifo */
			print_res( select_res, rpl);
			/* free the query response */
			fifo_dbf.free_result( fifo_db_con, select_res);
		}
		/* done with success */
		goto done;
	}

	/* read the table name */
	line.s = tbl_buf;/*buf;*/
	if (!read_line( line.s, MAX_SIZE_LINE-1, fifo, &line.len) || !line.len) {
		double_log("Table name expected");
		goto error1;
	}
	trim_spaces(line);

	/* select the correct table */
	line.s[line.len] = 0; /* make it null terminated */

	if (fifo_dbf.use_table( fifo_db_con, line.s) < 0) {
		double_log("use_table function failed");
		goto error1;
	}

	/*read 'where' avps */
	if (get_avps( fifo , keys2, ops2, vals2, &nr2, MAX_ARRAY)!=0 )
		goto error1;

	switch (db_cmd) {
		case SELECT_CMD:
			/* push the query */
			n = fifo_dbf.query( fifo_db_con, nr2?keys2:0, nr2?ops2:0,
						nr2?vals2:0, nr1?keys1:0, nr2, nr1, 0, &select_res );
			if (n!=0) {
				double_log("Internal Server error - DB query failed");
				goto error2;
			}
			/* get all response and write them into reply fifo */
			print_res( select_res, rpl);
			/* free the query response */
			fifo_dbf.free_result( fifo_db_con, select_res);
			break;
		case UPDATE_CMD:
			if (nr1==0) {
				double_log("No values for update (empty first AVP list)");
				goto error;
			}
			/* all the operators must be '=' in the first avp list */
			for(n=0;n<nr1;n++) {
				if (ops1[n][0]!='=' || ops1[n][1]!='\0') {
					double_log("Invalid operator in updated fields "
						"(expected = )");
					goto error;
				}
			}/*end for*/
			/* push the query */
			n = fifo_dbf.update( fifo_db_con, nr2?keys2:0, nr2?ops2:0,
					nr2?vals2:0, keys1, vals1, nr2, nr1 );
			if (n!=0) {
				double_log("Internal Server error - DB query failed");
				goto error2;
			}
			break;
		case DELETE_CMD:
			/* push the query */
			n = fifo_dbf.delete( fifo_db_con, nr2?keys2:0, nr2?ops2:0,
					nr2?vals2:0, nr2);
			if (n!=0) {
				double_log("Internal Server error - DB query failed");
				goto error2;
			}
			break;
		case INSERT_CMD:
			if (nr2==0) {
				double_log("Nothing to insert (empty AVP list)");
				goto error;
			}
			/* all the operators must be '=' */
			for(n=0;n<nr2;n++) {
				if (ops2[n][0]!='=' || ops2[n][1]!='\0') {
					double_log("Invalid operator in inserted fields "
						"(expected = )");
					goto error;
				}
			}/*end for*/
			/* push the query */
			n = fifo_dbf.insert( fifo_db_con, nr2?keys2:0, nr2?vals2:0, nr2);
			if (n!=0) {
				double_log("Internal Server error - DB query failed");
				goto error2;
			}
			break;
	}

	/* success */
done:
	ret = 0;
error2:
	for(;nr2;nr2--)
		pkg_free( (void*)keys2[nr2-1] );
error1:
	for(;nr1;nr1--)
		pkg_free( (void*)keys1[nr1-1] );
error:
	if (rpl) fclose(rpl);
	return ret;
}

