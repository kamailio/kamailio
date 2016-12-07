/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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

#include "../../onsend.h"
#include "../../parser/parse_fline.h"
#include "../../ut.h"
#include "permissions.h"
#include "im_hash.h"
#include "im_db.h"
#include "im_locks.h"
#include "ipmatch.h"

/* initialize ipmatch table */
int init_ipmatch(void)
{
	if (db_mode != ENABLE_CACHE) {
		/* not an error, but ipmatch functions will not operate */
		LOG(L_WARN, "WARNING: ipmatch_init(): Database cache is disabled, thus ipmatch functions cannot be used\n");
		return 0;
	}

	if (init_im_hash()) {
		LOG(L_ERR, "ERROR: ipmatch_init(): cannot init ipmatch hash table\n");
		return -1;
	}

	if (reload_im_cache()) {
		LOG(L_ERR, "ERROR: ipmatch_init(): error occured while caching ipmatch table\n");
		return -1;
	}
	return 0;
}

/* destroy function */
void clean_ipmatch(void) {
	LOG(L_DBG, "DEBUG: clean_ipmatch(): free shared memory required by ipmatch table\n");
	destroy_im_hash();
}

/* process variable to filter on entry->mark */
static unsigned int IM_FILTER = ~0;

/* tries to find the given IP address and port number in the global hash table
 * return value
 *   1: found
 *   0: not found
 *  -1: error
 */
static int ipmatch(struct ip_addr *ip, unsigned short port,
			avp_ident_t *avp)
{
	im_entry_t	*entry;
	int		ret;
	avp_value_t	avp_val;

	ret = 0;

	if (!IM_HASH) {
		LOG(L_CRIT, "ERROR: ipmatch(): ipmatch hash table is not initialied. "
			"Have you set the database url?\n");
		return 0;
	}

	/* lock hash table for reading */
	reader_lock_imhash();

	LOG(L_DBG, "DEBUG: ipmatch(): start searching... (ip=%s, port=%u, filter=%u)\n",
			ip_addr2a(ip), port, IM_FILTER);

	if (!IM_HASH->entries) {
		LOG(L_DBG, "DEBUG: ipmatch(): cache is empty\n");
		goto done;
	}

	entry = IM_HASH->entries[im_hash(ip)];
	while (entry) {
		if ((entry->mark & IM_FILTER)
		&& ((entry->port == 0) || (port == 0) || (entry->port == port))
		&& (ip_addr_cmp(&entry->ip, ip))) {

			LOG(L_DBG, "DEBUG: ipmatch(): entry found\n");

			/* shall we set the AVP? */
			if (avp) {
				/* delete AVP before inserting */
				delete_avp(avp->flags, avp->name);

				avp_val.s.s = entry->avp_val.s;
				avp_val.s.len = entry->avp_val.len;
				if (add_avp(avp->flags | AVP_VAL_STR, avp->name, avp_val)) {
					LOG(L_ERR, "ERROR: ipmatch(): failed to add AVP\n");
					ret = -1;
					break;
				}
			}

			ret = 1;
			break;
		}

		entry = entry->next;
	}

	if (!entry) LOG(L_DBG, "DEBUG: ipmatch(): entry not found\n");

done:
	/* release hash table */
	reader_release_imhash();

	/* reset filter */
	IM_FILTER = ~0;

	return ret;
}

/* wrapper function for ipmatch */
int ipmatch_2(struct sip_msg *msg, char *str1, char *str2)
{
	int		ret;
	fparam_t	*param1;
	str		s;
	struct ip_addr	*ip, ip_buf;
	unsigned short	port;
	unsigned int	iport;

	param1 = (fparam_t *)str1;

	switch(param1->type) {
	case FPARAM_AVP:
	case FPARAM_SELECT:
		if (get_str_fparam(&s, msg, param1)) {
			LOG(L_ERR, "ERROR: w_ipmatch_2(): could not get first parameter\n");
			return -1;
		}
		if (parse_ip(&s, &ip_buf, &port)) {
			LOG(L_ERR, "ERROR: w_ipmatch_2(): could not parse ip address\n");
			return -1;
		}
		ip = &ip_buf;
		break;

	case FPARAM_STR:
		if (param1->v.str.s[0] == 's') {
			/* "src" */
			ip = &msg->rcv.src_ip;
			port = msg->rcv.src_port;

		} else {
			/* "via2" */
			if (!msg->via2 &&
				((parse_headers(msg, HDR_VIA2_F, 0) == -1) || !msg->via2)) {

				LOG(L_ERR, "ERROR: w_ipmatch_2(): could not get 2nd VIA HF\n");
				return -1;
			}
			if (!msg->via2->received || !msg->via2->received->value.s) {
				LOG(L_ERR, "ERROR: w_ipmatch_2(): there is no received param in the 2nd VIA HF\n");
				return -1;
			}
			if (parse_ip(&msg->via2->received->value, &ip_buf, &port)) {
				LOG(L_ERR, "ERROR: w_ipmatch_2(): could not parse ip address\n");
				return -1;
			}
			ip = &ip_buf;
			if (!msg->via2->rport || !msg->via2->rport->value.s) {
				LOG(L_WARN, "WARNING: w_ipmatch_2(): there is no rport param in the 2nd VIA HF\n");
				port = 0;
			} else {
				if (str2int(&msg->via2->rport->value, &iport)) {			
					LOG(L_ERR, "ERROR: w_ipmatch_2(): invalid port number %.*s\n",
							msg->via2->rport->value.len, msg->via2->rport->value.s);
					return -1;
				}
 				port = iport;
			}
		}
		break;

	default:
		LOG(L_ERR, "ERROR: w_ipmatch_2(): unknown parameter type\n");
		return -1;
	}

	ret = ipmatch( 
			ip,
			port,
			(str2) ? &((fparam_t *)str2)->v.avp : 0
		);
			

	return (ret == 0) ? -1 : 1;
}

/* wrapper function for ipmatch */
int ipmatch_1(struct sip_msg *msg, char *str1, char *str2)
{
	return ipmatch_2(msg, str1, 0);
}

/* wrapper function for ipmatch */
int ipmatch_onsend(struct sip_msg *msg, char *str1, char *str2)
{
	int		ret;
	struct ip_addr	ip;
	unsigned short	port;
	char		*buf, *ch1, *ch2;
	struct msg_start	fl;
	str		*uri, s;

	if (str1[0] == 'd') {
		/* get info from destination address */
		port = su_getport(get_onsend_info()->to);
		su2ip_addr(&ip, get_onsend_info()->to);

	} else {
		/* get info from Request URI 
		we need to parse the first line again because the parsed uri can be
		changed by another branch */

		/* use another buffer pointer, because parse_first_list() modifies it */
		buf = get_onsend_info()->buf;
		parse_first_line(buf, get_onsend_info()->len, &fl);
		if (fl.type != SIP_REQUEST) {
			LOG(L_ERR, "ERROR: w_ipmatch_onsend(): message type is not request\n");
			return -1;
		}
		uri = &(fl.u.request.uri);

		/* find the host:port part in the uri */
		if ((!(ch1 = memchr(uri->s, '@', uri->len))) && (!(ch1 = memchr(uri->s, ':', uri->len)))) {
			LOG(L_ERR, "ERROR: w_ipmatch_onsend(): unable to get host:port part of uri: %.*s\n",
					uri->len, uri->s);
			return -1;
		}
		/* is there a parameter in the uri? */
		if ((ch2 = memchr(uri->s, ';', uri->len))) {
			s.s = ch1 + 1;
			s.len = ch2 - ch1 - 1;
		} else {
			s.s = ch1 + 1;
			s.len = uri->len - (ch1 - uri->s) - 1;
		}
		if (parse_ip(&s, &ip, &port)) {
			LOG(L_ERR, "ERROR: w_ipmatch_onsend(): could not parse ip address\n");
			return -1;
		}
	}

	ret = ipmatch( 
			&ip,
			port,
			0 /* AVP operations are unsafe in onsend_route! */
		);

	return (ret == 0) ? -1 : 1;	
}

/* set IM_FILTER */
int ipmatch_filter(struct sip_msg *msg, char *str1, char *str2)
{
	int	i;

	if (get_int_fparam(&i, msg, (fparam_t *)str1)) return -1;
	IM_FILTER = (unsigned int)i;
	return 1;
}
