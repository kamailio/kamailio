/*
 * openser osp module. 
 *
 * This module enables openser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <osp/osp.h>
#include <osp/ospb64.h>
#include "../../forward.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "osp_mod.h"
#include "sipheader.h"

static void ospCopyStrToBuffer(str* source, char* buffer, int buffersize);
static void ospSkipPlus(char* e164);
static int ospAppendHeader(struct sip_msg* msg, str* header); 

/* 
 * Copy str to buffer and check overflow 
 * param source Str
 * param buffer Buffer
 * param buffersize Size of buffer
 */
static void ospCopyStrToBuffer(
    str* source, 
    char* buffer, 
    int buffersize)
{
    int copybytes;

    LOG(L_DBG, "osp: ospCopyStrToBuffer\n");

    if (source->len > buffersize) {
        LOG(L_ERR,
            "osp: ERROR: buffer for copying '%.*s' is too small, will copy the first '%d' bytes\n",
            source->len,
            source->s, 
            buffersize);
        copybytes = buffersize;
    } else {
        copybytes = source->len;
    }

    strncpy(buffer, source->s, copybytes);
    buffer[copybytes] = '\0';
}

/* 
 * Remove '+' in E164 string
 * param e164 E164 string
 */
static void ospSkipPlus(
    char* e164)
{
    LOG(L_DBG, "osp: ospSkipPlus\n");

    if (*e164 == '+') {
        strncpy(e164, e164 + 1, strlen(e164) - 1);
        e164[strlen(e164) - 1] = '\0';
    }
}

/* 
 * Get calling number from From header
 * param msg SIP message
 * param fromuser User part of From header
 * param buffersize Size of fromuser buffer
 * return 0 success, 1 failure
 */
int ospGetFromUserpart(
    struct sip_msg* msg, 
    char* fromuser, 
    int buffersize)
{
    struct to_body* from;
    struct sip_uri uri;
    int result = 1;

    LOG(L_DBG, "osp: ospGetFromUserpart\n");

    fromuser[0] = '\0';

    if (msg->from != NULL) {
        if (parse_from_header(msg) == 0) {
            from = (struct to_body*)msg->from->parsed;
            if (parse_uri(from->uri.s, from->uri.len, &uri) == 0) {
                ospCopyStrToBuffer(&uri.user, fromuser, buffersize);
                ospSkipPlus(fromuser);
                result = 0;
            } else {
                LOG(L_ERR, "osp: ERROR: failed to parse From uri\n");
            }
        } else {
            LOG(L_ERR, "osp: ERROR: failed to parse From header\n");
        }
    } else {
        LOG(L_ERR, "osp: ERROR: failed to find From header\n");
    }

    return result;
}

/* 
 * Get called number from To header
 * param msg SIP message
 * param touser User part of To header
 * param buffersize Size of touser buffer
 * return 0 success, 1 failure
 */
int ospGetToUserpart(
    struct sip_msg* msg, 
    char* touser, 
    int buffersize)
{
    struct to_body* to;
    struct sip_uri uri;
    int result = 1;

    LOG(L_DBG, "osp: ospGetToUserpart\n");

    touser[0] = '\0';

    if (msg->to != NULL) {
        to = (struct to_body*)msg->to->parsed;
        if (parse_uri(to->uri.s, to->uri.len, &uri) == 0) {
            ospCopyStrToBuffer(&uri.user, touser, buffersize);
            ospSkipPlus(touser);
            result = 0;
        } else {
            LOG(L_ERR, "osp: ERROR: failed to parse To uri\n");
        }
    } else {
        LOG(L_ERR, "osp: ERROR: failed to find To header\n");
    }

    return result;
}

/* 
 * Get called number from Request-Line header
 * param msg SIP message
 * param touser User part of To header
 * param buffersize Size of touser buffer
 * return 0 success, 1 failure
 */
int ospGetUriUserpart(
    struct sip_msg* msg, 
    char* uriuser, 
    int buffersize)
{
    int result = 1;

    LOG(L_DBG, "osp: ospGetUriUserpart\n");

    uriuser[0] = '\0';

    if (parse_sip_msg_uri(msg) >= 0) {
        ospCopyStrToBuffer(&msg->parsed_uri.user, uriuser, buffersize);
        ospSkipPlus(uriuser);
        result = 0;
    } else {
        LOG(L_ERR, "osp: ERROR: failed to parse Request-Line URI\n");
    }

    return result;
}

/* 
 * Append header to SIP message
 * param msg SIP message
 * param header Header to be appended
 * return 0 success, -1 failure
 */
static int ospAppendHeader(
    struct sip_msg* msg, 
    str* header)
{
    char* s;
    struct lump* anchor;
    
    LOG(L_DBG, "osp: ospAppendHeader\n");

    if((msg == 0) || (header == 0) || (header->s == 0) || (header->len <= 0)) {
        LOG(L_ERR, "osp: ERROR: bad parameters for appending header\n");
        return -1;
    }

    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
        LOG(L_ERR, "osp: ERROR: failed to parse message\n");
        return -1;
    }

    anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
    if (anchor == 0) {
        LOG(L_ERR, "osp: ERROR: failed to get anchor\n");
        return -1;
    }

    s = (char*)pkg_malloc(header->len);
    if (s == 0) {
        LOG(L_ERR, "osp: ERROR: no memory\n");
        return -1;
    }

    memcpy(s, header->s, header->len);

    if (insert_new_lump_before(anchor, s, header->len, 0) == 0) {
        LOG(L_ERR, "osp: ERROR: failed to insert lump\n");
        pkg_free(s);
        return -1;
    }
    
    return 0;
}

/* 
 * Add OSP token header to SIP message
 * param msg SIP message
 * param token OSP authorization token
 * param tokensize Size of OSP authorization token
 * return 0 success, 1 failure
 */
int ospAddOspHeader(
    struct sip_msg* msg, 
    unsigned char* token, 
    unsigned int tokensize)
{
    str headerval;
    char buffer[OSP_HEADERBUF_SIZE];
    unsigned char encodedtoken[OSP_TOKENBUF_SIZE];
    unsigned int encodedtokensize = sizeof(encodedtoken);
    int  result = 1;

    LOG(L_DBG, "osp: ospAddOspHeader\n");

    if (tokensize == 0) {
        LOG(L_DBG, "osp: destination is not OSP device\n");
        result = 0;
    } else {
        if (OSPPBase64Encode(token, tokensize, encodedtoken, &encodedtokensize) == 0) {
            snprintf(buffer,
                sizeof(buffer),
                "%s%.*s\r\n", 
                OSP_TOKEN_HEADER,
                encodedtokensize,
                encodedtoken);
    
            headerval.s = buffer;
            headerval.len = strlen(buffer);
    
            LOG(L_DBG, "osp: setting osp token header field '%s'\n", buffer);
    
            if (ospAppendHeader(msg, &headerval) == 0) {
                result = 0;
            } else {
                LOG(L_ERR, "osp: ERROR: failed to append osp header\n");
            }
        } else {
            LOG(L_ERR, "osp: ERROR: failed to base64 encode token\n");
        }
    }

    return result;
}

/* 
 * Get OSP token from SIP message
 * param msg SIP message
 * param token OSP authorization token
 * param tokensize Size of OSP authorization token
 * return 0 success, 1 failure
 */
int ospGetOspHeader(
    struct sip_msg* msg, 
    unsigned char* token, 
    unsigned int* tokensize)
{
    struct hdr_field* hf;
    int errorcode;
    int result = 1;

    LOG(L_DBG, "osp: ospGetOspHeader\n");

    parse_headers(msg, HDR_EOH_T, 0);

    for (hf = msg->headers; hf; hf = hf->next) {
        if ((hf->type == HDR_OTHER_T) && (hf->name.len == OSP_HEADER_SIZE - 2)) {
            // possible hit
            if (strncasecmp(hf->name.s, OSP_TOKEN_HEADER, OSP_HEADER_SIZE) == 0) {
                if ((errorcode = OSPPBase64Decode(hf->body.s, hf->body.len, token, tokensize)) == 0) {
                    result = 0;
                } else {
                    LOG(L_ERR, "osp: ERROR: failed to base64 decode token (%d)\n", errorcode);
                    LOG(L_ERR, "osp: ERROR: header '%.*s' length %d\n", hf->body.len, hf->body.s, hf->body.len);
                }
                break;
            }        
        } 
    }

    return result;
}

/* 
 * Get first VIA header and use the IP or host name
 * param msg SIP message
 * param sourceaddress Source address
 * param buffersize Size of sourceaddress
 * return 0 success, 1 failure
 */
int ospGetSourceAddress(
    struct sip_msg* msg, 
    char* sourceaddress, 
    int buffersize)
{
    struct hdr_field* hf;
    struct via_body* via;
    int result = 1;

    LOG(L_DBG, "osp: ospGetSourceAddress\n");

    /* 
     * No need to call parse_headers, called already and VIA is parsed
     * anyway by default 
     */
    for (hf = msg->headers; hf; hf = hf->next) {
        if (hf->type == HDR_VIA_T) {
            // found first VIA
            via = (struct via_body*)hf->parsed;    
            ospCopyStrToBuffer(&via->host, sourceaddress, buffersize);

            LOG(L_DBG, "osp: source address '%s'\n", sourceaddress);

            result = 0;
            break;
        } 
    }

    return result;
}

/* 
 * Get Call-ID header from SIP message
 * param msg SIP message
 * param callid Call ID
 * return 0 success, 1 failure
 */
int ospGetCallId(
    struct sip_msg* msg, 
    OSPTCALLID** callid)
{
    struct hdr_field* hf;
    int result = 1;

    LOG(L_DBG, "osp: ospGetCallId\n");

    hf = (struct hdr_field*)msg->callid;

    if (hf != NULL) {
        *callid = OSPPCallIdNew(hf->body.len, (unsigned char*)hf->body.s);
        if (*callid) {
            result = 0;
        } else {
            LOG(L_ERR, "osp: ERROR: failed to allocate OSPCALLID object for '%.*s'\n", hf->body.len, hf->body.s);
        }
    } else {
        LOG(L_ERR, "osp: ERROR: failed to find Call-ID header\n");
    }    

    return result;
}

/* 
 * Get route parameters from the 1st Route or Request-Line
 * param msg SIP message
 * param routeparameters Route parameters
 * param buffersize Size of routeparameters
 * return 0 success, 1 failure
 */
int ospGetRouteParameters(
    struct sip_msg* msg, 
    char* routeparameters, 
    int buffersize)
{
    struct hdr_field* hf;
    rr_t* rt;
    struct sip_uri uri;
    int result = 1;

    LOG(L_DBG, "osp: ospGetRouterParameters\n");
    LOG(L_DBG, "osp: parsed uri host '%.*s' port '%d' vars '%.*s'\n",
        msg->parsed_uri.host.len,
        msg->parsed_uri.host.s,
        msg->parsed_uri.port_no,
        msg->parsed_uri.params.len,
        msg->parsed_uri.params.s);

    if (!(hf = msg->route)) {
        LOG(L_DBG, "osp: there is no Route headers\n");
    } else if (!(rt = (rr_t*)hf->parsed)) {
        LOG(L_ERR, "osp: ERROR: route headers are not parsed\n");
    } else if (parse_uri(rt->nameaddr.uri.s, rt->nameaddr.uri.len, &uri) != 0) {
        LOG(L_ERR, "osp: ERROR: failed to parse the Route uri '%.*s'\n", rt->nameaddr.uri.len, rt->nameaddr.uri.s);
    } else if (check_self(&uri.host, uri.port_no ? uri.port_no : SIP_PORT, PROTO_NONE) != 1) {
        LOG(L_DBG, "osp: the Route uri is NOT mine\n");
        LOG(L_DBG, "osp: host '%.*s' port '%d'\n", uri.host.len, uri.host.s, uri.port_no);
        LOG(L_DBG, "osp: params '%.*s'\n", uri.params.len, uri.params.s);
    } else {
        LOG(L_DBG, "osp: the Route uri IS mine - '%.*s'\n", uri.params.len, uri.params.s);
        LOG(L_DBG, "osp: host '%.*s' port '%d'\n", uri.host.len, uri.host.s, uri.port_no);
        ospCopyStrToBuffer(&uri.params, routeparameters, buffersize);
        result = 0;
    }

    if ((result == 1) && (msg->parsed_uri.params.len > 0)) {
        LOG(L_DBG, "osp: using route parameters from Request-Line uri\n");
        ospCopyStrToBuffer(&msg->parsed_uri.params, routeparameters, buffersize);
        routeparameters[msg->parsed_uri.params.len] = '\0';
        result = 0;
    }

    return result;
}

/* 
 * Rebuild URI using called number, destination IP, and port
 * param newuri URI to be built
 * param called Called number
 * param dest Destination IP
 * param port Destination port
 * return 0 success, 1 failure
 */
int ospRebuildDestionationUri(
    str* newuri, 
    char* called,
    char* dest, 
    char* port) 
{
#define TRANS       ";transport=tcp" 
#define TRANS_LEN   14
    char* buffer;
    int calledsize;
    int destsize;
    int portsize;

    LOG(L_DBG, "osp: ospRebuildDestinationUri\n");

    calledsize = strlen(called);
    destsize = strlen(dest);
    portsize = strlen(port);

    LOG(L_DBG, "osp: '%s'(%i) '%s'(%i) '%s'(%i)\n",
        called, 
        calledsize,
        dest, 
        destsize, 
        port, 
        portsize); 

    /* "sip:" + called + "@" + dest + : + port + " SIP/2.0" */
    newuri->s = (char*)pkg_malloc(4 + calledsize + 1 + destsize + 1 + portsize + 1 + 16 + TRANS_LEN);
    if (newuri == NULL) {
        LOG(L_ERR, "osp: ERROR: no memory\n");
        return 1;
    }    
    buffer = newuri->s;

    *buffer++ = 's';
    *buffer++ = 'i';
    *buffer++ = 'p';
    *buffer++ = ':';

    memcpy(buffer, called, calledsize);
    buffer += calledsize;
    *buffer++ = '@';
    
    if (*dest == '[') {
        /* leave out annoying [] */
        memcpy(buffer, dest + 1, destsize - 2);
        buffer += destsize - 2;
    } else {
        memcpy(buffer, dest, destsize);
        buffer += destsize;
    }
    
    if (portsize > 0) {
        *buffer++ = ':';
        memcpy(buffer, port, portsize);
        buffer += portsize;
    }

/*    
    *buffer++ = ' ';
    *buffer++ = 'S';
    *buffer++ = 'I';
    *buffer++ = 'P';
    *buffer++ = '/';
    *buffer++ = '2';
    *buffer++ = '.';
    *buffer++ = '0';

    memcpy(buffer, TRANS, TRANS_LEN);
    buffer += TRANS_LEN;
    *buffer = '\0';
*/

    newuri->len = buffer - newuri->s;

    LOG(L_DBG, "osp: new uri '%.*s'\n", newuri->len, newuri->s);

    return 0;
}

/* 
 * Get next hop using the first Route not generated by this proxy or URI from the Request-Line
 * param msg SIP message
 * param nexthop Next hop IP
 * param buffersize Size of nexthop
 */
void ospGetNextHop(
    struct sip_msg* msg, 
    char* nexthop, 
    int buffersize)
{
    struct hdr_field* hf;
    struct sip_uri uri;
    rr_t* rt;
    int found = 0;

    LOG(L_DBG, "osp: ospGetNextHop\n");

    for (hf = msg->headers; hf; hf = hf->next) {
        if (hf->type == HDR_ROUTE_T) {
            rt = (rr_t*)hf->parsed;    
            if (parse_uri(rt->nameaddr.uri.s, rt->nameaddr.uri.len, &uri) == 0) {
                LOG(L_DBG, "osp: host '%.*s' port '%d'\n", uri.host.len, uri.host.s, uri.port_no);

                if (check_self(&uri.host, uri.port_no ? uri.port_no : SIP_PORT, PROTO_NONE) != 1) {
                    LOG(L_DBG, "osp: it is NOT me, FOUND!\n");

                    ospCopyStrToBuffer(&uri.host, nexthop, buffersize);
                    found = 1;
                    break;
                } else {
                    LOG(L_DBG, "osp: it IS me, keep looking\n");
                }
            } else {
                LOG(L_ERR, 
                    "osp: ERROR: failed to parsed route uri '%.*s'\n", 
                    rt->nameaddr.uri.len, 
                    rt->nameaddr.uri.s);
            }
        }
    }

    if (!found) {
        LOG(L_DBG, "osp: using the Request-Line instead host '%.*s' port '%d'\n",
             msg->parsed_uri.host.len,
             msg->parsed_uri.host.s,
             msg->parsed_uri.port_no);

        ospCopyStrToBuffer(&msg->parsed_uri.host, nexthop, buffersize);
        found =1;
    }
}

