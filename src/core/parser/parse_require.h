/*
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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

/*!
 * \file
 * \brief Require parser
 * \ingroup parser
 */

#ifndef PARSE_REQUIRE_H
#define PARSE_REQUIRE_H

#include "msg_parser.h"
#include "hf.h"
#include "../mem/mem.h"
#include "parse_option_tags.h"

#define get_require(p_msg) \
	((p_msg)->require ? ((struct option_tag_body*)(p_msg)->require->parsed)->option_tags_all : 0)


/*!
 * Parse all Require headers.
 */
int parse_require( struct sip_msg *msg);


void free_require(struct option_tag_body **rb);

#endif /* PARSE_REQUIRE_H */
