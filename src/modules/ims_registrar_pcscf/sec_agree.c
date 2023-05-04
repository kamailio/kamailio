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
        SYS_MEM_ERROR;
        return ret;
    }

    memset(buf, 0, buf_len);
    memcpy(buf, value.s, value.len);

    ret = atoll(buf);

    free(buf);

    return ret;
}

static void trim_whitespaces(str* string) {
    // skip leading whitespace
    while(string->len && (string->s[0]==' ' || string->s[0]=='\t' || string->s[0]=='<')){
        string->s = string->s + 1;
        string->len --;
    }

    // skip trailing whitespace
    while(string->len && (string->s[string->len-1]==' ' || string->s[string->len-1]=='\t')){
        string->len--;
    }
}

#define SEC_COPY_STR_PARAM(DST, SRC)\
        DST.s = shm_malloc(SRC.len);\
        if(DST.s == NULL) {\
            SHM_MEM_ERROR;\
            return -1;\
        }\
        memcpy(DST.s, SRC.s, SRC.len);\
        DST.len = SRC.len;


static int process_sec_agree_param(str name, str value, ipsec_t *ret)
{
    trim_whitespaces(&name);
    trim_whitespaces(&value);

    if(strncasecmp(name.s, "alg", name.len) == 0) {
        SEC_COPY_STR_PARAM(ret->r_alg, value);
    }
    else if(strncasecmp(name.s, "prot", name.len) == 0) {
        SEC_COPY_STR_PARAM(ret->prot, value);
    }
    else if(strncasecmp(name.s, "mod", name.len) == 0) {
        SEC_COPY_STR_PARAM(ret->mod, value);
    }
    else if(strncasecmp(name.s, "ealg", name.len) == 0) {
        SEC_COPY_STR_PARAM(ret->r_ealg, value);
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

    return 0;
}

static security_t* parse_sec_agree(struct hdr_field* h)
{
    int i = 0;

    str name = {0,0};
    str value = {0,0};
    str mechanism_name = {0,0};
    security_t* params = NULL;
    str body = h->body;

    trim_whitespaces(&body);

    // find mechanism name end
    for(i = 0; body.s[i] != ';' && i < body.len; i++);

    mechanism_name.s = body.s;
    mechanism_name.len = i;

    if(strncasecmp(mechanism_name.s, "ipsec-3gpp", 10) != 0) {
        // unsupported mechanism
        LM_ERR("Unsupported mechanism: %.*s\n", STR_FMT(&mechanism_name));
        goto cleanup;
    }

    // allocate shm memory for security_t (it will be saved in contact)
    if ((params = shm_malloc(sizeof(security_t))) == NULL) {
        SHM_MEM_ERROR_FMT("for security_t parameters during sec-agree parsing\n");
        return NULL;
    }
    memset(params, 0, sizeof(security_t));

    if((params->sec_header.s = shm_malloc(h->name.len)) == NULL) {
        SHM_MEM_ERROR_FMT("for security_t sec_header parameter during sec-agree parsing\n");
        goto cleanup;
    }
    memcpy(params->sec_header.s, h->name.s, h->name.len);
    params->sec_header.len = h->name.len;

    // allocate memory for ipsec_t in security_t
    params->data.ipsec = shm_malloc(sizeof(ipsec_t));
    if(!params->data.ipsec) {
        SHM_MEM_ERROR_FMT("for ipsec parameters during sec-agree parsing\n");
        goto cleanup;
    }
    memset(params->data.ipsec, 0, sizeof(ipsec_t));


    // set security type to IPSEC
    params->type = SECURITY_IPSEC;

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
                if(process_sec_agree_param(name, value, params->data.ipsec)) {
                    goto cleanup;
                }
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

    return params;

cleanup:
    // The same piece of code also lives in modules/ims_usrloc_pcscf/pcontact.c
    // Function - free_security()
    // Keep them in sync!
    if (params) {
        shm_free(params->sec_header.s);
        shm_free(params->data.ipsec);
        if(params->type == SECURITY_IPSEC && params->data.ipsec) {
            shm_free(params->data.ipsec->ealg.s);
            shm_free(params->data.ipsec->r_ealg.s);
            shm_free(params->data.ipsec->ck.s);
            shm_free(params->data.ipsec->alg.s);
            shm_free(params->data.ipsec->r_alg.s);
            shm_free(params->data.ipsec->ik.s);
            shm_free(params->data.ipsec->prot.s);
            shm_free(params->data.ipsec->mod.s);
            shm_free(params->data.ipsec);
        }

        shm_free(params);
    }

    return NULL;
}


static str s_security_client={"Security-Client",15};
//static str s_security_server={"Security-Server",15};
static str s_security_verify={"Security-Verify",15};
/**
 * Looks for the Security-Client header
 * @param msg - the sip message
 * @param params - ptr to struct sec_agree_params, where parsed values will be saved
 * @returns 0 on success, error code on failure
 */
security_t* cscf_get_security(struct sip_msg *msg)
{
    struct hdr_field *h = NULL;

    if (!msg) return NULL;

    if (parse_headers(msg, HDR_EOH_F, 0)<0) {
        return NULL;
    }

    h = msg->headers;
    while(h)
    {
        if (h->name.len == s_security_client.len && strncasecmp(h->name.s, s_security_client.s, s_security_client.len)==0)
        {
            return parse_sec_agree(h);
        }

        h = h->next;
    }

    LM_INFO("No security parameters found\n");

    return NULL;
}

/**
 * Looks for the Security-Verify header
 * @param msg - the sip message
 * @param params - ptr to struct sec_agree_params, where parsed values will be saved
 * @returns 0 on success, error code on failure
 */
security_t* cscf_get_security_verify(struct sip_msg *msg)
{
    struct hdr_field *h = NULL;

    if (!msg) return NULL;

    if (parse_headers(msg, HDR_EOH_F, 0)<0) {
        return NULL;
    }

    h = msg->headers;
    while(h)
    {
        if (h->name.len == s_security_verify.len && strncasecmp(h->name.s, s_security_verify.s, s_security_verify.len)==0)
        {
            return parse_sec_agree(h);
        }

        h = h->next;
    }

    LM_INFO("No security-verify parameters found\n");

    return NULL;
}