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
#include "sca_common.h"

#include <assert.h>

#include "sca_dialog.h"

    int
sca_dialog_build_from_tags( sca_dialog *dialog, int maxlen, str *call_id,
	str *from_tag, str *to_tag )
{
    int		len = 0;

    assert( dialog != NULL && dialog->id.s != NULL );
    assert( call_id != NULL );
    assert( from_tag != NULL );

    len = call_id->len + from_tag->len;
    if ( !SCA_STR_EMPTY( to_tag )) {
	len += to_tag->len;
    }

    if ( len >= maxlen ) {
	LM_ERR( "sca_dialog_build_from_tags: tags too long" );
	return( -1 );
    }

    memcpy( dialog->id.s, call_id->s, call_id->len );
    dialog->call_id.s = dialog->id.s;
    dialog->call_id.len = call_id->len;

    memcpy( dialog->id.s + call_id->len, from_tag->s, from_tag->len );
    dialog->from_tag.s = dialog->id.s + call_id->len;
    dialog->from_tag.len = from_tag->len;

    if ( !SCA_STR_EMPTY( to_tag )) {
	memcpy( dialog->id.s + call_id->len + from_tag->len,
		to_tag->s, to_tag->len );
	dialog->to_tag.s = dialog->id.s + call_id->len + from_tag->len;
	dialog->to_tag.len = to_tag->len;
    }
    dialog->id.len = len;

    return( len );
}

#define SCA_REPLACES_HDR_PREFIX		"Replaces: "
#define SCA_REPLACES_HDR_PREFIX_LEN	strlen( SCA_REPLACES_HDR_PREFIX )
#define SCA_REPLACES_TO_TAG		"to-tag="
#define SCA_REPLACES_TO_TAG_LEN		strlen( SCA_REPLACES_TO_TAG )
#define SCA_REPLACES_FROM_TAG		"from-tag="
#define SCA_REPLACES_FROM_TAG_LEN	strlen( SCA_REPLACES_FROM_TAG )
    int
sca_dialog_create_replaces_header( sca_dialog *dlg, str *replaces_hdr )
{
    int		len;

    assert( replaces_hdr != NULL );

    if ( SCA_STR_EMPTY( &dlg->call_id ) || SCA_STR_EMPTY( &dlg->from_tag ) ||
		SCA_STR_EMPTY( &dlg->to_tag )) {
	LM_ERR( "sca_dialog_create_replaces_header: dialog %.*s is not a "
		"confirmed dialog, cannot build Replaces header",
		STR_FMT( &dlg->id ));
	return( -1 );
    }

    memset( replaces_hdr, 0, sizeof( str ));

    /* +2 for semicolons separating tags, +2 for CRLF */
    replaces_hdr->s = pkg_malloc( SCA_REPLACES_HDR_PREFIX_LEN +
				  SCA_REPLACES_TO_TAG_LEN +
				  SCA_REPLACES_FROM_TAG_LEN +
				  dlg->id.len + 2 + 2 );

    memcpy( replaces_hdr->s, SCA_REPLACES_HDR_PREFIX,
	    SCA_REPLACES_HDR_PREFIX_LEN );
    len = SCA_REPLACES_HDR_PREFIX_LEN;

    memcpy( replaces_hdr->s + len, dlg->call_id.s, dlg->call_id.len );
    len += dlg->call_id.len;

    memcpy( replaces_hdr->s + len, ";", strlen( ";" ));
    len += strlen( ";" );

    memcpy( replaces_hdr->s + len, SCA_REPLACES_TO_TAG,
	    SCA_REPLACES_TO_TAG_LEN );
    len += SCA_REPLACES_TO_TAG_LEN;
    memcpy( replaces_hdr->s + len, dlg->to_tag.s, dlg->to_tag.len );
    len += dlg->to_tag.len;

    memcpy( replaces_hdr->s + len, ";", strlen( ";" ));
    len += strlen( ";" );

    memcpy( replaces_hdr->s + len, SCA_REPLACES_FROM_TAG,
	    SCA_REPLACES_FROM_TAG_LEN );
    len += SCA_REPLACES_FROM_TAG_LEN;
    memcpy( replaces_hdr->s + len, dlg->from_tag.s, dlg->from_tag.len );
    len += dlg->from_tag.len;

    memcpy( replaces_hdr->s + len, CRLF, CRLF_LEN );
    len += CRLF_LEN;

    replaces_hdr->len = len;

    return( len );
}
