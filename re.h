/*
 * $Id$
 *
 * regexp and regexp substitutions implementations
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
 *
 *
 * History:
 * --------
 *   2003-08-04  created by andrei
 */

#ifndef _re_h
#define _re_h

#include "str.h"
#include "parser/msg_parser.h"
#include <sys/types.h> /* for regex */
#include <regex.h>

enum replace_special { REPLACE_NMATCH, REPLACE_CHAR, REPLACE_URI };

struct replace_with{
	int offset; /* offset in string */
	int size;   /* size of replace "anchor" in string */
	enum replace_special type;
	union{
		int nmatch;
		char c;
	}u;
};

struct subst_expr{
	regex_t* re;
	str replacement;
	int replace_all; 
	int n_escapes; /* escapes number (replace[] size) */
	int max_pmatch ; /* highest () referenced */
	struct replace_with replace[1]; /* 0 does not work on all compilers */
};

struct replace_lst{
	int offset;
	int size;   /* at offset, delete size bytes and replace them with rpl */
	str rpl;
	struct replace_lst *next;
};



void subst_expr_free(struct subst_expr* se);
void replace_lst_free(struct replace_lst* l);
struct subst_expr*  subst_parser(str* subst);
struct replace_lst* subst_run( struct subst_expr* se, char* input, 
		                       struct sip_msg* msg);
str* subst_str(char* input, struct sip_msg* msg, struct subst_expr* se);



#endif

