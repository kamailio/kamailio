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
