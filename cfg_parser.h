/*
 * Standalone Configuration File Parser
 *
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*!
 * \file
 * \brief Kamailio core :: Standalone Configuration File Parser
 * Written by Jan Janak <jan@iptel.org>
 *
 * \ingroup core
 * Module: \ref core
 *
 * See \ref ConfigEngine
 */

#ifndef _CFG_PARSER_H
#define _CFG_PARSER_H

#include "str.h"
#include <stdio.h>

#define MAX_TOKEN_LEN 512 /*!< Token names cannot be longer than this value */


/*! \brief Configuration flags */
typedef enum cfg_flags {
	/*! \brief Extended tokens can contain also delimiters, in addition to
	 * alpha-numeric characters, this is used on the righ side of assignments
	 * where no quotes are used.
	 */
	CFG_EXTENDED_ALPHA = (1 << 0),

	/*! \brief The parser performs case-insensitive comparisons of token strings by
	 * default. The parser will use case-sensitive comparison instead if this
	 * flag is set.
	 */
	CFG_CASE_SENSITIVE = (1 << 1), 

	/*! \brief This is a flag that can be set in the last element of cfg_option
	 * arrays (this is the one with 0 as token name). When this flag is set
	 * then the value or parsing function of the element will be used for
	 * options that do not match any other element in the array.
	 */
	CFG_DEFAULT = (1 << 2),


	/*! \brief When this flag is set then the name of the options is a prefix and all
	 * options that have the same prefix will be matched by this entry.
	 */
	CFG_PREFIX = (1 << 3),

	/*! \brief The result of cfg_parse_str_val will be in a buffer allocated by
	 * pkg_malloc, if the destination varaiable contains a pointer to a buffer
	 * already then it will be freed with pkg_free first.
	 */
	CFG_STR_PKGMEM = (1 << 4),

	/*! \brief The result of cfg_parse_str_val will be in a buffer allocated by
	 * shm_malloc, if the destination variable contains a pointer to a buffer
	 * already then it will be freed with shm_free first.
	 */
	CFG_STR_SHMMEM = (1 << 5),

	/*! \brief The result of cfg_parse_str_val will be in a buffer allocated by
	 * malloc, if the destination variable contains a pointer to a buffer
	 * already then it will be freed with free first.
	 */
	CFG_STR_MALLOC = (1 << 6),

	/*! \brief The result of cfg_parse_str_val will be copied into a pre-allocated
	 * buffer with a fixed size, a pointer to str variable which contains the
	 * buffer and its size is passed to the function in parameter 'param'.
	 */
	CFG_STR_STATIC = (1 << 7),

} cfg_flags_t;


enum cfg_token_type {
	CFG_TOKEN_EOF    = -1,
	CFG_TOKEN_ALPHA  = -2,
	CFG_TOKEN_STRING = -3
};


/*! \brief Structure representing a lexical token */
typedef struct cfg_token {
	char buf [MAX_TOKEN_LEN];
	int type;  /*!< Token type */
	str val;   /*!< Token value */
	struct {   /*!< Position of first and last character of token in file */
		int line; /*!< The starting/ending line of the token */
		int col;  /*!< The starting/ending column of the token */
	} start, end;
} cfg_token_t;


struct cfg_parser;

typedef int (*cfg_func_f)(void* param, struct cfg_parser* st,
						  unsigned int flags);


/*! \brief Token mapping structure.
 *
 * This structure is used to map tokens to values or function calls. Arrays of
 * such structures are typically provided by the caller of the parser.
 */
typedef struct cfg_option {
	char* name;    /*!< Token name */
	unsigned int flags;
	void* param;   /*!< Pointer to the destination variable */
	int val;       /*!< Value */
	cfg_func_f f;  /*!< Parser function to be called */
} cfg_option_t;


/*! \brief Parser state */
typedef struct cfg_parser {
	FILE* f;                 /*!< Handle of the currently open file */
	char* file;              /*!< Current file name */
	int line;                /*!< Current line */
	int col;                 /*!< Column index */
	struct cfg_option* options; /*!< Array of supported options */
	struct {
		cfg_func_f parser;   /*!< Section parser function */
		void* param;         /*!< Parameter value for the parser function */
	} section;
	struct cfg_token* cur_opt; /*!< Current option */
} cfg_parser_t;


extern struct cfg_option cfg_bool_values[];

struct cfg_parser* cfg_parser_init(str* basedir, str* filename);

void cfg_section_parser(struct cfg_parser* st, cfg_func_f parser, void* param);

void cfg_set_options(struct cfg_parser* st, struct cfg_option* options);

int sr_cfg_parse(struct cfg_parser* st);

void cfg_parser_close(struct cfg_parser* st);

struct cfg_option* cfg_lookup_token(struct cfg_option* options, str* token);

/*! ! \brief Interface to the lexical scanner */
int cfg_get_token(struct cfg_token* token, struct cfg_parser* st, unsigned int flags);

/* Commonly needed parser functions */

int cfg_eat_equal(struct cfg_parser* st, unsigned int flags);

int cfg_eat_eol(struct cfg_parser* st, unsigned int flags);

/*! \brief Parse section identifier of form [section]. The function expects parameter
 * param to be of type (str*). The result string is allocated using pkg_malloc
 * and is zero terminated. To free the memory use pkg_free(((str*)param)->s)
 */
int cfg_parse_section(void* param, struct cfg_parser* st, unsigned int flags);

/*! \brief Parse string parameter value, either quoted or unquoted */
int cfg_parse_str_opt(void* param, struct cfg_parser* st, unsigned int flags);

int cfg_parse_str(void* param, struct cfg_parser* st, unsigned int flags);

int cfg_parse_enum_opt(void* param, struct cfg_parser* st, unsigned int flags);

int cfg_parse_enum(void* param, struct cfg_parser* st, unsigned int flags);

/*! \brief Parser integer parameter value */
int cfg_parse_int_opt(void* param, struct cfg_parser* st, unsigned int flags);

int cfg_parse_int(void* param, struct cfg_parser* st, unsigned int flags);

/*! \brief Parse boolean parameter value */
int cfg_parse_bool_opt(void* param, struct cfg_parser* st, unsigned int flags);

int cfg_parse_bool(void* param, struct cfg_parser* st, unsigned int flags);

#endif /* _CFG_PARSER_H */
