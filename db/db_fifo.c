/*
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>

#include "../mem/mem.h"
#include "../fifo_server.h"
#include "../dprint.h"
#include "../str.h"
#include "db.h"

#define MAX_SIZE_LINE 256
#define MAX_ARRAY     32

#define SELECT_CMD  1
#define DELETE_CMD  2
#define INSERT_CMD  3
#define UPDATE_CMD  4
#define SELECT_STR      "select"
#define SELECT_STR_LEN  (sizeof(SELECT_STR)-1)
#define DELETE_STR      "delete"
#define DELETE_STR_LEN  (sizeof(DELETE_STR)-1)
#define INSERT_STR      "insert"
#define INSERT_STR_LEN  (sizeof(INSERT_STR)-1)
#define UPDATE_STR      "update"
#define UPDATE_STR_LEN  (sizeof(UPDATE_STR)-1)
#define END_CHR    '.'


#define trim_spaces(string) \
	do { \
		for(;*string.s==' ';string.s++,string.len--);\
		for(;string.s[string.len-1]==' ';string.len--);\
	}while(0)


static char buf[MAX_SIZE_LINE];




int get_avps( FILE *fifo , db_key_t *keys, db_op_t *ops, db_val_t *vals,
													int *nr, int max_nr)
{
	str  line;
	str  key,op,val;
	char *c;

	*nr = 0;

	while(1) {
		/* read a new line */
		line.s = buf;
		if (read_line( line.s, MAX_SIZE_LINE, fifo, &line.len)!=1) {
			LOG(L_ERR,"ERROR:get_avps: cannot read avp(key|op|val)\n");
			goto error;
		}
		trim_spaces(line);
		/* is this the separter/end char? */
		if (line.len==1 && *line.s==END_CHR)
			return 1;
		/* we have a new avp */
		if (*nr<max_nr) {
			/* parse the line key|op|val */
			c = line.s;
			/* parse thr key name */
			for( key.s=c ; isalnum(*c)||*c=='_'||*c=='-' ; c++ );
			key.len = c-key.s;
			if (key.len==0) goto parse_error;
			/* possible spaces? */
			for( ; isspace(*c) ; c++ );
			/* parse the operater */
			op.s = c;
			if (*c=='='||*c=='<'||*c=='>')
				c++;
			else
				goto parse_error;
			op.len = c - op.s;
			/* possible spaces? */
			for( ; isspace(*c) ; c++ );
			/* parse value */
			val.s = c;
			val.len = line.len - (c-line.s);
			/* duplicate the avp -> make all null terminated */
			c = (char*)pkg_malloc(key.len+op.len+val.len+3);
			if (c==0) {
				LOG(L_ERR,"ERROR:get_avps: no more pkg memory\n");
				goto error;
			}
			/*copy the key */
			keys[*nr] = c;
			memcpy( c, key.s, key.len);
			c[key.len] = 0;
			c += key.len;
			/*copy the op */
			ops[*nr] = c;
			memcpy( c, op.s, op.len);
			c[op.len] = 0;
			c += op.len;
			/*copy the val */
			vals[*nr].val.string_val = c;
			memcpy( c, val.s, val.len);
			c[val.len] = 0;
			c += val.len;
			/* done */
			(*nr)++;
		} else {
			LOG(L_WARN,"WARNING:get_avps: too many avps (max=%d), ignoring "
				"\"%.*s\"\n",max_nr,line.len,line.s);
		}
	}
parse_error:
	LOG(L_ERR,"ERROR:get_avps: parse error in \"%.*s\" at char [%d][%c] "
		"offset %d\n",line.len,line.s,*c,*c,c-line.s);
error:
	for(;*nr;*nr--)
		pkg_free( (void*)keys[*nr] );
	return -1;
}


/* returns : -1 bad key list
 *           -2 server error
 *            1 success */
int get_keys( FILE *fifo , db_key_t *keys, int *nr, int max_nr)
{
	str line;
	char *key;
	int ret;

	*nr = 0;
	ret = -1;

	while(1) {
		/* read a new line */
		line.s = buf;
		if (!read_line( line.s, MAX_SIZE_LINE, fifo, &line.len) || !line.len) {
			LOG(L_ERR,"ERROR:get_keys: cannot read key name\n");
			goto error;
		}
		trim_spaces(line);
		DBG("---read <%.*s>\n",line.len,line.s);
		/* is this the separter/end char? */
		if (line.len==1 && *line.s==END_CHR)
			return 1;
		/* we have a new key */
		if (*nr<max_nr) {
			/* duplicate the key -> null terminated */
			key = (char*)pkg_malloc(line.len+1);
			if (key==0) {
				LOG(L_ERR,"ERROR:get_key: no more pkg memory\n");
				ret = -2;
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
	for(;*nr;*nr--)
		pkg_free( (void*)keys[*nr] );
	return ret;
}



int db_fifo( FILE *fifo, char *response_file )
{
	static db_key_t keys1[MAX_ARRAY];
	//static db_op_t  ops1[MAX_ARRAY];
	//static db_val_t vals1[MAX_ARRAY];
	static db_key_t keys2[MAX_ARRAY];
	static db_op_t  ops2[MAX_ARRAY];
	static db_val_t vals2[MAX_ARRAY];
	FILE *rpl = 0;
	str   line;
	int   db_cmd;
	int   nr1, nr2;
	int   ret;

	/* first check the response file */
	rpl = open_reply_pipe( response_file );
	if (rpl==0)
		goto error;

	/* first name must be the real name of the DB operation */
	line.s = buf;
	if (!read_line( line.s, MAX_SIZE_LINE, fifo, &line.len) || line.len==0) {
		fprintf( rpl, "DB command name expected\n");
		LOG(L_ERR,"ERROR:db_fifo: cannot read fifo cmd name\n");
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
	} else {
		fprintf( rpl, "unknown DB command \"%.*s\"\n",line.len,line.s);
		LOG(L_ERR,"ERROR:db_fifo: unknown command \"%.*s\"\n",
			line.len,line.s);
		goto error;
	}
	DBG("DEBUG:db_fifo: cmd \"%.*s\" received\n",line.len,line.s);

	if (db_cmd==SELECT_CMD) {
		/* read the colums to be fetched */
		ret = get_keys( fifo, keys1, &nr1, MAX_ARRAY);
		if (ret==-1) {
			fprintf( rpl, "Bad key list in SELECT DB command "
				"(missing '.' at the end?)\n");
			LOG(L_ERR,"ERROR:db_fifo: bad key list termination in SELECT cmd"
				"(missing '.' at the end?)\n");
			goto error;
		} else if (ret==-2) {
			fprintf( rpl, "Internal Server error\n");
			goto error;
		} else if (nr1==0) {
			fprintf( rpl, "Empty key list found in SELECT DB command\n");
			LOG(L_ERR,"ERROR:db_fifo: no keys specified in SELECT cmd\n");
			goto error;
		}
	} else if (db_cmd==UPDATE_CMD) {
		/* read the col=val pairs to be updated */
	}

	/* read the table name */
	line.s = buf;
	if (!read_line( line.s, MAX_SIZE_LINE, fifo, &line.len) || !line.len) {
		fprintf( rpl, "Table name expected\n");
		LOG(L_ERR,"ERROR:db_fifo: cannot read table name\n");
		goto error;
	}
	trim_spaces(line);

	/*read 'where' avps */
	//if (get_avps( fifo , keys2, ops2, vals2, &nr2, MAX_ARRAY)==-1)
	//	goto error;


	if (db_cmd==SELECT_CMD) {
		 
	}

	fclose(rpl);
	return 0;
error:
	if (rpl) fclose(rpl);
	return -1;
}

