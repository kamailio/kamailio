/*
 * Various lcr related functions
 *
 * Copyright (C) 2005 Juha Heinanen
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
 * --------
 * 2005-02-06: created by jh
 */


#ifndef LCR_MOD_H
#define LCR_MOD_H

#include <stdio.h>
#include "../../parser/msg_parser.h"

#define MAX_NO_OF_GWS 32

/*
 * Type definitions
 */

typedef enum sip_protos uri_transport;

struct gw_info {
    unsigned int ip_addr;
    unsigned int port;
    uri_type scheme;
    uri_transport transport;
    unsigned int prefix_len;
    char prefix[16];
};

extern struct gw_info **gws;	/* Pointer to current gw table pointer */

void print_gws (FILE *reply_file);
int reload_gws (void);

#endif /* LCR_MOD_H */
