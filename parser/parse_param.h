/* 
 * Generic Parameter Parser
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*! \file
 * \brief Parser :: Generic Parameter Parser
 *
 * \ingroup parser
 */


#ifndef PARSE_PARAM_H
#define PARSE_PARAM_H

#include <stdio.h>

#include "../str.h"

/*! \brief
 * Supported types of parameters
 */
typedef enum ptype {
	P_OTHER = 0, /*!< Unknown parameter */
	P_Q,         /*!< Contact: q parameter */
	P_EXPIRES,   /*!< Contact: expires parameter */
	P_METHODS,   /*!< Contact: methods parameter */
	P_RECEIVED,  /*!< Contact: received parameter */
	P_TRANSPORT, /*!< URI: transport parameter */
	P_LR,        /*!< URI: lr parameter */
	P_R2,        /*!< URI: r2 parameter (ser specific) */
	P_MADDR,     /*!< URI: maddr parameter */
	P_TTL,       /*!< URI: ttl parameter */
	P_DSTIP,     /*!< URI: dstip parameter */
	P_DSTPORT,   /*!< URi: dstport parameter */
	P_INSTANCE,  /*!< Contact: sip.instance parameter */
	P_REG_ID,    /*!< Contact: reg-id parameter */
	P_FTAG,      /*!< URI: ftag parameter */
	P_CALL_ID,   /*!< Dialog event package: call-id */
	P_FROM_TAG,  /*!< Dialog event package: from-tag */
	P_TO_TAG,    /*!< Dialog event package: to-tag */
	P_ISD,       /*!< Dialog event package: include-session-description */
	P_SLA,       /*!< Dialog event package: sla */
	P_MA,        /*!< Dialog event package: ma */
	P_OB         /*!< Contact|URI: ob parameter */
} ptype_t;


/*! \brief
 * Class of parameters
 */
typedef enum pclass {
	CLASS_ANY = 0,      /*!< Any parameters, well-known hooks will be not used */
	CLASS_CONTACT,      /*!< Contact parameters */
	CLASS_URI,          /*!< URI parameters */
	CLASS_EVENT_DIALOG  /*!< Event dialog parameters */
} pclass_t;


/*! \brief
 * Structure representing a parameter
 */
typedef struct param {
	ptype_t type;         /*!< Type of the parameter */
	str name;             /*!< Parameter name */
	str body;             /*!< Parameter body */
	int len;              /*!< Total length of the parameter including = and quotes */
	struct param* next;   /*!< Next parameter in the list */
} param_t;


/*! \brief
 * Hooks to well known parameters for contact class of parameters
 */
struct contact_hooks {
	struct param* expires;  /*!< expires parameter */
	struct param* q;        /*!< q parameter */
	struct param* methods;  /*!< methods parameter */
	struct param* received; /*!< received parameter */
	struct param* instance; /*!< sip.instance parameter */
	struct param* reg_id;   /*!< reg-id parameter */
	struct param* ob;       /*!< ob parameter */
};


/*! \brief
 * Hooks to well known parameter for URI class of parameters
 */
struct uri_hooks {
	struct param* transport; /*!< transport parameter */
	struct param* lr;        /*!< lr parameter */
	struct param* r2;        /*!< r2 parameter */
	struct param* maddr;     /*!< maddr parameter */
	struct param* ttl;       /*!< ttl parameter */
	struct param* dstip;     /*!< Destination IP */
	struct param* dstport;   /*!< Destination port */
	struct param* ftag;      /*!< From tag in the original request */
	struct param* ob;        /*!< ob parameter */
};


struct event_dialog_hooks {
	struct param* call_id;
	struct param* from_tag;
	struct param* to_tag;
	struct param* include_session_description;
	struct param* sla;
	struct param* ma;
};

/*! \brief
 * Union of hooks structures for all classes
 */
typedef union param_hooks {
	struct contact_hooks contact; /*!< Contact hooks */
	struct uri_hooks uri;         /*!< URI hooks */
	struct event_dialog_hooks event_dialog;
} param_hooks_t;

/*! \brief
 * Only parse one parameter
 * @return:
 * 	t: out parameter
 * 	-1: on error
 * 	0: success, but expect a next paramter
 * 	1: success and exepect no more parameters
 */
extern inline int parse_param(str *_s, pclass_t _c, param_hooks_t *_h, param_t *t);


/*! \brief
 * Parse parameters
 *  \param _s is string containing parameters
 *  \param _c is class of parameters
 *  \param _h is pointer to structure that will be filled with pointer to well known parameters
 * linked list of parsed parameters will be stored in the variable _p is pointing to
 * \return The function returns 0 on success and negative number
 * on an error
 */
int parse_params(str* _s, pclass_t _c, param_hooks_t* _h, param_t** _p);

/*! \brief
 * Parse parameters
 *  \param _s is string containing parameters
 *  \param _c is class of parameters
 *  \param _h is pointer to structure that will be filled with pointer to well known parameters
 * linked list of parsed parameters will be stored in the variable _p is pointing to
 * \param separator single character separator
 * \return The function returns 0 on success and negative number
 * on an error
 */
int parse_params2(str* _s, pclass_t _c, param_hooks_t* _h, param_t** _p,
			char separator);


/*! \brief
 * Free linked list of parameters
 */
void free_params(param_t* _p);


/*! \brief
 * Free linked list of parameters from shared memory
 */
void shm_free_params(param_t* _p);


/*! \brief
 * Print linked list of parameters, just for debugging
 */
void print_params(FILE* _o, param_t* _p);


/*! \brief
 * Duplicate linked list of parameters
 */
int duplicate_params(param_t** _n, param_t* _p);


/*! \brief
 * Duplicate linked list of parameters
 */
int shm_duplicate_params(param_t** _n, param_t* _p);


#endif /* PARSE_PARAM_H */
