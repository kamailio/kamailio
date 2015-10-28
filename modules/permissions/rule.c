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
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../lib/srutils/tmrec.h"
#include "rule.h"


/* 
 * allocate memory for a new rule 
 */
rule *new_rule(void) 
{
	rule	*r;

	r = (rule *)pkg_malloc(sizeof(rule));
	if (!r) {
		LM_ERR("not enough pkg memory\n");
		return 0;
	}

	memset(r, 0, sizeof(rule));
	return r;
}


/* 
 * free memory allocated by a rule 
 */
void free_rule(rule *r) 
{
	if (!r) return;
		
	if (r->left) free_expression(r->left);
	if (r->left_exceptions) free_expression(r->left_exceptions);
	if (r->right) free_expression(r->right);
	if (r->right_exceptions) free_expression(r->right_exceptions);
	if (r->time_period) {
		free(r->time_period);
		r->time_period = NULL;
	}

	if (r->next) free_rule(r->next);
	pkg_free(r);
}


/* 
 * list rules 
 */
void print_rule(rule *r) 
{
	if (!r) return;
		
	printf("\nNEW RULE:\n");
	printf("\n\tLEFT: ");
	if (r->left) print_expression(r->left);  else printf("ALL");
	if (r->left_exceptions) {
		printf("\n\tLEFT EXCEPTIONS: ");
		print_expression(r->left_exceptions);
	}
	printf("\n\tRIGHT: ");
	if (r->right) print_expression(r->right);  else printf("ALL");
	if (r->right_exceptions) {
		printf("\n\tRIGHT EXCEPTIONS: ");
		print_expression(r->right_exceptions);
	}
	printf("\n");
	if (r->next) print_rule(r->next);
}


/* 
 * look for a proper rule matching with left:right and correct time
 */
int search_rule(rule *r, char *left, char *right) 
{
	rule	*r1;

	r1 = r;
	while (r1) {
		if (( (!r1->left) || (search_expression(r1->left, left)) )
		&& (!search_expression(r1->left_exceptions, left))
		&& ( (!r1->right) || (search_expression(r1->right, right)) )
		&& (!search_expression(r1->right_exceptions, right))
        && (check_time(r1->time_period))) return 1;

		r1 = r1->next;
	}

	return 0;
}


/*
 * allocate memory for a new expression 
 * str is saved in vale, and compiled to POSIX regexp (reg_value)
 */
expression *new_expression(char *str) 
{
	expression	*e;
	
	if (!str) return 0;

	e = (expression *)pkg_malloc(sizeof(expression));
	if (!e) {
		LM_ERR("not enough pkg memory\n");
		return 0;
	}

	strcpy(e->value, str);
	
	e->reg_value = (regex_t*)pkg_malloc(sizeof(regex_t));
	if (!e->reg_value) {
		LM_ERR("not enough pkg memory\n");
		pkg_free(e);
		return 0;
	}

	if (regcomp(e->reg_value, str, REG_EXTENDED|REG_NOSUB|REG_ICASE) ) {
		LM_ERR("bad regular expression: %s\n", str);
		pkg_free(e->reg_value);
		pkg_free(e);
		return NULL;
	}
	
	e->next = 0;
	return e;
}


/* 
 * free memory allocated by an expression 
 */
void free_expression(expression *e) 
{
	if (!e) return;

	if (e->next) free_expression(e->next);
	regfree(e->reg_value);
	pkg_free(e);
}


/* 
 * list expressions 
 */
void print_expression(expression *e) 
{
	if (!e) return;

	printf("%s, ", e->value);
	if (e->next) print_expression(e->next);
}


/*
 * look for matching expression
 */
int search_expression(expression *e, char *value)
{
	expression	*e1;

	e1 = e;
	while (e1) {
		if (regexec(e1->reg_value, value, 0, 0, 0) == 0) 	return 1;
		e1 = e1->next;
	}
	return 0;
}

/* Function to check if we are in a time period */
int check_time(tmrec_t *time_period)
 {
	time_t tv;
 	ac_tm_t act;

	/* set current time */
 	tv = time(NULL);
	memset(&act, 0, sizeof(act));
	if (ac_tm_set_time(&act, tv)<0)
	   goto error;

 	/* if there is no dstart, timerec is valid */
 	if (time_period->dtstart==0)
 		goto done;

 	/* match the specified recurence */
 	if (tr_check_recurrence(time_period, &act, 0)!=0)
 		goto error;

 done:
 	ac_tm_destroy(&act);
 	return 1;

 error:
 	ac_tm_destroy(&act);
 	return 0;
 }
