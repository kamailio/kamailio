/*
 * $Id$
 *
 * Various lcr related functions
 *
 * Copyright (C) 2005-2008 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2005-02-06: created by jh
 */


#ifndef LCR_MOD_H
#define LCR_MOD_H

#include <stdio.h>
#include <pcre.h>
#include "../../mi/mi.h"
#include "../../locking.h"

#define MAX_PREFIX_LEN 32
#define MAX_URI_LEN 256

struct lcr_info {
    char prefix[MAX_PREFIX_LEN + 1];
    unsigned short prefix_len;
    char from_uri[MAX_URI_LEN + 1];
    unsigned short from_uri_len;
    pcre *from_uri_re;
    unsigned int grp_id;
    unsigned short first_gw;  /* gw table index of first gw in gw's group */
    unsigned short priority;
    struct lcr_info *next;
};

extern unsigned int lcr_hash_size_param;

extern gen_lock_t *reload_lock;

int  mi_print_gws(struct mi_node* rpl);
int  mi_print_lcrs(struct mi_node* rpl);
int  reload_gws_and_lcrs(void);

#endif /* LCR_MOD_H */
