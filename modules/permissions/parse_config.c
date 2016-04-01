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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../../sr_module.h"
#include "rule.h"
#include "parse_config.h"


/*
 * parse a comma separated expression list like a, b, c
 * return 0 on success, -1 on error
 * parsed expressions are returned in **e
 */
static int parse_expression_list(char *str, expression **e) 
{
	int start=0, i=-1, j=-1, apost=0;
	char str2[EXPRESSION_LENGTH];
	expression *e1=NULL, *e2;
	
	if (!str || !e) return -1;

	*e = NULL;
	do {
		i++;
		switch(str[i]) {
			case '"':	apost = !apost;
					break;
			case ',':	if (apost) break;
			case '\0':	/* word found */
					while ((str[start] == ' ') || (str[start] == '\t')) start++;
					if (str[start] == '"') start++;
					j = i-1;
					while ((0 < j) && ((str[j] == ' ') || (str[j] == '\t'))) j--;
					if ((0 < j) && (str[j] == '"')) j--;
					if (start<=j) {
						/* valid word */
						if (j-start+1+1>EXPRESSION_LENGTH) {
							LM_ERR("expression too long "
								"<%.*s>(%d)\n",j-start+1,str+start,j-start+1);
							goto error;
						}
						strncpy(str2, str+start, j-start+1);
						str2[j-start+1] = '\0';
						
						e2 = new_expression(str2);
						if (!e2)
							/* memory error */
							goto error;
						
						if (e1) {
							/* it is not the first */
							e1->next = e2;
							e1 = e2;
						} else {
							/* it is the first */
							*e = e1 = e2;
						}
					} else {
						/* parsing error */
						goto error;
					}
					/* for the next word */
					start = i+1;
		}
	} while (str[i] != '\0');

	return 0;
error:
	if (*e) {
		free_expression(*e);
		*e = NULL;
	}
	return -1;
}


/*
 * parse a complex expression list like a, b, c EXCEPT d, e
 * return 0 on success, -1 on error
 * parsed expressions are returned in **e, and exceptions are returned in **e_exceptions
 */
static int parse_expression(char *str, expression **e, expression **e_exceptions) 
{
	char *except, str2[LINE_LENGTH+1];
	int  i,j;

	if (!str || !e || !e_exceptions) return -1;

	except = strstr(str, " EXCEPT ");
	if (except) {
		/* exception found */
		strncpy(str2, str, except-str);
		str2[except-str] = '\0';
		/* except+8 points to the exception */
		if (parse_expression_list(except+8, e_exceptions)) {
			/* error */
			*e = *e_exceptions = NULL;
			return -1;
		}
	} else {
		/* no exception */
		strcpy(str2, str);
		*e_exceptions = NULL;
	}

	for( i=0; isspace((int)str2[i]) ; i++);
	for( j=strlen(str2)-1 ; isspace((int)str2[j]) ; str2[j--]=0);

	if (strcmp("ALL", str2+i) == 0) {
		*e = NULL;
	} else {
		if (parse_expression_list(str2+i, e)) {
			/* error */
			if (*e_exceptions) free_expression(*e_exceptions);
			*e = *e_exceptions = NULL;
			return -1;
		}
	}
	return 0;
}


/*
 * parse one line of the config file
 * return the rule according to line
 */
static rule *parse_config_line(char *line) 
{
	rule	*rule1;
	expression *left, *left_exceptions, *right, *right_exceptions;
	int	i=-1, exit=0, apost=0, colon=-1, eval=0;
	static char	str1[LINE_LENGTH], str2[LINE_LENGTH+1];

	if (!line) return 0;

	rule1 = 0;
	left = left_exceptions = right = right_exceptions = 0;

	while (!exit) {
		i++;
		switch(line[i]) {
			case '"':	apost = !apost;
					eval = 1;
					break;
			
			case ':':	if (!apost) colon = i;
					eval = 1;
					break;
			
			case '#':	if (apost) break;			
			case '\0':
			case '\n':
					exit = 1;
					break;
			case ' ':	break;
			case '\t':	break;
				
			default:	eval = 1;
			
		}
	}

	if (eval) {
		if ((0<colon) && (colon+1<i)) {
			/* valid line */
			
			/* left expression */
			strncpy(str1, line, colon);
			str1[colon] = '\0';
			if (parse_expression(str1, &left, &left_exceptions)) {
				/* error */
				LM_ERR("failed to parse line-left: %s\n", line);
				goto error;
			}
			
			/* right expression */
			strncpy(str2, line+colon+1, i-colon-1);
			str2[i-colon-1] = '\0';
			if (parse_expression(str2, &right, &right_exceptions)) {
				/* error */
				LM_ERR("failed to parse line-right: %s\n", line);
				goto error;
			}
			
			rule1 = new_rule();
			if (!rule1) {
				LM_ERR("can't create new rule\n");
				goto error;
			}

			rule1->left = left;
			rule1->left_exceptions = left_exceptions;
			rule1->right = right;
			rule1->right_exceptions = right_exceptions;
			return rule1;
		} else {
			/* error */
			LM_ERR("failed to parse line: %s\n", line);
		}
	}
	return 0;

 error:
	if (left) free_expression(left);
	if (left_exceptions) free_expression(left_exceptions);

	if (right) free_expression(right);
	if (right_exceptions) free_expression(right_exceptions);
	
	return 0;
}


/*
 * parse a config file
 * return a list of rules
 */
rule *parse_config_file(char *filename) 
{
	FILE	*file;
	char	line[LINE_LENGTH+1];
	rule	*start_rule = NULL, *rule1 = NULL, *rule2 = NULL;

	file = fopen(filename,"r");
	if (!file) {
		LM_INFO("file not found: %s\n", filename);
		return NULL;
	}
	
	while (fgets(line, LINE_LENGTH, file)) {
		rule2 = parse_config_line(line);
		if (rule2) {
			if (rule1) {
				/* it is not the first rule */
				rule1->next = rule2;
			} else {
				/* it is the first rule */
				start_rule = rule2;
			}
			rule1 = rule2;
		}
	}
	
	fclose(file);
	return start_rule;	/* returns the linked list */
}
