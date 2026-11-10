/*
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * Copyright (C) 2019 Aleksandar Yosifov
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fraunhofer FOKUS Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: A lot of this code was originally part of OpenIMSCore,
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
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "cmd.h"
#include "sec_agree.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>


extern str ipsec_listen_addr;
extern str ipsec_listen_addr6;
extern int ipsec_reuse_server_port;
extern ip_addr_t ipsec_listen_ip_addr;
extern ip_addr_t ipsec_listen_ip_addr6;

extern int spi_id_start;

extern atomic_t *ipsec_reconfig_flag;
extern unsigned int ipsec_init_flag;

// check http://www.asipto.com/pub/kamailio-devel-guide/#c16return_values
const int IPSEC_CMD_FAIL = -1;
const int IPSEC_CMD_SUCCESS = 1;

extern usrloc_api_t ul;
extern struct tm_binds tmb;

/* if set - set send force socket for request messages */
#define IPSEC_SEND_FORCE_SOCKET 1
/* if set - start searching from the last element */
#define IPSEC_REVERSE_SEARCH 2
/* if set - use destination address for IPSec tunnel search */
#define IPSEC_DSTADDR_SEARCH (1 << 2)
/* if set - use new r-uri address for IPSec tunnel search */
#define IPSEC_RURIADDR_SEARCH (1 << 3)
/* if set - do not use alias for IPSec tunnel received details */
#define IPSEC_NOALIAS_SEARCH (1 << 4)
/* if set - do not reset dst uri for IPsec forward */
#define IPSEC_NODSTURI_RESET (1 << 5)
/* if set - use user equipment client port as target for requests over TCP/TLS */
#define IPSEC_TCPPORT_UEC (1 << 6)
/* if set - build new dst uri with transport parameter for TCP/TLS */
#define IPSEC_SETDSTURI_FULL (1 << 7)
/* if set - use Via attributes for routing reply
 * and protocol from next hop address for request */
#define IPSEC_FORWARD_USEVIA (1 << 8)
/* if set - try TCP if corresponding UDP socket is not found */
#define IPSEC_FORWARD_TRYTCP (1 << 9)

/* if set - delete unused tunnels before every registration */
#define IPSEC_CREATE_DELETE_UNUSED_TUNNELS 0x01

int bind_ipsec_pcscf(ipsec_pcscf_api_t *api)
{
	if(!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	if(ipsec_init_flag == 0) {
		LM_ERR("configuration error - trying to bind to ipsec pscscf module "
			   "before being initialized\n");
		return -1;
	}

	api->ipsec_on_expire = ipsec_on_expire;
	api->ipsec_reconfig = ipsec_reconfig;
	api->ipsec_promote = ipsec_promote;

	return 0;
}

static str get_www_auth_param(const char *param_name, str www_auth)
{
	str val = {0, 0};
	int i = 0;
	int param_len = strlen(param_name);
	int start = 0;
	int end = 0;

	for(i = 0; i < www_auth.len; i++) {
		if(www_auth.s[i] == ' ')
			continue;

		if(strncasecmp(www_auth.s + i, param_name, param_len) == 0) {
			i += param_len;

			//find first double quote
			while(www_auth.s[i] != '"' && i < www_auth.len)
				i++;
			i++; //and skip it

			if(i == www_auth.len)
				return val; //error
			start = i;
			i++;

			//find second double quote
			while(www_auth.s[i] != '"' && i < www_auth.len)
				i++;
			if(i == www_auth.len)
				return val; //error
			end = i;
			i++;

			val.s = www_auth.s + start;
			val.len = end - start;
			break;
		}

		//parameter not relevant - fast forward
		do {
			i++;
		} while(www_auth.s[i] != ',' && i < www_auth.len);
	}

	return val;
}

static int fill_contact(
		pcontact_info_t *ci, sip_msg_t *m, tm_cell_t *t, str *ruri, int sflags)
{
	contact_body_t *cb = NULL;
	struct via_body *vb = NULL;
	char *srcip = NULL;
	str aor = STR_NULL;
	sip_msg_t tmsg;
	char tbuf[BUF_SIZE + 1];
	int i;

	if(!ci) {
		LM_ERR("called with null ptr\n");
		return -1;
	}

	memset(ci, 0, sizeof(struct pcontact_info));

	if(m->first_line.type == SIP_REQUEST) {
		char *alias_start;
		struct sip_uri uri;
		str suri = STR_NULL;

		memset(&uri, 0, sizeof(struct sip_uri));

		if(ruri != NULL && ruri->len > 0) {
			suri.s = ruri->s;
			suri.len = ruri->len;
			LM_DBG("using param r-uri for contact filling: %.*s\n", suri.len,
					suri.s);
		}
		if((sflags & IPSEC_DSTADDR_SEARCH) && suri.s == NULL
				&& m->dst_uri.s != NULL && m->dst_uri.len > 0) {
			suri = m->dst_uri;
			LM_DBG("using dst uri for contact filling: %.*s\n", suri.len,
					suri.s);
		}
		if((sflags & IPSEC_RURIADDR_SEARCH) && suri.s == NULL
				&& m->new_uri.s != NULL && m->new_uri.len > 0) {
			suri = m->new_uri;
			LM_DBG("using new r-uri for contact filling: %.*s\n", suri.len,
					suri.s);
		}
		if(suri.s == NULL) {
			suri = m->first_line.u.request.uri;
			LM_DBG("using original uri for contact filling: %.*s\n", suri.len,
					suri.s);
		}
		if(parse_uri(suri.s, suri.len, &uri) < 0) {
			LM_ERR("failed to parse the URI: %.*s / flags: 0x%x\n", suri.len,
					suri.s, sflags);
			return -1;
		}

		// populate host,port, aor in CI
		ci->via_host = uri.host;
		ci->via_port = uri.port_no ? uri.port_no : 5060;
		ci->via_prot = 0;
		if(pkg_str_dup(&aor, &suri) < 0) {
			LM_ERR("failed to duplicate aor\n");
			return -1;
		}
		ci->searchflag = SEARCH_NORMAL;

		if(ci->via_host.s == NULL || ci->via_host.len == 0) {
			// no host included in RURI
			vb = cscf_get_ue_via(m);
			if(!vb) {
				LM_ERR("Reply No via body headers\n");
				goto error;
			}

			// populate CI with bare minimum
			ci->via_host = vb->host;
			ci->via_port = vb->port;
			ci->via_prot = vb->proto;
		}

		alias_start = NULL;
		if((!(sflags & IPSEC_NOALIAS_SEARCH)) && uri.params.len > 6) {
			alias_start = _strnistr(uri.params.s, "alias=", uri.params.len);
		}
		if(alias_start != NULL && *(alias_start - 1) == ';') {
			char *p, *port_s, *proto_s;
			char portbuf[6];
			str alias_s;

			LM_DBG("contact has an alias [%.*s] - use that as the received\n",
					uri.params.len, uri.params.s);

			alias_s.len = uri.params.len - (alias_start - uri.params.s) - 6;
			alias_s.s = alias_start + 6;

			p = _strnistr(alias_s.s, "~", alias_s.len);
			if(p != NULL) {
				ci->received_host.len = p - alias_s.s;

				if(ci->received_host.len > IP6_MAX_STR_SIZE + 2) {
					LM_ERR("Invalid length for source IP address\n");
					goto error;
				}

				if((srcip = pkg_malloc(50)) == NULL) {
					LM_ERR("Error allocating memory for source IP address\n");
					goto error;
				}

				memcpy(srcip, alias_s.s, ci->received_host.len);
				ci->received_host.s = srcip;

				port_s = p + 1;
				p = _strnistr(port_s, "~", alias_s.len - ci->received_host.len);
				if(p != NULL) {
					if((p - port_s) > 5) {
						LM_ERR("invalid port value\n");
						goto error;
					}
					memset(portbuf, 0, 6);
					memcpy(portbuf, port_s, (p - port_s));
					ci->received_port = atoi(portbuf);

					proto_s = p + 1;
					memset(portbuf, 0, 6);
					memcpy(portbuf, proto_s, 1);
					ci->received_proto = atoi(portbuf);

					ci->searchflag = SEARCH_RECEIVED;
				}

				LM_DBG("parsed alias [%d://%.*s:%d]\n", ci->received_proto,
						ci->received_host.len, ci->received_host.s,
						ci->received_port);
			}
		} else {
			if((srcip = pkg_malloc(50)) == NULL) {
				LM_ERR("Error allocating memory for source IP address\n");
				goto error;
			}

			ci->received_host.len = ip_addr2sbuf(&m->rcv.src_ip, srcip, 50);
			ci->received_host.s = srcip;
			ci->received_port = m->rcv.src_port;
			ci->received_proto = m->rcv.proto;
		}
	} else if(m->first_line.type == SIP_REPLY) {
		if(!t || t == (void *)-1) {
			LM_ERR("Reply without transaction\n");
			goto error;
		}

		if(t->uas.request->len >= BUF_SIZE - 1) {
			LM_ERR("message too long\n");
			goto error;
		}
		memset(&tmsg, 0, sizeof(sip_msg_t));
		tmsg.buf = tbuf;
		memcpy(tmsg.buf, t->uas.request->buf, t->uas.request->len);
		tmsg.buf[t->uas.request->len] = '\0';
		tmsg.len = t->uas.request->len;
		if(parse_msg(tmsg.buf, tmsg.len, &tmsg) != 0) {
			LM_ERR("buffer parsing failed!");
			goto error;
		}
		if((vb = cscf_get_ue_via(m)) == NULL) {
			LM_ERR("Reply No via body headers\n");
			free_sip_msg(&tmsg);
			goto error;
		}
		cb = cscf_parse_contacts(&tmsg);
		free_sip_msg(&tmsg);
		if(!cb || (!cb->contacts)) {
			LM_DBG("Reply No contact headers, building AOR from Via\n");
			// Handle the messages without contacts (e.g.BYE)
			// Build AOR as "sip:<via_host>:<via_port>"
			char aor_buf[256];
			int aor_len = snprintf(aor_buf, sizeof(aor_buf), "sip:%.*s:%d",
					vb->host.len, vb->host.s, vb->port);
			if(aor_len <= 0 || aor_len >= (int)sizeof(aor_buf)) {
				LM_ERR("Failed to build AOR from Via\n");
				goto error;
			}
			if((aor.s = pkg_malloc(aor_len)) == NULL) {
				PKG_MEM_ERROR;
				goto error;
			}
			memcpy(aor.s, aor_buf, aor_len);
			aor.len = aor_len;
			LM_DBG("set AOR [%.*s], VIA [%d://%.*s:%d]\n", STR_FMT(&aor),
					vb->proto, STR_FMT(&vb->host), vb->port);
		} else {
			if(pkg_str_dup(&aor, &cb->contacts->uri) < 0) {
				LM_ERR("failed to duplicate aor\n");
				goto error;
			}
		}

		// populate CI with bare minimum
		ci->via_host = vb->host;
		ci->via_port = vb->port;
		ci->via_prot = vb->proto;
		ci->searchflag = SEARCH_RECEIVED;

		if((srcip = pkg_malloc(50)) == NULL) {
			PKG_MEM_ERROR;
			goto error;
		}

		ci->received_host.len =
				ip_addr2sbuf(&t->uas.request->rcv.src_ip, srcip, 50);
		ci->received_host.s = srcip;
		ci->received_port = t->uas.request->rcv.src_port;
		ci->received_proto = t->uas.request->rcv.proto;
	} else {
		LM_ERR("Unknown first line type: %d\n", m->first_line.type);
		goto error;
	}

	for(i = 4; i < aor.len; i++) {
		if(aor.s[i] == ';') {
			aor.len = i;
			break;
		}
	}

	ci->aor = aor;

	LM_DBG("SIP %s fill contact with AOR [%.*s], VIA [%d://%.*s:%d], "
		   "received_host [%d://%.*s:%d]\n",
			m->first_line.type == SIP_REQUEST ? "REQUEST" : "REPLY",
			ci->aor.len, ci->aor.s, ci->via_prot, ci->via_host.len,
			ci->via_host.s, ci->via_port, ci->received_proto,
			ci->received_host.len, ci->received_host.s, ci->received_port);

	// Set to default, if not set:
	if(ci->received_port == 0)
		ci->received_port = 5060;

	return 0;

error:
	if(aor.s != NULL) {
		pkg_free(aor.s);
	}
	if(srcip != NULL) {
		pkg_free(srcip);
	}
	ci->aor.s = NULL;
	ci->received_host.s = NULL;
	return -1;
}

// Get CK and IK from WWW-Authenticate
static int get_ck_ik(const struct sip_msg *m, str *ck, str *ik)
{
	struct hdr_field *www_auth_hdr = NULL;
	str www_auth;
	memset(&www_auth, 0, sizeof(str));

	www_auth = cscf_get_authenticate((sip_msg_t *)m, &www_auth_hdr);

	*ck = get_www_auth_param("ck", www_auth);
	if(ck->len == 0) {
		LM_ERR("Error getting CK\n");
		return -1;
	}

	*ik = get_www_auth_param("ik", www_auth);
	if(ik->len == 0) {
		LM_ERR("Error getting IK\n");
		return -1;
	}

	return 0;
}

// s_est: the tunnel the UE is registered over, whose P-CSCF ports the new one
// stays off where it can (see tunnels_collide())
static int update_contact_ipsec_params(
		ipsec_t *s, const struct sip_msg *m, ipsec_t *s_old, ipsec_t *s_est)
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
		s->ck.s = NULL;
		s->ck.len = 0;
		s->ik.s = NULL;
		s->ik.len = 0;
		return -1;
	}
	memcpy(s->ik.s, ik.s, ik.len);
	s->ik.len = ik.len;

	// Reuse only if both CK and IK match.
	if(s_old && s_old->ck.s != NULL && s_old->ik.s != NULL
			&& STR_EQ(s_old->ck, s->ck) && STR_EQ(s_old->ik, s->ik)) {
		if(s_old->spi_pc && s_old->spi_ps && s_old->port_pc && s_old->port_ps) {
			LM_INFO("Reusing IPSEC tunnel\n");
			s->spi_pc = s_old->spi_pc;
			s->spi_ps = s_old->spi_ps;
			s->port_pc = s_old->port_pc;
			s->port_ps = s_old->port_ps;
			return 0;
		}
	}

	if(acquire_spi(&s->spi_pc, &s->spi_ps, &s->port_pc, &s->port_ps,
			   s_est ? s_est->port_pc : 0, s_est ? s_est->port_ps : 0)
			== 0) {
		LM_ERR("Error generating client SPI for IPSEC tunnel creation\n");
		shm_free(s->ck.s);
		s->ck.s = NULL;
		s->ck.len = 0;
		shm_free(s->ik.s);
		s->ik.s = NULL;
		s->ik.len = 0;
		return -1;
	}

	return 0;
}

static int create_ipsec_tunnel(const struct ip_addr *remote_addr, ipsec_t *s)
{
	struct mnl_socket *sock = init_mnl_socket();
	if(sock == NULL) {
		return -1;
	}

	// pointer to the current listen address
	ip_addr_t *ipsec_addr;

	if(remote_addr->af == AF_INET) {
		ipsec_addr = &ipsec_listen_ip_addr;
	} else if(remote_addr->af == AF_INET6) {
		ipsec_addr = &ipsec_listen_ip_addr6;
	} else {
		LM_ERR("Unsupported AF %d\n", remote_addr->af);
		close_mnl_socket(sock);
		return -1;
	}

	//Convert to char* for logging
	char remote_addr_str[128];
	memset(remote_addr_str, 0, sizeof(remote_addr_str));
	if(inet_ntop(remote_addr->af, remote_addr->u.addr, remote_addr_str,
			   sizeof(remote_addr_str))
			== NULL) {
		LM_CRIT("Error converting remote IP address: %s\n", strerror(errno));
		close_mnl_socket(sock);
		return -1;
	}

	LM_DBG("Creating security associations: Local IP: %.*s port_pc: %d "
		   "port_ps: %d; UE IP: %s; port_uc %d port_us %d; spi_pc %u, spi_ps "
		   "%u, spi_uc %u, spi_us %u, alg %.*s, ealg %.*s\n",
			remote_addr->af == AF_INET ? ipsec_listen_addr.len
									   : ipsec_listen_addr6.len,
			remote_addr->af == AF_INET ? ipsec_listen_addr.s
									   : ipsec_listen_addr6.s,
			s->port_pc, s->port_ps, remote_addr_str, s->port_uc, s->port_us,
			s->spi_pc, s->spi_ps, s->spi_uc, s->spi_us, s->r_alg.len,
			s->r_alg.s, s->r_ealg.len, s->r_ealg.s);

	// SA1 UE client to P-CSCF server
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, remote_addr, ipsec_addr, s->port_uc, s->port_ps, s->spi_ps,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, remote_addr, ipsec_addr, s->port_uc, s->port_ps, s->spi_ps,
			IPSEC_POLICY_DIRECTION_IN);

	// SA2 P-CSCF client to UE server
	//               src adrr     dst addr     src port           dst port
	add_sa(sock, ipsec_addr, remote_addr, s->port_pc, s->port_us, s->spi_us,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, ipsec_addr, remote_addr, s->port_pc, s->port_us, s->spi_us,
			IPSEC_POLICY_DIRECTION_OUT);

	// SA3 P-CSCF server to UE client
	//               src adrr     dst addr     src port           dst port
	add_sa(sock, ipsec_addr, remote_addr, s->port_ps, s->port_uc, s->spi_uc,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, ipsec_addr, remote_addr, s->port_ps, s->port_uc, s->spi_uc,
			IPSEC_POLICY_DIRECTION_OUT);

	// SA4 UE server to P-CSCF client
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, remote_addr, ipsec_addr, s->port_us, s->port_pc, s->spi_pc,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, remote_addr, ipsec_addr, s->port_us, s->port_pc, s->spi_pc,
			IPSEC_POLICY_DIRECTION_IN);

	/* cope with some broken In-Dialog routing */
	// SA5 UE client to P-CSCF client
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, remote_addr, ipsec_addr, s->port_uc, s->port_pc, s->spi_ps,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, remote_addr, ipsec_addr, s->port_uc, s->port_pc, s->spi_ps,
			IPSEC_POLICY_DIRECTION_IN);

	// SA6 P-CSCF client to UE client
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, ipsec_addr, remote_addr, s->port_pc, s->port_uc, s->spi_us,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, ipsec_addr, remote_addr, s->port_pc, s->port_uc, s->spi_us,
			IPSEC_POLICY_DIRECTION_OUT);

	// SA7 P-CSCF server to UE server
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, ipsec_addr, remote_addr, s->port_ps, s->port_us, s->spi_uc,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, ipsec_addr, remote_addr, s->port_ps, s->port_us, s->spi_uc,
			IPSEC_POLICY_DIRECTION_OUT);

	// SA8 UE server to P-CSCF server
	//               src adrr     dst addr     src port    dst port
	add_sa(sock, remote_addr, ipsec_addr, s->port_us, s->port_ps, s->spi_pc,
			s->ck, s->ik, s->r_alg, s->r_ealg);
	add_policy(sock, remote_addr, ipsec_addr, s->port_us, s->port_ps, s->spi_pc,
			IPSEC_POLICY_DIRECTION_IN);

	close_mnl_socket(sock);

	return 0;
}

// Remove a tunnel's SAs and policies from the kernel, leaving its SPI pair
// marked used. retire_ipsec_tunnel() is this plus the release.
// Fails only before removing anything.
static int remove_ipsec_tunnel(str remote_addr, ipsec_t *s)
{
	struct mnl_socket *sock = init_mnl_socket();
	if(sock == NULL) {
		return -1;
	}

	ip_addr_t ip_addr;
	str ipsec_addr;

	// convert 'remote_addr' ip string to ip_addr_t
	if(str2ipxbuf(&remote_addr, &ip_addr) < 0) {
		LM_ERR("Unable to convert remote address [%.*s]\n", remote_addr.len,
				remote_addr.s);
		close_mnl_socket(sock);
		return -1;
	}

	if(ip_addr.af == AF_INET6) {
		ipsec_addr = ipsec_listen_addr6;
	} else {
		ipsec_addr = ipsec_listen_addr;
	}

	LM_DBG("Destroying security associations: Local IP: %.*s client port: %d "
		   "server port: %d; UE IP: %.*s; client port %d server port %d; "
		   "spi_ps %u, spi_pc %u, spi_us %u, spi_uc %u\n",
			ipsec_addr.len, ipsec_addr.s, s->port_pc, s->port_ps,
			remote_addr.len, remote_addr.s, s->port_uc, s->port_us, s->spi_ps,
			s->spi_pc, s->spi_us, s->spi_uc);

	// SA1 UE client to P-CSCF server
	remove_sa(sock, remote_addr, ipsec_addr, s->port_uc, s->port_ps, s->spi_ps,
			ip_addr.af);
	remove_policy(sock, remote_addr, ipsec_addr, s->port_uc, s->port_ps,
			s->spi_ps, ip_addr.af, IPSEC_POLICY_DIRECTION_IN);

	// SA2 P-CSCF client to UE server
	remove_sa(sock, ipsec_addr, remote_addr, s->port_pc, s->port_us, s->spi_us,
			ip_addr.af);
	remove_policy(sock, ipsec_addr, remote_addr, s->port_pc, s->port_us,
			s->spi_us, ip_addr.af, IPSEC_POLICY_DIRECTION_OUT);

	// SA3 P-CSCF server to UE client
	remove_sa(sock, ipsec_addr, remote_addr, s->port_ps, s->port_uc, s->spi_uc,
			ip_addr.af);
	remove_policy(sock, ipsec_addr, remote_addr, s->port_ps, s->port_uc,
			s->spi_uc, ip_addr.af, IPSEC_POLICY_DIRECTION_OUT);

	// SA4 UE server to P-CSCF client
	remove_sa(sock, remote_addr, ipsec_addr, s->port_us, s->port_pc, s->spi_pc,
			ip_addr.af);
	remove_policy(sock, remote_addr, ipsec_addr, s->port_us, s->port_pc,
			s->spi_pc, ip_addr.af, IPSEC_POLICY_DIRECTION_IN);

	/* cope with some broken In-Dialog routing */
	// SA5 UE client to P-CSCF client
	remove_sa(sock, remote_addr, ipsec_addr, s->port_uc, s->port_pc, s->spi_ps,
			ip_addr.af);
	remove_policy(sock, remote_addr, ipsec_addr, s->port_uc, s->port_pc,
			s->spi_ps, ip_addr.af, IPSEC_POLICY_DIRECTION_IN);

	// SA6 P-CSCF client to UE client
	remove_sa(sock, ipsec_addr, remote_addr, s->port_pc, s->port_uc, s->spi_us,
			ip_addr.af);
	remove_policy(sock, ipsec_addr, remote_addr, s->port_pc, s->port_uc,
			s->spi_us, ip_addr.af, IPSEC_POLICY_DIRECTION_OUT);

	// SA7 P-CSCF server to UE server
	remove_sa(sock, ipsec_addr, remote_addr, s->port_ps, s->port_us, s->spi_uc,
			ip_addr.af);
	remove_policy(sock, ipsec_addr, remote_addr, s->port_ps, s->port_us,
			s->spi_uc, ip_addr.af, IPSEC_POLICY_DIRECTION_OUT);

	// SA8 UE server to P-CSCF server
	remove_sa(sock, remote_addr, ipsec_addr, s->port_us, s->port_ps, s->spi_pc,
			ip_addr.af);
	remove_policy(sock, remote_addr, ipsec_addr, s->port_us, s->port_ps,
			s->spi_pc, ip_addr.af, IPSEC_POLICY_DIRECTION_IN);

	close_mnl_socket(sock);
	return 0;
}

/*
 * A contact holds up to two tunnels, as TS 33.203 7.4 has it: the one the UE
 * registered over, in security, and the one a 401 has just offered, in
 * security_temp, until a REGISTER over it is answered 200 and ipsec_promote()
 * makes it the registered one. Until then the registered tunnel keeps
 * carrying everything - MT requests, and whatever the UE still sends over it -
 * so a challenge the UE never answers costs nothing but the offer.
 *
 * An offer that shares kernel state with the registered tunnel (see
 * tunnels_collide()) cannot be installed beside it. It waits in security_temp,
 * deferred (tunnel_deferred()), while the 401 that makes it goes out over the
 * registered tunnel, and replaces that one once the 401 is out
 * (install_deferred_tunnel()).
 *
 * A promotion removes the tunnel it replaces.
 *
 * security_temp can also hold what save_pending() stored for a first
 * registration: the UE's half of the offer only, with the P-CSCF's SPIs and
 * ports still zero. That is not a tunnel yet; contact_tunnel() leaves it out.
 */

// The tunnel a security slot holds, or NULL when it holds none (yet)
static ipsec_t *contact_tunnel(security_t *sec)
{
	ipsec_t *s;

	if(!sec || sec->type != SECURITY_IPSEC || !sec->data.ipsec) {
		return NULL;
	}
	s = sec->data.ipsec;
	return (s->spi_pc && s->spi_ps && s->port_pc && s->port_ps) ? s : NULL;
}

// Forget a tunnel's SPI pair once it is released (or never installed), so
// the slot holds only the UE's half again and no later destroy releases the
// pair a second time - maybe from under another UE that acquired it since
static inline void clear_spi_pair(ipsec_t *s)
{
	s->spi_pc = s->spi_ps = 0;
	s->port_pc = s->port_ps = 0;
}

// Whether a message came in over a tunnel: from one of the UE's protected
// ports to one of the P-CSCF's. Two tunnels of a contact that are installed
// side by side never both match, as they do not collide.
static inline int came_over_tunnel(ipsec_t *s, struct receive_info *rcv)
{
	return (rcv->src_port == s->port_uc || rcv->src_port == s->port_us)
		   && (rcv->dst_port == s->port_ps || rcv->dst_port == s->port_pc);
}

// Whether two tunnels hold the same SPI pair from the generator
static inline int same_spi_pair(ipsec_t *a, ipsec_t *b)
{
	return a->spi_pc == b->spi_pc && a->spi_ps == b->spi_ps;
}

// Whether two slots hold the same tunnel, both ends of it
static inline int same_tunnel(ipsec_t *a, ipsec_t *b)
{
	return same_spi_pair(a, b) && a->port_pc == b->port_pc
		   && a->port_ps == b->port_ps && a->spi_uc == b->spi_uc
		   && a->spi_us == b->spi_us && a->port_uc == b->port_uc
		   && a->port_us == b->port_us;
}

static inline int share_any(
		unsigned int a1, unsigned int a2, unsigned int b1, unsigned int b2)
{
	return a1 == b1 || a1 == b2 || a2 == b1 || a2 == b2;
}

/*
 * Whether two tunnels of the same UE would be the same kernel state anywhere.
 *
 * An SA is keyed by its destination and SPI: the inbound ones by the P-CSCF's
 * spi_pc/spi_ps, the outbound ones by the UE's address and its spi_uc/spi_us.
 * A policy is keyed by its selector, a UE port against a P-CSCF port. Two
 * tunnels that share any of these cannot be installed side by side - the add
 * of the second one fails with EEXIST and leaves the first one's state, keys
 * and all - and removing one takes the other's with it.
 *
 * Every UE port has a policy against every P-CSCF port, and a UE commonly
 * keeps its port_us across re-registrations, so two of its tunnels collide
 * as soon as they share a P-CSCF port too. update_contact_ipsec_params()
 * therefore has the generator pick the new tunnel's P-CSCF ports apart from
 * the registered one's, client and server port alike, and never reuses the
 * registered one's pair. What still collides - a UE that keeps its SPIs, or
 * port ranges too small to keep the ports apart (see spi_ports_apart(),
 * warned about at startup) - is deferred.
 */
static int tunnels_collide(ipsec_t *a, ipsec_t *b)
{
	return share_any(a->spi_pc, a->spi_ps, b->spi_pc, b->spi_ps)
		   || share_any(a->spi_uc, a->spi_us, b->spi_uc, b->spi_us)
		   || (share_any(a->port_uc, a->port_us, b->port_uc, b->port_us)
				   && share_any(
						   a->port_pc, a->port_ps, b->port_pc, b->port_ps));
}

// Whether pend, the offer in security_temp, is deferred: one that collides
// with est, the registered tunnel, cannot have been installed beside it
static inline int tunnel_deferred(ipsec_t *est, ipsec_t *pend)
{
	return est && pend && tunnels_collide(est, pend);
}

// Remove a tunnel. Its SPI pair goes back to the generator unless keep, the
// tunnel replacing it, took the same one over, or the removal failed.
static int retire_ipsec_tunnel(str remote_addr, ipsec_t *s, ipsec_t *keep)
{
	if(remove_ipsec_tunnel(remote_addr, s) != 0) {
		LM_ERR("Failed to remove the tunnel with spi_pc %u, spi_ps %u, keeping "
			   "its SPIs\n",
				s->spi_pc, s->spi_ps);
		return -1;
	}
	if(!keep || !same_spi_pair(s, keep)) {
		release_spi(s->spi_pc, s->spi_ps, s->port_pc, s->port_ps);
	}
	return 0;
}

// Retire the offer s in favour of keep. One that was never installed, as it
// collides with est, has nothing in the kernel - removing it would take est's
// with it - so only its pair goes back.
static int drop_ipsec_tunnel(
		str remote_addr, ipsec_t *s, ipsec_t *est, ipsec_t *keep)
{
	if(!tunnel_deferred(est, s)) {
		return retire_ipsec_tunnel(remote_addr, s, keep);
	}
	if(!keep || !same_spi_pair(s, keep)) {
		release_spi(s->spi_pc, s->spi_ps, s->port_pc, s->port_ps);
	}
	return 0;
}

// Remove both of a contact's tunnels. Each one removed is cleared from its
// slot, so the expiry that follows an ipsec_destroy() finds nothing left; one
// that failed stays for it to retry.
static void destroy_contact_tunnels(str remote_addr, pcontact_t *c)
{
	ipsec_t *est = contact_tunnel(c->security);
	ipsec_t *pend = contact_tunnel(c->security_temp);

	if(pend && drop_ipsec_tunnel(remote_addr, pend, est, est) == 0) {
		clear_spi_pair(pend);
	}
	if(est && retire_ipsec_tunnel(remote_addr, est, NULL) == 0) {
		clear_spi_pair(est);
	}
}

void ipsec_on_expire(struct pcontact *c, int type, void *param)
{
	if(type != PCSCF_CONTACT_EXPIRE && type != PCSCF_CONTACT_DELETE) {
		LM_ERR("Unexpected event type %d\n", type);
		return;
	}


	if(!contact_tunnel(c->security) && !contact_tunnel(c->security_temp)) {
		LM_DBG("No tunnel found in contact\n");
		return;
	}

	destroy_contact_tunnels(c->received_host, c);
}

int add_supported_secagree_header(struct sip_msg *m)
{
	// Add sec-agree header in the reply
	const char *supported_sec_agree = "Supported: sec-agree\r\n";
	const int supported_sec_agree_len = 22;

	str *supported = NULL;
	if((supported = pkg_malloc(sizeof(str))) == NULL) {
		LM_ERR("Error allocating pkg memory for supported header\n");
		return -1;
	}

	if((supported->s = pkg_malloc(supported_sec_agree_len)) == NULL) {
		LM_ERR("Error allocating pkg memory for supported header str\n");
		pkg_free(supported);
		return -1;
	}
	memcpy(supported->s, supported_sec_agree, supported_sec_agree_len);
	supported->len = supported_sec_agree_len;

	if(cscf_add_header(m, supported, HDR_SUPPORTED_T) != 1) {
		pkg_free(supported->s);
		pkg_free(supported);
		LM_ERR("Error adding supported header to reply!\n");
		return -1;
	}
	pkg_free(supported);

	return 0;
}

int add_require_secagree_header(struct sip_msg *m)
{
	// Add require sec-agree header in the reply
	const char *require_sec_agree = "Require: sec-agree\r\n";
	const int require_sec_agree_len = 20;

	str *require = NULL;
	if((require = pkg_malloc(sizeof(str))) == NULL) {
		LM_ERR("Error allocating pkg memory for require header\n");
		return -1;
	}

	if((require->s = pkg_malloc(require_sec_agree_len)) == NULL) {
		LM_ERR("Error allocating pkg memory for require header str\n");
		pkg_free(require);
		return -1;
	}

	memcpy(require->s, require_sec_agree, require_sec_agree_len);
	require->len = require_sec_agree_len;

	if(cscf_add_header(m, require, HDR_REQUIRE_T) != 1) {
		pkg_free(require->s);
		pkg_free(require);
		LM_ERR("Error adding require header to reply!\n");
		return -1;
	}

	pkg_free(require);
	return 0;
}

int add_security_server_header(struct sip_msg *m, ipsec_t *s)
{
	// allocate memory for the header itself
	str *sec_header = NULL;
	if((sec_header = pkg_malloc(sizeof(str))) == NULL) {
		LM_ERR("Error allocating pkg memory for security header\n");
		return -1;
	}
	memset(sec_header, 0, sizeof(str));

	// create a temporary buffer and set the value in it
	char sec_hdr_buf[1024];
	memset(sec_hdr_buf, 0, sizeof(sec_hdr_buf));
	sec_header->len = snprintf(sec_hdr_buf, sizeof(sec_hdr_buf) - 1,
			"Security-Server: "
			"ipsec-3gpp;q=0.1;prot=esp;mod=trans;spi-c=%d;spi-s=%d;port-c=%d;"
			"port-s=%"
			"d;alg=%.*s;ealg=%.*s\r\n",
			s->spi_pc, s->spi_ps, s->port_pc, s->port_ps, s->r_alg.len,
			s->r_alg.s, s->r_ealg.len, s->r_ealg.s);

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

/*
 * An offer ipsec_create() deferred, to install once the 401 that makes it has
 * gone out. The contact is looked up again then, the way ipsec_create() found
 * it.
 */
typedef struct deferred_tunnel
{
	udomain_t *d;
	pcontact_info_t ci; // its strings point into buf
	ip_addr_t ue_addr;
	// the offer's pair, to tell it from a later one
	unsigned int spi_pc, spi_ps;
	char buf[];
} deferred_tunnel_t;

static void free_deferred_tunnel(void *param)
{
	shm_free(param);
}

// TMCB_RESPONSE_OUT: the 401 has gone out over the registered tunnel, so the
// offer it made can replace that one now
static void install_deferred_tunnel(
		tm_cell_t *t, int type, struct tmcb_params *ps)
{
	deferred_tunnel_t *dt = (deferred_tunnel_t *)*ps->param;
	pcontact_t *c = NULL;
	security_t *old_sec;
	ipsec_t *s, *est;

	ul.lock_udomain(dt->d, &dt->ci.via_host, dt->ci.via_port, dt->ci.via_prot);

	if(ul.get_pcontact(dt->d, &dt->ci, &c, 0) != 0 || c == NULL) {
		LM_DBG("contact [%.*s] is gone, no tunnel to install\n", dt->ci.aor.len,
				dt->ci.aor.s);
		goto done;
	}
	s = contact_tunnel(c->security_temp);
	est = contact_tunnel(c->security);
	if(!tunnel_deferred(est, s) || s->spi_pc != dt->spi_pc
			|| s->spi_ps != dt->spi_ps) {
		LM_DBG("contact [%.*s]: the offer has changed since, no tunnel to "
			   "install\n",
				dt->ci.aor.len, dt->ci.aor.s);
		goto done;
	}

	if(retire_ipsec_tunnel(dt->ci.received_host, est, s) != 0) {
		// still installed, so the offer cannot be: it stays deferred
		goto done;
	}
	old_sec = c->security;
	if(ul.update_security(dt->d, SECURITY_IPSEC, NULL, c) != 0) {
		LM_ERR("Error clearing security\n");
	}
	c->security = NULL;
	free_security_t(old_sec);

	if(create_ipsec_tunnel(&dt->ue_addr, s) != 0) {
		// nothing was installed: removing it when it goes finds nothing, and
		// gives its pair back
		LM_ERR("IPSEC tunnel creation failed for contact [%.*s]\n",
				dt->ci.aor.len, dt->ci.aor.s);
		goto done;
	}
	LM_DBG("contact [%.*s]: installed the offered tunnel (spi_pc %u, spi_ps "
		   "%u) in place of the registered one\n",
			dt->ci.aor.len, dt->ci.aor.s, s->spi_pc, s->spi_ps);

done:
	ul.unlock_udomain(
			dt->d, &dt->ci.via_host, dt->ci.via_port, dt->ci.via_prot);
}

// Have the deferred offer s installed once t's reply is out
static int defer_ipsec_tunnel(tm_cell_t *t, udomain_t *d, pcontact_info_t *ci,
		const ip_addr_t *ue_addr, ipsec_t *s)
{
	deferred_tunnel_t *dt;
	char *p;

	dt = shm_malloc(sizeof(deferred_tunnel_t) + ci->aor.len + ci->via_host.len
					+ ci->received_host.len);
	if(dt == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(dt, 0, sizeof(deferred_tunnel_t));
	dt->d = d;
	dt->ci.searchflag = ci->searchflag;
	dt->ci.via_port = ci->via_port;
	dt->ci.via_prot = ci->via_prot;
	dt->ci.received_port = ci->received_port;
	dt->ci.received_proto = ci->received_proto;
	p = dt->buf;
	dt->ci.aor.s = p;
	dt->ci.aor.len = ci->aor.len;
	memcpy(p, ci->aor.s, ci->aor.len);
	p += ci->aor.len;
	dt->ci.via_host.s = p;
	dt->ci.via_host.len = ci->via_host.len;
	memcpy(p, ci->via_host.s, ci->via_host.len);
	p += ci->via_host.len;
	dt->ci.received_host.s = p;
	dt->ci.received_host.len = ci->received_host.len;
	memcpy(p, ci->received_host.s, ci->received_host.len);
	dt->ue_addr = *ue_addr;
	dt->spi_pc = s->spi_pc;
	dt->spi_ps = s->spi_ps;

	if(tmb.register_tmcb(NULL, t, TMCB_RESPONSE_OUT, install_deferred_tunnel,
			   dt, free_deferred_tunnel)
			<= 0) {
		LM_ERR("Error registering the callback to install the tunnel once "
			   "the 401 is out\n");
		shm_free(dt);
		return -1;
	}
	return 0;
}

static int promote_offer(
		udomain_t *d, pcontact_t *c, struct receive_info *rcv, int registered);

int ipsec_create(struct sip_msg *m, udomain_t *d, int _cflags)
{
	pcontact_t *pcontact = NULL;
	struct pcontact_info ci;
	int ret = IPSEC_CMD_FAIL; // FAIL by default
	int fun_ret_c;			  // Used to store return codes of some functions
	tm_cell_t *t = NULL;
	sip_msg_t *req = NULL;
	security_t *req_sec_params = NULL;
	security_t *old_sec = NULL;
	ipsec_t *s = NULL;
	ipsec_t *old_s = NULL;
	ipsec_t *est = NULL;  // the tunnel the UE is registered over
	ipsec_t *pend = NULL; // an offer a previous 401 made
	// set to 1 once req_sec_params is handed to the contact; if it stays 0 it
	// was only used to build the tunnel and must be freed before returning,
	// otherwise it leaks
	int sec_stored = 0;
	int pend_retired = 0;
	int deferred = 0; // s collides with est, see tunnel_deferred()

	if(m->first_line.type == SIP_REPLY) {
		t = tmb.t_gett();
	}
	// Find the contact
	if(fill_contact(&ci, m, t, NULL, _cflags) != 0) {
		LM_ERR("Error filling in contact data\n");
		return ret;
	}

	if(_cflags & IPSEC_CREATE_DELETE_UNUSED_TUNNELS) {
		delete_unused_tunnels();
	}

	ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

	if(ul.get_pcontact(d, &ci, &pcontact, 0) != 0 || pcontact == NULL) {
		LM_ERR("Contact doesn't exist\n");
		goto cleanup;
	}

	est = contact_tunnel(pcontact->security);
	pend = contact_tunnel(pcontact->security_temp);

	// Get security parameters
	if(!est
			&& (pcontact->security_temp == NULL
					|| pcontact->security_temp->type != SECURITY_IPSEC
					|| pcontact->security_temp->data.ipsec == NULL)) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	// Get request from reply
	if(!t)
		t = tmb.t_gett();
	if(!t || t == (void *)-1) {
		LM_ERR("Reply without transaction\n");
		goto cleanup;
	}

	req = t->uas.request;

	// A REGISTER that came in over the offered tunnel - a re-sync, a wrong
	// RES, or a 200 that did not promote it - proves the UE has it, and the
	// 401 to it has to go back over it: make it the registered tunnel now, as
	// the 200 would have, instead of replacing it as an offer never taken up.
	if(pend && promote_offer(d, pcontact, &req->rcv, 0)) {
		// pend may have been taken over from another contact
		est = contact_tunnel(pcontact->security);
		// the tunnel it replaced, if removing it failed
		pend = contact_tunnel(pcontact->security_temp);
	}

	// Parse security parameters from the REGISTER request and get some data for the new tunnels
	req_sec_params = cscf_get_security(req);

	if(!est && !pend) {
		LM_DBG("Registration for contact with AOR [%.*s], VIA [%d://%.*s:%d], "
			   "received_host [%d://%.*s:%d]\n",
				ci.aor.len, ci.aor.s, ci.via_prot, ci.via_host.len,
				ci.via_host.s, ci.via_port, ci.received_proto,
				ci.received_host.len, ci.received_host.s, ci.received_port);

		// the UE's offer save_pending() stored, when the REGISTER has none
		if(req_sec_params == NULL)
			s = pcontact->security_temp->data.ipsec;
		else
			s = req_sec_params->data.ipsec;
	} else {
		LM_DBG("RE-Registration for contact with AOR [%.*s], VIA "
			   "[%d://%.*s:%d], received_host [%d://%.*s:%d]\n",
				ci.aor.len, ci.aor.s, ci.via_prot, ci.via_host.len,
				ci.via_host.s, ci.via_port, ci.received_proto,
				ci.received_host.len, ci.received_host.s, ci.received_port);

		if(req_sec_params == NULL) {
			LM_CRIT("No security parameters in REGISTER request\n");
			goto cleanup;
		}

		s = req_sec_params->data.ipsec;
		// the latest offer: for a second 401 to the same challenge, the one
		// the first 401 made. Never the registered tunnel, which the new one
		// has to be set up beside.
		old_s = ipsec_reuse_server_port ? pend : NULL;
	}

	if(update_contact_ipsec_params(s, m, old_s, est) != 0) {
		LM_ERR("Could not update ipsec params in contact.\n");
		goto cleanup;
	}

	// What security_temp holds is superseded by this offer: an offer the UE
	// never registered over - had it, ipsec_promote() would have made it the
	// registered tunnel - or the tunnel a promotion failed to remove. Remove
	// it, and before the new one is created: the two may share state (the same
	// pair again from ipsec_reuse_server_port, or a UE that keeps its SPIs),
	// and removing it afterwards would take the new tunnel's with it.
	if(pend) {
		LM_DBG("Replacing the %s tunnel on contact [%.*s] (client port %d, "
			   "server port %d, spi_pc %u, spi_ps %u)\n",
				tunnel_deferred(est, pend) ? "deferred" : "offered", ci.aor.len,
				ci.aor.s, pend->port_pc, pend->port_ps, pend->spi_pc,
				pend->spi_ps);
		// a deferred offer can still hold the registered tunnel's pair
		if(drop_ipsec_tunnel(ci.received_host, pend, est,
				   (est && same_spi_pair(pend, est)) ? est : s)
				!= 0) {
			// it stays, still installed, and this offer is not made
			if(!same_spi_pair(pend, s)) {
				release_spi(s->spi_pc, s->spi_ps, s->port_pc, s->port_ps);
			}
			clear_spi_pair(s);
			goto cleanup;
		}
		// what is left in the slot is only the UE's half again: nothing to
		// re-offer or release should this 401 fail from here on
		clear_spi_pair(pend);
		pend_retired = 1;
	}

	// The registered tunnel stays until the UE has registered over the new
	// one. A new one that cannot be installed beside it waits, deferred,
	// while this 401 goes out over the registered tunnel - the UE cannot
	// read it over any other - and replaces that one then.
	if(est && tunnels_collide(est, s)) {
		LM_INFO("Contact [%.*s]: the new tunnel (spi_uc %u, spi_us %u, spi_pc "
				"%u, spi_ps %u) shares state with the registered one (spi_uc "
				"%u, spi_us %u, spi_pc %u, spi_ps %u), installing it in place "
				"of that one once the 401 is out\n",
				ci.aor.len, ci.aor.s, s->spi_uc, s->spi_us, s->spi_pc,
				s->spi_ps, est->spi_uc, est->spi_us, est->spi_pc, est->spi_ps);
		deferred = 1;
	} else {
		fun_ret_c = create_ipsec_tunnel(&req->rcv.src_ip, s);
		if(fun_ret_c != 0) {
			LM_ERR("IPSEC tunnel creation failed.\n");
			// Nothing was installed, and no slot holds the pair - one that
			// shared it was retired above - so give it back
			release_spi(s->spi_pc, s->spi_ps, s->port_pc, s->port_ps);
			clear_spi_pair(s);
			goto cleanup;
		}
	}

	if(req_sec_params != NULL) {
		// the contact takes the request's security over, offer and all, and
		// frees what it replaces: what save_pending() stored, or the slot
		// cleared above
		old_sec = pcontact->security_temp;
		pcontact->security_temp = req_sec_params;
		sec_stored = 1;
		free_security_t(old_sec);
	}
	// for ipsec_promote() to find this contact again, should the UE register
	// over the offer from a new client port
	spi_set_owner(s->spi_pc, s->spi_ps, pcontact->received_port);
	// Update temp security parameters
	if(ul.update_temp_security(d, pcontact->security_temp->type,
			   pcontact->security_temp, pcontact)
			!= 0) {
		LM_ERR("Error updating temp security\n");
	}

	ci.reg_state = pcontact->reg_state;

	if(ul.update_pcontact(d, &ci, pcontact) != 0) {
		LM_ERR("Error updating contact\n");
		goto cleanup;
	}

	fun_ret_c = add_supported_secagree_header(m);
	if(fun_ret_c != 0) {
		LM_ERR("Could not add secagree header. Failed with code: %d\n",
				fun_ret_c);
		goto cleanup;
	}

	fun_ret_c = add_security_server_header(m, s);
	if(fun_ret_c != 0) {
		LM_ERR("Could not add security server header. Code: %d\n", fun_ret_c);
		goto cleanup;
	}

	if(!ul.is_ulcb_registered(pcontact, ipsec_on_expire)
			&& ul.register_ulcb(pcontact,
					   PCSCF_CONTACT_EXPIRE | PCSCF_CONTACT_DELETE,
					   ipsec_on_expire, (void *)&pcontact->received_port)
					   != 1) {
		LM_ERR("Error subscribing for contact\n");
		goto cleanup;
	}

	// Only now: a deferred offer this 401 failed to make stays uninstalled,
	// and costs the registered tunnel nothing
	if(deferred && defer_ipsec_tunnel(t, d, &ci, &req->rcv.src_ip, s) != 0) {
		goto cleanup;
	}

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	if(req_sec_params && !sec_stored) {
		free_security_t(req_sec_params);
	}
	if(pend_retired && !sec_stored
			&& ul.update_temp_security(d, pcontact->security_temp->type,
					   pcontact->security_temp, pcontact)
					   != 0) {
		LM_ERR("Error updating temp security\n");
	}
	ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
	pkg_free(ci.received_host.s);
	pkg_free(ci.aor.s);
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
}

// The UE's contact other than c that came from client port port, holding
// offer in security_temp if offer is given, or no offer at all if not
static pcontact_t *contact_from_port(
		udomain_t *d, pcontact_t *c, uint16_t port, ipsec_t *offer)
{
	pcontact_info_t ci;
	pcontact_t *x;
	ipsec_t *x_pend;
	int reverse;

	// the same UE, so the same slot, which the caller has locked. The search
	// also takes a contact whose Contact port is port, which c can be: look
	// from the other end of the slot too.
	memset(&ci, 0, sizeof(pcontact_info_t));
	ci.aor = c->aor;
	ci.via_host = c->via_host;
	ci.via_port = c->via_port;
	ci.via_prot = c->via_proto;
	ci.received_host = c->received_host;
	ci.received_port = port;
	ci.received_proto = c->received_proto;
	ci.searchflag = SEARCH_RECEIVED;
	ci.reg_state = PCONTACT_ANY;
	for(reverse = 0; reverse <= 1; reverse++) {
		x = NULL;
		if(ul.get_pcontact(d, &ci, &x, reverse) != 0 || x == NULL || x == c
				|| x->received_port != port) {
			continue;
		}
		x_pend = contact_tunnel(x->security_temp);
		if(!offer && !x_pend) {
			return x;
		}
		if(offer && x_pend && same_tunnel(x_pend, offer)
				&& !tunnel_deferred(contact_tunnel(x->security), x_pend)) {
			return x;
		}
	}
	return NULL;
}

// The contact other than c the offer pend was made to, if it still holds it
static pcontact_t *offering_contact(udomain_t *d, pcontact_t *c, ipsec_t *pend)
{
	pcontact_t *x;
	uint16_t port = spi_get_owner(pend->spi_pc, pend->spi_ps);

	if(!port || port == c->received_port) {
		return NULL;
	}
	if((x = contact_from_port(d, c, port, pend)) == NULL) {
		LM_WARN("contact [%.*s]: the offer (spi_pc %u, spi_ps %u) was made to "
				"the contact received from port %u, which no longer holds "
				"it\n",
				c->aor.len, c->aor.s, pend->spi_pc, pend->spi_ps, port);
	}
	return x;
}

// Delete x, the UE's registration from a client port it has moved off, with
// the tunnel x is registered over - unless it is est, which the caller
// removes. Left without a tunnel x would still be found, by a request to the
// UE, or by a REGISTER from that port again, for which save_pending() stores
// no copy of the offer.
static void drop_superseded_contact(
		udomain_t *d, pcontact_t *x, ipsec_t *est, ipsec_t *keep)
{
	ipsec_t *x_est = contact_tunnel(x->security);

	LM_DBG("deleting contact [%.*s] received from port %u\n", x->aor.len,
			x->aor.s, x->received_port);

	if(x_est && !(est && same_tunnel(x_est, est))
			&& retire_ipsec_tunnel(x->received_host, x_est, keep) != 0) {
		// still installed: x's delete callback, ipsec_on_expire(), retries
	} else {
		free_security_t(x->security);
		x->security = NULL;
	}

	if(ul.delete_pcontact(d, x) != 0) {
		LM_ERR("Error deleting the contact received from port %u\n",
				x->received_port);
	}
}

// Move the offer x holds to c, in place of the copy save_pending() stored
static void take_over_offer(udomain_t *d, pcontact_t *c, pcontact_t *x)
{
	security_t *copy = c->security_temp;

	LM_DBG("contact [%.*s]: taking the offered tunnel (spi_pc %u, spi_ps %u) "
		   "over from the contact received from port %u\n",
			c->aor.len, c->aor.s, x->security_temp->data.ipsec->spi_pc,
			x->security_temp->data.ipsec->spi_ps, x->received_port);

	if(ul.update_temp_security(d, SECURITY_IPSEC, x->security_temp, c) != 0) {
		LM_ERR("Error updating temp security\n");
	}
	c->security_temp = x->security_temp;
	if(ul.update_temp_security(d, SECURITY_IPSEC, NULL, x) != 0) {
		LM_ERR("Error clearing temp security\n");
	}
	x->security_temp = NULL;
	free_security_t(copy);
}

/*
 * Make the offered tunnel the registered one, as ipsec_promote() describes.
 * registered: the REGISTER has been answered 200, rather than challenged
 * again, so the UE is registered from c from now on.
 *
 * A UE that moves to a new client port with the offer, as TS 33.203 has it,
 * registers over it from a contact of its own: contacts are told apart by
 * the port they came from. save_pending() stores a copy of the offer there,
 * from Security-Verify, while the offer itself and the tunnel it replaces
 * stay on the contact the 401 went to. c takes the offer over
 * (take_over_offer()), so that one contact holds each tunnel and no other's
 * expiry removes it. The contact the 401 went to stays registered, with its
 * tunnel, until c is: the 200 deletes it (drop_superseded_contact()) - or,
 * after a REGISTER challenged again, the 200 to the next one, which finds it
 * by the owner of the tunnel c took over.
 */
static int promote_offer(
		udomain_t *d, pcontact_t *c, struct receive_info *rcv, int registered)
{
	ipsec_t *pend = contact_tunnel(c->security_temp);
	ipsec_t *est = contact_tunnel(c->security);
	pcontact_t *x;
	security_t *old_sec;
	ip_addr_t ue_addr;
	uint16_t port;

	// a deferred offer is not installed, so cannot have been registered over
	if(!pend || tunnel_deferred(est, pend) || !rcv) {
		return 0;
	}
	if(!came_over_tunnel(pend, rcv)
			|| str2ipxbuf(&c->received_host, &ue_addr) < 0
			|| !ip_addr_cmp(&ue_addr, &rcv->src_ip)) {
		LM_DBG("contact [%.*s]: REGISTER from port %u to %u did not come over "
			   "the offered tunnel (%u to %u)\n",
				c->aor.len, c->aor.s, rcv->src_port, rcv->dst_port,
				pend->port_uc, pend->port_ps);
		return 0;
	}

	if((x = offering_contact(d, c, pend)) != NULL) {
		take_over_offer(d, c, x);
		pend = contact_tunnel(c->security_temp);
		if(registered) {
			drop_superseded_contact(d, x, est, pend);
		}
	}
	if(registered) {
		// the contact a 401 to c took est over from, kept until now
		if(est && (port = spi_get_owner(est->spi_pc, est->spi_ps)) != 0
				&& port != c->received_port
				&& (x = contact_from_port(d, c, port, NULL)) != NULL) {
			drop_superseded_contact(d, x, est, pend);
		}
		// c holds it for good: nothing to look for from here on
		spi_set_owner(pend->spi_pc, pend->spi_ps, c->received_port);
	}

	LM_DBG("contact [%.*s]: registered over the offered tunnel (client port "
		   "%d, server port %d, spi_pc %u, spi_ps %u)%s\n",
			c->aor.len, c->aor.s, pend->port_pc, pend->port_ps, pend->spi_pc,
			pend->spi_ps, est ? ", removing the previous one" : "");

	old_sec = c->security;
	if(ul.update_security(d, SECURITY_IPSEC, c->security_temp, c) != 0) {
		LM_ERR("Error updating security\n");
	}
	c->security = c->security_temp;
	// one that cannot be removed stays in security_temp, for the next 401 or
	// the contact's expiry to retry
	if(!est || retire_ipsec_tunnel(c->received_host, est, pend) == 0) {
		free_security_t(old_sec);
		old_sec = NULL;
	}
	if(ul.update_temp_security(d, SECURITY_IPSEC, old_sec, c) != 0) {
		LM_ERR("Error updating temp security\n");
	}
	c->security_temp = old_sec;

	return 1;
}

/**
 * Make the offered tunnel the registered one, once the UE has registered over
 * it (TS 33.203 7.4).
 *
 * Called by ims_registrar_pcscf with the domain locked, for each contact a 200
 * to a REGISTER updates; ipsec_create() does the same for a REGISTER that is
 * challenged again. Only a REGISTER that came in over the offered tunnel
 * proves the UE has it: a 200 to one sent over the registered tunnel (a
 * refresh that was not challenged), or to another contact's, changes nothing.
 * The registered tunnel it replaces is removed. See promote_offer() for a UE
 * that moved to a new client port.
 *
 * \return 1 if the tunnel was promoted, 0 if there was nothing to promote
 */
int ipsec_promote(udomain_t *d, pcontact_t *c, struct receive_info *rcv)
{
	return promote_offer(d, c, rcv, 1);
}

/**
 *
 */
int ims_ipsec_get_forward_proto(sip_msg_t *msg)
{
	struct sip_uri parsed_uri;
	str uri;

	if(msg == NULL) {
		LM_ERR("no message structure - fallback to UDP\n");
		return PROTO_UDP;
	}

	if(msg->dst_uri.s != NULL && msg->dst_uri.len > 0) {
		uri = msg->dst_uri;
	} else {
		if(msg->new_uri.s != NULL && msg->new_uri.len > 0) {
			uri = msg->new_uri;
		} else {
			uri = msg->first_line.u.request.uri;
		}
	}
	if(parse_uri(uri.s, uri.len, &parsed_uri) != 0) {
		LM_ERR("failed to parse next hop uri [%.*s]\n", uri.len, uri.s);
		return PROTO_UDP;
	}
	if(parsed_uri.proto == PROTO_NONE || parsed_uri.proto == PROTO_OTHER) {
		return PROTO_UDP;
	}
	return parsed_uri.proto;
}

int ipsec_forward(struct sip_msg *m, udomain_t *d, int _cflags)
{
	struct pcontact_info ci;
	pcontact_t *pcontact = NULL;
	int ret = IPSEC_CMD_FAIL; // FAIL by default
	unsigned char dst_proto = PROTO_UDP;
	unsigned short dst_port = 0;
	unsigned short src_port = 0;
	ip_addr_t via_host;
	struct sip_msg *req = NULL;
	tm_cell_t *t = NULL;
	struct socket_info *client_sock = NULL;
	via_body_t *vb = NULL;

	LM_DBG("processing with flags: 0x%x\n", _cflags);

	if(m->first_line.type == SIP_REPLY) {
		// Get request from reply
		t = tmb.t_gett();
		if(t == NULL || t == T_UNDEFINED) {
			LM_ERR("Error getting transaction\n");
			return ret;
		}
		req = t->uas.request;
	} else {
		req = m;
	}

	//
	// Find the contact
	//
	if(fill_contact(&ci, m, t, NULL, _cflags) != 0) {
		LM_ERR("Error filling in contact data\n");
		return ret;
	}

	ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

	if(ul.get_pcontact(d, &ci, &pcontact, _cflags & IPSEC_REVERSE_SEARCH) != 0
			|| pcontact == NULL) {
		LM_ERR("contact not found [%d:%.*s:%d]\n", (int)ci.via_prot,
				ci.via_host.len, ci.via_host.s, (int)ci.via_port);
		goto cleanup;
	}

	if(str2ipxbuf(&ci.via_host, &via_host) < 0) {
		LM_ERR("Error getting AF from ci.via_host\n");
		goto cleanup;
	}

	// A reply goes over the tunnel its request came in on, an installed
	// offer; a request, and the 401 that makes a deferred offer, over the one
	// the UE is registered over, or, before the first registration has
	// completed, the one it is registering over.
	ipsec_t *s = NULL;
	ipsec_t *est = contact_tunnel(pcontact->security);
	ipsec_t *pend = contact_tunnel(pcontact->security_temp);
	if(m->first_line.type == SIP_REPLY && pend && !tunnel_deferred(est, pend)
			&& came_over_tunnel(pend, &req->rcv)) {
		s = pend;
	} else if(est) {
		s = est;
	} else if(pend) {
		s = pend;
	} else if(pcontact->security_temp
			  && pcontact->security_temp->type == SECURITY_IPSEC
			  && pcontact->security_temp->data.ipsec) {
		s = pcontact->security_temp->data.ipsec;
	}

	if(s == NULL) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	// Update the destination
	//
	//       from sec-agree
	//            v
	// sip:host:port
	//       ^
	//    from URI
	//int uri_len = 4 /* strlen("sip:") */ + ci.via_host.len + 5 /* max len of port number */ ;

	if(_cflags & IPSEC_FORWARD_USEVIA) {
		vb = cscf_get_last_via(m);
	}

	if(m->first_line.type == SIP_REPLY) {
		/* reply handling */
		if(_cflags & IPSEC_FORWARD_USEVIA) {
			/* req - corresponding request from transaction */
			dst_proto = vb ? vb->proto : req->rcv.proto;

			// As per ETSI TS 133 203 V11.2.0, 7.1 Security association parameters
			// https://tools.ietf.org/html/rfc3261#section-18
			// From Reply and TCP send via the same ports Request was recevied.
			if(dst_proto == PROTO_TCP || dst_proto == PROTO_TLS) {
				src_port = req->rcv.dst_port;
				dst_port = req->rcv.src_port;
			} else {
				src_port = s->port_pc;
				if(vb
						&& ((vb->port == s->port_uc)
								|| (vb->port == s->port_us))) {
					dst_port = vb->port;
				} else {
					dst_port = s->port_us;
				}
			}
		} else {
			// dest proto from the corresponding request from transaction
			dst_proto = req->rcv.proto;
			if(dst_proto == PROTO_TCP || dst_proto == PROTO_TLS) {
				// for TCP/TLS send from P-CSCF server port
				src_port = s->port_ps;

				// for TCP/TLS send to UE client port
				dst_port = s->port_uc;
			} else {
				// for UDP send from P-CSCF client port
				src_port = s->port_pc;

				// for UDP send to UE server port
				dst_port = s->port_us;
			}
			// find send socket
			client_sock =
					grep_sock_info(via_host.af == AF_INET ? &ipsec_listen_addr
														  : &ipsec_listen_addr6,
							src_port, dst_proto);
			if(!client_sock) {
				/* fallback: P-CSCF client port to UE server port */
				src_port = s->port_pc;
				dst_port = s->port_us;
			}
		}
	} else {
		/* request handling */
		if(_cflags & IPSEC_FORWARD_USEVIA) {
			dst_proto = ims_ipsec_get_forward_proto(m);
		} else {
			dst_proto = pcontact->received_proto;
		}
		if((_cflags & IPSEC_TCPPORT_UEC)
				&& ((dst_proto == PROTO_TCP) || (dst_proto == PROTO_TLS))) {
			// TCP/TLS send from P-CSCF server port, UDP sends from P-CSCF client port
			src_port = s->port_ps;

			// for TCP/TLS sends to UE client port, for UDP sends to UE server port
			dst_port = s->port_uc;

		} else {
			// send from P-CSCF client port
			src_port = s->port_pc;

			// send to UE server port
			dst_port = s->port_us;
		}
	}

	// a slot without a tunnel - save_pending()'s half of an offer, or one
	// destroyed - has no P-CSCF or UE ports, and port 0 would match any socket
	if(src_port == 0 || dst_port == 0) {
		LM_ERR("No tunnel to forward over for contact [%.*s] (ports %u to "
			   "%u)\n",
				pcontact->aor.len, pcontact->aor.s, src_port, dst_port);
		goto cleanup;
	}

	if(_cflags & IPSEC_FORWARD_TRYTCP) {
		client_sock =
				grep_sock_info(via_host.af == AF_INET ? &ipsec_listen_addr
													  : &ipsec_listen_addr6,
						src_port, dst_proto);
		if(!client_sock && dst_proto == PROTO_UDP) {
			LM_ERR("UDP socket not found for IPSec forward, trying for TCP\n");
			dst_proto = PROTO_TCP;
		}
	}

	if(!(_cflags & IPSEC_NODSTURI_RESET) && (m->dst_uri.s != NULL)) {
		LM_DBG("resetting dst uri [%.*s]\n", m->dst_uri.len, m->dst_uri.s);
		pkg_free(m->dst_uri.s);
		m->dst_uri.s = NULL;
		m->dst_uri.len = 0;
	}

	if(!(_cflags & IPSEC_NODSTURI_RESET)) {
		char buf[1024];
		int buf_len;
		if((_cflags & IPSEC_SETDSTURI_FULL) && (dst_proto == PROTO_TCP)) {
			buf_len =
					snprintf(buf, sizeof(buf) - 1, "sip:%.*s:%d;transport=tcp",
							ci.via_host.len, ci.via_host.s, dst_port);
		} else if((_cflags & IPSEC_SETDSTURI_FULL)
				  && (dst_proto == PROTO_TLS)) {
			buf_len =
					snprintf(buf, sizeof(buf) - 1, "sip:%.*s:%d;transport=tls",
							ci.via_host.len, ci.via_host.s, dst_port);
		} else {
			buf_len = snprintf(buf, sizeof(buf) - 1, "sip:%.*s:%d",
					ci.via_host.len, ci.via_host.s, dst_port);
		}

		if((m->dst_uri.s = pkg_malloc(buf_len + 1)) == NULL) {
			LM_ERR("Error allocating memory for dst_uri\n");
			goto cleanup;
		}

		memcpy(m->dst_uri.s, buf, buf_len);
		m->dst_uri.len = buf_len;
		m->dst_uri.s[m->dst_uri.len] = '\0';
		LM_INFO("new destination URI: %.*s\n", m->dst_uri.len, m->dst_uri.s);
	}

	// Set send socket
	client_sock = grep_sock_info(
			via_host.af == AF_INET ? &ipsec_listen_addr : &ipsec_listen_addr6,
			src_port, dst_proto);
	if(!client_sock) {
		LM_ERR("Error calling grep_sock_info() for ipsec client port\n");
		goto cleanup;
	}
	m->force_send_socket = client_sock;

	// Set destination info
	struct dest_info dst_info;
	init_dest_info(&dst_info);
	dst_info.send_sock = client_sock;
	if(m->first_line.type == SIP_REQUEST
			&& (_cflags & IPSEC_SEND_FORCE_SOCKET)) {
		dst_info.send_flags.f |= SND_F_FORCE_SOCKET;
		m->fwd_send_flags.f |= SND_F_FORCE_SOCKET;
	}
#ifdef USE_DNS_FAILOVER
	if(!uri2dst(NULL, &dst_info, m, &m->dst_uri, dst_proto)) {
#else
	if(!uri2dst(&dst_info, m, &m->dst_uri, dst_proto)) {
#endif
		LM_ERR("Error converting dst_uri (%.*s) to struct dst_info\n",
				m->dst_uri.len, m->dst_uri.s);
		goto cleanup;
	}

	// Update dst_info in message
	if(m->first_line.type == SIP_REPLY) {
		if(t == NULL || t == T_UNDEFINED) {
			t = tmb.t_gett();
			if(t == NULL || t == T_UNDEFINED) {
				LM_ERR("Error getting transaction\n");
				goto cleanup;
			}
		}
		t->uas.response.dst = dst_info;
	}

	LM_DBG("Destination changed to [%d://%.*s], from [%d:%d]\n", dst_info.proto,
			m->dst_uri.len, m->dst_uri.s, dst_info.send_sock->proto,
			dst_info.send_sock->port_no);

	ret = IPSEC_CMD_SUCCESS; // all good, return SUCCESS

	if(m->first_line.type == SIP_REPLY
			&& m->first_line.u.reply.statuscode == 200
			&& req->first_line.u.request.method_value == METHOD_REGISTER) {
		if(add_supported_secagree_header(m) != 0) {
			goto cleanup;
		}
		if(add_require_secagree_header(m) != 0) {
			goto cleanup;
		}
	}

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
	pkg_free(ci.received_host.s);
	pkg_free(ci.aor.s);
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
}


int ipsec_destroy(struct sip_msg *m, udomain_t *d, str *uri)
{
	struct pcontact_info ci;
	pcontact_t *pcontact = NULL;
	int ret = IPSEC_CMD_FAIL; // FAIL by default
	tm_cell_t *t = NULL;

	if(m->first_line.type == SIP_REPLY) {
		t = tmb.t_gett();
	}

	// Find the contact
	if(fill_contact(&ci, m, t, uri, 0) != 0) {
		LM_ERR("Error filling in contact data\n");
		return ret;
	}

	ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

	if(ul.get_pcontact(d, &ci, &pcontact, 0) != 0 || pcontact == NULL) {
		LM_ERR("Contact doesn't exist\n");
		goto cleanup;
	}


	if(!contact_tunnel(pcontact->security)
			&& !contact_tunnel(pcontact->security_temp)) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	destroy_contact_tunnels(ci.received_host, pcontact);

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
	pkg_free(ci.received_host.s);
	pkg_free(ci.aor.s);
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
}

int ipsec_destroy_by_contact(
		udomain_t *_d, str *uri, str *received_host, int received_port)
{

	pcontact_t *pcontact = NULL;
	int ret = IPSEC_CMD_FAIL; // FAIL by default

	pcontact_info_t search_ci;
	memset(&search_ci, 0, sizeof(struct pcontact_info));

	sip_uri_t contact_uri;
	if(parse_uri(uri->s, uri->len, &contact_uri) != 0) {
		LM_WARN("Failed to parse aor [%.*s]\n", uri->len, uri->s);
		return ret;
	}

	search_ci.received_host.s = received_host->s;
	search_ci.received_host.len = received_host->len;
	search_ci.received_port = received_port;
	search_ci.received_proto =
			contact_uri.proto ? contact_uri.proto : PROTO_UDP;
	search_ci.searchflag = SEARCH_RECEIVED;
	search_ci.via_host.s = received_host->s;
	search_ci.via_host.len = received_host->len;
	search_ci.via_port = received_port;
	search_ci.via_prot = search_ci.received_proto;
	search_ci.aor.s = uri->s;
	search_ci.aor.len = uri->len;
	search_ci.reg_state = PCONTACT_ANY;

	if(ul.get_pcontact(_d, &search_ci, &pcontact, 0) != 0) {
		LM_ERR("Contact doesn't exist\n");
		return ret;
	}

	/* Lock this record while working with the data: */
	ul.lock_udomain(
			_d, &pcontact->via_host, pcontact->via_port, pcontact->via_proto);

	if(!contact_tunnel(pcontact->security)
			&& !contact_tunnel(pcontact->security_temp)) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	destroy_contact_tunnels(search_ci.received_host, pcontact);

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	/* Unlock domain */
	ul.unlock_udomain(
			_d, &pcontact->via_host, pcontact->via_port, pcontact->via_proto);
	return ret;
}

int ipsec_reconfig()
{
	int ret;

	if(ul.get_number_of_contacts() != 0) {
		return 0;
	}

	if(atomic_get(ipsec_reconfig_flag) != 0) {
		LM_DBG("reconfig already called\n");
		return 0;
	}
	atomic_set(ipsec_reconfig_flag, 1);

	if(clean_spi_list() != 0) {
		return 1;
	}

	ret = ipsec_cleanall();
	atomic_set(ipsec_reconfig_flag, 0);
	return ret;
}

int ipsec_cleanall()
{
	struct mnl_socket *nlsock = init_mnl_socket();
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
