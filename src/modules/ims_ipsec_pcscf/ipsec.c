/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Alexander Yosifov
 * Copyright (C) 2018 Tsvetomir Dimitrov
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

#include "ipsec.h"
#include "spi_gen.h"
#include "port_gen.h"

#include "../../core/dprint.h"
#include "../../core/mem/pkg.h"
#include "../../core/ip_addr.h"
#include "../../core/resolve.h"
#include "../ims_usrloc_pcscf/usrloc.h"

#include <errno.h>
#include <arpa/inet.h>
#include <libmnl/libmnl.h>
#include <linux/xfrm.h>
#include <time.h>


#define XFRM_TMPLS_BUF_SIZE 1024
#define NLMSG_BUF_SIZE 4096
#define NLMSG_DELETEALL_BUF_SIZE 8192

extern usrloc_api_t ul;

extern str ipsec_listen_addr;
extern str ipsec_listen_addr6;
extern ip_addr_t ipsec_listen_ip_addr;
extern ip_addr_t ipsec_listen_ip_addr6;
extern int xfrm_user_selector;

struct xfrm_buffer {
    char buf[NLMSG_DELETEALL_BUF_SIZE];
    int offset;
};

struct del_tunnels {
    void *contacts;
    struct xfrm_buffer delmsg_buf;
};

//
// This file contains all Linux specific IPSec code.
//

struct mnl_socket* init_mnl_socket()
{
    struct mnl_socket*  mnl_socket = mnl_socket_open(NETLINK_XFRM);
    if(NULL == mnl_socket) {
        LM_ERR("Error opening a MNL socket\n");
        return NULL;
    }

    if(mnl_socket_bind(mnl_socket, 0, MNL_SOCKET_AUTOPID) < 0) {
        LM_ERR("Error binding a MNL socket\n");
        close_mnl_socket(mnl_socket);
        return NULL;
    }

    return mnl_socket;
}

void close_mnl_socket(struct mnl_socket* sock)
{
    if(mnl_socket_close(sock) != 0) {
        LM_WARN("Error closing netlink socket\n");
    }
}

static void string_to_key(char* dst, const str key_string)
{
    int i = 0;
    char *pos = key_string.s;

    for (i = 0; i < key_string.len/2; i++) {
        sscanf(pos, "%2hhx", &dst[i]);
        pos += 2;
    }
}

static uint choose_nlmsg_seq (void)
{
    static double Tini=0;
    struct timespec ts;
    ksr_clock_gettime(&ts);
    return(1000*(ts.tv_sec - Tini + (ts.tv_nsec * 1E-9))); // us
}

int add_sa(struct mnl_socket* nl_sock, const struct ip_addr *src_addr_param, const struct ip_addr *dest_addr_param, int s_port, int d_port, int long id, str ck, str ik, str r_alg, str r_ealg)
{
    char l_msg_buf[MNL_SOCKET_BUFFER_SIZE];
    char l_auth_algo_buf[XFRM_TMPLS_BUF_SIZE];
    char l_enc_algo_buf[XFRM_TMPLS_BUF_SIZE];
    struct nlmsghdr* l_nlh = NULL;
    struct xfrm_usersa_info* l_xsainfo = NULL;

    struct xfrm_algo* l_auth_algo = NULL;
    struct xfrm_algo* l_enc_algo  = NULL;


    memset(l_msg_buf, 0, sizeof(l_msg_buf));
    memset(l_auth_algo_buf, 0, sizeof(l_auth_algo_buf));
    memset(l_enc_algo_buf, 0, sizeof(l_enc_algo_buf));

    // nlmsghdr initialization
    l_nlh = mnl_nlmsg_put_header(l_msg_buf);
    l_nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    l_nlh->nlmsg_type = XFRM_MSG_NEWSA;
    l_nlh->nlmsg_seq = choose_nlmsg_seq();
    l_nlh->nlmsg_pid = id;

    // add Security association
    l_xsainfo = (struct xfrm_usersa_info*)mnl_nlmsg_put_extra_header(l_nlh, sizeof(struct xfrm_usersa_info));
    l_xsainfo->sel.family       = dest_addr_param->af;
    if(dest_addr_param->af == AF_INET6) {
        memcpy(l_xsainfo->sel.daddr.a6, dest_addr_param->u.addr32, sizeof(l_xsainfo->sel.daddr.a6));
        memcpy(l_xsainfo->sel.saddr.a6, src_addr_param->u.addr32, sizeof(l_xsainfo->sel.saddr.a6));
        l_xsainfo->sel.prefixlen_d  = 128;
        l_xsainfo->sel.prefixlen_s  = 128;
    }
    else {
        l_xsainfo->sel.daddr.a4     = dest_addr_param->u.addr32[0];
        l_xsainfo->sel.saddr.a4     = src_addr_param->u.addr32[0];
        l_xsainfo->sel.prefixlen_d  = 32;
        l_xsainfo->sel.prefixlen_s  = 32;
    }
    l_xsainfo->sel.dport        = htons(d_port);
    l_xsainfo->sel.dport_mask   = 0xFFFF;
    l_xsainfo->sel.sport        = htons(s_port);
    l_xsainfo->sel.sport_mask   = 0xFFFF;
    l_xsainfo->sel.user         = htonl(xfrm_user_selector);

    if(dest_addr_param->af == AF_INET6) {
        memcpy(l_xsainfo->id.daddr.a6, dest_addr_param->u.addr32, sizeof(l_xsainfo->id.daddr.a6));
        memcpy(l_xsainfo->saddr.a6, src_addr_param->u.addr32, sizeof(l_xsainfo->saddr.a6));
    }
    else {
        l_xsainfo->id.daddr.a4      = dest_addr_param->u.addr32[0];
        l_xsainfo->saddr.a4         = src_addr_param->u.addr32[0];
    }
    l_xsainfo->id.spi           = htonl(id);
    l_xsainfo->id.proto         = IPPROTO_ESP;

    l_xsainfo->lft.soft_byte_limit      = XFRM_INF;
    l_xsainfo->lft.hard_byte_limit      = XFRM_INF;
    l_xsainfo->lft.soft_packet_limit    = XFRM_INF;
    l_xsainfo->lft.hard_packet_limit    = XFRM_INF;
    l_xsainfo->reqid                    = id;
    l_xsainfo->family                   = dest_addr_param->af;
    l_xsainfo->mode                     = XFRM_MODE_TRANSPORT;
    l_xsainfo->replay_window            = 32;

    // Add authentication algorithm for this SA
    // 3GPP TS 33.203 Annex I
    // NOTE: hmac-md5-96 and des-ede3-cbc has been deprecated in Rel12+

    // The cast below is performed because alg_key from struct xfrm_algo is char[0]
    // The point is to provide a continuous chunk of memory with the key in it
    l_auth_algo = (struct xfrm_algo *)l_auth_algo_buf;

    // Set the proper algorithm by r_alg str
    if(strncasecmp(r_alg.s, "hmac-md5-96", r_alg.len) == 0) {
        strcpy(l_auth_algo->alg_name,"md5");
        l_auth_algo->alg_key_len = ik.len * 4;
        string_to_key(l_auth_algo->alg_key, ik);
    } else if(strncasecmp(r_alg.s, "hmac-sha-1-96", r_alg.len) == 0) {
        strcpy(l_auth_algo->alg_name,"sha1");
        str ik1;
        ik1.len = ik.len+8;
        ik1.s = pkg_malloc (ik1.len+1);
        if (ik1.s == NULL) {
            LM_ERR("Error allocating memory\n");
            return -1;
        }
        memcpy (ik1.s,ik.s,ik.len);
        ik1.s[ik.len]=0;
        strcat (ik1.s,"00000000");
        l_auth_algo->alg_key_len = ik1.len * 4;
        string_to_key(l_auth_algo->alg_key, ik1);
        pkg_free(ik1.s);
    } else {
        LM_DBG("Creating security associations: UNKNOW Auth Algorithm\n");
        return -1;
    }


    mnl_attr_put(l_nlh, XFRMA_ALG_AUTH, sizeof(struct xfrm_algo) + l_auth_algo->alg_key_len, l_auth_algo);

    // add encription algorithm for this SA
    l_enc_algo = (struct xfrm_algo *)l_enc_algo_buf;
    // cipher_null, des,  des3_ede, aes
    if (strncasecmp(r_ealg.s,"aes-cbc",r_ealg.len) == 0) {
        strcpy(l_enc_algo->alg_name,"aes");
        l_enc_algo->alg_key_len = ck.len * 4;
        string_to_key(l_enc_algo->alg_key, ck);
    } else if (strncasecmp(r_ealg.s,"des-ede3-cbc",r_ealg.len) == 0) {
        strcpy(l_enc_algo->alg_name,"des3_ede");
        str ck1;
        ck1.len = ck.len+ck.len/2;
        ck1.s = pkg_malloc (ck1.len+1);
        if (ck1.s == NULL) {
            LM_ERR("Error allocating memory\n");
            return -1;
        }
        memcpy (ck1.s,ck.s,ck.len);
        memcpy (ck1.s+ck.len,ck.s,ck.len/2);
        l_enc_algo->alg_key_len = ck1.len * 4;
        string_to_key(l_enc_algo->alg_key, ck1);
        pkg_free(ck1.s);
    } else if (strncasecmp(r_ealg.s,"null",r_ealg.len) == 0) {
        strcpy(l_enc_algo->alg_name,"cipher_null");
        l_enc_algo->alg_key_len = 0;
    } else {
        LM_DBG("Creating security associations: UNKNOW Enc Algorithm\n");
        return -1;
    }

    mnl_attr_put(l_nlh, XFRMA_ALG_CRYPT, sizeof(struct xfrm_algo) + l_enc_algo->alg_key_len, l_enc_algo);

    // send it to Netlink socket
    if(mnl_socket_sendto(nl_sock, l_nlh, l_nlh->nlmsg_len) < 0)
    {
        LM_ERR("Failed to send Netlink message for SA creation, error: %s\n", strerror(errno));
        return -3;
    }

    return 0;
}


int remove_sa(struct mnl_socket* nl_sock, str src_addr_param, str dest_addr_param, int s_port, int d_port, int long id, unsigned int af)
{
    char* src_addr = NULL;
    char* dest_addr = NULL;

    // convert input IP addresses to char*
    if((src_addr = pkg_malloc(src_addr_param.len+1)) == NULL) {
        LM_ERR("Error allocating memory for src addr during SA removal\n");
        return -1;
    }

    if((dest_addr = pkg_malloc(dest_addr_param.len+1)) == NULL) {
        pkg_free(src_addr);
        LM_ERR("Error allocating memory for dest addr during SA removal\n");
        return -2;
    }

    memset(src_addr, 0, src_addr_param.len+1);
    memset(dest_addr, 0, dest_addr_param.len+1);

    memcpy(src_addr, src_addr_param.s, src_addr_param.len);
    memcpy(dest_addr, dest_addr_param.s, dest_addr_param.len);

    struct {
        struct nlmsghdr n;
        struct xfrm_usersa_id   xsid;
        char buf[XFRM_TMPLS_BUF_SIZE];

    } req = {
        .n.nlmsg_len    = NLMSG_LENGTH(sizeof(req.xsid)),
        .n.nlmsg_flags  = NLM_F_REQUEST,
        .n.nlmsg_type   = XFRM_MSG_DELSA,
        .n.nlmsg_pid    = id,
        .xsid.spi       = htonl(id),
        .xsid.family    = af,
        .xsid.proto     = IPPROTO_ESP
    };

    xfrm_address_t saddr;
    memset(&saddr, 0, sizeof(saddr));

    if(af == AF_INET6){
        ip_addr_t ip_addr;

        if(str2ipxbuf(&dest_addr_param, &ip_addr) < 0){
            LM_ERR("Unable to convert dest address [%.*s]\n", dest_addr_param.len, dest_addr_param.s);
            pkg_free(src_addr);
            pkg_free(dest_addr);
            return -1;
        }
        memcpy(req.xsid.daddr.a6, ip_addr.u.addr32, sizeof(req.xsid.daddr.a6));

        memset(&ip_addr, 0, sizeof(ip_addr_t));
        if(str2ipxbuf(&src_addr_param, &ip_addr) < 0){
            LM_ERR("Unable to convert src address [%.*s]\n", src_addr_param.len, src_addr_param.s);
            pkg_free(src_addr);
            pkg_free(dest_addr);
            return -1;
        }
        memcpy(saddr.a6, ip_addr.u.addr32, sizeof(saddr.a6));
    }else{
        req.xsid.daddr.a4   = inet_addr(dest_addr);
        saddr.a4            = inet_addr(src_addr);
    }

    mnl_attr_put(&req.n, XFRMA_SRCADDR, sizeof(saddr), (void *)&saddr);

    if(mnl_socket_sendto(nl_sock, &req.n, req.n.nlmsg_len) < 0)
    {
        LM_ERR("Failed to send Netlink message, error: %s\n", strerror(errno));
        pkg_free(src_addr);
        pkg_free(dest_addr);
        return -1;
    }

    pkg_free(src_addr);
    pkg_free(dest_addr);

    return 0;
}


int add_policy(struct mnl_socket* mnl_socket, const struct ip_addr *src_addr_param, const struct ip_addr *dest_addr_param, int src_port, int dst_port, int long p_id, enum ipsec_policy_direction dir)
{
    char                            l_msg_buf[MNL_SOCKET_BUFFER_SIZE];
    char                            l_tmpls_buf[XFRM_TMPLS_BUF_SIZE];
    struct nlmsghdr*                l_nlh;
    struct xfrm_userpolicy_info*    l_xpinfo;

    memset(l_msg_buf, 0, sizeof(l_msg_buf));
    memset(l_tmpls_buf, 0, sizeof(l_tmpls_buf));

    // nlmsghdr initialization
    l_nlh = mnl_nlmsg_put_header(l_msg_buf);
    l_nlh->nlmsg_flags  = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    l_nlh->nlmsg_type   = XFRM_MSG_NEWPOLICY;
    l_nlh->nlmsg_seq    = time(NULL);
    l_nlh->nlmsg_pid    = p_id;

    // add OUT policy
    l_xpinfo = (struct xfrm_userpolicy_info*)mnl_nlmsg_put_extra_header(l_nlh, sizeof(struct xfrm_userpolicy_info));
    l_xpinfo->sel.family        = dest_addr_param->af;
    if(dest_addr_param->af == AF_INET6) {
        memcpy(l_xpinfo->sel.daddr.a6, dest_addr_param->u.addr32, sizeof(l_xpinfo->sel.daddr.a6));
        memcpy(l_xpinfo->sel.saddr.a6, src_addr_param->u.addr32, sizeof(l_xpinfo->sel.saddr.a6));
        l_xpinfo->sel.prefixlen_d  = 128;
        l_xpinfo->sel.prefixlen_s  = 128;
    }
    else {
        l_xpinfo->sel.daddr.a4     = dest_addr_param->u.addr32[0];
        l_xpinfo->sel.saddr.a4     = src_addr_param->u.addr32[0];
        l_xpinfo->sel.prefixlen_d  = 32;
        l_xpinfo->sel.prefixlen_s  = 32;
    }
    l_xpinfo->sel.dport         = htons(dst_port);
    l_xpinfo->sel.dport_mask    = 0xFFFF;
    l_xpinfo->sel.sport         = htons(src_port);
    l_xpinfo->sel.sport_mask    = 0xFFFF;
    //l_xpinfo->sel.proto         = sel_proto;
    l_xpinfo->sel.user          = htonl(xfrm_user_selector);

    l_xpinfo->lft.soft_byte_limit   = XFRM_INF;
    l_xpinfo->lft.hard_byte_limit   = XFRM_INF;
    l_xpinfo->lft.soft_packet_limit = XFRM_INF;
    l_xpinfo->lft.hard_packet_limit = XFRM_INF;
    l_xpinfo->priority              = 2080;
    l_xpinfo->action                = XFRM_POLICY_ALLOW;
    l_xpinfo->share                 = XFRM_SHARE_ANY;

    if (dir == IPSEC_POLICY_DIRECTION_IN) {
        l_xpinfo->dir               = XFRM_POLICY_IN;
    }
    else if(dir == IPSEC_POLICY_DIRECTION_OUT) {
        l_xpinfo->dir               = XFRM_POLICY_OUT;
    }
    else {
        LM_ERR("Invalid direction parameter passed to add_policy: %d\n", dir);

        return -3;
    }

    // xfrm_user_tmpl initialization
    struct xfrm_user_tmpl* l_tmpl = (struct xfrm_user_tmpl*)l_tmpls_buf;
    l_tmpl->id.proto    = IPPROTO_ESP;
    l_tmpl->family      = dest_addr_param->af;
    if(dest_addr_param->af == AF_INET6) {
        memcpy(l_tmpl->id.daddr.a6, dest_addr_param->u.addr32, sizeof(l_tmpl->id.daddr.a6));
        memcpy(l_tmpl->saddr.a6, src_addr_param->u.addr32, sizeof(l_tmpl->saddr.a6));
    }
    else {
        l_tmpl->id.daddr.a4        = dest_addr_param->u.addr32[0];
        l_tmpl->saddr.a4           = src_addr_param->u.addr32[0];
    }
    l_tmpl->reqid       = p_id;
    l_tmpl->mode        = XFRM_MODE_TRANSPORT;
    l_tmpl->aalgos      = (~(__u32)0);
    l_tmpl->ealgos      = (~(__u32)0);
    l_tmpl->calgos      = (~(__u32)0);

    mnl_attr_put(l_nlh, XFRMA_TMPL, sizeof(struct xfrm_user_tmpl), l_tmpl);

    if(mnl_socket_sendto(mnl_socket, l_nlh, l_nlh->nlmsg_len) < 0)
    {
        LM_ERR("Failed to send Netlink message, error: %s\n", strerror(errno));
        return -4;
    }

    return 0;
}

int remove_policy(struct mnl_socket* mnl_socket, str src_addr_param, str dest_addr_param, int src_port, int dst_port, int long p_id, unsigned int af, enum ipsec_policy_direction dir)
{
    unsigned char policy_dir = 0;

    if(dir == IPSEC_POLICY_DIRECTION_IN) {
         policy_dir = XFRM_POLICY_IN;
    }
    else if(dir == IPSEC_POLICY_DIRECTION_OUT) {
         policy_dir = XFRM_POLICY_OUT;
    }
    else {
        LM_ERR("Invalid direction parameter passed to remove_policy: %d\n", dir);
        return -1;
    }

    char* src_addr = NULL;
    char* dest_addr = NULL;

    // convert input IP addresses to char*
    if((src_addr = pkg_malloc(src_addr_param.len+1)) == NULL) {
        LM_ERR("Error allocating memory for src addr during SA removal\n");
        return -1;
    }

    if((dest_addr = pkg_malloc(dest_addr_param.len+1)) == NULL) {
        pkg_free(src_addr);
        LM_ERR("Error allocating memory for dest addr during SA removal\n");
        return -2;
    }

    memset(src_addr, 0, src_addr_param.len+1);
    memset(dest_addr, 0, dest_addr_param.len+1);

    memcpy(src_addr, src_addr_param.s, src_addr_param.len);
    memcpy(dest_addr, dest_addr_param.s, dest_addr_param.len);

    struct {
        struct nlmsghdr n;
        struct xfrm_userpolicy_id xpid;
        char buf[XFRM_TMPLS_BUF_SIZE];
    } req = {
        .n.nlmsg_len            = NLMSG_LENGTH(sizeof(req.xpid)),
        .n.nlmsg_flags          = NLM_F_REQUEST,
        .n.nlmsg_type           = XFRM_MSG_DELPOLICY,
        .n.nlmsg_pid            = p_id,
        .xpid.dir               = policy_dir,
        .xpid.sel.family        = af,
        .xpid.sel.dport         = htons(dst_port),
        .xpid.sel.dport_mask    = 0xFFFF,
        .xpid.sel.sport         = htons(src_port),
        .xpid.sel.sport_mask    = 0xFFFF,
        .xpid.sel.user          = htonl(xfrm_user_selector)
        //.xpid.sel.proto         = sel_proto
    };

    if(af == AF_INET6){
        ip_addr_t ip_addr;

        if(str2ipxbuf(&dest_addr_param, &ip_addr) < 0){
            LM_ERR("Unable to convert dest address [%.*s]\n", dest_addr_param.len, dest_addr_param.s);
            pkg_free(src_addr);
            pkg_free(dest_addr);
            return -1;
        }
        memcpy(req.xpid.sel.daddr.a6, ip_addr.u.addr32, sizeof(req.xpid.sel.daddr.a6));

        if(str2ipxbuf(&src_addr_param, &ip_addr) < 0){
            LM_ERR("Unable to convert src address [%.*s]\n", src_addr_param.len, src_addr_param.s);
            pkg_free(src_addr);
            pkg_free(dest_addr);
            return -1;
        }
        memcpy(req.xpid.sel.saddr.a6, ip_addr.u.addr32, sizeof(req.xpid.sel.saddr.a6));

        req.xpid.sel.prefixlen_d = 128;
        req.xpid.sel.prefixlen_s = 128;
    }else{
        req.xpid.sel.daddr.a4       = inet_addr(dest_addr);
        req.xpid.sel.saddr.a4       = inet_addr(src_addr);

        req.xpid.sel.prefixlen_d    = 32;
        req.xpid.sel.prefixlen_s    = 32;
    }

    if(mnl_socket_sendto(mnl_socket, &req.n, req.n.nlmsg_len) < 0)
    {
        LM_ERR("Failed to send Netlink message, error: %s\n", strerror(errno));
        pkg_free(src_addr);
        pkg_free(dest_addr);
        return -1;
    }

    pkg_free(src_addr);
    pkg_free(dest_addr);

    return 0;
}

static int delsa_data_cb(const struct nlmsghdr *nlh, void *data)
{
    struct xfrm_usersa_info *xsinfo = NLMSG_DATA(nlh);
    int xfrm_userid = ntohl(xsinfo->sel.user);

    //Check if user id is different from Kamailio's
    if(xfrm_userid != xfrm_user_selector)
        return MNL_CB_OK;

    struct xfrm_buffer* delmsg_buf = (struct xfrm_buffer*)data;
    uint32_t new_delmsg_len = NLMSG_LENGTH(sizeof(struct xfrm_usersa_id));

    if(delmsg_buf->offset + new_delmsg_len > sizeof(delmsg_buf->buf)/sizeof(delmsg_buf->buf[0])) {
        LM_ERR("Not enough memory allocated for delete SAs netlink command\n");
        return MNL_CB_ERROR;
    }

    struct nlmsghdr *new_delmsg = (struct nlmsghdr *)&delmsg_buf->buf[delmsg_buf->offset];
    new_delmsg->nlmsg_len = new_delmsg_len;
    new_delmsg->nlmsg_flags = NLM_F_REQUEST;
    new_delmsg->nlmsg_type = XFRM_MSG_DELSA;
    new_delmsg->nlmsg_seq = time(NULL);

    struct xfrm_usersa_id *xsid = NLMSG_DATA(new_delmsg);
    xsid->family = xsinfo->family;
    memcpy(&xsid->daddr, &xsinfo->id.daddr, sizeof(xsid->daddr));
    xsid->spi = xsinfo->id.spi;
    xsid->proto = xsinfo->id.proto;

    mnl_attr_put(new_delmsg, XFRMA_SRCADDR, sizeof(xsid->daddr), &xsinfo->saddr);

    delmsg_buf->offset += new_delmsg->nlmsg_len;

    return MNL_CB_OK;
}

static int delpolicy_data_cb(const struct nlmsghdr *nlh, void *data)
{
    struct xfrm_userpolicy_info *xpinfo = NLMSG_DATA(nlh);
    int xfrm_userid = ntohl(xpinfo->sel.user);

    //Check if user id is different from Kamailio's
    if(xfrm_userid != xfrm_user_selector)
        return MNL_CB_OK;

    struct xfrm_buffer* delmsg_buf = (struct xfrm_buffer*)data;
    uint32_t new_delmsg_len = NLMSG_LENGTH(sizeof(struct xfrm_userpolicy_id));

    if(delmsg_buf->offset + new_delmsg_len > sizeof(delmsg_buf->buf)/sizeof(delmsg_buf->buf[0])) {
        LM_ERR("Not enough memory allocated for delete policies netlink command\n");
        return MNL_CB_ERROR;
    }

    struct nlmsghdr *new_delmsg = (struct nlmsghdr *)&delmsg_buf->buf[delmsg_buf->offset];
    new_delmsg->nlmsg_len = new_delmsg_len;
    new_delmsg->nlmsg_flags = NLM_F_REQUEST;
    new_delmsg->nlmsg_type = XFRM_MSG_DELPOLICY;
    new_delmsg->nlmsg_seq = time(NULL);

    struct xfrm_userpolicy_id *xpid = NLMSG_DATA(new_delmsg);
    memcpy(&xpid->sel, &xpinfo->sel, sizeof(xpid->sel));
    xpid->dir = xpinfo->dir;
    xpid->index = xpinfo->index;

    delmsg_buf->offset += new_delmsg->nlmsg_len;

    return MNL_CB_OK;
}

int clean_sa(struct mnl_socket*  mnl_socket)
{
    struct {
        struct nlmsghdr n;
        //char buf[NLMSG_DELETEALL_BUF_SIZE];
    } req = {
        .n.nlmsg_len = NLMSG_HDRLEN,
        .n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
        .n.nlmsg_type = XFRM_MSG_GETSA,
        .n.nlmsg_seq = time(NULL),
    };

    if(mnl_socket_sendto(mnl_socket, &req, req.n.nlmsg_len) == -1) {
        LM_ERR("Error sending get all SAs command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    char buf[NLMSG_DELETEALL_BUF_SIZE];
    memset(&buf, 0, sizeof(buf));

    struct xfrm_buffer delmsg_buf;
    memset(&delmsg_buf, 0, sizeof(struct xfrm_buffer));

    int ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, req.n.nlmsg_seq, mnl_socket_get_portid(mnl_socket), delsa_data_cb, &delmsg_buf);
        if (ret <= MNL_CB_STOP) {

            break;
        }
        ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    }

    // DELETE SAs
    if(mnl_socket_sendto(mnl_socket, &delmsg_buf.buf, delmsg_buf.offset) == -1) {
        LM_ERR("Error sending delete SAs command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

int clean_policy(struct mnl_socket*  mnl_socket)
{
    struct {
        struct nlmsghdr n;
        //char buf[NLMSG_DELETEALL_BUF_SIZE];
    } req = {
        .n.nlmsg_len = NLMSG_HDRLEN,
        .n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
        .n.nlmsg_type = XFRM_MSG_GETPOLICY,
        .n.nlmsg_seq = time(NULL),
    };

    if(mnl_socket_sendto(mnl_socket, &req, req.n.nlmsg_len) == -1) {
        LM_ERR("Error sending get all policies command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    char buf[NLMSG_DELETEALL_BUF_SIZE];
    memset(&buf, 0, sizeof(buf));

    struct xfrm_buffer delmsg_buf;
    memset(&delmsg_buf, 0, sizeof(struct xfrm_buffer));

    int ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, req.n.nlmsg_seq, mnl_socket_get_portid(mnl_socket), delpolicy_data_cb, &delmsg_buf);
        if (ret <= MNL_CB_STOP) {

            break;
        }
        ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    }

    // DELETE POLICIES
    if(mnl_socket_sendto(mnl_socket, &delmsg_buf.buf, delmsg_buf.offset) == -1) {
        LM_ERR("Error sending delete policies command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

static int delete_unused_sa_cb(const struct nlmsghdr *nlh, void *data)
{
    struct xfrm_usersa_info *xsinfo = NLMSG_DATA(nlh);

    // Check if user id is different from Kamailio's
    if (ntohl(xsinfo->sel.user) != xfrm_user_selector) {
        return MNL_CB_OK;
    }

    ip_addr_t sa_src_addr, sa_dst_addr;
    unsigned int sa_spi;
    unsigned short sa_sport, sa_dport;

    ip_addr_t *proxy_ip_addr = NULL;

    sa_src_addr.af = sa_dst_addr.af = xsinfo->sel.family;
    if(xsinfo->sel.family == AF_INET6) {
        memcpy(sa_src_addr.u.addr32, xsinfo->sel.saddr.a6, sizeof(xsinfo->sel.saddr.a6));
        memcpy(sa_dst_addr.u.addr32, xsinfo->sel.daddr.a6, sizeof(xsinfo->sel.daddr.a6));
        sa_src_addr.len = sa_dst_addr.len = 16;

        proxy_ip_addr = &ipsec_listen_ip_addr6;
    } else if(xsinfo->sel.family == AF_INET) {
        sa_src_addr.u.addr32[0] = xsinfo->sel.saddr.a4;
        sa_dst_addr.u.addr32[0] = xsinfo->sel.daddr.a4;
        sa_src_addr.len = sa_dst_addr.len = 4;

        proxy_ip_addr = &ipsec_listen_ip_addr;
    } else {
        LM_CRIT("Unknown AF %d\n", xsinfo->sel.family);
        return MNL_CB_OK;
    }

    sa_spi = htonl(xsinfo->id.spi);
    sa_sport = htons(xsinfo->sel.sport);
    sa_dport = htons(xsinfo->sel.dport);

    char saddr_buf[IP_ADDR_MAX_STRZ_SIZE];
    char daddr_buf[IP_ADDR_MAX_STRZ_SIZE];
    memset(saddr_buf, 0, IP_ADDR_MAX_STRZ_SIZE);
    memset(daddr_buf, 0, IP_ADDR_MAX_STRZ_SIZE);
    ip_addr2sbufz(&sa_src_addr, saddr_buf, IP_ADDR_MAX_STRZ_SIZE);
    ip_addr2sbufz(&sa_dst_addr, daddr_buf, IP_ADDR_MAX_STRZ_SIZE);

//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |.............................................................................................|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |0000|
//  * +----+
    void  *cp;
    str received_host;
    ip_addr_t received_host_addr;
    ipsec_t ipsec = {0};

    cp = ((struct del_tunnels*)data)->contacts;
    while (1) {
        memcpy(&(received_host.len), cp, sizeof(received_host.len));
        if (received_host.len == 0) {
            break; // no more records
        }

        received_host.s = (char*)cp + sizeof(received_host.len);
        cp = (char*)cp + sizeof(received_host.len) + received_host.len;

        memcpy(&ipsec.spi_uc, cp, sizeof(ipsec.spi_uc));
        cp = (char*)cp + sizeof(ipsec.spi_uc);

        memcpy(&ipsec.spi_us, cp, sizeof(ipsec.spi_us));
        cp = (char*)cp + sizeof(ipsec.spi_us);

        memcpy(&ipsec.spi_pc, cp, sizeof(ipsec.spi_pc));
        cp = (char*)cp + sizeof(ipsec.spi_pc);

        memcpy(&ipsec.spi_ps, cp, sizeof(ipsec.spi_ps));
        cp = (char*)cp + sizeof(ipsec.spi_ps);

        memcpy(&ipsec.port_uc, cp, sizeof(ipsec.port_uc));
        cp = (char*)cp + sizeof(ipsec.port_uc);

        memcpy(&ipsec.port_us, cp, sizeof(ipsec.port_us));
        cp = (char*)cp + sizeof(ipsec.port_us);

        memcpy(&ipsec.port_pc, cp, sizeof(ipsec.port_pc));
        cp = (char*)cp + sizeof(ipsec.port_pc);

        memcpy(&ipsec.port_ps, cp, sizeof(ipsec.port_ps));
        cp = (char*)cp + sizeof(ipsec.port_ps);

        // convert 'received host' ip string to ip_addr_t
        if (str2ipxbuf(&received_host, &received_host_addr) < 0){
            LM_ERR("Unable to convert received host [%.*s]\n", received_host.len, received_host.s);
            return 1;
        }

        // SA  Src address  Dst address Src port    Dst port        SPI
        // SA1 UE-IP        Proxy-IP    UE-Server   Proxy-Client    Proxy-Client
        // SA2 UE-IP        Proxy-IP    UE-Client   Proxy-Server    Proxy-Server
        // SA3 Proxy-IP     UE-IP       P-Server    UE-Client       UE-Client
        // SA4 Proxy-IP     UE-IP       P-Client    UE-Server       UE-Server

        // Check for SA1 or SA2
        if(ip_addr_cmp(&sa_src_addr, &received_host_addr) && ip_addr_cmp(&sa_dst_addr, proxy_ip_addr)) {
            // check for SA1
            if(sa_sport == ipsec.port_us && sa_dport == ipsec.port_pc && sa_spi == ipsec.spi_pc){
                return MNL_CB_OK;
            }

            // check for SA2
            if(sa_sport == ipsec.port_uc && sa_dport == ipsec.port_ps && sa_spi == ipsec.spi_ps){
                return MNL_CB_OK;
            }
        }

        // Check for SA3 or SA4
        if(ip_addr_cmp(&sa_src_addr, proxy_ip_addr) && ip_addr_cmp(&sa_dst_addr, &received_host_addr)) {
            // check for SA3
            if(sa_sport == ipsec.port_ps && sa_dport == ipsec.port_uc && sa_spi == ipsec.spi_uc){
                return MNL_CB_OK;
            }

            // check for SA4
            if(sa_sport == ipsec.port_pc && sa_dport == ipsec.port_us && sa_spi == ipsec.spi_us){
                return MNL_CB_OK;
            }
        }
    }

    LM_DBG("The SA is not used and will be deleted: spi:%u | saddr:%s:%u | daddr:%s:%u\n", sa_spi, saddr_buf, sa_sport, daddr_buf, sa_dport);

    struct xfrm_buffer* delmsg_buf = &((struct del_tunnels*)data)->delmsg_buf;
    uint32_t new_delmsg_len = NLMSG_LENGTH(sizeof(struct xfrm_usersa_id));

    if(delmsg_buf->offset + new_delmsg_len > sizeof(delmsg_buf->buf)/sizeof(delmsg_buf->buf[0])) {
        LM_ERR("Not enough memory allocated for delete SAs netlink command\n");
        return MNL_CB_ERROR;
    }

    struct nlmsghdr *new_delmsg = (struct nlmsghdr *)&delmsg_buf->buf[delmsg_buf->offset];
    new_delmsg->nlmsg_len = new_delmsg_len;
    new_delmsg->nlmsg_flags = NLM_F_REQUEST;
    new_delmsg->nlmsg_type = XFRM_MSG_DELSA;
    new_delmsg->nlmsg_seq = time(NULL);

    struct xfrm_usersa_id *xsid = NLMSG_DATA(new_delmsg);
    xsid->family = xsinfo->family;
    memcpy(&xsid->daddr, &xsinfo->id.daddr, sizeof(xsid->daddr));
    xsid->spi = xsinfo->id.spi;
    xsid->proto = xsinfo->id.proto;

    mnl_attr_put(new_delmsg, XFRMA_SRCADDR, sizeof(xsid->daddr), &xsinfo->saddr);

    delmsg_buf->offset += new_delmsg->nlmsg_len;

    // NOTE: Release the Proxy SPIs and Ports only here. Do not release the same SPIs and ports in delete unsused policy callback.
    // Release SPIs
    release_spi(ipsec.spi_pc);
    release_spi(ipsec.spi_ps);

    // Release the client and the server ports
    release_cport(ipsec.port_pc);
    release_sport(ipsec.port_ps);

    return MNL_CB_OK;
}

static int delete_unused_sa(struct mnl_socket *mnl_socket, void *contacts)
{
    struct {
        struct nlmsghdr n;
        //char buf[NLMSG_DELETEALL_BUF_SIZE];
    } req = {
        .n.nlmsg_len = NLMSG_HDRLEN,
        .n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
        .n.nlmsg_type = XFRM_MSG_GETSA,
        .n.nlmsg_seq = time(NULL),
    };

    if(mnl_socket_sendto(mnl_socket, &req, req.n.nlmsg_len) == -1) {
        LM_ERR("Error sending get all SAs command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    char buf[NLMSG_DELETEALL_BUF_SIZE];
    memset(&buf, 0, sizeof(buf));

    struct del_tunnels del_data;
    del_data.contacts = contacts;
    memset(&del_data.delmsg_buf, 0, sizeof(del_data.delmsg_buf));

    int ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, req.n.nlmsg_seq, mnl_socket_get_portid(mnl_socket), delete_unused_sa_cb, &del_data);
        if (ret <= MNL_CB_STOP) {
            break;
        }

        ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    }

    // DELETE unused SAs
    if(mnl_socket_sendto(mnl_socket, &del_data.delmsg_buf.buf, del_data.delmsg_buf.offset) == -1) {
        LM_ERR("Error sending delete unused SAs command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

static int delete_unused_policy_cb(const struct nlmsghdr *nlh, void *data)
{
    struct xfrm_userpolicy_info *xpinfo = NLMSG_DATA(nlh);

    //Check if user id is different from Kamailio's
    if (ntohl(xpinfo->sel.user) != xfrm_user_selector) {
        return MNL_CB_OK;
    }

    ip_addr_t sa_src_addr, sa_dst_addr;
    unsigned short sa_sport, sa_dport;

    ip_addr_t *proxy_ip_addr = NULL;

    sa_src_addr.af = sa_dst_addr.af = xpinfo->sel.family;
    if(xpinfo->sel.family == AF_INET6) {
        memcpy(sa_src_addr.u.addr32, xpinfo->sel.saddr.a6, sizeof(xpinfo->sel.saddr.a6));
        memcpy(sa_dst_addr.u.addr32, xpinfo->sel.daddr.a6, sizeof(xpinfo->sel.daddr.a6));
        sa_src_addr.len = sa_dst_addr.len = 16;

        proxy_ip_addr = &ipsec_listen_ip_addr6;
    } else if(xpinfo->sel.family == AF_INET) {
        sa_src_addr.u.addr32[0] = xpinfo->sel.saddr.a4;
        sa_dst_addr.u.addr32[0] = xpinfo->sel.daddr.a4;
        sa_src_addr.len = sa_dst_addr.len = 4;

        proxy_ip_addr = &ipsec_listen_ip_addr;
    } else {
        LM_CRIT("Unknown AF %d\n", xpinfo->sel.family);
        return MNL_CB_OK;
    }

    sa_sport = htons(xpinfo->sel.sport);
    sa_dport = htons(xpinfo->sel.dport);

    char saddr_buf[IP_ADDR_MAX_STRZ_SIZE];
    char daddr_buf[IP_ADDR_MAX_STRZ_SIZE];
    memset(saddr_buf, 0, IP_ADDR_MAX_STRZ_SIZE);
    memset(daddr_buf, 0, IP_ADDR_MAX_STRZ_SIZE);
    ip_addr2sbufz(&sa_src_addr, saddr_buf, IP_ADDR_MAX_STRZ_SIZE);
    ip_addr2sbufz(&sa_dst_addr, daddr_buf, IP_ADDR_MAX_STRZ_SIZE);

//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |.............................................................................................|
//  * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
//  * |0000|
//  * +----+
    void  *cp;
    str received_host;
    ip_addr_t received_host_addr;
    ipsec_t ipsec;

    cp = ((struct del_tunnels*)data)->contacts;
    while (1) {
        memcpy(&(received_host.len), cp, sizeof(received_host.len));
        if (received_host.len == 0) {
            break; // no more records
        }

        received_host.s = (char*)cp + sizeof(received_host.len);
        cp = (char*)cp + sizeof(received_host.len) + received_host.len;

        memcpy(&ipsec.spi_uc, cp, sizeof(ipsec.spi_uc));
        cp = (char*)cp + sizeof(ipsec.spi_uc);

        memcpy(&ipsec.spi_us, cp, sizeof(ipsec.spi_us));
        cp = (char*)cp + sizeof(ipsec.spi_us);

        memcpy(&ipsec.spi_pc, cp, sizeof(ipsec.spi_pc));
        cp = (char*)cp + sizeof(ipsec.spi_pc);

        memcpy(&ipsec.spi_ps, cp, sizeof(ipsec.spi_ps));
        cp = (char*)cp + sizeof(ipsec.spi_ps);

        memcpy(&ipsec.port_uc, cp, sizeof(ipsec.port_uc));
        cp = (char*)cp + sizeof(ipsec.port_uc);

        memcpy(&ipsec.port_us, cp, sizeof(ipsec.port_us));
        cp = (char*)cp + sizeof(ipsec.port_us);

        memcpy(&ipsec.port_pc, cp, sizeof(ipsec.port_pc));
        cp = (char*)cp + sizeof(ipsec.port_pc);

        memcpy(&ipsec.port_ps, cp, sizeof(ipsec.port_ps));
        cp = (char*)cp + sizeof(ipsec.port_ps);

        // convert 'received host' ip string to ip_addr_t
        if (str2ipxbuf(&received_host, &received_host_addr) < 0){
            LM_ERR("Unable to convert received host [%.*s]\n", received_host.len, received_host.s);
            return 1;
        }

        // Policy  Src address  Dst address Src port    Dst port
        // Policy1 UE-IP        Proxy-IP    UE-Server   Proxy-Client
        // Policy2 UE-IP        Proxy-IP    UE-Client   Proxy-Server
        // Policy3 Proxy-IP     UE-IP       P-Server    UE-Client
        // Policy4 Proxy-IP     UE-IP       P-Client    UE-Server

        // Check for Policy1 or Policy2
        if(ip_addr_cmp(&sa_src_addr, &received_host_addr) && ip_addr_cmp(&sa_dst_addr, proxy_ip_addr)) {
            // check for SA1
            if(sa_sport == ipsec.port_us && sa_dport == ipsec.port_pc){
                return MNL_CB_OK;
            }

            // check for SA2
            if(sa_sport == ipsec.port_uc && sa_dport == ipsec.port_ps){
                return MNL_CB_OK;
            }
        }

        // Check for Policy3 or Policy4
        if(ip_addr_cmp(&sa_src_addr, proxy_ip_addr) && ip_addr_cmp(&sa_dst_addr, &received_host_addr)) {
            // check for SA3
            if(sa_sport == ipsec.port_ps && sa_dport == ipsec.port_uc){
                return MNL_CB_OK;
            }

            // check for SA4
            if(sa_sport == ipsec.port_pc && sa_dport == ipsec.port_us){
                return MNL_CB_OK;
            }
        }
    }

    LM_DBG("The POLICY is not used and will be deleted: saddr:%s:%u | daddr:%s:%u\n", saddr_buf, sa_sport, daddr_buf, sa_dport);

    struct xfrm_buffer* delmsg_buf = &((struct del_tunnels*)data)->delmsg_buf;
    uint32_t new_delmsg_len = NLMSG_LENGTH(sizeof(struct xfrm_userpolicy_id));

    if(delmsg_buf->offset + new_delmsg_len > sizeof(delmsg_buf->buf)/sizeof(delmsg_buf->buf[0])) {
        LM_ERR("Not enough memory allocated for delete policies netlink command\n");
        return MNL_CB_ERROR;
    }

    struct nlmsghdr *new_delmsg = (struct nlmsghdr *)&delmsg_buf->buf[delmsg_buf->offset];
    new_delmsg->nlmsg_len = new_delmsg_len;
    new_delmsg->nlmsg_flags = NLM_F_REQUEST;
    new_delmsg->nlmsg_type = XFRM_MSG_DELPOLICY;
    new_delmsg->nlmsg_seq = time(NULL);

    struct xfrm_userpolicy_id *xpid = NLMSG_DATA(new_delmsg);
    memcpy(&xpid->sel, &xpinfo->sel, sizeof(xpid->sel));
    xpid->dir = xpinfo->dir;
    xpid->index = xpinfo->index;

    delmsg_buf->offset += new_delmsg->nlmsg_len;

    return MNL_CB_OK;
}

static int delete_unused_policy(struct mnl_socket *mnl_socket, void *contacts)
{
    struct {
        struct nlmsghdr n;
        //char buf[NLMSG_DELETEALL_BUF_SIZE];
    } req = {
        .n.nlmsg_len = NLMSG_HDRLEN,
        .n.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
        .n.nlmsg_type = XFRM_MSG_GETPOLICY,
        .n.nlmsg_seq = time(NULL),
    };

    if(mnl_socket_sendto(mnl_socket, &req, req.n.nlmsg_len) == -1) {
        LM_ERR("Error sending get all POLICY command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    char buf[NLMSG_DELETEALL_BUF_SIZE];
    memset(&buf, 0, sizeof(buf));

    struct del_tunnels del_data;
    del_data.contacts = contacts;
    memset(&del_data.delmsg_buf, 0, sizeof(del_data.delmsg_buf));

    int ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, req.n.nlmsg_seq, mnl_socket_get_portid(mnl_socket), delete_unused_policy_cb, &del_data);
        if (ret <= MNL_CB_STOP) {
            break;
        }

        ret = mnl_socket_recvfrom(mnl_socket, buf, sizeof(buf));
    }

    // DELETE unused POLICIES
    if(mnl_socket_sendto(mnl_socket, &del_data.delmsg_buf.buf, del_data.delmsg_buf.offset) == -1) {
        LM_ERR("Error sending delete unused policies command via netlink socket: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

int delete_unused_tunnels()
{
    int rval, len = 0;
    void *buf = NULL;

    // first try to fetch required size for all contacts
    rval = ul.get_all_ucontacts(buf, len, 0, 0, 1);
    LM_DBG("Minimum required size %d\n", rval);
    if (rval < 0) {
        LM_ERR("Failed to fetch contacts\n");
        return 1;
    }

    if (rval > 0) {
        len = rval * 2;
        buf = malloc(len);
        if (buf == NULL) {
            LM_ERR("Out of memory\n");
            return 1;
        }

        rval = ul.get_all_ucontacts(buf, len, 0, 0, 1);
        if (rval != 0) {
            free(buf);
            return 1;
        }
    }
    if (buf == NULL) {
        return 1;
    }

    struct mnl_socket* sock = init_mnl_socket();
    if (sock == NULL) {
        LM_ERR("Can't init mnl socket\n");
        free(buf);
        return 1;
    }

    delete_unused_sa(sock, buf);
    delete_unused_policy(sock, buf);

    close_mnl_socket(sock);

    free(buf);
    return 0;
}
