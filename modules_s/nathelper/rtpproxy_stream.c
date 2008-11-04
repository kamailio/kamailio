/* $Id$
 *
 * Copyright (C) 2008 Sippy Software, Inc., http://www.sippysoft.com
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

#include <stdio.h>

#include "../../ip_addr.h"
#include "../../parser/msg_parser.h"
#include "../../sr_module.h"
#include "nathelper.h"
#include "nhelpr_funcs.h"

int
fixup_var_str_int(void **param, int param_no)
{
    int ret;

    if (param_no == 1) {
        ret = fix_param(FPARAM_AVP, param);
        if (ret <= 0)
            return ret;
        if (fix_param(FPARAM_STR, param) != 0)
            return -1;
    } else if (param_no == 2) {
        if (fix_param(FPARAM_INT, param) != 0)
            return -1;
    }
    return 0;
}

static int
rtpproxy_stream(struct sip_msg* msg, str *pname, int count, int stream2uac)
{
    int nitems;
    str callid, from_tag, to_tag;
    struct rtpp_node *node;
    char cbuf[16];
    struct iovec v[] = {
        {NULL,        0},
        {cbuf,        0}, /* 1 P<count> */
        {" ",         1},
        {NULL,        0}, /* 3 callid */
        {" ",         1},
        {NULL,        0}, /* 5 pname */
        {" session ", 9},
        {NULL,        0}, /* 7 from tag */
        {";1 ",       3},
        {NULL,        0}, /* 9 to tag */
        {";1",        2}
    };

    if (get_callid(msg, &callid) == -1 || callid.len == 0) {
        LOG(L_ERR, "ERROR: rtpproxy_stream: can't get Call-Id field\n");
        return -1;
    }
    if (get_to_tag(msg, &to_tag) == -1) {
        LOG(L_ERR, "ERROR: rtpproxy_stream: can't get To tag\n");
        return -1;
    }
    if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
        LOG(L_ERR, "ERROR: rtpproxy_stream: can't get From tag\n");
        return -1;
    }
    v[1].iov_len = sprintf(cbuf, "P%d", count);
    STR2IOVEC(callid, v[3]);
    STR2IOVEC(*pname, v[5]);
    node = select_rtpp_node(callid, 1, -1);
    if (!node) {
        LOG(L_ERR, "ERROR: rtpproxy_stream: no available proxies\n");
        return -1;
    }
    nitems = 11;
    if (stream2uac == 0) {
        if (to_tag.len == 0)
            return -1;
        STR2IOVEC(to_tag, v[7]);
        STR2IOVEC(from_tag, v[9]);
    } else {
        STR2IOVEC(from_tag, v[7]);
        STR2IOVEC(to_tag, v[9]);
        if (to_tag.len <= 0)
            nitems -= 2;
    }
    send_rtpp_command(node, v, nitems);

    return 1;
}

static int
rtpproxy_stream2_f(struct sip_msg *msg, char *str1, char *str2, int stream2uac)
{
    int count;
    str pname;

    if (get_str_fparam(&pname, msg, (fparam_t*) str1) < 0)
        return -1;
    if (get_int_fparam(&count, msg, (fparam_t*) str2) < 0)
        return -1;
    return rtpproxy_stream(msg, &pname, count, stream2uac);
}

int
rtpproxy_stream2uac2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stream2_f(msg, str1, str2, 1);
}

int
rtpproxy_stream2uas2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stream2_f(msg, str1, str2, 0);
}

static int
rtpproxy_stop_stream(struct sip_msg* msg, int stream2uac)
{
    int nitems;
    str callid, from_tag, to_tag;
    struct rtpp_node *node;
    struct iovec v[] = {
        {NULL,        0},
        {"S",         1}, /* 1 */
        {" ",         1},
        {NULL,        0}, /* 3 callid */
        {" ",         1},
        {NULL,        0}, /* 5 from tag */
        {";1 ",       3},
        {NULL,        0}, /* 7 to tag */
        {";1",        2}
    };

    if (get_callid(msg, &callid) == -1 || callid.len == 0) {
        LOG(L_ERR, "ERROR: rtpproxy_stop_stream: can't get Call-Id field\n");
        return -1;
    }
    if (get_to_tag(msg, &to_tag) == -1) {
        LOG(L_ERR, "ERROR: rtpproxy_stop_stream: can't get To tag\n");
        return -1;
    }
    if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
        LOG(L_ERR, "ERROR: rtpproxy_stop_stream: can't get From tag\n");
        return -1;
    }
    STR2IOVEC(callid, v[3]);
    node = select_rtpp_node(callid, 1, -1);
    if (!node) {
        LOG(L_ERR, "ERROR: rtpproxy_stop_stream: no available proxies\n");
        return -1;
    }
    nitems = 9;
    if (stream2uac == 0) {
        if (to_tag.len == 0)
            return -1;
        STR2IOVEC(to_tag, v[5]);
        STR2IOVEC(from_tag, v[7]);
    } else {
        STR2IOVEC(from_tag, v[5]);
        STR2IOVEC(to_tag, v[7]);
        if (to_tag.len <= 0)
            nitems -= 2;
    }
    send_rtpp_command(node, v, nitems);

    return 1;
}

int
rtpproxy_stop_stream2uac2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stop_stream(msg, 1);
}

int
rtpproxy_stop_stream2uas2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stop_stream(msg, 0);
}
