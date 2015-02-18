/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef SCA_UTIL_H
#define SCA_UTIL_H

#include "sca_common.h"

enum {
    SCA_AOR_TYPE_AUTO = (1 << 0),
    SCA_AOR_TYPE_UAC = (1 << 1),
    SCA_AOR_TYPE_UAS = (1 << 2),
};

/* get method, regardless of whether message is a request or response */
int	sca_get_msg_method( sip_msg_t * );

/* populate a str pointer with contact uri from a SIP request or response */
int	sca_get_msg_contact_uri( sip_msg_t *, str * );

/* convenient extraction of cseq number from Cseq header */
int	sca_get_msg_cseq_number( sip_msg_t * );

/* convenient extraction of cseq method from Cseq header */
int	sca_get_msg_cseq_method( sip_msg_t * );

/* convenient From header parsing and extraction */
int	sca_get_msg_from_header( sip_msg_t *, struct to_body ** );

/* convenient To header parsing and extraction */
int	sca_get_msg_to_header( sip_msg_t *, struct to_body ** );

/* count number of characters requiring escape as defined by escape_common */
int	sca_uri_display_escapes_count( str * );

/* convenient AoR extraction from sip: URIs */
int	sca_uri_extract_aor( str *, str * );

/* convenient AoR creation from a Contact URI and another AoR */
int	sca_uri_build_aor( str *, int, str *, str * );

int	sca_aor_create_from_info( str *, uri_type, str *, str *, str * );

int	sca_create_canonical_aor_for_ua( sip_msg_t *, str *, int );
int	sca_create_canonical_aor( sip_msg_t *, str * );

/* convenient call hold detection */
int	sca_call_is_held( sip_msg_t * );

#endif /* SCA_UTIL_H */
