/*
 * $Id$
 *
 * Contact parameter datatype
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


#ifndef CPARAM_H
#define CPARAM_H

#include "../../str.h"

/*
 * Supported types of contact parameters
 */
typedef enum cptype {
	CP_OTHER = 0,  /* Unknown parameter */
	CP_Q,          /* Q parameter */
	CP_EXPIRES,    /* Expires parameter */
	CP_METHOD      /* Method parameter */
} cptype_t;


/*
 * Structure representing a contact
 */
typedef struct cparam {
	cptype_t type;       /* Type of the parameter */
	str name;            /* Parameter name */
	str body;            /* Parameter body */
	struct cparam* next; /* Next parameter in the list */
} cparam_t;


/*
 * Parse contact parameters
 */
int parse_cparams(str* _s, cparam_t** _p, cparam_t** _q, cparam_t** _e, cparam_t** _m);


/*
 * Free the whole contact parameter list
 */
void free_cparams(cparam_t** _p);


/*
 * Print contact parameter list
 */
void print_cparams(cparam_t* _p);


#endif /* CPARAM_H */
