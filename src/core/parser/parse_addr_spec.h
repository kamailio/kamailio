/*
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * \brief Parser :: Parse addr-spec
 *
 * \ingroup parser
 */

#ifndef PARSE_ADDR_SPEC
#define PARSE_ADDR_SPEC

#include "../str.h"
#include "msg_parser.h"

enum {
	TAG_PARAM = 400, GENERAL_PARAM
};

typedef struct to_param{
	int type;              /*!< Type of parameter */
	str name;              /*!< Name of parameter */
	str value;             /*!< Parameter value */
	struct to_param* next; /*!< Next parameter in the list */
} to_param_t;


typedef struct to_body{
	int error;                    /*!< Error code */
	str body;                     /*!< The whole header field body */
	str uri;                      /*!< URI */
	str display;				  /*!< Display Name */
	str tag_value;                /*!< Value of tag */
	struct sip_uri parsed_uri;
	struct to_param *param_lst;   /*!< Linked list of parameters */
	struct to_param *last_param;  /*!< Last parameter in the list */
} to_body_t;


/*! \brief
 * To header field parser
 */
char* parse_addr_spec(char* const buffer, const char* const end, struct to_body* const to_b, int allow_comma_separated);

void free_to_params(struct to_body* const tb);

void free_to(struct to_body* const tb);

int parse_to_header(struct sip_msg* const msg);

sip_uri_t *parse_to_uri(struct sip_msg* const msg);

#endif
