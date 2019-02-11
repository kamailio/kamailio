/*
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

#if 0

#include "defs.h"


#include "../../core/hash_func.h"
#include "t_funcs.h"
#include "../../core/dprint.h"
#include "../../core/config.h"
#include "../../core/parser/parser_f.h"
#include "../../core/data_lump.h"
#include "../../core/ut.h"
#include "../../core/timer.h"
#include "../../core/data_lump.h"

#include "t_hooks.h"

int _test_insert_to_reply( struct sip_msg *msg, char *str )
{
    struct lump* anchor;
    char *buf;
    int len;

    len=strlen( str );
    buf=pkg_malloc( len );
    if (!buf) {
        LM_ERR("no mem\n");
        return 0;
    }
    memcpy( buf, str, len );

    anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0 , 0);
    if (anchor == NULL) {
        LM_ERR("anchor_lump failed\n");
        return 0;
    }
    if (insert_new_lump_before(anchor,buf, len, 0)==0) {
        LM_ERR("insert_new_lump failed\n");
        return 0;
    }
    return 1;
}

#endif
