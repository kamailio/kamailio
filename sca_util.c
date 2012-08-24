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

    *to = t;

    return( 0 );
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
