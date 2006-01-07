/*
 * $Id$
 *
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 *  2005-12-19  select framework (mma)
 */

 
#ifndef _SELECT_H
#define _SELECT_H

#include "str.h"
#include "parser/msg_parser.h"

#define MAX_SELECT_PARAMS 32

// Flags for parser table FLAG bitfiels
#define DIVERSION_MASK   0x00FF
// if DIVERSION is set and the function is accepted and has STR param
// the param is changed into SEL_PARAM_DIV and the value is set to (flags & DIVERSION_MASK)
#define DIVERSION        1<<8
// if any parameter is expected at this stage
#define SEL_PARAM_EXPECTED   1<<9
// accept if following parameter is STR (any)
#define CONSUME_NEXT_STR 1<<10
// accept if following parameter is INT
#define CONSUME_NEXT_INT 1<<11
// next parameter is optional (use with CONSUME_NEXT_STR or CONSUME_NEXT_INT
#define OPTIONAL         1<<12
// if conversion to common alias is needed
// up-to now parsed function would be stored in parent_f
// NOTE: the parameter is not consumed for ALIAS, 
// so you can leave it as ..,SEL_PARAM_INT, STR_NULL,..
#define IS_ALIAS         1<<13

/*
 * Selector call parameter
 */
typedef enum {
	SEL_PARAM_INT,  /* Integer parameter */
	SEL_PARAM_STR,  /* String parameter */
	SEL_PARAM_DIV,  /* Integer value got from parsing table */
} select_param_type_t;
	
typedef union {
	int i;  /* Integer value */
	str s;  /* String value */
} select_param_value_t;
	
typedef struct sel_param {
        select_param_type_t type;
        select_param_value_t v;
} select_param_t;

struct select;

typedef int (*select_f)(str* res, struct select* s, struct sip_msg* msg);

typedef struct select {
	select_f f;
	select_f parent_f;
	select_param_t params[MAX_SELECT_PARAMS];
	int n;
} select_t;

typedef struct {
	select_f curr_f;
	select_param_type_t type;
	str name;
	select_f new_f;
	int flags;
} select_row_t;

typedef struct select_table {
  select_row_t *table;
  struct select_table *next;
} select_table_t;

/*
 * Lookup corresponding select function based on
 * the select parameters
 */
int resolve_select(select_t* s);

/*
 * Run the select function
 */
int run_select(str* res, select_t* s, struct sip_msg* msg);

/*
 * Print select for debugging purposes 
 */
void print_select(select_t* s);

/*
 * Register modules' own select parser table
 */
int register_select_table(select_row_t *table);

#define SELECT_F(function) extern int function (str* res, select_t* s, struct sip_msg* msg);
#define ABSTRACT_F(function) int function (str* res, select_t* s, struct sip_msg* msg) {return -1;}

#endif /* _SELECT_H */
