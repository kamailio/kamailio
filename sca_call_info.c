#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_appearance.h"
#include "sca_call_info.h"
#include "sca_dialog.h"
#include "sca_event.h"
#include "sca_notify.h"
#include "sca_subscribe.h"
#include "sca_util.h"

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
	/* +1 for ';', +1 for the '=' between param name and value */
	len += SCA_APPEARANCE_URI_STR.len + 1 + 1;
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

	if ( !SCA_STR_EMPTY( &app->uri )) {
	    hdrbuf[ len ] = ';';
	    len += 1;

	    memcpy( hdrbuf + len, SCA_APPEARANCE_URI_STR.s,
				  SCA_APPEARANCE_URI_STR.len );
	    len += SCA_APPEARANCE_URI_STR.len;

	    hdrbuf[ len ] = '=';
	    len += 1;

	    memcpy( hdrbuf + len, app->uri.s, app->uri.len );
	    len += app->uri.len;
	}

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
	    if ( strncasecmp( hdr->name.s, SCA_CALL_INFO_HEADER_NAME.s,
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

/*
 * return codes:
 *	-1: error, failed to remove call-info header.
 *	 0: no call-info headers found.
 *     >=1: removed 1 or more call-info headers.
 */
    static int
sca_call_info_header_remove( sip_msg_t *msg )
{
    hdr_field_t		*hdr;
    struct lump		*ci_hdr_lump;
    int			rc = 0;

    /* all headers must be parsed before using del_lump */
    if ( parse_headers( msg, HDR_EOH_F, 0 ) < 0 ) {
	LM_ERR( "Failed to parse_headers" );
	return( -1 );
    }

#ifdef notdef
    for ( hdr = sca_call_info_header_find( msg->headers ); hdr != NULL;
		hdr = sca_call_info_header_find( hdr->next )) {
#endif /* notdef */
    for ( hdr = msg->headers; hdr; hdr = hdr->next ) {
	if ( hdr->name.len != SCA_CALL_INFO_HEADER_NAME.len ) {
	    continue;
	}
	if ( memcmp( hdr->name.s, SCA_CALL_INFO_HEADER_NAME.s,
				hdr->name.len ) != 0 ) {
	    continue;
	}

	/* del_lump takes packet, offset, lump length, & hdr type */
	ci_hdr_lump = del_lump( msg, hdr->name.s - msg->buf,
				hdr->len, HDR_OTHER_T );
	if ( ci_hdr_lump == NULL ) {
	    LM_ERR( "Failed to del_lump Call-Info header" );
	    rc = -1;
	    break;
	}

	rc++;
    }

    return( rc );
}

    //static int
    int
sca_call_info_seize_held_call( sip_msg_t *msg, sca_call_info *call_info,
	str *contact_uri, struct to_body *from, struct to_body *to )
{
    sca_appearance	*app;
    struct lump		*anchor;
    struct sip_uri	tmp;
    str			aor = STR_NULL;
    str			replaces_hdr = STR_NULL;
    int			slot_idx = -1;
    int			rc = -1;

    if ( sca_uri_extract_aor( &from->uri, &aor ) < 0 ) {
	LM_ERR( "sca_call_info_seize_held_call: failed to extract AoR from "
		"From-URI %.*s", STR_FMT( &from->uri ));
	return( -1 );
    }

    slot_idx = sca_hash_table_index_for_key( sca->appearances, &aor );
    sca_hash_table_lock_index( sca->appearances, slot_idx );

    app = sca_appearance_for_index_unsafe( sca, &aor, call_info->index,
					   slot_idx );
    if ( app == NULL ) {
	LM_ERR( "sca_call_info_seize_held_call: no active appearances for "
		"%.*s", STR_FMT( &aor ));
	goto done;
    }

LM_INFO( "ADMORTEN DEBUG: seizing %.*s appearance-index %d, callee %.*s",
	STR_FMT( &aor ), app->index, STR_FMT( &app->callee ));

#ifdef notdef
    /* rewrite the RURI to use the callee in this SCA dialog */
    if ( msg->new_uri.s ) {
	/*
	 * someone already rewrote the URI. shouldn't happen, but we have
	 * to watch for it. log our overwriting of it.
	 */

	LM_INFO( "SCA caller retrieving held call, but RURI was already "
		 "rewritten as %.*s. Overwriting with %.*s.",
		 STR_FMT( &msg->new_uri ), STR_FMT( &app->callee ));

	pkg_free( msg->new_uri.s );
	msg->new_uri.s = NULL;
	msg->new_uri.len = 0;
    }
#endif /* notdef */

    /* msg->new_uri.s is free'd when transaction is torn down */
    msg->new_uri.s = (char *)pkg_malloc( app->callee.len );
    if ( msg->new_uri.s == NULL ) {
	LM_ERR( "sca_call_info_seize_held_call: pkg_malloc new RURI %.*s "
		"failed", STR_FMT( &app->callee ));
	goto done;
    }
    SCA_STR_COPY( &msg->new_uri, &app->callee );

    {
	int		idx; 

	for ( idx = 0; get_sip_branch( idx ) != NULL; idx++ )
	    ;

LM_INFO( "ADMORTEN DEBUG: %d sip branches", idx + 1 );

	for ( ; idx >= 0; idx-- ) {
LM_INFO( "ADMORTEN DEBUG: dropping branch %d", idx );
	    drop_sip_branch( idx );
	}
    }

    /* must reset to avoid using cached parsed RURI */
    msg->parsed_uri_ok = 0;
    ruri_mark_new();

    /* pkg_malloc's replaces_hdr.s, which is free'd if added as lump */
    if ( sca_dialog_create_replaces_header( &app->dialog,
					    &replaces_hdr ) < 0 ) {
	LM_ERR( "sca_call_info_seize_held_call: failed to create Replaces "
		"header for %.*s from dialog %.*s",
		STR_FMT( &aor ), STR_FMT( &app->dialog.id ));
	goto done;
    }

LM_INFO( "ADMORTEN DEBUG: new RURI: %.*s, Replaces header: %.*s",
	STR_FMT( &msg->new_uri ), STR_FMT( &replaces_hdr ));

    if ( parse_uri( msg->new_uri.s, msg->new_uri.len, &tmp ) < 0 ) {
	LM_ERR( "Bad URI \"%.*s\"", STR_FMT( &msg->new_uri ));
	goto done;
    }
LM_INFO( "ADMORTEN DEBUG: parsed new RURI: %.*s", STR_FMT( &msg->new_uri ));

    /* all headers must be parsed before using lump functions */
    if ( parse_headers( msg, HDR_EOH_F, 0 ) < 0 ) {
	LM_ERR( "Failed to parse_headers" );
	goto done;
    }

    anchor = anchor_lump( msg, msg->eoh - msg->buf, 0, HDR_OTHER_T );
    if ( anchor == NULL ) {
	LM_ERR( "Failed to anchor lump" );
	goto done;
    }

    /* append the Replaces header before the sdp body */
    if ( insert_new_lump_before( anchor, replaces_hdr.s, 
	    replaces_hdr.len, HDR_OTHER_T ) == NULL ) {
	LM_ERR( "Failed to add Replaces header %.*s", STR_FMT( &replaces_hdr ));
	pkg_free( replaces_hdr.s );
	goto done;
    }

    if ( parse_uri( msg->new_uri.s, msg->new_uri.len, &tmp ) < 0 ) {
	LM_ERR( "Bad URI \"%.*s\"", STR_FMT( &msg->new_uri ));
	goto done;
    }
LM_INFO( "ADMORTEN DEBUG: REDUX: parsed new RURI: %.*s", STR_FMT( &msg->new_uri ));

    rc = 1;

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }

    return( rc );
}

    static int
sca_call_info_uri_update( str *uri, sca_call_info *call_info,
	struct to_body *from, struct to_body *to, str *contact_uri,
	str *call_id )
{
    sca_appearance	*app;
    sca_dialog		dialog;
    str			aor = STR_NULL;
    str			state_str;
    str			*from_tag = &from->tag_value;
    str			*to_tag = &to->tag_value;
    char		dlg_buf[ 1024 ];
    int			slot_idx = -1;
    int			rc = -1;
    int			notify = 0;

    assert( uri != NULL );
    assert( call_info != NULL );

    if ( sca_uri_extract_aor( uri, &aor ) < 0 ) {
	LM_ERR( "sca_call_info_uri_update: sca_uri_extract_aor from "
		"%.*s failed", STR_FMT( uri ));
	return( -1 );
    }

    if ( !sca_uri_is_shared_appearance( sca, &aor )) {
	return( 0 );
    }

    dialog.id.s = dlg_buf;
    if ( sca_dialog_build_from_tags( &dialog, sizeof( dlg_buf ), call_id,
				to_tag, from_tag ) < 0 ) {
	LM_ERR( "sca_call_info_uri_update: Failed to build dialog from tags" );
	return( -1 );
    }

    slot_idx = sca_hash_table_index_for_key( sca->appearances, &aor );
    sca_hash_table_lock_index( sca->appearances, slot_idx );

    app = sca_appearance_for_index_unsafe( sca, &aor, call_info->index,
						slot_idx );
    if ( app != NULL ) {
LM_INFO( "ADMORTEN DEBUG: found appearance for %.*s", STR_FMT( &aor ));
	/* XXX must escape to->body before adding it to the appearance */
	/* XXX to->uri here should be the escaped to->body */
	if ( sca_appearance_update_unsafe( app, call_info->state,
		&to->uri, &dialog, contact_uri, NULL ) < 0 ) {
	    sca_appearance_state_to_str( call_info->state, &state_str );
	    LM_ERR( "sca_call_info_uri_update: failed to update appearance "
		    "%.*s appearance-index %d with dialog id %.*s to "
		    "state %.*s", STR_FMT( &app->owner ), app->index,
		    STR_FMT( &app->dialog.id ), STR_FMT( &state_str ));
	    goto done;
	}

	rc = 1;
    } else {
LM_INFO( "ADMORTEN DEBUG: no appearance for %.*s, seizing next", STR_FMT( &aor ));
	app = sca_appearance_seize_next_available_unsafe( sca, &aor,
				contact_uri, slot_idx );
	if ( app == NULL ) {
	    LM_ERR( "sca_call_info_uri_update: failed to seize index %d "
		    "for %.*s", call_info->index, STR_FMT( contact_uri ));
	    goto done;
	}

	if ( sca_appearance_update_unsafe( app, call_info->state, NULL,
		&dialog, NULL, NULL ) < 0 ) {
	    sca_appearance_state_to_str( call_info->state, &state_str );
	    LM_ERR( "sca_call_info_uri_update: failed to update appearance "
		    "%.*s appearance-index %d with dialog id %.*s to "
		    "state %.*s", STR_FMT( &app->owner ), app->index,
		    STR_FMT( &app->dialog.id ), STR_FMT( &state_str ));
	    goto done;
	}

	rc = 1;
    }

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }

    if ( notify ) {
	if ( sca_notify_call_info_subscribers( sca, &aor ) < 0 ) {
	    LM_ERR( "Failed to call-info NOTIFY %.*s subscribers",
		    STR_FMT( &aor ));
	    goto done;
	}
    }

    return( rc );
}

    static int
sca_call_info_is_line_seize_reinvite( sip_msg_t *msg, sca_call_info *call_info,
	struct to_body *from, struct to_body *to )
{
    str			*ruri;
    str			ruri_aor;
    str			from_aor;
    str			to_aor;
    int			state;

    /*
     * a handset in an SCA group is attempting to seize a held line if:
     *		the RURI, From URI and To URI are identical;
     *		the above are SCA AoRs;
     *		there is no to-tag;
     *		a Call-Info header is present
     */

    if ( !SCA_STR_EMPTY( &to->tag_value )) {
LM_INFO( "ADMORTEN DEBUG: non-empty to-tag %.*s", STR_FMT( &to->tag_value ));
	return( 0 );
    }

    ruri = GET_RURI( msg );
    if ( sca_uri_extract_aor( ruri, &ruri_aor ) < 0 ) {
	LM_ERR( "sca_call_info_is_line_seize_reinvite: failed to extract "
		"AoR from RURI %.*s", STR_FMT( ruri ));
	return( 0 );
    }
    if ( sca_uri_extract_aor( &from->uri, &from_aor ) < 0 ) {
	LM_ERR( "sca_call_info_is_line_seize_reinvite: failed to extract "
		"AoR from From-URI %.*s", STR_FMT( &from->uri ));
	return( 0 );
    }
    if ( sca_uri_extract_aor( &to->uri, &to_aor ) < 0 ) {
	LM_ERR( "sca_call_info_is_line_seize_reinvite: failed to extract "
		"AoR from To-URI %.*s", STR_FMT( &to->uri ));
	return( 0 );
    }

#ifdef notdef
    if ( !SCA_STR_EQ( &ruri_aor, &from_aor ) ||
		!SCA_STR_EQ( &ruri_aor, &to_aor )) {
#endif /* notdef */
    if ( !SCA_STR_EQ( &from_aor, &to_aor )) {
LM_INFO( "ADMORTEN DEBUG: %.*s != %.*s != %.*s",
	    STR_FMT( &ruri_aor ), STR_FMT( &from_aor ), STR_FMT( &to_aor ));
	return( 0 );
    }

    state = sca_appearance_state_for_index( sca, &from_aor, call_info->index );
    if ( state != SCA_APPEARANCE_STATE_HELD ) {
	LM_DBG( "sca_call_info_is_line_seize_reinvite: new INVITE to "
		"%.*s from %.*s appearance-index %d (not seizing held line)",
		STR_FMT( &to_aor ), STR_FMT( &ruri_aor ), call_info->index );
	return( 0 );
    }

    return( 1 );
}

    //static int
    int
sca_call_info_invite_request_handler( sip_msg_t *msg, sca_call_info *call_info,
	str *contact_uri, struct to_body *from, struct to_body *to )
{
    sca_dialog		dialog;
    char		dlg_buf[ 1024 ];
    str			state_str = STR_NULL;
    str			from_aor = STR_NULL;
    int			state = SCA_APPEARANCE_STATE_UNKNOWN;
    int			rc = -1;

    /* XXX check for to-tag, check SDP for hold/pickup, etc. */
    /* this is likely to be the most complicated one */
    /* if picking up held, this is where we need to inject Replaces hdr */

    if ( sca->tm_api->register_tmcb( msg, NULL, TMCB_E2EACK_IN,
				    sca_call_info_ack_cb, NULL, NULL ) < 0 ) {
	LM_ERR( "sca_call_info_invite_request_handler: failed to register "
		"callback for INVITE %.*s ACK", STR_FMT( &from_aor ));
	goto done;
    }

    if ( call_info == NULL ) {
	/* XXX just for now. will also need to check for reINVITE, etc. */
	LM_INFO( "ADMORTEN: no Call-Info header in INVITE %.*s",
		     STR_FMT( &to->uri ));
	rc = 1;
	goto done;
    }

    if ( sca_call_is_held( msg )) {
LM_INFO( "ADMORTEN DEBUG: call is held" );
	state = SCA_APPEARANCE_STATE_HELD;
    } else if ( !SCA_STR_EMPTY( &to->tag_value )) {
LM_INFO( "ADMORTEN DEBUG: to-tag is not empty" );
	/* this is a reINVITE from an SCA line that put the call on hold */
	state = SCA_APPEARANCE_STATE_ACTIVE;
    } else if ( sca_call_info_is_line_seize_reinvite( msg, call_info,
							from, to )) {
LM_INFO( "ADMORTEN DEBUG: is line seize reinvite from %.*s",
		STR_FMT( contact_uri ));
	rc = sca_call_info_seize_held_call( msg, call_info, contact_uri,
						from, to );
    } else {
LM_INFO( "ADMORTEN DEBUG: WTF" );
    }
    /* otherwise, this is an initial INVITE */

    if ( sca_uri_extract_aor( &from->uri, &from_aor ) < 0 ) {
	LM_ERR( "sca_call_info_invite_request_handler: failed to extract "
		"From AoR from %.*s", STR_FMT( &from->uri ));
	return( -1 );
    }

    sca_appearance_state_to_str( state, &state_str );
    LM_INFO( "ADMORTEN: updating %.*s appearance-index %d to %.*s, "
	     "dialog: callid: %.*s, from-tag: %.*s",
		STR_FMT( &from->uri ), call_info->index,
		STR_FMT( &state_str ),
		STR_FMT( &msg->callid->body ),
		STR_FMT( &from->tag_value ));

    dialog.id.s = dlg_buf;
    if ( sca_dialog_build_from_tags( &dialog, sizeof( dlg_buf ),
	    &msg->callid->body, &from->tag_value, &to->tag_value ) < 0 ) {
	LM_ERR( "Failed to build dialog from tags" );
	return( -1 );
    }

    if ( sca_appearance_update_index( sca, &from_aor, call_info->index,
		state, NULL, &dialog ) != SCA_APPEARANCE_OK ) {
	LM_ERR( "Failed to update %.*s appearance-index %d to %.*s",
		STR_FMT( &from_aor ), call_info->index,
		STR_FMT( &state_str ));
    }

    if ( sca_notify_call_info_subscribers( sca, &from_aor ) < 0 ) {
	LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on INVITE",
		STR_FMT( &from_aor ));
	goto done;
    }

    rc = 1;

done:
    return( rc );
}

    static int
sca_call_info_invite_reply_18x_handler( sip_msg_t *msg,
	sca_call_info *call_info, str *contact_uri,
	struct to_body *from, struct to_body *to )
{
    sca_appearance	*app = NULL;
    str			aor = STR_NULL;
    int			state = SCA_APPEARANCE_STATE_UNKNOWN;
    int			slot_idx = -1;
    int			rc = -1;

    switch ( msg->REPLY_STATUS ) {
    case 180:
	state = SCA_APPEARANCE_STATE_ALERTING;
	break;

    case 183:
	state = SCA_APPEARANCE_STATE_PROGRESSING;
	break;

    default:
	goto done;
    }

    if ( sca_uri_extract_aor( &from->uri, &aor ) < 0 ) {
	LM_ERR( "sca_call_info_invite_reply_18x_handler: "
		"sca_uri_extract_aor from %.*s", STR_FMT( &from->uri ));
	goto done;
    }

    slot_idx = sca_hash_table_index_for_key( sca->appearances, &aor );
    sca_hash_table_lock_index( sca->appearances, slot_idx );

    app = sca_appearance_for_tags_unsafe( sca, &aor, &msg->callid->body,
			  &from->tag_value, &to->tag_value, slot_idx );
    if ( app == NULL ) {
	goto done;
    }

    app->state = state;
    rc = 1;

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }

    if ( rc > 0 && app != NULL ) {
	if ( sca_subscription_terminate( sca, &aor,
		SCA_EVENT_TYPE_LINE_SEIZE, &app->owner,
		SCA_SUBSCRIPTION_STATE_TERMINATED_NORESOURCE,
		SCA_SUBSCRIPTION_TERMINATE_OPT_UNSUBSCRIBE ) < 0 ) {
	    LM_ERR( "sca_call_info_invite_reply_18x_handler: "
		    "failed to terminate line-seize subscription for %.*s",
		    STR_FMT( &app->owner ));
	    rc = -1;
	}
    }

    return( rc );
}

    static int
sca_call_info_invite_reply_200_handler( sip_msg_t *msg,
	sca_call_info *call_info, str *contact_uri,
	struct to_body *from, struct to_body *to )
{
    sca_appearance	*app;
    sca_dialog		dialog;
    char		dlg_buf[ 1024 ];
    str			state_str = STR_NULL;
    str			from_aor = STR_NULL;
    str			to_aor = STR_NULL;
    int			state = SCA_APPEARANCE_STATE_UNKNOWN;
    int			slot_idx = -1;
    int			rc = -1;

    if ( sca_uri_extract_aor( &from->uri, &from_aor ) < 0 ) {
	LM_ERR( "sca_call_info_invite_reply_200_handler: failed to "
		"extract From AoR from %.*s", STR_FMT( &from->uri ));
	return( -1 );
    }
    if ( sca_uri_extract_aor( &to->uri, &to_aor ) < 0 ) {
	LM_ERR( "sca_call_info_invite_reply_200_handler: failed to "
		"extract To AoR from %.*s", STR_FMT( &to->uri ));
	return( -1 );
    }

    if ( call_info != NULL ) {
	call_info->state = SCA_APPEARANCE_STATE_ACTIVE;
	rc = sca_call_info_uri_update( &to_aor, call_info, from, to,
			&msg->callid->body, contact_uri );
    }

    if ( !sca_uri_is_shared_appearance( sca, &from_aor )) {
	goto done;
    }

    /*
     * XXX in a reply with no Call-Info header, we look for a matching
     * dialog for the From URI. if we don't find one, this isn't an SCA
     * call, and we're done processing.
     *
     * if there *is* a Call-Info header, we update the appearance state of
     * the index in the header for the To URI (appearance owned by the URI
     * in the Contact header). we still then need to check to see if the
     * From URI is an SCA line, and update state if it is.
     */

    slot_idx = sca_hash_table_index_for_key( sca->appearances, &from_aor );
    sca_hash_table_lock_index( sca->appearances, slot_idx );

    app = sca_appearance_for_tags_unsafe( sca, &from_aor,
		&msg->callid->body, &from->tag_value, NULL, slot_idx );
    if ( app == NULL ) {
	/* no SCA line is involved with this call */
	LM_INFO( "ADMORTEN DEBUG: %.*s is not an SCA line",
		    STR_FMT( &from->uri ));
	rc = 1;
	goto done;
    }

    if ( app->state != SCA_APPEARANCE_STATE_HELD ) {
	state = SCA_APPEARANCE_STATE_ACTIVE;
    }

    /* if a Call-Info header is present, app-index goes to Contact */

    dialog.id.s = dlg_buf;
    if ( sca_dialog_build_from_tags( &dialog, sizeof( dlg_buf ),
	    &msg->callid->body, &from->tag_value, &to->tag_value ) < 0 ) {
	LM_ERR( "sca_call_info_invite_handler: failed to build sca_dialog "
		"from tags" );
	rc = -1;
	goto done;
    }

    /* XXX to->uri here should be an escaped to->body */
    if ( sca_appearance_update_unsafe( app, state, &to->uri,
	    &dialog, NULL, contact_uri ) < 0 ) {
	sca_appearance_state_to_str( state, &state_str );
	LM_ERR( "sca_call_info_invite_handler: failed to update appearance "
		"%.*s appearance-index %d with dialog id %.*s to "
		"state %.*s", STR_FMT( &app->owner ), app->index,
		STR_FMT( &app->dialog.id ), STR_FMT( &state_str ));
	rc = -1;
	goto done;
    }

    rc = 1;

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }

    return( rc );
}

    static int
sca_call_info_invite_reply_3xx_handler( sip_msg_t *msg,
	sca_call_info *call_info, str *contact_uri,
	struct to_body *from, struct to_body *to )
{
    return( 1 );
}

    static int
sca_call_info_invite_reply_error_handler( sip_msg_t *msg,
	sca_call_info *call_info, str *contact_uri,
	struct to_body *from, struct to_body *to )
{
    /*
     * XXX will need special handling here. must distinguish among the
     * following:
     * 		failed initial INVITE
     *		failed reINVITE from caller retrieving from hold
     *		failed INVITE retrieving remote UA from SCA hold
     *
     * for a start, we just deal with the first case.
     */

    sca_appearance	*app;
    str			aor;

    if ( msg->REPLY_STATUS == 487 ) {
	/* reply status for a CANCEL'd INVITE */
	return( 1 );
    }

    if ( sca_uri_extract_aor( &from->uri, &aor ) < 0 ) {
	LM_ERR( "sca_call_info_invite_reply_error_handler: failed to extract "
		"AoR from %.*s", STR_FMT( &from->uri ));
	return( -1 );
    }
    if ( sca_uri_is_shared_appearance( sca, &aor )) {
	app = sca_appearance_unlink_by_tags( sca, &aor,
		    &msg->callid->body, &from->tag_value, NULL );
	if ( app == NULL ) {
	    LM_ERR( "sca_call_info_invite_reply_error_handler: failed to "
		    "look up dialog for failed INVITE %.*s from %.*s",
		    STR_FMT( &to->uri ), STR_FMT( &aor ));
	    return( -1 );
	}
	sca_appearance_free( app );

	if ( sca_notify_call_info_subscribers( sca, &aor ) < 0 ) {
	    LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on "
		    "failed INVITE", STR_FMT( &aor ));
	    return( -1 );
	}
    }

    return( 1 );
}

    void
sca_call_info_ack_cb( struct cell *t, int type, struct tmcb_params *params )
{
    struct to_body	*to;
    str			to_aor = STR_NULL;

LM_INFO( "ADMORTEN DEBUG: entered sca_call_info_ack_cb" );

    if ( !(type & TMCB_E2EACK_IN)) {
	return;
    }

    if ( sca_get_msg_to_header( params->req, &to ) < 0 ) {
	LM_ERR( "sca_call_info_ack_cb: failed to get To-header" );
	return;
    }
    if ( sca_uri_extract_aor( &to->uri, &to_aor ) < 0 ) {
	LM_ERR( "sca_call_info_ack_cb: failed to extract To AoR from %.*s",
		STR_FMT( &to->uri ));
	return;
    }

    if ( !sca_uri_is_shared_appearance( sca, &to_aor )) {
	LM_DBG( "sca_call_info_ack_cb: %.*s is not a shared appearance",
		STR_FMT( &to_aor ));
	return;
    }

    if ( sca_notify_call_info_subscribers( sca, &to_aor ) < 0 ) {
	LM_ERR( "sca_call_info_ack_cb: failed to call-info "
		"NOTIFY %.*s subscribers", STR_FMT( &to_aor ));
	return;
    }

    LM_INFO( "ADMORTEN DEBUG: call-info NOTIFY to %.*s", STR_FMT( &to_aor ));
}

/* XXX may want to further split between requests & responses */
    static int
sca_call_info_invite_handler( sip_msg_t *msg, sca_call_info *call_info,
	str *contact_uri, struct to_body *from, struct to_body *to )
{
    int			rc = -1;

    if ( SCA_STR_EMPTY( contact_uri )) {
	LM_ERR( "sca_call_info_invite_handler: Contact header is empty. "
		"(From: %.*s To: %.*s)", STR_FMT( &from->uri ),
		 STR_FMT( &to->uri ));
	return( -1 );
    }

    if ( msg->first_line.type == SIP_REQUEST ) {
	rc = sca_call_info_invite_request_handler( msg, call_info, contact_uri,
						    from, to );
    } else {
	/* XXX replace with dispatch table. */
	switch ( msg->REPLY_STATUS ) {
	case 100:
	    rc = 1;
	    break;

	case 180:
	case 183:
	    rc = sca_call_info_invite_reply_18x_handler( msg, call_info,
						contact_uri, from, to );
	    break;

	case 200:
	    rc = sca_call_info_invite_reply_200_handler( msg, call_info,
						contact_uri, from, to );
	    break;

	default:
	    rc = sca_call_info_invite_reply_error_handler( msg, call_info,
						contact_uri, from, to );
	    break;
	}
    }

    return( rc );
}

    static int
sca_call_info_bye_handler( sip_msg_t *msg, sca_call_info *call_info,
	str *contact_uri, struct to_body *from, struct to_body *to )
{
    sca_appearance	*app;
    str			from_aor = STR_NULL;
    str			to_aor = STR_NULL;
    int			rc = -1;

    if ( sca_uri_extract_aor( &from->uri, &from_aor ) < 0 ) {
	LM_ERR( "sca_call_info_bye_handler: failed to extract From AoR "
		"from %.*s", STR_FMT( &from->uri ));
	goto done;
    }
    if ( sca_uri_extract_aor( &to->uri, &to_aor ) < 0 ) {
	LM_ERR( "sca_call_info_bye_handler: failed to extract To AoR "
		"from %.*s", STR_FMT( &to->uri ));
	goto done;
    }

    if ( msg->first_line.type == SIP_REQUEST ) {
	if ( call_info != NULL ) {
	    if ( sca_appearance_release_index( sca, &from_aor,
						call_info->index ) < 0 ) {
		LM_ERR( "Failed to release appearance-index %d "
			"for %.*s on BYE", call_info->index,
			STR_FMT( &from_aor ));
		goto done;
	    }

	    if ( sca_notify_call_info_subscribers( sca, &from_aor ) < 0 ) {
		LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on BYE",
			STR_FMT( &from_aor ));
		goto done;
	    }
	} else {
	    /* BYE from non-SCA line, see if the dialog is with an SCA line */
	    app = sca_appearance_unlink_by_tags( sca, &to_aor,
			&msg->callid->body, &to->tag_value, NULL );
	    if ( app == NULL ) {
		LM_ERR( "sca_call_info_bye_handler: failed to look up "
			"dialog for BYE %.*s from %.*s",
			STR_FMT( &to_aor ), STR_FMT( &from_aor ));
		goto done;
	    }
	    sca_appearance_free( app );

	    if ( sca_notify_call_info_subscribers( sca, &to_aor ) < 0 ) {
		LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on BYE",
			STR_FMT( &to->uri ));
		goto done;
	    }
	}
    } else {
	if ( call_info != NULL ) {
	    if ( sca_appearance_release_index( sca, &to_aor,
						call_info->index ) < 0 ) {
		LM_ERR( "Failed to release appearance-index %d "
			"for To-URI %.*s on BYE reply", call_info->index,
			STR_FMT( &to_aor ));
		goto done;
	    }

	    if ( sca_notify_call_info_subscribers( sca, &to_aor ) < 0 ) {
		LM_ERR( "Failed to call-info NOTIFY %.*s subscribers "
			"on BYE reply", STR_FMT( &to_aor ));
		goto done;
	    }
	}
    }

    rc = 1;

done:
    return( rc );
}

    static int
sca_call_info_cancel_handler( sip_msg_t *msg, sca_call_info *call_info,
	str *contact_uri, struct to_body *from, struct to_body *to )
{
    sca_appearance	*app;
    int			rc = 1;

    /*
     * Polycom SCA CANCELs as of sip.ld 3.3.4 don't include Call-Info headers;
     * find appearance by dialog if Call-Info not present.
     */
    if ( msg->first_line.type == SIP_REQUEST ) {
	/* XXX also handle CANCEL w/ Call-Info header? */

	app = sca_appearance_unlink_by_tags( sca, &from->uri,
			&msg->callid->body, &from->tag_value, NULL );
	if ( app ) {
	    sca_appearance_free( app );

	    if ( sca_notify_call_info_subscribers( sca, &from->uri ) < 0 ) {
		LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on CANCEL",
			STR_FMT( &from->uri ));
		rc = -1;
	    }
	}

	app = sca_appearance_unlink_by_tags( sca, &to->uri,
			&msg->callid->body, &to->tag_value, NULL );
	if ( app ) {
	    sca_appearance_free( app );

	    if ( sca_notify_call_info_subscribers( sca, &to->uri ) < 0 ) {
		LM_ERR( "Failed to call-info NOTIFY %.*s subscribers on CANCEL",
			STR_FMT( &to->uri ));
		rc = -1;
	    }
	}
    }

    return( rc );
}
struct sca_call_info_dispatch {
    int			method;
    int			(*handler)( sip_msg_t *, sca_call_info *, str *,
				    struct to_body *, struct to_body * );
};
struct sca_call_info_dispatch	call_info_dispatch[] = {
    { METHOD_INVITE,	sca_call_info_invite_handler },
    { METHOD_BYE,	sca_call_info_bye_handler },
    { METHOD_CANCEL,	sca_call_info_cancel_handler },
#ifdef notdef
    { METHOD_PRACK,	sca_call_info_prack_handler },
    { METHOD_REFER,	sca_call_info_refer_handler },
#endif /* notdef */
};

    void
sca_call_info_sl_reply_cb( void *cb_arg )
{
    sl_cbp_t		*slcbp = (sl_cbp_t *)cb_arg;
    sip_msg_t		*msg;
    struct to_body	*from;
    struct to_body	*to;
    str			aor = STR_NULL;
    str			contact_uri = STR_NULL;

    LM_INFO( "ADMORTEN DEBUG: entered sca_call_info_sl_reply_cb" );

    if ( slcbp == NULL ) {
	LM_INFO( "ADMORTEN DEBUG: sca_call_info_sl_reply_cb: slcbp is NULL" );
	return;
    }

    LM_INFO( "ADMORTEN DEBUG: sca_call_info_sl_reply_cb: ready reply "
	    "%d %.*s", slcbp->code, STR_FMT( slcbp->reason ));


    if ( slcbp->type != SLCB_REPLY_READY ) {
	return;
    }

    /* for now, it appears we only need this during INVITEs... */
    if ( slcbp->req->REQ_METHOD != METHOD_INVITE ) {
	return;
    }

    /* ...and even then only on error */
    if ( slcbp->code < 400 || slcbp->code == 401 || slcbp->code == 407 ) {
	/* LM_DBG( "sca_call_info_sl_reply_cb: ignoring stateless reply with "
		"status %d %.*s", slcbp->code, STR_FMT( slcbp->reason )); */
	return;
    }

    msg = slcbp->req;
    if ( sca_get_msg_from_header( msg, &from ) < 0 ) {
	LM_ERR( "sca_call_info_sl_reply_cb: failed to get From header from "
		"request before stateless reply with %d %.*s",
		slcbp->code, STR_FMT( slcbp->reason ));
	return;
    }
    if ( sca_uri_extract_aor( &from->uri, &aor ) < 0 ) {
	LM_ERR( "sca_call_info_sl_reply_cb: failed to extract AoR "
		"from URI %.*s", STR_FMT( &from->uri ));
	return;
    }

    if ( !sca_uri_is_shared_appearance( sca, &aor )) {
	/* LM_DBG( "sca_call_info_sl_reply_cb: ignoring non-shared appearance "
		"%.*s", STR_FMT( &aor )); */
	return;
    }

    if ( sca_get_msg_contact_uri( msg, &contact_uri ) < 0 ) {
	LM_ERR( "sca_call_info_sl_reply_cb: failed to get Contact from "
		"request before stateless reply with %d %.*s",
		slcbp->code, STR_FMT( slcbp->reason ));
	return;
    }

    if ( sca_get_msg_to_header( msg, &to ) < 0 ) {
	LM_ERR( "sca_call_info_sl_reply_cb: failed to get To header from "
		"request before stateless reply with %d %.*s",
		slcbp->code, STR_FMT( slcbp->reason ));
	return;
    }

    if ( sca_subscription_terminate( sca, &aor,
		SCA_EVENT_TYPE_LINE_SEIZE, &contact_uri,
		SCA_SUBSCRIPTION_STATE_TERMINATED_NORESOURCE,
		SCA_SUBSCRIPTION_TERMINATE_OPT_DEFAULT ) < 0 ) {
	LM_ERR( "sca_call_info_sl_reply_cb: failed to terminate "
		"line-seize subscription for %.*s", STR_FMT( &contact_uri ));
	return;
    }
}

    int
sca_call_info_update( sip_msg_t *msg, char *p1, char *p2 )
{
    sca_call_info	call_info, *call_info_p = NULL;
    hdr_field_t		*call_info_hdr;
    struct to_body	*from;
    struct to_body	*to;
    str			contact_uri = STR_NULL;
    int			n_dispatch;
    int			i;
    int			method;
    int			rc = -1;

    if ( parse_headers( msg, HDR_EOH_F, 0 ) < 0 ) {
	LM_ERR( "header parsing failed: bad request" );
	return( -1 );
    }

    memset( &call_info, 0, sizeof( sca_call_info ));
    call_info_hdr = sca_call_info_header_find( msg->headers );
    if ( !SCA_HEADER_EMPTY( call_info_hdr )) {
	/* this needs to accomodate comma-separated appearance info */
	if ( sca_call_info_body_parse( &call_info_hdr->body, &call_info ) < 0) {
	    LM_ERR( "Bad Call-Info header body: %.*s",
		    STR_FMT( &call_info_hdr->body ));
	    return( -1 );
	}

	call_info_p = &call_info;
    }

    if ( sca_get_msg_from_header( msg, &from ) < 0 ) {
	LM_ERR( "Bad From header" );
	return( -1 );
    }
    if ( sca_get_msg_to_header( msg, &to ) < 0 ) {
	LM_ERR( "Bad To header" );
	return( -1 );
    }
    rc = sca_get_msg_contact_uri( msg, &contact_uri );
    if ( rc < 0 ) {
	LM_ERR( "Bad Contact" );
	return( -1 );
    }

    method = sca_get_msg_method( msg );

    n_dispatch = sizeof( call_info_dispatch ) / sizeof( call_info_dispatch[0] );
    for ( i = 0; i < n_dispatch; i++ ) {
	if ( method == call_info_dispatch[ i ].method ) {
	    break;
	}
    }
    if ( i >= n_dispatch ) {
	LM_ERR( "BUG: Module does not support Call-Info headers "
		"in %.*s requests", STR_FMT( &REQ_LINE(msg).method ));
	return( -1 );
    }

    if ( sca_call_info_header_remove( msg ) < 0 ) {
	LM_ERR( "Failed to remove Call-Info header" );
	return( -1 );
    }

    rc = call_info_dispatch[ i ].handler( msg, call_info_p,
					  &contact_uri, from, to );
    if ( rc < 0 ) {
	LM_ERR( "Failed to update Call-Info state for %.*s",
		STR_FMT( &contact_uri ));
    }

    return( rc );
}
