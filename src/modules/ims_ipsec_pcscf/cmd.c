/*
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "../../core/parser/msg_parser.h"
#include "../../core/socket_info.h"
#include "../../lib/ims/ims_getters.h"
#include "../../modules/tm/tm_load.h"
#include "../ims_usrloc_pcscf/usrloc.h"

#include "ipsec.h"
#include "spi_gen.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>


extern str ipsec_listen_addr;
extern short ipsec_listen_port;
extern short ipsec_server_port;
extern short ipsec_client_port;

// check http://www.asipto.com/pub/kamailio-devel-guide/#c16return_values
const int IPSEC_CMD_FAIL = -1;
const int IPSEC_CMD_SUCCESS = 1;

extern usrloc_api_t ul;
extern struct tm_binds tmb;


static str get_www_auth_param(const char* param_name, str www_auth)
{
    str val = {0,0};
    int i = 0;
    int param_len = strlen(param_name);
    int start = 0;
    int end = 0;

    for(i = 0; i < www_auth.len; i++) {
        if (www_auth.s[i] == ' ') continue;

        if(strncasecmp(www_auth.s+i, param_name, param_len) == 0) {
            i += param_len;

            //find first double quote
            while(www_auth.s[i] != '"' && i < www_auth.len) i++;
            i++; //and skip it

            if (i == www_auth.len)
                return val; //error
            start = i;
            i++;

            //find second double quote
            while(www_auth.s[i] != '"' && i < www_auth.len) i++;
            if (i == www_auth.len)
                return val; //error
            end = i;
            i++;

            val.s = www_auth.s + start;
            val.len = end - start;
            break;
        }

        //parameter not relevant - fast forward
        do { i++; } while (www_auth.s[i] != ',' && i < www_auth.len);
    }

    return val;
}


static int fill_contact(struct pcontact_info* ci, struct sip_msg* m)
{
    contact_body_t* cb = NULL;
    struct via_body* vb = NULL;
    unsigned short port, proto = 0;
    struct sip_msg* req = NULL;


    if(!ci) {
        LM_ERR("fill_contact() called with null ptr\n");
        return -1;
    }

    memset(ci, 0, sizeof(struct pcontact_info));


    if(m->first_line.type == SIP_REQUEST) {
        struct sip_uri uri;
        memset(&uri, 0, sizeof(struct sip_uri));

        if(parse_uri(m->first_line.u.request.uri.s, m->first_line.u.request.uri.len, &uri)) {
            LM_ERR("Can't parse the request URI from first line\n");
            return -1;
        }

        // populate host,port, aor in CI
        ci->via_host = uri.host;
        ci->via_port = uri.port_no ? uri.port_no : 5060;
        ci->via_prot = proto;
        ci->aor = m->first_line.u.request.uri;

        req = m;
    }
    else if(m->first_line.type == SIP_REPLY) {

        cb = cscf_parse_contacts(m);
        vb = cscf_get_ue_via(m);
        port = vb->port?vb->port:5060;
        proto = vb->proto;

        struct cell *t = tmb.t_gett();
        if (!t || t == (void*) -1) {
            LM_ERR("fill_contact(): Reply without transaction\n");
            return -1;
        }

        req = t->uas.request;

        cb = cscf_parse_contacts(req);
        if (!cb || (!cb->contacts)) {
            LM_ERR("fill_contact(): No contact headers\n");
            return -1;
        }

        // populate CI with bare minimum
        ci->via_host = vb->host;
        ci->via_port = port;
        ci->via_prot = proto;
        ci->aor = cb->contacts->uri;
    }
    else {
        LM_ERR("fill_contact(): Unknown first line type: %d\n", m->first_line.type);
        return -1;
    }


    char* srcip = NULL;
    if((srcip = pkg_malloc(50)) == NULL) {
        LM_ERR("Error allocating memory for source IP address\n");
        return -1;
    }

    ci->received_host.len = ip_addr2sbuf(&req->rcv.src_ip, srcip, 50);
    ci->received_host.s = srcip;
    ci->received_port = req->rcv.src_port;
    ci->received_proto = req->rcv.proto;

    // Set to default, if not set:
    if (ci->received_port == 0)
        ci->received_port = 5060;


    return 0;
}

// Get CK and IK from WWW-Authenticate
static int get_ck_ik(const struct sip_msg* m, str* ck, str* ik)
{
    struct hdr_field *www_auth_hdr = NULL;
    str www_auth;
    memset(&www_auth, 0, sizeof(str));

    www_auth = cscf_get_authenticate((sip_msg_t*)m, &www_auth_hdr);

    *ck = get_www_auth_param("ck", www_auth);
    if (ck->len == 0) {
        LM_ERR("Error getting CK\n");
        return -1;
    }

    *ik = get_www_auth_param("ik", www_auth);
    if (ck->len == 0) {
        LM_ERR("Error getting IK\n");
        return -1;
    }

    return 0;
}

static int update_contact_ipsec_params(ipsec_t* s, const struct sip_msg* m)
{
    // Get CK and IK
    str ck, ik;
    if(get_ck_ik(m, &ck, &ik) != 0) {
        return -1;
    }

    // Save CK and IK in the contact
    s->ck.s = shm_malloc(ck.len);
    if(s->ck.s == NULL) {
        LM_ERR("Error allocating memory for CK\n");
        return -1;
    }
    memcpy(s->ck.s, ck.s, ck.len);
    s->ck.len = ck.len;

    s->ik.s = shm_malloc(ik.len);
    if(s->ik.s == NULL) {
        LM_ERR("Error allocating memory for IK\n");
        shm_free(s->ck.s);
        s->ck.s = NULL; s->ck.len = 0;
        s->ik.s = NULL; s->ik.len = 0;
        return -1;
    }
    memcpy(s->ik.s, ik.s, ik.len);
    s->ik.len = ik.len;

    // Generate SPI
    if((s->spi_pc = acquire_spi()) == 0) {
        LM_ERR("Error generating client SPI for IPSEC tunnel creation\n");
        shm_free(s->ck.s);
        s->ck.s = NULL; s->ck.len = 0;
        shm_free(s->ik.s);
        s->ik.s = NULL; s->ik.len = 0;
        return -1;
    }

    if((s->spi_ps = acquire_spi()) == 0) {
        LM_ERR("Error generating server SPI for IPSEC tunnel creation\n");
        shm_free(s->ck.s);
        s->ck.s = NULL; s->ck.len = 0;
        shm_free(s->ik.s);
        s->ik.s = NULL; s->ik.len = 0;
        return -1;
    }

    return 0;
}

static int convert_ip_address(const str ip_addr, const unsigned int af, struct ip_addr* result) {
    memset(result, 0, sizeof(struct ip_addr));
    int return_code = -1;

    //Allocate dynamically memory in order to avoid buffer overflows
    char* ipaddr_str = NULL;
    if((ipaddr_str = pkg_malloc(ip_addr.len + 1)) == NULL) {
        LM_CRIT("Error allocating memory for IP address conversion.\n");
        return -1;
    }
    memset(ipaddr_str, 0, ip_addr.len + 1);
    memcpy(ipaddr_str, ip_addr.s, ip_addr.len);

    int err = 0;

    if((err = inet_pton(af, ipaddr_str, &result->u.addr)) != 1) {
        if(err == 0) {
            LM_ERR("Error converting ipsec listen IP address. Bad format %.*s\n", ip_addr.len, ip_addr.s);
        }
        else {
            LM_ERR("Error converting ipsec listen IP address: %s\n", strerror(errno));
        }
        goto cleanup;   // return_code = -1 by default
    }

    //Set len by address family
    if(af == AF_INET6) {
        result->len = 16;
    }
    else {
        result->len = 4;
    }

    result->af = af;

    //Set success return code
    return_code = 0;

cleanup:
    pkg_free(ipaddr_str);
    return return_code;
}

static int create_ipsec_tunnel(const struct ip_addr *remote_addr, unsigned short proto, ipsec_t* s)
{
    struct mnl_socket* sock = init_mnl_socket();
    if (sock == NULL) {
        return -1;
    }

    //Convert ipsec address from str to struct ip_addr
    struct ip_addr ipsec_addr;
    if(convert_ip_address(ipsec_listen_addr, remote_addr->af, &ipsec_addr) != 0) {
        //there is an error msg in convert_ip_address()
        return -1;
    }

    //Convert to char* for logging
    char remote_addr_str[128];
    memset(remote_addr_str, 0, sizeof(remote_addr_str));
    if(inet_ntop(remote_addr->af, remote_addr->u.addr, remote_addr_str, sizeof(remote_addr_str)) == NULL) {
        LM_CRIT("Error converting remote IP address: %s\n", strerror(errno));
        return -1;
    }

    LM_DBG("Creating security associations: Local IP: %.*s client port: %d server port: %d; UE IP: %s; client port %d server port %d\n",
            ipsec_listen_addr.len, ipsec_listen_addr.s, ipsec_client_port, ipsec_server_port,
            remote_addr_str, s->port_uc, s->port_us);

    // P-CSCF 'client' tunnel to UE 'server'
    add_sa    (sock, proto, &ipsec_addr, remote_addr, ipsec_client_port, s->port_us, s->spi_us, s->ck, s->ik);
    add_policy(sock, proto, &ipsec_addr, remote_addr, ipsec_client_port, s->port_us, s->spi_us, IPSEC_POLICY_DIRECTION_OUT);

    // UE 'client' to P-CSCF 'server' tunnel
    add_sa    (sock, proto, remote_addr, &ipsec_addr, s->port_uc, ipsec_server_port, s->spi_ps, s->ck, s->ik);
    add_policy(sock, proto, remote_addr, &ipsec_addr, s->port_uc, ipsec_server_port, s->spi_ps, IPSEC_POLICY_DIRECTION_IN);

    close_mnl_socket(sock);

    return 0;
}

static int destroy_ipsec_tunnel(const str remote_addr, unsigned short proto, ipsec_t* s)
{
    struct mnl_socket* sock = init_mnl_socket();
    if (sock == NULL) {
        return -1;
    }

    LM_DBG("Destroying security associations: Local IP: %.*s client port: %d server port: %d; UE IP: %.*s; client port %d server port %d\n",
            ipsec_listen_addr.len, ipsec_listen_addr.s, ipsec_client_port, ipsec_server_port,
            remote_addr.len, remote_addr.s, s->port_uc, s->port_us);

    // P-CSCF 'client' tunnel to UE 'server'
    remove_sa    (sock, ipsec_listen_addr, remote_addr, ipsec_client_port, s->port_us, s->spi_us);
    remove_policy(sock, proto, ipsec_listen_addr, remote_addr, ipsec_client_port, s->port_us, s->spi_us, IPSEC_POLICY_DIRECTION_OUT);

    // UE 'client' to P-CSCF 'server' tunnel
    remove_sa    (sock, remote_addr, ipsec_listen_addr, s->port_uc, ipsec_server_port, s->spi_ps);
    remove_policy(sock, proto, remote_addr, ipsec_listen_addr, s->port_uc, ipsec_server_port, s->spi_ps, IPSEC_POLICY_DIRECTION_IN);

    // Release SPIs
    release_spi(s->spi_uc);
    release_spi(s->spi_us);


    close_mnl_socket(sock);
    return 0;
}

static void on_expire(struct pcontact *c, int type, void *param)
{
    if(type != PCSCF_CONTACT_EXPIRE && type != PCSCF_CONTACT_DELETE) {
        LM_ERR("Unexpected event type %d\n", type);
        return;
    }


    if(c->security_temp == NULL) {
        LM_ERR("No security parameters found in contact\n");
        return;
    }

    //get security parameters
    if(c->security_temp->type != SECURITY_IPSEC ) {
        LM_ERR("Unsupported security type: %d\n", c->security_temp->type);
        return;
    }

    destroy_ipsec_tunnel(c->received_host, c->received_proto, c->security_temp->data.ipsec);
}

int add_supported_secagree_header(struct sip_msg* m)
{
    // Add sec-agree header in the reply
    const char* supported_sec_agree = "Supported: sec-agree\r\n";
    const int supported_sec_agree_len = 22;

    str* supported = NULL;
    if((supported = pkg_malloc(sizeof(str))) == NULL) {
        LM_ERR("Error allocating pkg memory for supported header\n");
        return -1;
    }

    if((supported->s = pkg_malloc(supported_sec_agree_len)) == NULL) {
        LM_ERR("Error allcationg pkg memory for supported header str\n");
        pkg_free(supported);
        return -1;
    }
    memcpy(supported->s, supported_sec_agree, supported_sec_agree_len);
    supported->len = supported_sec_agree_len;

    if(cscf_add_header(m, supported, HDR_SUPPORTED_T) != 1) {
		pkg_free(supported->s);
		pkg_free(supported);
        LM_ERR("Error adding security header to reply!\n");
        return -1;
    }
    pkg_free(supported);

    return 0;
}

int add_security_server_header(struct sip_msg* m, ipsec_t* s)
{
    // allocate memory for the header itself
    str* sec_header = NULL;
    if((sec_header = pkg_malloc(sizeof(str))) == NULL) {
        LM_ERR("Error allocating pkg memory for security header\n");
        return -1;
    }
    memset(sec_header, 0, sizeof(str));

    // create a temporary buffer and set the value in it
    char sec_hdr_buf[1024];
    memset(sec_hdr_buf, 0, sizeof(sec_hdr_buf));
    sec_header->len = snprintf(sec_hdr_buf, sizeof(sec_hdr_buf) - 1,
                                "Security-Server: ipsec-3gpp;prot=esp;mod=trans;spi-c=%d;spi-s=%d;port-c=%d;port-s=%d;alg=%.*s;ealg=%.*s\r\n",
                                s->spi_pc, s->spi_ps, ipsec_client_port, ipsec_server_port,
                                s->r_alg.len, s->r_alg.s,
                                s->r_ealg.len, s->r_ealg.s
                              );

    // copy to the header and add
    if((sec_header->s = pkg_malloc(sec_header->len)) == NULL) {
        LM_ERR("Error allocating pkg memory for security header payload\n");
        pkg_free(sec_header);
        return -1;
    }
    memcpy(sec_header->s, sec_hdr_buf, sec_header->len);

    // add security-server header in reply
    if(cscf_add_header(m, sec_header, HDR_OTHER_T) != 1) {
        LM_ERR("Error adding security header to reply!\n");
        pkg_free(sec_header->s);
        pkg_free(sec_header);
        return -1;
    }

    pkg_free(sec_header);

    return 0;
}

int ipsec_create(struct sip_msg* m, udomain_t* d)
{
    pcontact_t* pcontact = NULL;
    struct pcontact_info ci;
    int ret = IPSEC_CMD_FAIL;   // FAIL by default

    // Find the contact
    if(fill_contact(&ci, m) != 0) {
        LM_ERR("Error filling in contact data\n");
        return ret;
    }

    ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

    if (ul.get_pcontact(d, &ci, &pcontact) != 0) {
        LM_ERR("Contact doesn't exist\n");
        goto cleanup;
    }

    // Get security parameters
    if(pcontact->security_temp == NULL) {
        LM_ERR("No security parameters found in contact\n");
        goto cleanup;
    }

    if(pcontact->security_temp->type != SECURITY_IPSEC ) {
        LM_ERR("Unsupported security type: %d\n", pcontact->security_temp->type);
        goto cleanup;
    }

    ipsec_t* s = pcontact->security_temp->data.ipsec;

    if(update_contact_ipsec_params(s, m) != 0) {
        goto cleanup;
    }

    // Get request from reply
    struct cell *t = tmb.t_gett();
    if (!t || t == (void*) -1) {
        LM_ERR("fill_contact(): Reply without transaction\n");
        return -1;
    }

    struct sip_msg* req = t->uas.request;
    ////

    if(create_ipsec_tunnel(&req->rcv.src_ip, ci.received_proto, s) != 0) {
        goto cleanup;
    }

    // TODO: Save security_tmp to security!!!!!

    if (ul.update_pcontact(d, &ci, pcontact) != 0) {
        LM_ERR("Error updating contact\n");
        goto cleanup;
    }

    // Destroy the tunnel, if the contact expires
    if(ul.register_ulcb(pcontact, PCSCF_CONTACT_EXPIRE|PCSCF_CONTACT_DELETE, on_expire, NULL) != 1) {
        LM_ERR("Error subscribing for contact\n");
        goto cleanup;
    }


    if(add_supported_secagree_header(m) != 0) {
        goto cleanup;
    }

    if(add_security_server_header(m, s) != 0) {
        goto cleanup;
    }

    ret = IPSEC_CMD_SUCCESS;    // all good, set ret to SUCCESS, and exit

cleanup:
    // Do not free str* sec_header! It will be freed in data_lump.c -> free_lump()
    ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
    pkg_free(ci.received_host.s);
    return ret;
}


int ipsec_forward(struct sip_msg* m, udomain_t* d)
{
    struct pcontact_info ci;
    pcontact_t* pcontact = NULL;
    int ret = IPSEC_CMD_FAIL; // FAIL by default

    //
    // Find the contact
    //
    if(fill_contact(&ci, m) != 0) {
        LM_ERR("Error filling in contact data\n");
        return ret;
    }

    ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

    if (ul.get_pcontact(d, &ci, &pcontact) != 0) {
        LM_ERR("Contact doesn't exist\n");
        goto cleanup;
    }


    if(pcontact->security_temp == NULL) {
        LM_ERR("No security parameters found in contact\n");
        goto cleanup;
    }

    //get security parameters
    if(pcontact->security_temp->type != SECURITY_IPSEC ) {
        LM_ERR("Unsupported security type: %d\n", pcontact->security_temp->type);
        goto cleanup;
    }

    ipsec_t* s = pcontact->security_temp->data.ipsec;


    // Update the destination
    //
    //       from sec-agree
    //            v
    // sip:host:port
    //       ^
    //    from URI
    //int uri_len = 4 /* strlen("sip:") */ + ci.via_host.len + 5 /* max len of port number */ ;

    if(m->dst_uri.s) {
        pkg_free(m->dst_uri.s);
        m->dst_uri.s = NULL;
        m->dst_uri.len = 0;
    }

    char buf[1024];
    int buf_len = snprintf(buf, sizeof(buf) - 1, "sip:%.*s:%d", ci.via_host.len, ci.via_host.s, s->port_us);

    if((m->dst_uri.s = pkg_malloc(buf_len)) == NULL) {
        LM_ERR("Error allocating memory for dst_uri\n");
        goto cleanup;
    }

    memcpy(m->dst_uri.s, buf, buf_len);
    m->dst_uri.len = buf_len;

    // Set send socket
    struct socket_info * client_sock = grep_sock_info(&ipsec_listen_addr, ipsec_client_port, PROTO_UDP);
    if(!client_sock) {
        LM_ERR("Error calling grep_sock_info() for ipsec client port in ipsec_forward\n");
        return -1;
    }
    m->force_send_socket = client_sock;

   // Set destination info
    struct dest_info dst_info;
    dst_info.send_sock = client_sock;
#ifdef USE_DNS_FAILOVER
    if (!uri2dst(NULL, &dst_info, m, &m->dst_uri, PROTO_UDP)) {
#else
    if (!uri2dst(&dst_info, m, &m->dst_uri, PROTO_UDP)) {
#endif
        LM_ERR("Error converting dst_uri (%.*s) to struct dst_info\n", m->dst_uri.len, m->dst_uri.s);
        goto cleanup;
    }

    // Update dst_info in message
    if(m->first_line.type == SIP_REPLY) {
        struct cell *t = tmb.t_gett();
        if (!t) {
            LM_ERR("Error getting transaction\n");
            goto cleanup;
        }
        t->uas.response.dst = dst_info;
    }

    LM_DBG("Destination changed to %.*s\n", m->dst_uri.len, m->dst_uri.s);

    ret = IPSEC_CMD_SUCCESS; // all good, return SUCCESS

    if(add_supported_secagree_header(m) != 0) {
        goto cleanup;
    }

    if(add_security_server_header(m, s) != 0) {
        goto cleanup;
    }

    ret = IPSEC_CMD_SUCCESS;    // all good, set ret to SUCCESS, and exit

cleanup:
    ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
    pkg_free(ci.received_host.s);
    return ret;
}


int ipsec_destroy(struct sip_msg* m, udomain_t* d)
{
    struct pcontact_info ci;
    pcontact_t* pcontact = NULL;
    int ret = IPSEC_CMD_FAIL; // FAIL by default

    //
    // Find the contact
    //
    if(fill_contact(&ci, m) != 0) {
        LM_ERR("Error filling in contact data\n");
        return ret;
    }

    ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

    if (ul.get_pcontact(d, &ci, &pcontact) != 0) {
        LM_ERR("Contact doesn't exist\n");
        goto cleanup;
    }


    if(pcontact->security_temp == NULL) {
        LM_ERR("No security parameters found in contact\n");
        goto cleanup;
    }

    //get security parameters
    if(pcontact->security_temp->type != SECURITY_IPSEC ) {
        LM_ERR("Unsupported security type: %d\n", pcontact->security_temp->type);
        goto cleanup;
    }

    destroy_ipsec_tunnel(ci.received_host, ci.received_proto, pcontact->security_temp->data.ipsec);

    ret = IPSEC_CMD_SUCCESS;    // all good, set ret to SUCCESS, and exit

cleanup:
    ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
    pkg_free(ci.received_host.s);
    return ret;
}

int ipsec_cleanall()
{
    struct mnl_socket* nlsock = init_mnl_socket();
    if(!nlsock) {
        return -1;
    }

    if(clean_sa(nlsock) != 0) {
        LM_WARN("Error cleaning IPSec Security associations during startup.\n");
    }

    if(clean_policy(nlsock) != 0) {
        LM_WARN("Error cleaning IPSec Policies during startup.\n");
    }

    close_mnl_socket(nlsock);

    return 0;
}
