/*
 * $Id$
 *
 * Contact parameter datatype
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
