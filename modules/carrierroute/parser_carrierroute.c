/*
 * $Id$
 *
 * Copyright (C) 1&1 Internet AG
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file parser_carrierroute.c
 * \brief Functions for parsing the config file of cr when using file mode.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <errno.h>
#include "parser_carrierroute.h"

#include "../../ut.h"
#include "../../trim.h"

#define assert_not_null(s) do { \
	if ( NULL == (s) ){ \
		LM_INFO("Unexpected null option \n"); \
		return ERROR_IN_PARSING;\
	} \
} while (0)


/**
 * Gets index of given option inside option list
 *
 * @param opt_name    name of option
 * @param opt_list    points to the option list
 * @param no_options  size of option list
 *
 * @return index of option inside option list, -1 not found
 */
int get_option_position(const char* opt_name, const option_description* opt_list, int no_options){
	int i;
	for (i = 0; i<no_options; i++){
		if (strcmp(opt_name, opt_list[i].name) == 0){
			return i;
		}
	}
	return -1;
}


/**
 * Fills special structure with params from src string
 *
 * @param int_list    string of type {INT_VAL [, INT_VAL]*}
 * @param opts        destination option_description variable
 *
 * @return number of integers in int_list, on success
 *         ERROR_IN_PARSING, on error
 */
int parse_int_list(char *int_list, option_description* opts){
	char *pch, *endptr;
	long val;

	pch = strtok (int_list,", \t");

	while ( pch != NULL )
	{
		LM_DBG("Parsing [%s] \n", pch);
		if ( INT_LIST_MAX_SIZE == opts->no_elems){
			LM_ERR("INT LIST exceeds max allowed size of: %d \n", INT_LIST_MAX_SIZE);
			return ERROR_IN_PARSING;
		}

		endptr = NULL;
		val = strtol(pch, &endptr, 10);

		if ( val < 0 || val> INT_MAX){
			LM_ERR("Parsed value is out of bounds \n");
			return ERROR_IN_PARSING;
		}

		if (*endptr != '\0'){
			LM_ERR("Parsed value is not integer \n");
			return ERROR_IN_PARSING;
		}

		opts->value.int_list[opts->no_elems] = (int) val;
		opts->no_elems ++;
		pch = strtok (NULL, ", \t");
	}

	if ( 0 == opts->no_elems ){
		LM_ERR("The number of int elements cannot be 0 \n");
		return ERROR_IN_PARSING;
	}
	return opts->no_elems;
}

/**
 * Tries to parse right value string into an option
 *
 * @param src   str source
 * @param opt   the option to be filled
 *
 * @return SUCCESSFUL_PARSING, ERROR_IN_PARSING
 */
int parse_rv_option(str src, option_description* opt){
	long l_val;
	double d_val;
	char* endptr;
	int i, ret = ERROR_IN_PARSING;

	switch (opt->type)
	{
		case CFG_INT:
			l_val = strtol(src.s, &endptr, 10);

			if (errno == ERANGE && (l_val == LONG_MAX || l_val == LONG_MIN)) {
					LM_ERR("Conversion error for %s\n", src.s);
			}else
			if (*endptr != '\0'){
				LM_ERR("Value is not integer \n");
			}else
			if ( l_val < 0 || l_val> INT_MAX){
				LM_ERR("The number is out of bounds \n");
			}
			else{// successful rv conversion
				opt->value.int_data = l_val;
				LM_DBG("Key=<%s>, value=<%i> \n", opt->name, opt->value.int_data);
				ret = SUCCESSFUL_PARSING;
			}
			break;
		case CFG_FLOAT:
			d_val = strtod(src.s, &endptr);

			if (errno == ERANGE && (d_val == -HUGE_VAL || d_val == HUGE_VAL)) {
				LM_ERR("Conversion error for %s\n", src.s);
			}else
			if (*endptr != '\0'){
				LM_ERR("Value is not float \n");
			}else
			if ( d_val < 0.0 || d_val> FLT_MAX){
				LM_ERR("The number is out of bounds \n");
			}else{
				opt->value.float_data = d_val;
				LM_DBG("Key=<%s>, value=<%f> \n", opt->name, opt->value.float_data);
				ret = SUCCESSFUL_PARSING;
			}
			break;
		case CFG_STR:
			if ((src.s[0] != '"') && (src.s[src.len-1] != '"')){
				LM_ERR("Not a string \n");
			}
			else{
				// we now expect a string with NO " inside
				for (i=1; i< src.len-2; i++)
					if (src.s[i] == '"') {
						LM_ERR("Not a string \n");
						return ERROR_IN_PARSING;
					}
				strncpy( opt->value.string_data.s, src.s+1, src.len-2);
				opt->value.string_data.s[src.len-2] = '\0';
				opt->value.string_data.len = src.len-2;
				LM_DBG("String Key=<%s>, value=<%s> \n", opt->name, opt->value.string_data.s);
				ret = SUCCESSFUL_PARSING;
			}
			break;
		case CFG_INT_LIST:
			// int list looks like: lv = {NO1 [,NO]*}
			if ((src.len == 2) || ((src.s[0] != '{') && (src.s[src.len-1] != '}'))){
				LM_ERR("Not an int list \n");
			}
			else{
				src.s[src.len-1]='\0';src.s++; src.len = src.len-2;
				//parse a list of comma separated integers
				if ( parse_int_list(src.s, opt) != ERROR_IN_PARSING ){
					// dbg printing of parsed values
					LM_DBG("The number of elements in int_list: %d \n", opt->no_elems);
					for ( i =0; i< opt->no_elems; i++ ){
						LM_DBG("	value=<%d> \n", opt->value.int_list[i]);
					}
					ret = SUCCESSFUL_PARSING;
				}
			}
			break;
		default:
			break;
	}

	opt->visited = VISITED;
	return ret;
}

/**
 * Parses the options part in a file and populates a destination structure.
 * The end of the options part should be signaled by end_str string.
 *
 * @param file pointer to source file
 * @param opts destiation option structure to be populated
 * @param no_options expected number of options
 * @param end_str the delimiter that signals end of options zone
 *
 * @return  SUCCESSFUL_PARSING on success, ERROR_IN_PARSING on error
 */
int parse_options(FILE* file, option_description* opts, int no_options, char* end_str){
	str  data, lv_str, rv_str;
	char *pch, buf[CR_MAX_LINE_SIZE], lv[CR_MAX_LINE_SIZE], rv[CR_MAX_LINE_SIZE];
	int opt_pos, full_line_len, ret;

	ret = ERROR_IN_PARSING;
	data.s = buf;

	/* refactor data.s = buf using get_non_blank_line */
	while ( get_non_blank_line( &data, CR_MAX_LINE_SIZE, file, &full_line_len) == 0  ) /* read a line */
	{
		LM_DBG("Parsing line: %.*s \n", data.len, data.s);

		/* parsing stops when end_str is reached: e.g: }, target */
		if ( strncmp(data.s, end_str, strlen(end_str)) == 0){
			LM_DBG("End of options list received \n");
			fseek(file, -full_line_len, SEEK_CUR);
			ret = SUCCESSFUL_PARSING;
			break;
		}

		/* Line must be of type lv = rv */
		if ( (pch =  strchr(data.s,'=')) == NULL){
			LM_ERR("Parsed line does not contain = delimiter \n");
			break;
		}
		else{ /* parse lv, rv */
			strncpy(lv, data.s, pch-data.s); lv[pch-data.s]='\0';
			strncpy(rv, pch+1, CR_MAX_LINE_SIZE-1); rv[CR_MAX_LINE_SIZE-1]='\0';
			LM_DBG("lv=%s, rv=%s\n", lv, rv);
			lv_str.s=lv; lv_str.len=(int)strlen(lv); trim(&lv_str); lv_str.s[lv_str.len] = '\0';
			rv_str.s=rv; rv_str.len=(int)strlen(rv); trim(&rv_str); rv_str.s[rv_str.len] = '\0';

			if ( (lv_str.len == 0) || (rv_str.len == 0)){
				LM_ERR("String is not lv = rv \n");
				break;
			}

			/* Parsing lv */
			if ( (opt_pos = get_option_position(lv_str.s, opts, no_options )) < 0){
				LM_ERR("Unexpected option received: %s \n", lv);
				break;
			}

			if ( VISITED == opts[opt_pos].visited ){
				LM_ERR("Duplicate option definition %s \n", opts[opt_pos].name);
				break;
			}

			/* Parsing rv: this is the only case the options parsing continues */
			if ( (rv_str.len != 0 ) && (parse_rv_option(rv_str, &opts[opt_pos]) == ERROR_IN_PARSING ))
			{
				LM_ERR("Error in parsing rv value \n");
				break;
			}
		}
	}
	return ret;
}


/**
 * Searches for next content line in the src file
 *
 * @param data the destination trimmed non blank line
 * @param size maximum accepted line length
 * @param file source file
 * @param p_full_len initial lenght of contents line
 *
 * @return  0 on success, -1 on error, 1 on EOF
 */
int get_non_blank_line(str* line, int size, FILE* file, int* pFull_len ){
	char* buf = line->s;

	while ( line->s = buf, fgets( line->s, size, file) != NULL ) /* read a line */
	{
		*pFull_len = line->len = strlen(line->s);
		LM_DBG("line is %s ", line->s);
		/*  missing newline indicates the line length was too big */
		if ( line->s[line->len-1] != '\n' ){
			LM_ERR("Unaccepted line length \n");
			return -1;
		}
		trim(line);
		if( line->len != 0 ){ /* we have a non blank line */
			line->s[line->len] = '\0'; /* just mark end of string*/
			return 0;
		}
	}
	//EOF
	return 1;
}


/**
 * Parses the header of structure part in the source file and retrieves name.
 *
 * @param file pointer to source file
 * @param expected_struct_type name of expected structure
 * @param struct_name the parsed name of the structure.
 *
 * @return SUCCESSFUL_PARSING, EOF_REACHED, ERROR_IN_PARSING
 */
int parse_struct_header(FILE* file, char* expected_struct_type, str* struct_name){
	str data;
	char buf[CR_MAX_LINE_SIZE], name[CR_MAX_LINE_SIZE];
	char str2[CR_MAX_LINE_SIZE], format[CR_MAX_LINE_SIZE];
	int no_toks, full_line_len, ret;

	ret = ERROR_IN_PARSING;

	data.s = buf;
	if( get_non_blank_line( &data, CR_MAX_LINE_SIZE, file, &full_line_len) == 1 ) {/* read a line */
		LM_DBG("Graceful exit out of parse struct \n");
		return EOF_REACHED;
	}

	snprintf(format, CR_MAX_LINE_SIZE-1, " %s %%s %%s %%*s", expected_struct_type);
	no_toks = sscanf(data.s, format, name, str2);
	LM_DBG("no_tok=<%d>, name=<%s> , delim=<%s>\n", no_toks, name, str2);

	switch (no_toks) {
	/* With 1 token parsed, correct is: "domain_name" OR "domain_name{" */
	case 1:
		if ( name[strlen(name)-1] == '{' ) {
			if (strlen(name) > 1) {
				name[strlen(name)-1]='\0';
				ret = SUCCESSFUL_PARSING;
			}
			else {
				LM_ERR("Domain name seems to be empty \n");
			}
		}
		else{ /* is the following non blank line a "{" ? */
			str new_line;
			char new_line_buf[CR_MAX_LINE_SIZE];
			new_line.s = new_line_buf;

			if ( get_non_blank_line(&new_line, CR_MAX_LINE_SIZE, file, &full_line_len) != 0 ) {
				LM_ERR("Unexpected end of file while waiting for { \n");
			} else
			if ( strncmp(new_line.s, "{", 1) != 0) {
				LM_ERR("Unexpected token while waiting for { \n");
			}
			else
				ret = SUCCESSFUL_PARSING;
		}
		break;
	/* with 2 tokens parsed, the second must be "{" */
	case 2:
		if (( strncmp(str2, "{", 1) != 0))
			LM_ERR("Wrongly formatted line: %s\n", data.s);
		else
			ret = SUCCESSFUL_PARSING;
		break;
	default:
		LM_DBG("Wrong number of tokens in line: %s\n", data.s);
	}

	if ( SUCCESSFUL_PARSING == ret ){
		LM_DBG( "Sucessfully parsed struct %s - <%s> header\n", expected_struct_type, name);
		struct_name->len = strlen(name);
		memcpy(struct_name->s, name, struct_name->len);
		struct_name->s[struct_name->len]='\0';
	}
	else
		fseek(file, -full_line_len, SEEK_CUR);

	return ret;
}

int parse_struct_stop(FILE* file){
	char buf[CR_MAX_LINE_SIZE];
	str data;
	int full_line_len;
	data.s = buf;

	if ( get_non_blank_line(&data, CR_MAX_LINE_SIZE, file, &full_line_len) == -1 )	{
		LM_INFO("EOF received \n");
		return ERROR_IN_PARSING;
	}

	if (strcmp(data.s, "}") != 0){
		LM_INFO("Unexpected <%s> while waiting for } \n", data.s);
		return ERROR_IN_PARSING;
	}
	return SUCCESSFUL_PARSING;
}
