/*
 * $Id$
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
 */


#ifndef PARSE_TO
#define PARSE_TO

#include "../str.h"

enum {
	TAG_PARAM = 400, GENERAL_PARAM
};

struct to_param{
	int type;              /* Type of parameter */
	str name;              /* Name of parameter */
	str value;             /* Parameter value */
	struct to_param* next; /* Next parameter in the list */
};


struct to_body{
	int error;                    /* Error code */
	str body;                     /* The whole header field body */
	str uri;                      /* URI */
	str display;				  /* Display Name */
	str tag_value;                /* Value of tag */
	struct to_param *param_lst;   /* Linked list of parameters */
	struct to_param *last_param;  /* Last parameter in the list */
};


/* casting macro for accessing To body */
#define get_to( p_msg)      ((struct to_body*)(p_msg)->to->parsed)


/*
 * To header field parser
 */
char* parse_to(char* buffer, char *end, struct to_body *to_b);


void free_to(struct to_body* tb);


#endif
