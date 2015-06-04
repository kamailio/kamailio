#include "common.h"
#include "log.h"
#include <string.h>

void pdb_msg_dbg(struct pdb_msg msg) {
    int i;

    LERR("version = %d\n", msg.hdr.version);
    LERR("type = %d\n", msg.hdr.type);
    LERR("code = %d\n", msg.hdr.code);
    LERR("id = %d\n", msg.hdr.id);
    LERR("len = %d\n", msg.hdr.length);
    LERR("payload = ");
    for (i = 0; i < msg.hdr.length - sizeof(msg.hdr); i++) {
        LERR("%02X ", msg.bdy.payload[i]);
    }
    LERR("\n");

    return ;
}

int pdb_msg_format_send(struct pdb_msg *msg,
                        uint8_t version, uint8_t type,
                        uint8_t code, uint16_t id,
                        char *payload, uint16_t payload_len)
{
    msg->hdr.version    = version;
    msg->hdr.type       = type;
    msg->hdr.code       = code;
    msg->hdr.id         = id;

    if (payload == NULL) {
        /* just ignore the NULL buff (called when just want to set the len) */
        msg->hdr.length     = sizeof(struct pdb_hdr);
        return 0;
    } else {
        msg->hdr.length     = sizeof(struct pdb_hdr) + payload_len;
        memcpy(msg->bdy.payload, payload, payload_len);
        return 0;
    }

    return 0;
}
