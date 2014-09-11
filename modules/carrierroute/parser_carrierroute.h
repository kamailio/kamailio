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
 * \file parser_carrierroute.h
 * \brief Functions for parsing the config file of cr when using file mode.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef PARSER_CARRIERROUTE_H_
#define PARSER_CARRIERROUTE_H_

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <errno.h>
#include "../../str.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"

#define CR_MAX_LINE_SIZE 256
#define MAX_FIELD_NAME 60
#define INT_LIST_MAX_SIZE 10

#define NOT_VISITED 0
#define VISITED 1

#define EXPECTED_END_OF_OPTS 0
#define ERROR_IN_PARSING  -1
#define SUCCESSFUL_PARSING 1
#define EOF_REACHED 2

#define DEFAULT_DOMAIN_NUM 16
#define MAX_DOMAIN_NUM 64 // must be a power of 2

enum opt_type { CFG_STR=0, CFG_INT, CFG_FLOAT, CFG_INT_LIST, MAX_OPTS};

union opt_data{
	int int_data;
	float float_data;
	str string_data;
	int  int_list[INT_LIST_MAX_SIZE];
};

typedef struct {
	char           	name[MAX_FIELD_NAME];
	enum opt_type	type;
	union opt_data	value;
	int         	visited;
	int 		no_elems; // TBD: name should suggest int_list_no_elems
	char		str_buf[CR_MAX_LINE_SIZE];
} option_description;

int get_non_blank_line(str* data, int size, FILE* file, int* full_line_len);

int get_option_position(const char* opt_name, const option_description* opt_list, int no_options);

int parse_options(FILE* file, option_description* opts, int no_options, char* end_str);

int parse_struct_header(FILE* file, char* expected_struct_type, str* struct_name);

int next_token_is_eof(FILE* file);

int next_token_is_struct_stop(FILE* file);

int parse_struct_stop(FILE* file);

#endif /* PARSER_CARRIERROUTE_H_ */
