/* $Id$
 *
 * Copyright (C) 2004 Dan Pascu
 * Copyright (C) 2003 Porta Software Ltd
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

static void
pingClients(unsigned int ticks, void *param)
{
    static char pingbuf[4] = "\0\0\0\0";
    static int length = 256;
    struct socket_info* sock;
    struct hostent* hostent;
    union sockaddr_union to;
    struct sip_uri uri;
    void *buf, *ptr;
    str contact;
    int needed;

    buf = pkg_malloc(length);
    if (buf == NULL) {
        LOG(L_ERR, "error: mediaproxy/pingClients(): out of memory\n");
        return;
    }
    needed = userLocation.get_all_ucontacts(buf, length, FL_NAT);
    if (needed > 0) {
        // make sure we alloc more than actually we were told is missing
        // (some clients may register while we are making these calls)
        length = (length + needed) * 2;
        ptr = pkg_realloc(buf, length);
        if (ptr == NULL) {
            LOG(L_ERR, "error: mediaproxy/pingClients(): out of memory\n");
            pkg_free(buf);
            return;
        } else {
            buf = ptr;
        }
        // try again. we may fail again if _many_ clients register in between
        needed = userLocation.get_all_ucontacts(buf, length, FL_NAT);
        if (needed != 0) {
            pkg_free(buf);
            return;
        }
    }

    ptr = buf;
    while (1) {
        memcpy(&(contact.len), ptr, sizeof(contact.len));
        if (contact.len == 0)
            break;
        contact.s = (char*)ptr + sizeof(contact.len);
        ptr = contact.s + contact.len;
        if (parse_uri(contact.s, contact.len, &uri) < 0) {
            LOG(L_ERR, "error: mediaproxy/pingClients(): can't parse contact uri\n");
            continue;
        }
        if (uri.proto != PROTO_UDP && uri.proto != PROTO_NONE)
            continue;
        if (uri.port_no == 0)
            uri.port_no = SIP_PORT;
        hostent = sip_resolvehost(&uri.host, &uri.port_no, PROTO_UDP);
        if (hostent == NULL){
            LOG(L_ERR, "error: mediaproxy/pingClients(): can't resolve host\n");
            continue;
        }
        hostent2su(&to, hostent, 0, uri.port_no);
        sock = get_send_socket(0, &to, PROTO_UDP);
        if (sock == NULL) {
            LOG(L_ERR, "error: mediaproxy/pingClients(): can't get sending socket\n");
            continue;
        }
        udp_send(sock, pingbuf, sizeof(pingbuf), &to);
    }
    pkg_free(buf);
}


// Replace IP:Port in Contact field with the source address of the packet.
// Preserve port for SIP asymmetric clients
static int
FixContact(struct sip_msg* msg, char* str1, char* str2)
{
    str beforeHost, after, agent;
    contact_t* contact;
    struct lump* anchor;
    struct sip_uri uri;
    char *newip, *buf;
    int len, newiplen, offset;
    Bool asymmetric;

    if (!getContactURI(msg, &uri, &contact))
        return -1;

    if (uri.proto != PROTO_UDP && uri.proto != PROTO_NONE)
        return -1;

    newip = ip_addr2a(&msg->rcv.src_ip);
    newiplen = strlen(newip);

    /* Don't do anything if the IP's are the same. Return success. */
    if (newiplen==uri.host.len && memcmp(uri.host.s, newip, newiplen)==0) {
        return 1;
    }

    if (uri.port.len == 0)
        uri.port.s = uri.host.s + uri.host.len;

    agent = getUserAgent(msg);
    asymmetric = isSIPAsymmetric(agent);

    beforeHost.s   = contact->uri.s;
    beforeHost.len = uri.host.s - contact->uri.s;
    if (asymmetric) {
        // for asymmetrics we preserve the original port
        after.s   = uri.port.s;
        after.len = contact->uri.s + contact->uri.len - after.s;
    } else {
        after.s   = uri.port.s + uri.port.len;
        after.len = contact->uri.s + contact->uri.len - after.s;
    }

    len = beforeHost.len + newiplen + after.len + 20;

    // first try to alloc mem. if we fail we don't want to have the lump
    // deleted and not replaced. at least this way we keep the original.
    buf = pkg_malloc(len);
    if (buf == NULL) {
        LOG(L_ERR, "error: fix_contact(): out of memory\n");
        return -1;
    }

    offset = contact->uri.s - msg->buf;
    anchor = del_lump(msg, offset, contact->uri.len, HDR_CONTACT);

    if (!anchor) {
        pkg_free(buf);
        return -1;
    }

    if (asymmetric && uri.port.len==0) {
        len = sprintf(buf, "%.*s%s%.*s", beforeHost.len, beforeHost.s,
                      newip, after.len, after.s);
    } else if (asymmetric) {
        len = sprintf(buf, "%.*s%s:%.*s", beforeHost.len, beforeHost.s,
                      newip, after.len, after.s);
    } else {
        len = sprintf(buf, "%.*s%s:%d%.*s", beforeHost.len, beforeHost.s,
                      newip, msg->rcv.src_port, after.len, after.s);
    }

    if (insert_new_lump_after(anchor, buf, len, HDR_CONTACT) == 0) {
        pkg_free(buf);
        return -1;
    }

    contact->uri.s   = buf;
    contact->uri.len = len;

    if (asymmetric) {
        LOG(L_INFO, "info: fix_contact(): preserved port for SIP "
            "asymmetric client: `%.*s'\n", agent.len, agent.s);
    }

    return 1;
}


