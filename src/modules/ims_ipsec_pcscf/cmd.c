/*
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * Copyright (C) 2019 Aleksandar Yosifov
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
#define IPSEC_DSTADDR_SEARCH (1<<2)
/* if set - use new r-uri address for IPSec tunnel search */
#define IPSEC_RURIADDR_SEARCH (1<<3)
/* if set - do not use alias for IPSec tunnel received details */
#define IPSEC_NOALIAS_SEARCH (1<<4)
/* if set - do not reset dst uri for IPsec forward */
#define IPSEC_NODSTURI_RESET (1<<5)

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
		pcontact_info_t *ci, sip_msg_t *m, tm_cell_t *t, int sflags)
{
	contact_body_t *cb = NULL;
	struct via_body *vb = NULL;
	struct sip_msg *req = NULL;
	char *srcip = NULL;

	if(!ci) {
		LM_ERR("called with null ptr\n");
		return -1;
	}

	memset(ci, 0, sizeof(struct pcontact_info));

	if(m->first_line.type == SIP_REQUEST) {
		char *alias_start;
		struct sip_uri uri;
		str suri;

		memset(&uri, 0, sizeof(struct sip_uri));

		if((sflags & IPSEC_DSTADDR_SEARCH) && m->dst_uri.s!=NULL
				&& m->dst_uri.len>0) {
			suri = m->dst_uri;
			LM_DBG("using dst uri for contact filling: %.*s\n",
					suri.len, suri.s);
		} else if((sflags & IPSEC_RURIADDR_SEARCH) && m->new_uri.s!=NULL
				&& m->new_uri.len>0) {
			suri = m->new_uri;
			LM_DBG("using new r-uri for contact filling: %.*s\n",
					suri.len, suri.s);
		} else {
			suri = m->first_line.u.request.uri;
			LM_DBG("using original uri for contact filling: %.*s\n",
					suri.len, suri.s);
		}
		if(parse_uri(suri.s, suri.len, &uri)<0) {
			LM_ERR("failed to parse the URI: %.*s / flags: 0x%x\n",
					suri.len, suri.s, sflags);
			return -1;
		}

		req = m;

		// populate host,port, aor in CI
		ci->via_host = uri.host;
		ci->via_port = uri.port_no ? uri.port_no : 5060;
		ci->via_prot = 0;
		ci->aor = m->first_line.u.request.uri;
		ci->searchflag = SEARCH_NORMAL;

		if(ci->via_host.s == NULL || ci->via_host.len == 0) {
			// no host included in RURI
			vb = cscf_get_ue_via(m);
			if(!vb) {
				LM_ERR("Reply No via body headers\n");
				return -1;
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
		if(alias_start!=NULL && *(alias_start-1)==';') {
			char *p, *port_s, *proto_s;
			char portbuf[5];
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
					return -1;
				}

				if((srcip = pkg_malloc(50)) == NULL) {
					LM_ERR("Error allocating memory for source IP address\n");
					return -1;
				}

				memcpy(srcip, alias_s.s, ci->received_host.len);
				ci->received_host.s = srcip;

				port_s = p + 1;
				p = _strnistr(port_s, "~", alias_s.len - ci->received_host.len);
				if(p != NULL) {
					memset(portbuf, 0, 5);
					memcpy(portbuf, port_s, (p - port_s));
					ci->received_port = atoi(portbuf);

					proto_s = p + 1;
					memset(portbuf, 0, 5);
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
				return -1;
			}

			ci->received_host.len = ip_addr2sbuf(&req->rcv.src_ip, srcip, 50);
			ci->received_host.s = srcip;
			ci->received_port = req->rcv.src_port;
			ci->received_proto = req->rcv.proto;
		}
	} else if(m->first_line.type == SIP_REPLY) {
		if(!t || t == (void *)-1) {
			LM_ERR("Reply without transaction\n");
			return -1;
		}

		req = t->uas.request;

		cb = cscf_parse_contacts(req);
		if(!cb || (!cb->contacts)) {
			LM_ERR("Reply No contact headers\n");
			return -1;
		}

		vb = cscf_get_ue_via(m);
		if(!vb) {
			LM_ERR("Reply No via body headers\n");
			return -1;
		}

		// populate CI with bare minimum
		ci->via_host = vb->host;
		ci->via_port = vb->port;
		ci->via_prot = vb->proto;
		ci->aor = cb->contacts->uri;
		ci->searchflag = SEARCH_RECEIVED;

		if((srcip = pkg_malloc(50)) == NULL) {
			LM_ERR("Error allocating memory for source IP address\n");
			return -1;
		}

		ci->received_host.len = ip_addr2sbuf(&req->rcv.src_ip, srcip, 50);
		ci->received_host.s = srcip;
		ci->received_port = req->rcv.src_port;
		ci->received_proto = req->rcv.proto;
	} else {
		LM_ERR("Unknown first line type: %d\n", m->first_line.type);
		return -1;
	}

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
	if(ck->len == 0) {
		LM_ERR("Error getting IK\n");
		return -1;
	}

	return 0;
}

static int update_contact_ipsec_params(
		ipsec_t *s, const struct sip_msg *m, ipsec_t *s_old)
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

	// Generate SPI
	if(s_old) {
		if(s_old->spi_pc && s_old->spi_ps && s_old->port_pc && s_old->port_ps) {
			LM_INFO("Reusing IPSEC tunnel\n");
			s->spi_pc = s_old->spi_pc;
			s->spi_ps = s_old->spi_ps;
			s->port_pc = s_old->port_pc;
			s->port_ps = s_old->port_ps;
			return 0;
		}
	}

	if(acquire_spi(&s->spi_pc, &s->spi_ps, &s->port_pc, &s->port_ps) == 0) {
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

	close_mnl_socket(sock);

	return 0;
}

static int destroy_ipsec_tunnel(
		str remote_addr, ipsec_t *s, unsigned short received_port)
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

	// Release SPIs
	release_spi(s->spi_pc, s->spi_ps, s->port_pc, s->port_ps);

	close_mnl_socket(sock);
	return 0;
}

void ipsec_on_expire(struct pcontact *c, int type, void *param)
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
	if(c->security_temp->type != SECURITY_IPSEC) {
		LM_ERR("Unsupported security type: %d\n", c->security_temp->type);
		return;
	}

	destroy_ipsec_tunnel(
			c->received_host, c->security_temp->data.ipsec, c->contact_port);
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
		LM_ERR("Error allcationg pkg memory for supported header str\n");
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
		LM_ERR("Error allcationg pkg memory for require header str\n");
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
			"ipsec-3gpp;prot=esp;mod=trans;spi-c=%d;spi-s=%d;port-c=%d;port-s=%"
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

int ipsec_create(struct sip_msg *m, udomain_t *d, int _cflags)
{
	pcontact_t *pcontact = NULL;
	struct pcontact_info ci;
	int ret = IPSEC_CMD_FAIL; // FAIL by default
	tm_cell_t *t = NULL;

	if(m->first_line.type == SIP_REPLY) {
		t = tmb.t_gett();
	}
	// Find the contact
	if(fill_contact(&ci, m, t, _cflags) != 0) {
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

	// Get security parameters
	if(pcontact->security_temp == NULL) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	if(pcontact->security_temp->type != SECURITY_IPSEC) {
		LM_ERR("Unsupported security type: %d\n",
				pcontact->security_temp->type);
		goto cleanup;
	}

	// Get request from reply
	if(!t)
		t = tmb.t_gett();
	if(!t || t == (void *)-1) {
		LM_ERR("Reply without transaction\n");
		goto cleanup;
	}

	struct sip_msg *req = t->uas.request;

	// Parse security parameters from the REGISTER request and get some data for the new tunnels
	security_t *req_sec_params = cscf_get_security(req);
	ipsec_t *s;
	ipsec_t *old_s = NULL;

	// Update contacts only for initial registration, for re-registration the existing contacts shouldn't be updated.
	if(ci.via_port == SIP_PORT) {
		LM_DBG("Registration for contact with AOR [%.*s], VIA [%d://%.*s:%d], "
			   "received_host [%d://%.*s:%d]\n",
				ci.aor.len, ci.aor.s, ci.via_prot, ci.via_host.len,
				ci.via_host.s, ci.via_port, ci.received_proto,
				ci.received_host.len, ci.received_host.s, ci.received_port);

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
		old_s = (ipsec_reuse_server_port && pcontact->security_temp)
						? pcontact->security_temp->data.ipsec
						: NULL;
	}

	if(update_contact_ipsec_params(s, m, old_s) != 0) {
		goto cleanup;
	}

	if(create_ipsec_tunnel(&req->rcv.src_ip, s) != 0) {
		goto cleanup;
	}

	if(ul.update_pcontact(d, &ci, pcontact) != 0) {
		LM_ERR("Error updating contact\n");
		goto cleanup;
	}

	if(ci.via_port == SIP_PORT) {
		// Update temp security parameters
		if(ul.update_temp_security(d, pcontact->security_temp->type,
				   pcontact->security_temp, pcontact)
				!= 0) {
			LM_ERR("Error updating temp security\n");
		}
	}


	if(add_supported_secagree_header(m) != 0) {
		goto cleanup;
	}

	if(add_security_server_header(m, s) != 0) {
		goto cleanup;
	}

	if(ci.via_port == SIP_PORT) {
		if(ul.register_ulcb(pcontact,
				   PCSCF_CONTACT_EXPIRE | PCSCF_CONTACT_DELETE, ipsec_on_expire,
				   (void *)&pcontact->received_port)
				!= 1) {
			LM_ERR("Error subscribing for contact\n");
			goto cleanup;
		}
	}

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	// Do not free str* sec_header! It will be freed in data_lump.c -> free_lump()
	ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
	pkg_free(ci.received_host.s);
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
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
	struct cell *t = NULL;

	if(m->first_line.type == SIP_REPLY) {
		// Get request from reply
		t = tmb.t_gett();
		if(!t) {
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
	if(fill_contact(&ci, m, t, _cflags) != 0) {
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

	if(pcontact->security_temp == NULL) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	//get security parameters
	if(pcontact->security_temp->type != SECURITY_IPSEC) {
		LM_ERR("Unsupported security type: %d\n",
				pcontact->security_temp->type);
		goto cleanup;
	}

	ipsec_t *s = pcontact->security_temp->data.ipsec;

	// Update the destination
	//
	//       from sec-agree
	//            v
	// sip:host:port
	//       ^
	//    from URI
	//int uri_len = 4 /* strlen("sip:") */ + ci.via_host.len + 5 /* max len of port number */ ;

	if(!(_cflags & IPSEC_NODSTURI_RESET) && (m->dst_uri.s!=NULL)) {
		LM_DBG("resetting dst uri [%.*s]\n", m->dst_uri.len, m->dst_uri.s);
		pkg_free(m->dst_uri.s);
		m->dst_uri.s = NULL;
		m->dst_uri.len = 0;
	}

	if(m->first_line.type == SIP_REPLY) {
		// for Reply get the dest proto from the received request
		dst_proto = req->rcv.proto;

		// for Reply and TCP sends from P-CSCF server port, for Reply and UDP sends from P-CSCF client port
		src_port = dst_proto == PROTO_TCP ? s->port_ps : s->port_pc;

		// for Reply and TCP sends to UE client port, for Reply and UDP sends to UE server port
		dst_port = dst_proto == PROTO_TCP ? s->port_uc : s->port_us;

		// Check send socket
		struct socket_info *client_sock =
				grep_sock_info(via_host.af == AF_INET ? &ipsec_listen_addr
													  : &ipsec_listen_addr6,
						src_port, dst_proto);
		if(!client_sock) {
			src_port = s->port_pc;
			dst_port = s->port_us;
		}
	} else {
		// for Request get the dest proto from the saved contact
		dst_proto = pcontact->received_proto;

		// for Request sends from P-CSCF client port
		src_port = s->port_pc;

		// for Request sends to UE server port
		dst_port = s->port_us;
	}

	if(!(_cflags & IPSEC_NODSTURI_RESET)) {
		char buf[1024];
		int buf_len = snprintf(buf, sizeof(buf) - 1, "sip:%.*s:%d", ci.via_host.len,
				ci.via_host.s, dst_port);

		if((m->dst_uri.s = pkg_malloc(buf_len + 1)) == NULL) {
			LM_ERR("Error allocating memory for dst_uri\n");
			goto cleanup;
		}

		memcpy(m->dst_uri.s, buf, buf_len);
		m->dst_uri.len = buf_len;
		m->dst_uri.s[m->dst_uri.len] = '\0';
		LM_ERR("new destination URI: %.*s\n", m->dst_uri.len, m->dst_uri.s);
	}

	// Set send socket
	struct socket_info *client_sock = grep_sock_info(
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
		struct cell *t = tmb.t_gett();
		if(!t) {
			LM_ERR("Error getting transaction\n");
			goto cleanup;
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
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
}


int ipsec_destroy(struct sip_msg *m, udomain_t *d)
{
	struct pcontact_info ci;
	pcontact_t *pcontact = NULL;
	int ret = IPSEC_CMD_FAIL; // FAIL by default
	tm_cell_t *t = NULL;

	if(m->first_line.type == SIP_REPLY) {
		t = tmb.t_gett();
	}

	// Find the contact
	if(fill_contact(&ci, m, t, 0) != 0) {
		LM_ERR("Error filling in contact data\n");
		return ret;
	}

	ul.lock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);

	if(ul.get_pcontact(d, &ci, &pcontact, 0) != 0 || pcontact == NULL) {
		LM_ERR("Contact doesn't exist\n");
		goto cleanup;
	}


	if(pcontact->security_temp == NULL) {
		LM_ERR("No security parameters found in contact\n");
		goto cleanup;
	}

	//get security parameters
	if(pcontact->security_temp->type != SECURITY_IPSEC) {
		LM_ERR("Unsupported security type: %d\n",
				pcontact->security_temp->type);
		goto cleanup;
	}

	destroy_ipsec_tunnel(ci.received_host, pcontact->security_temp->data.ipsec,
			pcontact->contact_port);

	ret = IPSEC_CMD_SUCCESS; // all good, set ret to SUCCESS, and exit

cleanup:
	ul.unlock_udomain(d, &ci.via_host, ci.via_port, ci.via_prot);
	pkg_free(ci.received_host.s);
	if(t) {
		tmb.t_uas_request_clean_parsed(t);
	}
	return ret;
}

int ipsec_reconfig()
{
	if(ul.get_number_of_contacts() != 0) {
		return 0;
	}

	if(clean_spi_list() != 0) {
		return 1;
	}

	return ipsec_cleanall();
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
