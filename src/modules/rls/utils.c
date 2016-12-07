/*
 * rls module - resource list server
 *
 * Copyright (C) 2012 AG Projects
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

#include "../../ut.h"

#define SIP_PREFIX        "sip:"
#define SIP_PREFIX_LEN    sizeof(SIP_PREFIX)-1

str* normalize_sip_uri(const str *uri)
{
        static str normalized_uri;
        static str null_str = {NULL, 0};
        static char buf[MAX_URI_SIZE];

        normalized_uri.s = buf;
        if (un_escape((str *)uri, &normalized_uri) < 0)
        {
                LM_ERR("un-escaping URI\n");
                return &null_str;
        }

        normalized_uri.s[normalized_uri.len] = '\0';
        if (strncasecmp(normalized_uri.s, SIP_PREFIX, SIP_PREFIX_LEN) != 0 && strchr(normalized_uri.s, '@') != NULL)
        {
                memmove(normalized_uri.s+SIP_PREFIX_LEN, normalized_uri.s, normalized_uri.len+1);
                memcpy(normalized_uri.s, SIP_PREFIX, SIP_PREFIX_LEN);
                normalized_uri.len += SIP_PREFIX_LEN;
        }

        return &normalized_uri;
}

#undef SIP_PREFIX
#undef SIP_PREFIX_LEN
