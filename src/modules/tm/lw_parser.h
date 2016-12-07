/*
 * Copyright (C) 2007 iptelorg GmbH
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
 * 2007-05-28	lightweight parser implemented for build_local_reparse()
 *              function. Basically copy-pasted from the core parser (Miklos)
 */

#ifndef _LW_PARSER_H
#define _LW_PARSER_H

/*
 * lightweight header field name parser
 * used by build_local_reparse() function in order to construct ACK or CANCEL request
 * from the INVITE buffer
 * this parser supports only the header fields which are needed by build_local_reparse()
 */
char *lw_get_hf_name(char *begin, char *end,
			enum _hdr_types_t *type);

/* returns a pointer to the next line */
char *lw_next_line(char *buf, char *buf_end);

#ifdef USE_DNS_FAILOVER
/* returns the pointer to the first VIA header */
char *lw_find_via(char *buf, char *buf_end);
#endif

#endif /* _LW_PARSER_H */
