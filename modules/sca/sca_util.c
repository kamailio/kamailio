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

#include "sca_util.h"

#include "../../parser/sdp/sdp.h"

    int
sca_get_msg_method( sip_msg_t *msg )
{
    assert( msg != NULL );

    if ( msg->first_line.type == SIP_REQUEST ) {
	return( msg->REQ_METHOD );
    }

    return( sca_get_msg_cseq_method( msg ));
}

    int
sca_get_msg_contact_uri( sip_msg_t *msg, str *contact_uri )
{
    contact_body_t	*contact_body;

    assert( msg != NULL );
    assert( contact_uri != NULL );

    if ( SCA_HEADER_EMPTY( msg->contact )) {
	LM_DBG( "Empty Contact header" );
	contact_uri->s = NULL;
	contact_uri->len = 0;

	return( 0 );
    }

    if ( parse_contact( msg->contact ) < 0 ) {
	LM_ERR( "Failed to parse Contact header: %.*s",
		STR_FMT( &msg->contact->body ));
	return( -1 );
    }
    if (( contact_body = (contact_body_t *)msg->contact->parsed ) == NULL ) {
	LM_ERR( "Invalid Contact header: %.*s", STR_FMT( &msg->contact->body ));
	return( -1 );
    }
    if ( contact_body->star ) {
	LM_ERR( "Invalid Contact header: SCA Contact must not be \"*\"" );
	return( -1 );
    }
    if ( contact_body->contacts == NULL ) {
	LM_ERR( "Invalid Contact header: parser found no contacts" );
	return( -1 );
    }
    if ( contact_body->contacts->next ) {
	LM_ERR( "Invalid Contact header: Contact may only contain one URI" );
	return( -1 );
    }

    contact_uri->s = contact_body->contacts->uri.s;
    contact_uri->len = contact_body->contacts->uri.len;

    return( 1 );
}

    int
sca_get_msg_cseq_number( sip_msg_t *msg )
{
    int		cseq;

    assert( msg != NULL );

    if ( SCA_HEADER_EMPTY( msg->cseq )) {
	LM_ERR( "Empty Cseq header" );
	return( -1 );
    }
    if ( str2int( &(get_cseq( msg )->number), (unsigned int *)&cseq ) != 0 ) {
	LM_ERR( "Bad Cseq header: %.*s", STR_FMT( &msg->cseq->body ));
	return( -1 );
    }

    return( cseq );
}

/* assumes cseq header in msg is already parsed */
    int
sca_get_msg_cseq_method( sip_msg_t *msg )
{
    assert( msg != NULL );

    if ( SCA_HEADER_EMPTY( msg->cseq )) {
	LM_ERR( "Empty Cseq header" );
	return( -1 );
    }

    return( get_cseq( msg )->method_id );
}


    int
sca_get_msg_from_header( sip_msg_t *msg, struct to_body **from )
{
    struct to_body	*f;

    assert( msg != NULL );
    assert( from != NULL );

    if ( SCA_HEADER_EMPTY( msg->from )) {
	LM_ERR( "Empty From header" );
	return( -1 );
    }
    if ( parse_from_header( msg ) < 0 ) {
	LM_ERR( "Bad From header" );
	return( -1 );
    }
    f = get_from( msg );
    if ( SCA_STR_EMPTY( &f->tag_value )) {
	LM_ERR( "Bad From header: no tag parameter" );
	return( -1 );
    }

    /* ensure the URI is parsed for future use */
    if ( parse_uri( f->uri.s, f->uri.len, GET_FROM_PURI( msg )) < 0 ) {
	LM_ERR( "Failed to parse From URI %.*s", STR_FMT( &f->uri ));
	return( -1 );
    }

    *from = f;

    return( 0 );
}

    int
sca_get_msg_to_header( sip_msg_t *msg, struct to_body **to )
{
    struct to_body	parsed_to;
    struct to_body	*t = NULL;

    assert( msg != NULL );
    assert( to != NULL );

    if ( SCA_HEADER_EMPTY( msg->to )) {
	LM_ERR( "Empty To header" );
	return( -1 );
    }
    t = get_to( msg );
    if ( t == NULL ) {
	parse_to( msg->to->body.s,
		  msg->to->body.s + msg->to->body.len + 1, /* end of buffer */
		  &parsed_to );
	if ( parsed_to.error != PARSE_OK ) {
	    LM_ERR( "Bad To header" );
	    return( -1 );
	}
	t = &parsed_to;
    }

    /* ensure the URI is parsed for future use */
    if ( parse_uri( t->uri.s, t->uri.len, GET_TO_PURI( msg )) < 0 ) {
	LM_ERR( "Failed to parse To URI %.*s", STR_FMT( &t->uri ));
	return( -1 );
    }

    *to = t;

    return( 0 );
}

/* count characters requiring escape as defined by escape_common */
    int
sca_uri_display_escapes_count( str *display )
{
    int			c = 0;
    int			i;

    if ( SCA_STR_EMPTY( display )) {
	return( 0 );
    }

    for ( i = 0; i < display->len; i++ ) {
	switch ( display->s[ i ] ) {
	case '\'':
	case '"':
	case '\\':
	case '\0':
	    c++;
	    
	default:
	    break;
	}
    }

    return( c );
}

    int
sca_uri_extract_aor( str *uri, str *aor )
{
    char		*semi;

    assert( aor != NULL );

    if ( uri == NULL ) {
	aor->s = NULL;
	aor->len = 0;
	return( -1 );
    }

    aor->s = uri->s;
    semi = memchr( uri->s, ';', uri->len );
    if ( semi != NULL ) {
	aor->len = semi - uri->s;
    } else {
	aor->len = uri->len;
    }

    return( 0 );
}

    int
sca_uri_build_aor( str *aor, int maxlen, str *contact_uri, str *domain_uri )
{
    char	*p;
    char	*dp;
    int		len;

    assert( aor != NULL );
    assert( contact_uri != NULL );
    assert( domain_uri != NULL );

    if ( contact_uri->len + domain_uri->len >= maxlen ) {
	return( -1 );
    }

    p = memchr( contact_uri->s, '@', contact_uri->len );
    if ( p == NULL ) {
	/* no username, by definition can't be an SCA line */
	aor->s = NULL;
	aor->len = 0;

	return( 0 );
    }
    dp = memchr( domain_uri->s, '@', domain_uri->len );
    if ( dp == NULL ) {
	/* may be nameless URI */
	dp = memchr( domain_uri->s, ':', domain_uri->len );
	if ( dp == NULL ) {
	    /* bad domain URI */
	    return( -1 );
	}
    }
    dp++;

    len = p - contact_uri->s;
    memcpy( aor->s, contact_uri->s, len );
    aor->s[ len ] = '@';
    len += 1;
    aor->len = len;

    len = domain_uri->len - ( dp - domain_uri->s );
    memcpy( aor->s + aor->len, dp, len );
    aor->len += len;

    return( aor->len );
}

    int
sca_aor_create_from_info( str *aor, uri_type type, str *user, str *domain,
	str *port )
{
    str		scheme = STR_NULL;
    int		len = 0;

    assert( aor != NULL );

    uri_type_to_str( type, &scheme );

    /* +1 for ':', +1 for '@' */
    len = scheme.len + 1 + user->len + 1 + domain->len;
    if ( !SCA_STR_EMPTY( port )) {
	/* +1 for ':' */
	len += 1 + port->len;
    }

    aor->s = (char *)pkg_malloc( len );
    if ( aor->s == NULL ) {
	LM_ERR( "sca_aor_create_from_info: pkg_malloc %d bytes failed", len );
	return( -1 );
    }

    len = 0;
    SCA_STR_COPY( aor, &scheme );
    len += scheme.len;

    *(aor->s + len) = ':';
    aor->len++;
    len++;

    SCA_STR_APPEND( aor, user );
    len += user->len;

    *(aor->s + len) = '@';
    aor->len++;
    len++;

    SCA_STR_APPEND( aor, domain );
    len += domain->len;

    if ( !SCA_STR_EMPTY( port )) {
	*(aor->s + len) = ':';
	len += 1;

	SCA_STR_APPEND( aor, port );
	len += port->len;
    }

    return( aor->len );
}

    int
sca_create_canonical_aor_for_ua( sip_msg_t *msg, str *c_aor, int ua_opts )
{
    struct to_body	*tf = NULL;
    sip_uri_t		c_uri;
    str			tf_aor = STR_NULL;
    str			contact_uri = STR_NULL;
    int			rc = -1;

    assert( msg != NULL );
    assert( c_aor != NULL );

    memset( c_aor, 0, sizeof( str ));

    if (( ua_opts & SCA_AOR_TYPE_AUTO )) {
	if ( msg->first_line.type == SIP_REQUEST ) {
	    ua_opts = SCA_AOR_TYPE_UAC;
	} else {
	    ua_opts = SCA_AOR_TYPE_UAS;
	}
    }

    if (( ua_opts & SCA_AOR_TYPE_UAC )) {
	if ( sca_get_msg_from_header( msg, &tf ) < 0 ) {
	    LM_ERR( "sca_create_canonical_aor: failed to get From header" );
	    goto done;
	}
    } else {
	if ( sca_get_msg_to_header( msg, &tf ) < 0 ) {
	    LM_ERR( "sca_create_canonical_aor: failed to get To header" );
	    goto done;
	}
    }

    if ( sca_uri_extract_aor( &tf->uri, &tf_aor ) < 0 ) {
	LM_ERR( "sca_create_canonical_aor: failed to extract AoR from "
		"URI <%.*s>", STR_FMT( &tf->uri ));
	goto done;
    }

    memset( &c_uri, 0, sizeof( sip_uri_t ));
    if (( rc = sca_get_msg_contact_uri( msg, &contact_uri )) < 0 ) {
	LM_ERR( "sca_create_canonical_aor: failed to get contact URI from "
		"Contact <%.*s>", STR_FMT( &msg->contact->body ));
	goto done;
    }
    if ( rc > 0 ) {
	if ( parse_uri( contact_uri.s, contact_uri.len, &c_uri ) < 0 ) {
	    LM_ERR( "sca_create_canonical_aor: failed to parse Contact URI "
		    "<%.*s>", STR_FMT( &contact_uri ));
	    rc = -1;
	    goto done;
	}
    }

    if ( SCA_STR_EMPTY( &c_uri.user ) ||
	    SCA_STR_EQ( &c_uri.user, &tf->parsed_uri.user )) {
	/* empty contact header or Contact user matches To/From AoR */
	c_aor->s = (char *)pkg_malloc( tf_aor.len );
	c_aor->len = tf_aor.len;
	memcpy( c_aor->s, tf_aor.s, tf_aor.len );
    } else {
	/* Contact user and To/From user mismatch */
	if ( sca_aor_create_from_info( c_aor, c_uri.type,
		&c_uri.user, &tf->parsed_uri.host,
		&tf->parsed_uri.port ) < 0 ) {
	    LM_ERR( "sca_create_canonical_aor: failed to create AoR from "
		    "Contact <%.*s> and URI <%.*s>",
		    STR_FMT( &contact_uri ), STR_FMT( &tf_aor ));
	    goto done;
	}
    }

    rc = 1;

done:
    return( rc );
}

    int
sca_create_canonical_aor( sip_msg_t *msg, str *c_aor )
{
    return( sca_create_canonical_aor_for_ua( msg, c_aor, SCA_AOR_TYPE_AUTO ));
}

/* XXX this considers any held stream to mean the call is on hold. correct? */
    int
sca_call_is_held( sip_msg_t *msg )
{
    sdp_session_cell_t	*session;
    sdp_stream_cell_t	*stream;
    int			n_sess;
    int			n_str;
    int			is_held = 0;
    int			rc;

    rc = parse_sdp( msg );
    if ( rc < 0 ) {
	LM_ERR( "sca_call_is_held: parse_sdp body failed" );
	return( 0 );
    } else if ( rc > 0 ) {
	LM_DBG( "sca_call_is_held: parse_sdp returned %d, no SDP body", rc );
	return( 0 );
    }

    /* Cf. modules_k/textops's exported is_audio_on_hold */
    for ( n_sess = 0, session = get_sdp_session( msg, n_sess );
	    session != NULL;
	    n_sess++, session = get_sdp_session( msg, n_sess )) {

	for ( n_str = 0, stream = get_sdp_stream( msg, n_sess, n_str );
		stream != NULL;
		n_str++, stream = get_sdp_stream( msg, n_sess, n_str )) {
	    if ( stream->is_on_hold ) {
		is_held = 1;
		goto done;
	    }
	}
    }

done:
    return( is_held );
}
