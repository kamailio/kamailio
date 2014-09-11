/* $Id: rtpproxy_stream.c,v 1.2 2008/11/04 22:28:04 sobomax Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <sys/uio.h>

#include "../../ip_addr.h"
#include "../../parser/msg_parser.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "rtpproxy.h"
#include "rtpproxy_funcs.h"

int
fixup_var_str_int(void **param, int param_no)
{
    int ret;
    pv_elem_t *model;
    str s;

    if (param_no == 1) {
        model = NULL;
        s.s = (char *)(*param);
        s.len = strlen(s.s);
        if (pv_parse_format(&s, &model) < 0) {
            LM_ERR("wrong format[%s]!\n", (char *)(*param));
            return E_UNSPEC;
        }
        if (model == NULL) {
            LM_ERR("empty parameter!\n");
            return E_UNSPEC;
        }
        *param = (void *)model;
    } else if (param_no == 2) {
	/* According to
	 * http://www.kamailio.org/docs/modules/1.5.x/nathelper.html#rtpproxy_stream2xxx
	 * this could be -1 */
	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (str2sint(&s, &ret)==0) {
		pkg_free(*param);
		*param = (void *)(long)ret;
        } else {
            LM_ERR("bad number <%s>\n", (char *)(*param));
            return E_CFG;
        }
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
        LM_ERR("can't get Call-Id field\n");
        return -1;
    }
    if (get_to_tag(msg, &to_tag) == -1) {
        LM_ERR("can't get To tag\n");
        return -1;
    }
    if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
        LM_ERR("can't get From tag\n");
        return -1;
    }
    v[1].iov_len = sprintf(cbuf, "P%d", count);
    STR2IOVEC(callid, v[3]);
    STR2IOVEC(*pname, v[5]);
    node = select_rtpp_node(callid, 1);
    if (!node) {
        LM_ERR("no available proxies\n");
        return -1;
    }
    if (node->rn_ptl_supported == 0) {
        LM_ERR("required functionality is not "
          "supported by the version of the RTPproxy running on the selected "
          "node.  Please upgrade the RTPproxy and try again.\n");
        return -1;
    }
    set_rtp_inst_pvar(msg, &node->rn_url);
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
rtpproxy_stream2_f(struct sip_msg *msg, char *str1, int count, int stream2uac)
{
    str pname;

    if (str1 == NULL || pv_printf_s(msg, (pv_elem_p)str1, &pname) != 0)
	return -1;
    return rtpproxy_stream(msg, &pname, count, stream2uac);
}

int
rtpproxy_stream2uac2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stream2_f(msg, str1, (int)(long)str2, 1);
}

int
rtpproxy_stream2uas2_f(struct sip_msg* msg, char* str1, char* str2)
{

    return rtpproxy_stream2_f(msg, str1, (int)(long)str2, 0);
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
        LM_ERR("can't get Call-Id field\n");
        return -1;
    }
    if (get_to_tag(msg, &to_tag) == -1) {
        LM_ERR("can't get To tag\n");
        return -1;
    }
    if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
        LM_ERR("can't get From tag\n");
        return -1;
    }
    STR2IOVEC(callid, v[3]);
    node = select_rtpp_node(callid, 1);
    if (!node) {
        LM_ERR("no available proxies\n");
        return -1;
    }
    if (node->rn_ptl_supported == 0) {
        LM_ERR("required functionality is not "
          "supported by the version of the RTPproxy running on the selected "
          "node.  Please upgrade the RTPproxy and try again.\n");
        return -1;
    }
    set_rtp_inst_pvar(msg, &node->rn_url);
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
