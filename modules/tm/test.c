#include "../../hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"

#include "t_hooks.h"

int _test_insert_to_reply( struct sip_msg *msg, char *str )
{
    struct lump* anchor;
    char *buf;
    int len;

    len=strlen( str );
    buf=pkg_malloc( len );
    if (!buf) {
        LOG(L_ERR, "_test_insert_to_reply: no mem\n");
        return 0;
    }
    memcpy( buf, str, len );

    anchor = anchor_lump(&msg->add_rm,
        msg->headers->name.s - msg->buf, 0 , 0);
    if (anchor == NULL) {
        LOG(L_ERR, "_test_insert_to_reply: anchor_lump failed\n");
        return 0;
    }
    if (insert_new_lump_before(anchor,buf, len, 0)==0) {
        LOG(L_ERR, "_test_insert_to_reply: inser_new_lump failed\n");
        return 0;
    }
    return 1;
}

