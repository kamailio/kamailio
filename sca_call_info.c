#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_appearance.h"
#include "sca_dialog.h"
#include "sca_event.h"
#include "sca_call_info.h"
#include "sca_subscribe.h"

const str	SCA_CALL_INFO_HEADER_STR = STR_STATIC_INIT( "Call-Info: " );
const str	SCA_CALL_INFO_HEADER_NAME = STR_STATIC_INIT( "Call-Info" );

    static int
sca_call_info_domain_from_uri( str *uri, str *domain )
{
    assert( !SCA_STR_EMPTY( uri ));
    assert( domain != NULL );

    domain->s = memchr( uri->s, '@', uri->len );
    if ( domain->s == NULL ) {
	/* may be a sip:domain URI */
	domain->s = memchr( uri->s, ':', uri->len );
	if ( domain->s == NULL ) {
	    LM_ERR( "Bad URI %.*s", STR_FMT( uri ));
	    return( -1 );
	}
    }
    domain->s++;

    domain->len = ( uri->s + uri->len ) - domain->s;
    /* XXX handle :port in URI? */

    return( domain->len );
}

    static int
sca_call_info_header_length( int given_length )
{
    assert( given_length >= 0 );

    /*
     * given_length is assumed to contain length of index string,
     * state string, and uri string, if any
     */

     /* "<sip:DOMAIN>;appearance-index=N;appearance-state=STATE" */
    given_length += strlen( "<sip:>=;=;" );
    given_length += SCA_APPEARANCE_INDEX_STR.len;
    given_length += SCA_APPEARANCE_STATE_STR.len;

    return( given_length );
}

    static int
sca_call_info_header_length_for_appearance( sca_appearance *appearance,
	str *aor )
{
    int		len = 0;
    str		domain = STR_NULL;
    str		state_str = STR_NULL;

    assert( aor != NULL );

    /* get length of stringified index, since conversion's destructive */
    (void)int2str( appearance->index, &len );

    sca_appearance_state_to_str( appearance->state, &state_str );
    len += state_str.len;

    if ( !SCA_STR_EMPTY( &appearance->uri )) {
	len += appearance->uri.len;
    }

    if ( sca_call_info_domain_from_uri( aor, &domain ) < 0 ) {
	return( -1 );
    }
    len += domain.len;
    
    len += sca_call_info_header_length( domain.len );

    return( len );
}

    static int
sca_call_info_header_append_appearances( sca_mod *scam, sca_subscription *sub, 
	char *hdrbuf, int maxlen )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    sca_hash_slot	*slot;
    str			domain;
    str			state_str;
    int			slot_idx;
    int			len = -1;
    int			usedlen = -1;

    slot_idx = sca_hash_table_index_for_key( scam->appearances,
					     &sub->target_aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app_list = sca_hash_table_slot_kv_find_unsafe( slot, &sub->target_aor );
    if ( app_list == NULL ) {
	len = 0;
	goto done;
    }

    usedlen = 0;
    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	len = sca_call_info_header_length_for_appearance( app,
							  &sub->target_aor );
	if ( len < 0 ) {
	    goto done;
	}

	if (( maxlen - len ) < 0 ) {
	    LM_ERR( "Call-Info header for AoR %.*s is too long",
		    STR_FMT( &sub->target_aor ));
	    len = -1;
	    goto done;
	}

	memcpy( hdrbuf, "<sip:", strlen( "<sip:" ));
	len = strlen( "<sip:" );

	if ( sca_call_info_domain_from_uri( &sub->target_aor, &domain ) < 0 ) {
	    return( -1 );
	}

	memcpy( hdrbuf + len, domain.s, domain.len );
	len += domain.len;

	sca_appearance_state_to_str( app->state, &state_str );

	/* state_str.s is a nul-terminated string literal */
	len += snprintf( hdrbuf + len, maxlen - len,
		  ">;appearance-index=%d;appearance-state=%s",
		  app->index, state_str.s );

	if ( app->next ) {
	    memcpy( hdrbuf + len, ",", 1 );
	    len++;
	}

	maxlen -= len;
	hdrbuf += len;

	usedlen += len;
    }

done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( usedlen );
}

    static int
sca_call_info_header_length_for_idle_appearance( sca_mod *scam )
{
    int		given_length = 0;

    /* appearance-index=* */
    given_length += strlen( "*" );

    /* appearance-state=idle */
    given_length += SCA_APPEARANCE_STATE_STR_IDLE.len;

    given_length += scam->cfg->domain->len;

    return( sca_call_info_header_length( given_length ));
}

    static int
sca_call_info_build_idle_value( sca_mod *scam, char *hdrbuf, int maxlen )
{
    int			len;

    if ( sca_call_info_header_length_for_idle_appearance( scam ) >= maxlen ) {
	LM_ERR( "Failed to add idle appearances: Call-Info header too long" );
	return( -1 );
    }

#ifdef notdef
    strcpy( hdrbuf, "<sip:" );
    len = strlen( "<sip:" );

    memcpy( hdrbuf + len, scam->cfg->domain->s, scam->cfg->domain->len );
    len += scam->cfg->domain->len;
#endif /* notdef */

    /* the SCA_APPEARANCE_ strs' s member are literal C strings */
    len = snprintf( hdrbuf, maxlen,
		"<sip:%s>;%s=*;%s=%s%s",
		scam->cfg->domain->s,
		SCA_APPEARANCE_INDEX_STR.s,
		SCA_APPEARANCE_STATE_STR.s,
		SCA_APPEARANCE_STATE_STR_IDLE.s, CRLF );

    return( len );
}

    int
sca_call_info_build_header( sca_mod *scam, sca_subscription *sub,
	char *hdrbuf, int maxlen )
{
    /* we send one Call-Info header, appearances separated by commas */
    int			len;
    int			usedlen = SCA_CALL_INFO_HEADER_STR.len;

    /* begin with "Call-Info: " */
    memcpy( hdrbuf, SCA_CALL_INFO_HEADER_STR.s, SCA_CALL_INFO_HEADER_STR.len );

    len = sca_call_info_header_append_appearances( scam, sub,
			hdrbuf + usedlen, maxlen - usedlen );
    usedlen += len;
    if ( usedlen > SCA_CALL_INFO_HEADER_STR.len ) {
	/* we added an indexed appearance, append a comma */
	memcpy( hdrbuf + usedlen, ",", 1 );
	usedlen++;
    }

    /* line-seize NOTIFYs will contain only the seized appearance index */
    if ( sub->event != SCA_EVENT_TYPE_LINE_SEIZE ) {
	/* if not all appearances in use, add *-index idle */
	len = sca_call_info_build_idle_value( scam,
		    hdrbuf + usedlen, maxlen - usedlen );
	if ( len < 0 || len + usedlen >= maxlen ) {
	    LM_ERR( "Cannot build idle Call-Info value: buffer too small" );
	    return( -1 );
	}
	usedlen += len;
    }

    return( usedlen );
}

    int
sca_call_info_append_header_for_appearance_index( sca_subscription *sub,
	int appearance_index, char *hdrbuf, int maxlen )
{
    str		domain = STR_NULL;
    char	*app_index_p = NULL;
    int		len = 0, idx_len;

    memcpy( hdrbuf, SCA_CALL_INFO_HEADER_STR.s, SCA_CALL_INFO_HEADER_STR.len );
    len += SCA_CALL_INFO_HEADER_STR.len;
    if ( len >= maxlen ) {
	goto error;
    }

    memcpy( hdrbuf + len, "<sip:", strlen( "<sip:" ));
    len += strlen( "<sip:" );
    if ( len >= maxlen ) {
	goto error;
    }

    sca_call_info_domain_from_uri( &sub->target_aor, &domain );
    memcpy( hdrbuf + len, domain.s, domain.len );
    len += domain.len;
    if ( len >= maxlen ) {
	goto error;
    }

    memcpy( hdrbuf + len, ">;appearance-index=",
		strlen( ">;appearance-index=" ));
    len += strlen( ">;appearance-index=" );
    if ( len >= maxlen ) {
	goto error;
    }

    app_index_p = int2str( appearance_index, &idx_len );	
    memcpy( hdrbuf + len, app_index_p, idx_len );
    len += idx_len;
    if ( len >= maxlen ) {
	goto error;
    }

    memcpy( hdrbuf + len, CRLF, CRLF_LEN );
    len += CRLF_LEN;
    if ( len >= maxlen ) {
	goto error;
    }

    return( len );

error:
    LM_ERR( "Failed to append Call-Info header for %.*s appearance index %d",
		STR_FMT( &sub->subscriber ), appearance_index );
    return( -1 );
}

    hdr_field_t *
sca_call_info_header_find( hdr_field_t *msg_hdrs )
{
    hdr_field_t		*hdr = NULL;

    for ( hdr = msg_hdrs; hdr != NULL; hdr = hdr->next ) {
	if ( hdr->type == HDR_OTHER_T &&
		hdr->name.len == SCA_CALL_INFO_HEADER_NAME.len ) {
	    if ( memcmp( hdr->name.s, SCA_CALL_INFO_HEADER_NAME.s,
			 SCA_CALL_INFO_HEADER_NAME.len ) == 0) {
		break;
	    }
	}
    }

    return( hdr );
}

    int
sca_call_info_body_parse( str *hdr_body, sca_call_info *call_info )
{
    str		s = STR_NULL;
    char	*p;
    char	*semi;
    int		len;

    assert( call_info != NULL );

    if ( SCA_STR_EMPTY( hdr_body )) {
	LM_ERR( "Call-Info header body is empty" );
	return( -1 );
    }

    call_info->sca_uri.s = NULL;
    call_info->sca_uri.len = 0;
    call_info->index = -1;
    call_info->state = SCA_APPEARANCE_STATE_UNKNOWN;
    call_info->uri.s = NULL;
    call_info->uri.len = 0;

    p = hdr_body->s;
    if ( memcmp( p, "<sip:", strlen( "<sip:" )) != 0 ) {
	LM_ERR( "Bad Call-Info header body: must begin with \"<sip:\"" );
	return( -1 );
    }
    /* +5 == strlen( "<sip:" ) */
    semi = memchr( p + 5, ';', hdr_body->len );
    if ( semi == NULL ) {
	LM_ERR( "Bad Call-Info header body: missing ';' between uri and "
		"%.*s", STR_FMT( &SCA_APPEARANCE_INDEX_STR ));
	return( -1 );
    }
    if ( *(semi - 1) != '>' ) {
	LM_ERR( "Bad Call-Info header body: SCA URI missing '>' terminator" );
	return( -1 );
    }

    call_info->sca_uri.s = p;
    call_info->sca_uri.len = semi - p;

LM_INFO( "ADMORTEN: Call-Info SCA URI: %.*s", STR_FMT( &call_info->sca_uri ));
    
    p = semi;
    p++;
    if ( memcmp( p, SCA_APPEARANCE_INDEX_STR.s,
			    SCA_APPEARANCE_INDEX_STR.len ) != 0 ) {
	LM_ERR( "Bad Call-Info header body: does not begin with %.*s",
		STR_FMT( &SCA_APPEARANCE_INDEX_STR ));
	return( -1 );
    }

    p += SCA_APPEARANCE_INDEX_STR.len;
    if ( *p != '=' ) {
	LM_ERR( "Bad Call-Info header body: missing '=' after %.*s",
		STR_FMT( &SCA_APPEARANCE_INDEX_STR ));
	return( -1 );
    }

    p++;
    len = ( hdr_body->s + hdr_body->len ) - p;
    semi = memchr( p, ';', len );
    if ( semi != NULL ) {
	len = semi - p;
    }
    s.s = p;
    s.len = len;

    if ( str2int( &s, (unsigned int *)&call_info->index ) != 0 ) {
	LM_ERR( "Bad Call-Info header: failed to convert %.*s %.*s to an "
		"integer", STR_FMT( &SCA_APPEARANCE_INDEX_STR ), STR_FMT( &s ));
	return( -1 );
    }

    if ( semi == NULL ) {
	/* Call-Info header only contained an appearance-index */
	goto done;
    }

    /* advance appearance-index value + semi-colon */
    p += ( len + 1 );
    if ( memcmp( p, SCA_APPEARANCE_STATE_STR.s,
		    SCA_APPEARANCE_STATE_STR.len ) != 0 ) {
	LM_ERR( "Bad Call-Info header: missing %.*s",
		STR_FMT( &SCA_APPEARANCE_STATE_STR ));
	return( -1 );
    }

    p += SCA_APPEARANCE_STATE_STR.len;
    if ( *p != '=' ) {
	LM_ERR( "Bad Call-Info header body: missing '=' after %.*s",
		STR_FMT( &SCA_APPEARANCE_STATE_STR ));
	return( -1 );
	
    }

    p++;
    len = ( hdr_body->s + hdr_body->len ) - p;
    semi = memchr( p, ';', len );
    if ( semi != NULL ) {
	len = semi - p;
    }
    s.s = p;
    s.len = len;

    call_info->state = sca_appearance_state_from_str( &s );
    if ( call_info->state == SCA_APPEARANCE_STATE_UNKNOWN ) {
	LM_ERR( "Bad Call-Info header: unrecognized state \"%.*s\"",
		STR_FMT( &s ));
	return( -1 );
    }

    if ( semi == NULL ) {
	/* Call-Info header only had appearance-index & appearance-state */
	goto done;
    }

    /* advance length of state + semi-colon */
    p += ( len + 1 );
    if ( memcmp( p, SCA_APPEARANCE_URI_STR.s,
		 SCA_APPEARANCE_URI_STR.len ) != 0 ) {
	LM_ERR( "Bad Call-Info header: missing %.*s",
		STR_FMT( &SCA_APPEARANCE_URI_STR ));
	return( -1 );
    }

    p += SCA_APPEARANCE_URI_STR.len;
    if ( *p != '=' ) {
	LM_ERR( "Bad Call-Info header: missing '=' after %.*s",
		STR_FMT( &SCA_APPEARANCE_URI_STR ));
	return( -1 );
    }

    p++;
    call_info->uri.s = p;
    call_info->uri.len = ( hdr_body->s + hdr_body->len ) - p;

    if ( SCA_STR_EMPTY( &call_info->uri )) {
	LM_ERR( "Bad Call-Info header: empty %.*s",
		STR_FMT( &SCA_APPEARANCE_URI_STR ));
	return( -1 );
    }

done:
    return( 0 );
}
