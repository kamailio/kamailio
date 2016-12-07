/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <osp/osp.h>
#include <osp/ospb64.h>
#include "../../trim.h"
#include "../../forward.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rpid.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "osp_mod.h"
#include "sipheader.h"

extern int _osp_use_rpid;

static void ospSkipPlus(char* e164);
static int ospAppendHeader(struct sip_msg* msg, str* header); 

/* 
 * Copy str to buffer and check overflow 
 * param source Str
 * param buffer Buffer
 * param buffersize Size of buffer
 */
void ospCopyStrToBuffer(
    str* source, 
    char* buffer, 
    int buffersize)
{
    int copybytes;

    LOG(L_DBG, "osp: ospCopyStrToBuffer\n");

    if (source->len > buffersize - 1) {
        LOG(L_ERR,
            "osp: ERROR: buffer for copying '%.*s' is too small, will copy the first '%d' bytes\n",
            source->len,
            source->s, 
            buffersize);
        copybytes = buffersize - 1;
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
 * return 0 success, -1 failure
 */
int ospGetFromUserpart(
    struct sip_msg* msg, 
    char* fromuser, 
    int buffersize)
{
    struct to_body* from;
    struct sip_uri uri;
    int result = -1;

    LOG(L_DBG, "osp: ospGetFromUserpart\n");

    fromuser[0] = '\0';

    if (msg->from != NULL) {
        if (parse_from_header(msg) == 0) {
            from = get_from(msg);
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
 * Get calling number from Remote-Party-ID header
 * param msg SIP message
 * param rpiduser User part of Remote-Party-ID header
 * param buffersize Size of fromuser buffer
 * return 0 success, -1 failure
 */
int ospGetRpidUserpart(
    struct sip_msg* msg, 
    char* rpiduser, 
    int buffersize)
{
    struct to_body* rpid;
    struct sip_uri uri;
    int result = -1;

    LOG(L_DBG, "osp: ospGetRpidUserpart\n");

    rpiduser[0] = '\0';

    if (_osp_use_rpid != 0) {
        if (msg->rpid != NULL) {
            if (parse_rpid_header(msg) == 0) {
                rpid = get_rpid(msg);
                if (parse_uri(rpid->uri.s, rpid->uri.len, &uri) == 0) {
                    ospCopyStrToBuffer(&uri.user, rpiduser, buffersize);
                    ospSkipPlus(rpiduser);
                    result = 0;
                } else {
                    LOG(L_ERR, "osp: ERROR: failed to parse RPID uri\n");
                }
            } else {
                LOG(L_ERR, "osp: ERROR: failed to parse RPID header\n");
            }
        } else {
            LOG(L_DBG, "osp: without RPID header\n");
        }
    } else {
        LOG(L_DBG, "osp: do not use RPID header\n");
    }

    return result;
}

/* 
 * Get called number from To header
 * param msg SIP message
 * param touser User part of To header
 * param buffersize Size of touser buffer
 * return 0 success, -1 failure
 */
int ospGetToUserpart(
    struct sip_msg* msg, 
    char* touser, 
    int buffersize)
{
    struct to_body* to;
    struct sip_uri uri;
    int result = -1;

    LOG(L_DBG, "osp: ospGetToUserpart\n");

    touser[0] = '\0';

    if (msg->to != NULL) {
        if (parse_headers(msg, HDR_TO_F, 0) == 0) {
            to = get_to(msg);
            if (parse_uri(to->uri.s, to->uri.len, &uri) == 0) {
                ospCopyStrToBuffer(&uri.user, touser, buffersize);
                ospSkipPlus(touser);
                result = 0;
            } else {
                LOG(L_ERR, "osp: ERROR: failed to parse To uri\n");
            }
        } else {
            LOG(L_ERR, "osp: ERROR: failed to parse To header\n");
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
 * return 0 success, -1 failure
 */
int ospGetUriUserpart(
    struct sip_msg* msg, 
    char* uriuser, 
    int buffersize)
{
    int result = -1;

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
 * return 0 success, -1 failure
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
    int  result = -1;

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
 * return 0 success, -1 failure
 */
int ospGetOspHeader(
    struct sip_msg* msg, 
    unsigned char* token, 
    unsigned int* tokensize)
{
    struct hdr_field* hf;
    int errorcode;
    int result = -1;

    LOG(L_DBG, "osp: ospGetOspHeader\n");

    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
        LOG(L_ERR, "osp: ERROR: failed to parse headers\n");
        return result;
    }

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
 * return 0 success, -1 failure
 */
int ospGetSourceAddress(
    struct sip_msg* msg, 
    char* sourceaddress, 
    int buffersize)
{
    struct via_body* via;
    int result = -1;

    LOG(L_DBG, "osp: ospGetSourceAddress\n");

    if (msg->h_via1 || (parse_headers(msg, HDR_VIA_F, 0) == 0 && msg->h_via1)) {
        via = msg->h_via1->parsed;
        ospCopyStrToBuffer(&via->host, sourceaddress, buffersize);
        LOG(L_DBG, "osp: source address '%s'\n", sourceaddress);
        result = 0;
    }

    return result;
}

/* 
 * Get Call-ID header from SIP message
 * param msg SIP message
 * param callid Call ID
 * return 0 success, -1 failure
 */
int ospGetCallId(
    struct sip_msg* msg, 
    OSPTCALLID** callid)
{
    struct hdr_field* hf;
    int result = -1;

    LOG(L_DBG, "osp: ospGetCallId\n");

    if (!msg->callid && parse_headers(msg, HDR_CALLID_F, 0) == -1) {
        LOG(L_ERR, "osp: failed to parse Call-ID\n");
        return result;
    }

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
 * return 0 success, -1 failure
 */
int ospGetRouteParameters(
    struct sip_msg* msg, 
    char* routeparameters, 
    int buffersize)
{
    struct hdr_field* hf;
    rr_t* rt;
    struct sip_uri uri;
    int result = -1;

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

    if ((result == -1) && (msg->parsed_uri.params.len > 0)) {
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
 * param format URI format
 * return 0 success, -1 failure
 */
int ospRebuildDestionationUri(
    str* newuri, 
    char* called,
    char* dest, 
    char* port,
    int format) 
{
    static const str TRANS = {";transport=tcp", 14};
    char* buffer;
    int calledsize;
    int destsize;
    int portsize;

    LOG(L_DBG, "osp: ospRebuildDestinationUri\n");

    calledsize = strlen(called);
    destsize = strlen(dest);
    portsize = strlen(port);

    LOG(L_DBG, "osp: '%s'(%i) '%s'(%i) '%s'(%i) '%d'\n",
        called, 
        calledsize,
        dest, 
        destsize, 
        port, 
        portsize,
        format); 

    /* "sip:" + called + "@" + dest + : + port + " SIP/2.0" for URI format 0 */
    /* "<sip:" + called + "@" + dest + : + port> + " SIP/2.0" for URI format 1 */
    newuri->s = (char*)pkg_malloc(1 + 4 + calledsize + 1 + destsize + 1 + portsize + 1 + 1 + 16 + TRANS.len);
    if (newuri == NULL) {
        LOG(L_ERR, "osp: ERROR: no memory\n");
        return -1;
    }    
    buffer = newuri->s;

    if (format == 1) {
      *buffer++ = '<';
    }
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

    if (format == 1) {
      *buffer++ = '>';
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

    memcpy(buffer, TRANS.s, TRANS.len);
    buffer += TRANS.len;
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
            for (rt = (rr_t*)hf->parsed; rt; rt = rt->next) {
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
            if (found == 1) {
                break;
            }
        }
    }

    if (!found) {
        LOG(L_DBG, "osp: using the Request-Line instead host '%.*s' port '%d'\n",
             msg->parsed_uri.host.len,
             msg->parsed_uri.host.s,
             msg->parsed_uri.port_no);

        ospCopyStrToBuffer(&msg->parsed_uri.host, nexthop, buffersize);
        found = 1;
    }
}

/* 
 * Get direction using the first Route not generated by this proxy or URI from the Request-Line
 *     SER does not have is_direction as Kamailio. We have to write it.
 *     The problem is we cannot check append_fromtag option before running. So, ftag may not exist.
 * param msg SIP message
 * return 0 originating, 1 terminating, -1 failed 
 */
int ospGetDirection(struct sip_msg* msg)
{
    static const str FTAG = {"ftag", 4};
    char parameters[OSP_HEADERBUF_SIZE];
    char* tmp;
    char* token;
    str ftag;
    struct to_body* from;
    int result = -1;

    LOG(L_DBG, "osp: ospGetDirection\n");

    if (ospGetRouteParameters(msg, parameters, sizeof(parameters)) == 0) {
        for (token = strtok_r(parameters, ";", &tmp);
             token;
             token = strtok_r(NULL, ";", &tmp))
        {
            ftag.s = token;
            ftag.len = strlen(token);

            /* Remove leading white space char */
            trim_leading(&ftag);

            if (strncmp(ftag.s, FTAG.s, FTAG.len) == 0) {
                /* Remove "ftag" string */
                ftag.s += FTAG.len;
                ftag.len -= FTAG.len;

                /* Remove leading white space char */
                trim_leading(&ftag);

                /* Remove '=' char */
                ftag.s++;
                ftag.len--;

                /* Remove leading and tailing white space char */
                trim(&ftag);

                LOG(L_DBG, "osp: ftag '%s'\n", ftag.s);

                if ((parse_from_header(msg) == 0) && ((from = get_from(msg)) != 0)) {
                    if ((ftag.len == from->tag_value.len) && !strncmp(ftag.s, from->tag_value.s, ftag.len)) {
                        LOG(L_DBG, "osp: originating\n");
                        result = 0;
                    } else {
                        LOG(L_DBG, "osp: terminating\n");
                        result = 1;
                    }
                }
                break;
            } else {
                LOG(L_DBG, "osp: ignoring parameter '%s'\n", token);
            }
        }
    }

    return result;
}
