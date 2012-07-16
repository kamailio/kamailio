#include "sca_common.h"

#include "sca_event.h"

str		SCA_EVENT_NAME_CALL_INFO = STR_STATIC_INIT( "call-info" );
str		SCA_EVENT_NAME_LINE_SEIZE = STR_STATIC_INIT( "line-seize" );

    int
sca_event_from_str( str *event_str )
{
    int		event = SCA_EVENT_TYPE_UNKNOWN;

    if ( event_str == NULL || event_str->s == NULL ) {
	return( SCA_EVENT_TYPE_UNKNOWN );
    }

    if ( STR_EQ( *event_str, SCA_EVENT_NAME_CALL_INFO )) {
	event = SCA_EVENT_TYPE_CALL_INFO;
    } else if ( STR_EQ( *event_str, SCA_EVENT_NAME_LINE_SEIZE )) {
	event = SCA_EVENT_TYPE_LINE_SEIZE;
    }

    return( event );
}

    char *
sca_event_name_from_type( int event_type )
{
    switch ( event_type ) {
    case SCA_EVENT_TYPE_CALL_INFO:
	return( SCA_EVENT_NAME_CALL_INFO.s );

    case SCA_EVENT_TYPE_LINE_SEIZE:
	return( SCA_EVENT_NAME_LINE_SEIZE.s );

    default:
	break;
    }

    return( "unknown" );
}

    int
sca_event_append_header_for_type( int event_type, char *hdrbuf, int maxlen )
{
    int		len;

    len = snprintf( hdrbuf, maxlen, "Event: %s%s",
		    sca_event_name_from_type( event_type ), CRLF );
    if ( len >= maxlen ) {
	LM_ERR( "%s Event header too long",
		sca_event_name_from_type( event_type));
	return( -1 );
    }

    return( len );
}
