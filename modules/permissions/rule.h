/*
 * $Id$
 *
 * PERMISSIONS module
 *
 * Copyright (C) 2003 Miklós Tirpák (mtirpak@sztaki.hu)
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
 
#ifndef RULE_H
#define RULE_H 1

#include <regex.h>

#define EXPRESSION_LENGTH 256	/* maximum length of an expression */
#define LINE_LENGTH 500		/* maximum length of lines in the config file */


struct rule_struct;
typedef struct rule_struct rule;

struct expression_struct;
typedef struct expression_struct expression;
	
rule *new_rule(void);
void free_rule(rule *r);
void print_rule(rule *r);
int search_rule(rule *r, char *left, char *right);

expression *new_expression(char *str);
void free_expression(expression *e);
void print_expression(expression *e);
int search_expression(expression *e, char *value);

/*
 * stores an expression
 * value represents the string, and reg_value is the compiled string to POSIX regular expression
*/
struct expression_struct  {
	char	value[EXPRESSION_LENGTH+1];
	regex_t	*reg_value;
	struct expression_struct	*next;
};


/*
 * stores 4 lists of expressions in the following way:
 *
 * a, b, c EXCEPT d, e : f, g EXCEPT h
 * left = a, b, c
 * left_exceptions = d, e
 * right = f, g
 * right_exceptions = h
 */
struct rule_struct {
	expression *left, *left_exceptions, *right, *right_exceptions;
	struct rule_struct	*next;
};


#endif /* RULE_H */
