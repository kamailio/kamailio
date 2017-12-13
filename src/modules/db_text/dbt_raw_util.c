/*
 * DBText library
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

#include "../../core/mem/mem.h"

#include "dbt_raw_util.h"

static const char* _regexp = "\\s*(and|or|where|,)?\\s*(\\w*)\\s*(>=|<=|<>|=|>|<)\\s*([0-9\\.]+)?(\"([^\\\\\"]|\\\\\")*\")?";

void log_regerror(int errcode, regex_t *compiled)
{
	size_t length = regerror (errcode, compiled, NULL, 0);
	char *buffer = pkg_malloc (length);
	(void) regerror (errcode, compiled, buffer, length);
	LM_ERR("error compiling regex : %s\n", buffer);
	pkg_free(buffer);
}

char** dbt_str_split(char* a_str, const char a_delim, int* c)
{
	char** result    = 0;
	size_t count     = 0;
	char* tmp        = a_str;
	char* last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;
	int len = 0;

	/* Count how many elements will be extracted. */
	while (*tmp)
	{
		if (a_delim == *tmp)
		{
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	/* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

	*c = count;

	/* Add space for terminating null string so caller
	   knows where the list of returned strings ends. */
	count++;

	result = pkg_malloc(sizeof(char*) * count);

	if (result)
	{
		size_t idx  = 0;
		char* token = strtok(a_str, delim);

		while (token)
		{
			assert(idx < count);
			len = strlen(token);
			char* ptr = pkg_malloc( (len+1) * sizeof(char));
			memcpy(ptr, token, len);
			ptr[len] = '\0';
			*(result + idx) = dbt_trim(ptr);
			token = strtok(0, delim);
			idx++;
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}


char* dbt_trim(char *str)
{
	size_t len = 0;
	char *frontp = str;
	char *endp = NULL;

	if( str == NULL ) { return NULL; }
	if( str[0] == '\0' ) { return str; }

	len = strlen(str);
	endp = str + len;

	/* Move the front and back pointers to address the first non-whitespace
	 * characters from each end.
	 */
	while( isspace(*frontp) ) { ++frontp; }
	if( endp != frontp )
	{
		while( isspace(*(--endp)) && endp != frontp ) {}
	}

	if( str + len - 1 != endp )
		*(endp + 1) = '\0';
	else if( frontp != str &&  endp == frontp )
		*str = '\0';

	/* Shift the string so that it starts at str so that if it's dynamically
	 * allocated, we can still free it on the returned pointer.  Note the reuse
	 * of endp to mean the front of the string buffer now.
	 */
	endp = str;
	if( frontp != str )
	{
		while( *frontp ) { *endp++ = *frontp++; }
		*endp = '\0';
	}


	return str;
}


#define MAX_MATCH 10
#define MAX_CLAUSES 20

void dbt_clean_where(int n, db_key_t* _k, db_op_t* _op, db_val_t* _v)
{
	int i;
	if(_k) {
		for(i=0; i < n; i++) {
			pkg_free(_k[i]->s);
			pkg_free(_k[i]);
		}
		pkg_free(_k);
	}

	if(_op) {
		for(i=0; i < n; i++) {
			pkg_free((char*)_op[i]);
		}
		pkg_free(_op);
	}

	if(_v) {
		for(i=0; i < n; i++) {
			if(_v[i].type == DB1_STR)
				pkg_free(_v[i].val.str_val.s);
		}
		pkg_free(_v);
	}
}

int dbt_build_where(char* where, db_key_t** _k, db_op_t** _o, db_val_t** _v)
{
	db_key_t* _k1 = NULL;
	char** _o1 = NULL;
	db_val_t* _v1 = NULL;
	regmatch_t* matches = NULL;
	int l;
	int len;
	regex_t preg;
	int offset = 0;
	int idx = -1;
	char int_buf[50];
	int res;

	*_k = NULL;
	*_o = NULL;
	*_v = NULL;

	len = strlen(where);

	res = regcomp(&preg, _regexp, REG_EXTENDED);
	if(res)	{
		log_regerror(res, &preg);
		return -1;
	}

	_k1 = pkg_malloc(sizeof(db_key_t) * MAX_CLAUSES);
	_o1 = pkg_malloc(sizeof(char*) * MAX_CLAUSES);
	_v1 = pkg_malloc(sizeof(db_val_t) * MAX_CLAUSES);
	matches = (regmatch_t*)pkg_malloc(sizeof(regmatch_t) * MAX_MATCH);

	if(_k1==NULL || _o1==NULL || _v1==NULL || matches==NULL) {
		LM_ERR("error getting pkg memory\n");
		if(_k1) pkg_free(_k1);
		if(_o1) pkg_free(_o1);
		if(_v1) pkg_free(_v1);
		if(matches) pkg_free(matches);
		return -1;
	}
	memset(_k1, 0, sizeof(db_key_t) * MAX_CLAUSES);
	memset(_o1, 0, sizeof(char*) * MAX_CLAUSES);
	memset(_v1, 0, sizeof(db_val_t) * MAX_CLAUSES);

	while(offset < len) {
		char* buffer = where + offset;

		if (regexec(&preg, buffer, MAX_MATCH, matches, REG_ICASE)) {
			LM_ERR("error running regexp %i '%s'\n", idx, buffer);
			break;
		}
		if(matches[0].rm_so == -1) {
			break;
		}
		idx++;

		// TODO figure out a way to combine and / or
		//      needs changes in dbt_query / dbt_row_match

		l = matches[2].rm_eo - matches[2].rm_so;
		_k1[idx] = pkg_malloc(sizeof(str));
		_k1[idx]->len = l;
		_k1[idx]->s = pkg_malloc(sizeof(char) * (l+1));
		strncpy(_k1[idx]->s, buffer+matches[2].rm_so, l);
		_k1[idx]->s[l]='\0';

		l = matches[3].rm_eo - matches[3].rm_so;
		_o1[idx] = (char*) pkg_malloc(l+1);
		strncpy(_o1[idx], buffer+matches[3].rm_so, l);
		_o1[idx][l]='\0';

		if(matches[5].rm_so == -1) {
			l = matches[4].rm_eo - matches[4].rm_so;
			strncpy(int_buf, buffer+matches[4].rm_so, l);
			int_buf[l] = '\0';
			_v1[idx].type = DB1_INT;
			_v1[idx].val.int_val = atoi(int_buf);
		} else {
			char *start = buffer+matches[5].rm_so+1;
			int writer = 0, reader = 0;
			l = matches[5].rm_eo - matches[5].rm_so - 2;
			_v1[idx].type = DB1_STR;
			_v1[idx].val.str_val.len = l;
			_v1[idx].val.str_val.s = pkg_malloc(l+1);

			for(reader=0; reader < l; reader++) {
				if(start[reader] == '\\' && start[reader+1] == '\"')
					continue;
				_v1[idx].val.str_val.s[writer++] = start[reader];
			}
			_v1[idx].val.str_val.s[writer] = '\0';
			_v1[idx].val.str_val.len = writer;
		}
		if(matches[0].rm_eo != -1)
			offset += matches[0].rm_eo;

	}
	regfree(&preg);
	pkg_free(matches);

	*_k = _k1;
	*_o = (db_op_t*)_o1;
	*_v = _v1;

	return idx+1;
}
