/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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
/*!
 * \file
 * \brief Kamailio core :: Selcct Framework
 * \author mma
 * \ingroup core
 * Module: \ref core
 */

 
#ifndef _SELECT_H
#define _SELECT_H

#include "str.h"
#include "parser/msg_parser.h"

#define MAX_SELECT_PARAMS 32
#define MAX_NESTED_CALLS  4

/* Flags for parser table FLAG bitfiels 
 */
#define DIVERSION_MASK   0x00FF

/* if DIVERSION is set and the function is accepted
 * the param is changed into SEL_PARAM_DIV and the value is set to (flags & DIVERSION_MASK)
 *  - it is valuable for STR params (saves parsing time)
 *  - does not release the memory occupied by the parameter
 */
#define DIVERSION        1<<8

/* set if any parameter is expected at this stage
 * (the function must be resolved further)
 */
#define SEL_PARAM_EXPECTED   1<<9

/* accept if following parameter is STR (any)
 * consume that extra parameter in one step
 */
#define CONSUME_NEXT_STR 1<<10

/* accept if following parameter is INT
 * consume that extra parameter in one ste
 */
#define CONSUME_NEXT_INT 1<<11

/* accept all the following parameters
 * without checking them
 */
#define CONSUME_ALL	1<<12

/* next parameter is optional (use with CONSUME_NEXT_STR or CONSUME_NEXT_INT
 * resolution is accepted even if there is no other parameter
 * or the parameter is of wrong type
 */
#define OPTIONAL         1<<13

/* left function is noted to be called
 * rigth function continues in resolution
 * NOTE: the parameter is not consumed for PARENT, 
 * so you can leave it as ..,SEL_PARAM_INT, 0,..
 *
 * run_select then calls all functions with PARENT flag
 * in the order of resolution until the final call or 
 * the result is != 0 (<0 error, 1 null str) 
 * the only one parameter passed between nested calls
 * is the result str*
 */
#define NESTED		1<<14

/* "fixup call" would be done, when the structure is resolved to this node
 * which means call with res and msg NULL
 *
 * if the fixup call return value <0, the select resolution will fail
 */
#define FIXUP_CALL	1<<15

/*
 * Selector call parameter
 */
typedef enum {
	SEL_PARAM_INT,  /* Integer parameter */
	SEL_PARAM_STR,  /* String parameter */
	SEL_PARAM_DIV,  /* Integer value got from parsing table */
	SEL_PARAM_PTR   /* void* data got from e.g. fixup call */
} select_param_type_t;
	
typedef union {
	int i;  /* Integer value */
	str s;  /* String value */
	void* p;/* Any data ptr */
} select_param_value_t;
	
typedef struct sel_param {
        select_param_type_t type;
        select_param_value_t v;
} select_param_t;

struct select;

typedef int (*select_f)(str* res, struct select* s, struct sip_msg* msg);

typedef struct select {
	select_f f[MAX_NESTED_CALLS];
	int param_offset[MAX_NESTED_CALLS+1];
	/* contains broken down select string (@foo.bar[-2].foo -> 4 entries) */
	select_param_t params[MAX_SELECT_PARAMS];
	/* how many elements are used in 'params' */
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

/* the level of the select call that is beeing evaluated
 * by the child process
 */
extern int select_level;

/* pointer to the SIP uri beeing processed.
 * Nested function calls can pass information to each
 * other using this pointer. Only for performace reasons.
 * (Miklos)
 */
extern struct sip_uri	*select_uri_p;

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

/*
 * Tries to parse string pointed by *p (up to first non alpha char) into select structure
 * if parsing succeeded, call resolve_select
 * if resolving passes, returns final structure
 * *p moves to first unused char
 * return 0
 *
 * if memory allocation fails, returns -1
 * if parsing or resolving fails, returns -2
 */
int parse_select (char** p, select_t** s);

/**
 * Frees the select obtained with parse_select().
 */
void free_select(select_t *s);
/*
 * Select parser, result is stored in SHARED memory
 * 
 * If you call this, you must ensure, that the string which
 * is beeing parsed MUST be at the same place for all child
 * processes, e.g. allocated in the shared memory as well
 *
 * parameters and results same as parse_select
 */
int shm_parse_select (char** p, select_t** s);

/**
 * Frees the select obtained with shm_parse_select().
 */
void shm_free_select(select_t *s);


#define SELECT_F(function) extern int function (str* res, select_t* s, struct sip_msg* msg);
#define ABSTRACT_F(function) int function (str* res, select_t* s, struct sip_msg* msg) {return -1;}

#endif /* _SELECT_H */
