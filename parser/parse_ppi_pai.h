/*
 * Copyright (C) 2013 Hugh Waite
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
 * \brief Parser :: Parse P-Asserted-Identity: header
 *
 * \ingroup parser
 */

#ifndef PARSE_PAI_PPI_H
#define PARSE_PAI_PPI_H

#include "../str.h"
#include "msg_parser.h"
#include "parse_to.h"

typedef struct p_id_body {
	to_body_t *id;
	int num_ids;
	struct p_id_body *next;
} p_id_body_t;

int parse_pai_header(struct sip_msg* const msg);
int parse_ppi_header(struct sip_msg* const msg);

/*! casting macro for accessing P-Asserted-Identity body */
#define get_pai(p_msg)  ((p_id_body_t*)(p_msg)->pai->parsed)

/*! casting macro for accessing P-Preferred-Identity body */
#define get_ppi(p_msg)  ((p_id_body_t*)(p_msg)->ppi->parsed)

int free_pai_ppi_body(p_id_body_t *pid_b);

#endif
