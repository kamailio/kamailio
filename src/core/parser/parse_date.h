/*
 * Copyright (c) 2007 iptelorg GmbH
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
 * \brief Parser :: Date header
 *
 * \ingroup parser
 */



#ifndef PARSE_DATE
#define PARSE_DATE

#include <time.h>
#include "msg_parser.h"

#define RFC1123DATELENGTH	29

struct date_body{
	int error;  /* Error code */
	struct tm date;
};


/* casting macro for accessing DATE body */
#define get_date(p_msg) ((struct date_body*)(p_msg)->date->parsed)


/*
 * Parse Date header field
 */
void parse_date(char *buf, char *end, struct date_body *db);
int parse_date_header(struct sip_msg *msg);

/*
 * Free all associated memory
 */
void free_date(struct date_body *db);


#endif
