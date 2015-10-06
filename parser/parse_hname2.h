/* 
 * Fast 32-bit Header Field Name Parser
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
 * \brief Parser :: Fast 32-bit Header Field Name Parser
 *
 * \ingroup parser
 */

#ifndef PARSE_HNAME2_H
#define PARSE_HNAME2_H

#include "hf.h"


/** Fast 32-bit header field name parser.
 * @file
 */
char* parse_hname2(char* const begin, const char* const end, struct hdr_field* const hdr);
char* parse_hname2_short(char* const begin, const char* const end, struct hdr_field* const hdr);

#endif /* PARSE_HNAME2_H */
