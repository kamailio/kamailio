/* 
 * $Id$
 *
 * Generic Parameter Parser
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
 * History:
 * -------
 * 2003-03-24 Created by janakj
 * 2003-04-07 shm duplication support (janakj)
 * 2003-04-07 URI class added (janakj)
 */

#ifndef PARSE_PARAM_H
#define PARSE_PARAM_H

#include <stdio.h>


/*
 * Supported types of parameters
 */
typedef enum ptype {
	P_OTHER = 0, /* Unknown parameter */
	P_Q,         /* Contact: q parameter */
	P_EXPIRES,   /* Contact: expires parameter */
	P_METHODS,   /* Contact: method parameter */
	P_TRANSPORT, /* URI: transport parameter */
	P_LR,        /* URI: lr parameter */
	P_R2,        /* URI: r2 parameter (ser specific) */
	P_MADDR,     /* URI: maddr parameter */
	P_TTL,       /* URI: ttl parameter */
} ptype_t;


/*
 * Class of parameters
 */
typedef enum pclass {
	CLASS_ANY = 0,  /* Any parameters, well-known hooks will be not used */
	CLASS_CONTACT,  /* Contact parameters */
	CLASS_URI       /* URI parameters */
} pclass_t;


/*
 * Structure representing a parameter
 */
typedef struct param {
	ptype_t type;         /* Type of the parameter */
	str name;             /* Parameter name */
	str body;             /* Parameter body */
	int len;              /* Total lenght of the parameter including = and quotes */
	struct param* next;   /* Next parameter in the list */
} param_t;


/*
 * Hooks to well known parameters for contact class of parameters
 */
struct contact_hooks {
	struct param* expires; /* expires parameter */
	struct param* q;       /* q parameter */
	struct param* method;  /* method parameter */
};


/*
 * Hooks to well known parameter for URI class of parameters
 */
struct uri_hooks {
	struct param* transport; /* transport parameter */
	struct param* lr;        /* lr parameter */
	struct param* r2;        /* r2 parameter */
	struct param* maddr;     /* maddr parameter */
	struct param* ttl;       /* ttl parameter */
};


/*
 * Union of hooks structures for all classes
 */
typedef union param_hooks {
	struct contact_hooks contact; /* Contact hooks */
	struct uri_hooks uri;         /* URI hooks */
} param_hooks_t;


/*
 * Parse parameters
 * _s is string containing parameters
 * _c is class of parameters
 * _h is pointer to structure that will be filled with pointer to well known parameters
 * linked list of parsed parameters will be stored in
 * the variable _p is pointing to
 * The function returns 0 on success and negative number
 * on an error
 */
int parse_params(str* _s, pclass_t _c, param_hooks_t* _h, param_t** _p);


/*
 * Free linked list of parameters
 */
void free_params(param_t* _p);


/*
 * Free linked list of parameters from shared memory
 */
void shm_free_params(param_t* _p);


/*
 * Print linked list of parameters, just for debugging
 */
void print_params(FILE* _o, param_t* _p);


/*
 * Duplicate linked list of parameters
 */
int duplicate_params(param_t** _n, param_t* _p);


/*
 * Duplicate linked list of parameters
 */
int shm_duplicate_params(param_t** _n, param_t* _p);


#endif /* PARSE_PARAM_H */
