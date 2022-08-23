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

#ifndef IMS_IPSEC_PCSCF_IPSEC
#define IMS_IPSEC_PCSCF_IPSEC

#include "../../core/str.h"
#include "../../core/ip_addr.h"


struct mnl_socket;

enum ipsec_policy_direction
{
	IPSEC_POLICY_DIRECTION_IN = 0,
	IPSEC_POLICY_DIRECTION_OUT = 1
};


struct mnl_socket *init_mnl_socket();
void close_mnl_socket(struct mnl_socket *sock);

int add_sa(struct mnl_socket *nl_sock, const struct ip_addr *src_addr_param,
		const struct ip_addr *dest_addr_param, int s_port, int d_port,
		int long id, str ck, str ik, str r_alg, str r_ealg);
int remove_sa(struct mnl_socket *nl_sock, str src_addr_param,
		str dest_addr_param, int s_port, int d_port, int long id,
		unsigned int af);

int add_policy(struct mnl_socket *mnl_socket,
		const struct ip_addr *src_addr_param,
		const struct ip_addr *dest_addr_param, int src_port, int dst_port,
		int long p_id, enum ipsec_policy_direction dir);
int remove_policy(struct mnl_socket *mnl_socket, str src_addr_param,
		str dest_addr_param, int src_port, int dst_port, int long p_id,
		unsigned int af, enum ipsec_policy_direction dir);

int clean_sa(struct mnl_socket *mnl_socket);
int clean_policy(struct mnl_socket *mnl_socket);

int delete_unused_tunnels();

#endif /* IMS_IPSEC_PCSCF_IPSEC */
