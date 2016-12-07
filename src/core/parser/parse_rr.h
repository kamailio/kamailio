/*
 * Route & Record-Route Parser
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
 */

/*! \file
 * \brief Parser :: Route & Record-Route header field parser
 *
 * \ingroup parser
 */

#ifndef PARSE_RR_H
#define PARSE_RR_H

#include <stdio.h>
#include "msg_parser.h"
#include "parse_nameaddr.h"
#include "parse_param.h"
#include "hf.h"


/*! \brief
 * Structure representing a Route & Record-Route HF body
 */
typedef struct rr {
	name_addr_t nameaddr; /*!< Name-addr part */
	param_t* r2;          /*!< Hook to r2 parameter */
	param_t* params;      /*!< Linked list of other parameters */
	int len;              /*!< Length of the whole route field */
	struct rr* next;      /*!< Next RR in the list */
} rr_t;


/*
 * Parse Route & Record-Route header fields
 */
int parse_rr(struct hdr_field* _r);

/*
 * Parse the body of Route & Record-Route headers
 */
int parse_rr_body(char *buf, int len, rr_t **head);

/*
 * Free list of rr
 * _c is head of the list
 */
void free_rr(rr_t** _r);


/*
 * Free list of rr
 * _c is head of the list
 */
void shm_free_rr(rr_t** _r);


/*
 * Print list of rrs, just for debugging
 */
void print_rr(FILE* _o, rr_t* _r);


/*
 * Duplicate a single rr_t structure using pkg_malloc
 */
int duplicate_rr(rr_t** _new, rr_t* _r);


/*
 * Duplicate a single rr_t structure using pkg_malloc
 */
int shm_duplicate_rr(rr_t** _new, rr_t* _r);

/*
 * Find out if a URI contains r2 parameter which indicates
 * that we put 2 record routes
 */
static inline int is_2rr(str* _params)
{
	str s;
	int i, state = 0;

	if (_params->len == 0) return 0;
	s = *_params;

	for(i = 0; i < s.len; i++) {
		switch(state) {
		case 0:
			switch(s.s[i]) {
			case ' ':
			case '\r':
			case '\n':
			case '\t':           break;
			case 'r':
			case 'R': state = 1; break;
			default:  state = 4; break;
			}
			break;

		case 1:
			switch(s.s[i]) {
			case '2': state = 2; break;
			default:  state = 4; break;
			}
			break;

		case 2:
			switch(s.s[i]) {
			case ';':  return 1;
			case '=':  return 1;
			case ' ':
			case '\r':
			case '\n':
			case '\t': state = 3; break;
			default:   state = 4; break;
			}
			break;

		case 3:
			switch(s.s[i]) {
			case ';':  return 1;
			case '=':  return 1;
			case ' ':
			case '\r':
			case '\n':
			case '\t': break;
			default:   state = 4; break;
			}
			break;

		case 4:
			switch(s.s[i]) {
			case '\"': state = 5; break;
			case ';':  state = 0; break;
			default:              break;
			}
			break;

		case 5:
			switch(s.s[i]) {
			case '\\': state = 6; break;
			case '\"': state = 4; break;
			default:              break;
			}
			break;

		case 6: state = 5; break;
		}
	}

	if ((state == 2) || (state == 3)) return 1;
	else return 0;
}

/*!
 * get first RR header and print comma separated bodies in oroute
 * - order = 0 normal; order = 1 reverse
 * - nb_recs - input=skip number of rr; output=number of printed rrs
 */
int print_rr_body(struct hdr_field *iroute, str *oroute, int order,
				  unsigned int * nb_recs);

int get_path_dst_uri(str *_p, str *_dst);

#endif /* PARSE_RR_H */
