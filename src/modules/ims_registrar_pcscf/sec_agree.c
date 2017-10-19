/**
 * Copyright (C) 2017 kamailio.org
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "sec_agree.h"

#include "../../core/str.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/mem/mem.h"


static uint32_t parse_digits(str value)
{
    uint32_t ret = 0;

    int buf_len = value.len+1;
    char* buf = (char*)malloc(buf_len);

    if(!buf) {
        return ret;
    }

    memset(buf, 0, buf_len);
    memcpy(buf, value.s, value.len);

    ret = atoll(buf);

    free(buf);

    return ret;
}

static void process_sec_agree_param(str name, str value, ipsec_t *ret)
{
    if(strncasecmp(name.s, "alg", name.len) == 0) {
        ret->r_alg = value;
    }
    else if(strncasecmp(name.s, "prot", name.len) == 0) {
        ret->prot = value;
    }
    else if(strncasecmp(name.s, "mod", name.len) == 0) {
        ret->mod = value;
    }
    else if(strncasecmp(name.s, "ealg", name.len) == 0) {
        ret->r_alg = value;
    }
    else if(strncasecmp(name.s, "spi-c", name.len) == 0) {
        ret->spi_uc = parse_digits(value);
    }
    else if(strncasecmp(name.s, "spi-s", name.len) == 0) {
        ret->spi_us = parse_digits(value);
    }
    else if(strncasecmp(name.s, "port-c", name.len) == 0) {
        ret->port_uc = parse_digits(value);
    }
    else if(strncasecmp(name.s, "port-s", name.len) == 0) {
        ret->port_us = parse_digits(value);
    }
    else {
        //unknown parameter
    }
}

static int parse_sec_agree(str body, security_t *params)
{
    int i = 0;

    str name = {0,0};
    str value = {0,0};
    str mechanism_name = {0,0};

    if(!params) {
        return 10;
    }

    // skip leading whitespace
    while(body.len && (body.s[0]==' ' || body.s[0]=='\t' || body.s[0]=='<')){
        body.s = body.s + 1;
        body.len --;
    }

    // skip trailing whitespace
    while(body.len && (body.s[body.len-1]==' ' || body.s[body.len-1]=='\t')){
        body.len--;
    }

    // skip mechanism name - noone seems to need it
    for(i = 0; body.s[i] != ';' && i < body.len; i++);

    mechanism_name.s = body.s;
    mechanism_name.len = i;

    if(strncasecmp(mechanism_name.s, "ipsec-3gpp", 10) != 0) {
        //unsupported mechanism
        LM_ERR("Unsupported mechanism: %.*s\n", STR_FMT(&mechanism_name));
        return 11;
    }

    params->type = SECURITY_IPSEC;

    params->data.ipsec = pkg_malloc(sizeof(ipsec_t));
    if(!params->data.ipsec) {
        LM_ERR("Error allocating memory for ipsec parameters during sec-agree parsing\n");
        return 12;
    }

    memset(params->data.ipsec, 0, sizeof(ipsec_t));

    body.s=body.s+i+1;
    body.len=body.len-i-1;

    // get the rest of the parameters
    i = 0;
    while(i <= body.len) {
        //look for end of buffer or parameter separator
        if(i == body.len || body.s[i] == ';' ) {
            if(name.len) {
                // if(name.len) => a param name is parsed
                // and now i points to the end of its value
                value.s = body.s;
                value.len = i;
            }
            //else - name is not read but there is a value
            //so there is some error - skip ahead
            body.s=body.s+i+1;
            body.len=body.len-i-1;

            i=0;

            if(name.len && value.len) {
                process_sec_agree_param(name, value, params->data.ipsec);
            }
            //else - something's wrong. Ignore!

            //processing is done - reset
            name.len=0;
            value.len=0;
        }
        //look for param=value separator
        else if(body.s[i] == '=') {
            name.s = body.s;
            name.len = i;

            //position saved - skip ahead
            body.s=body.s+i+1;
            body.len=body.len-i-1;

            i=0;
        }
        //nothing interesting - move on
        else {
            i++;
        }
    }

    return 0;
}

static str s_security_client={"Security-Client",15};
static str s_security_server={"Security-Server",15};
static str s_security_verify={"Security-Verify",15};
/**
 * Looks for the Security-Client header
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field
 * @param params - ptr to struct sec_agree_params, where parsed values will be saved
 * @returns 0 on success, error code on failure
 */
int cscf_get_security(struct sip_msg *msg, security_t *params)
{
    struct hdr_field *h = NULL;

    if (!msg) return 1;
    if (!params) return 2;

    if (parse_headers(msg, HDR_EOH_F, 0)<0) {
        return 3;
    }

    h = msg->headers;
    while(h)
    {
        if ((h->name.len == s_security_client.len && strncasecmp(h->name.s, s_security_client.s, s_security_client.len)==0) ||
            (h->name.len == s_security_server.len && strncasecmp(h->name.s, s_security_server.s, s_security_server.len)==0) ||
            (h->name.len == s_security_verify.len && strncasecmp(h->name.s, s_security_verify.s, s_security_verify.len)==0) )
        {
            params->sec_header = h->name;
            return parse_sec_agree(h->body, params);
        }

        h = h->next;
    }

    return 4;
}

void free_security_t(security_t *params)
{
    switch (params->type)
    {
        case SECURITY_IPSEC:
            pkg_free(params->data.ipsec);
        break;

        case SECURITY_TLS:
            pkg_free(params->data.ipsec);
        break;

        //default: Nothing to deallocate
    }
}
