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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../str.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "im_locks.h"
#include "im_hash.h"

/* global variable for DB cache */
im_hash_t	*IM_HASH = NULL;

/* parses an ipv4 address with or without port number */
static int parse_ipv4(str *_s, struct ip_addr *_ip, unsigned short *_port)
{
	int	i, j;
	str	s;
	unsigned int	v;
	unsigned int	port;

	_ip->af = AF_INET;
	_ip->len = 4;

	s.s = _s->s;
	for (i = 0, j = 0; i < 4; i++) {
		while ((j < _s->len) && (_s->s[j] != '.') && (_s->s[j] != ':')) j++;
		if ((i != 3) && ((j >= _s->len) || (_s->s[j] == ':'))) return -1;

		s.len = j - (s.s - _s->s);
		if (str2int(&s, &v)) return -1;
		if (v > 255) return -1;
		_ip->u.addr[i] = v;

		if (i < 3) {
			s.s = _s->s + j + 1;
			j++;
		} else {
			if ((j < _s->len) && (_s->s[j] == ':')) {
				s.s = _s->s + j + 1;
				s.len = _s->len - j - 1;
				if (s.len <= 0) return -1;
				if (str2int(&s, &port)) return -1;
				*_port = port;
			} else {
				*_port = 0;
			}
		}
	}

	return 0;
}

/* parses an ipv6 address with or without port number
 * must be in the form [address]:port if port is specified
 */
static int parse_ipv6(str *_s, struct ip_addr *_ip, unsigned short *_port)
{
	char	buf[IP_ADDR_MAX_STR_SIZE+1];
	char	*c;
	str	s;
	unsigned int	port;

	_ip->af = AF_INET6;
	_ip->len = 16;

	if (_s->s[0] == '[') {
		c = memchr(_s->s, ']', _s->len);
		if (!c) return -1;
		if (c - _s->s - 1 > IP_ADDR_MAX_STR_SIZE - 1) return -1;
		memcpy(buf, _s->s + 1, (c - _s->s - 1)*sizeof(char));
		buf[c - _s->s - 1] = '\0';

		if (c[1] == ':') {
			/* port is specified */
			s.s = c+2;
			s.len = _s->len - (s.s - _s->s);
			if (s.len <= 0) return -1;
			if (str2int(&s, &port)) return -1;
			*_port = port;
		} else {
			*_port = 0;
		}
	} else {
		memcpy(buf, _s->s, _s->len*sizeof(char));
		buf[_s->len] = '\0';
		*_port = 0;
	}
	if (inet_pton(AF_INET6, buf, _ip->u.addr) <= 0) return -1;
	
	return 0;
}

/* parse ipv4 or ipv6 address
 */
int parse_ip(str *s, struct ip_addr *ip, unsigned short *port)
{
	if (!s || !s->len) return -1;

	if (memchr(s->s, '.', s->len)) {
		/* ipv4 address */
		if (parse_ipv4(s, ip, port)) {
			LOG(L_ERR, "ERROR: parse_ip(): failed to parse ipv4 iddress: %.*s\n",
					s->len, s->s);
			return -1;
		}
	} else {
		/* ipv6 address */
		if (parse_ipv6(s, ip, port)) {
			LOG(L_ERR, "ERROR: parse_ip(): failed to parse ipv6 iddress: %.*s\n",
					s->len, s->s);
			return -1;
		}
	}

	return 0;
}

/* allocate memory for a new ipmatch entry */
static im_entry_t *new_im_entry(char *ip, char *avp_val, unsigned int mark)
{
	im_entry_t	*entry;
	int		len;
	str		s;

	if (!ip) return NULL;

	entry = (im_entry_t *)shm_malloc(sizeof(im_entry_t));
	if (!entry) {
		LOG(L_ERR, "ERROR: new_im_entry(): not enough shm memory\n");
		return NULL;
	}
	memset(entry, 0, sizeof(im_entry_t));

	s.s = ip;
	s.len = strlen(ip);
	if (parse_ip(&s, &entry->ip, &entry->port)) {
		LOG(L_ERR, "ERROR: new_im_entry(): failed to parse ip iddress\n");
		goto error;
	}

	if (avp_val) {
		len = strlen(avp_val);
		entry->avp_val.s = (char *)shm_malloc(len * sizeof(char));
		if (!entry->avp_val.s) {
			LOG(L_ERR, "ERROR: new_im_entry(): not enough shm memory\n");
			goto error;
		}
		memcpy(entry->avp_val.s, avp_val, len);
		entry->avp_val.len = len;
	}

	entry->mark = mark;

	/* LOG(L_DBG, "DEBUG: new_im_entry(): ip=%s, port=%u, avp_val=%.*s, mark=%u\n",
			ip_addr2a(&entry->ip),
			entry->port,
			entry->avp_val.len, entry->avp_val.s,
			entry->mark);
	*/

	return entry;

error:
	if (entry->avp_val.s) shm_free(entry->avp_val.s);
	shm_free(entry);

	return NULL;
}

/* free the liked list of entries */
static void free_im_entry(im_entry_t *entry)
{
	if (!entry) return;
	if (entry->next) free_im_entry(entry->next);

	if (entry->avp_val.s) shm_free(entry->avp_val.s);
	shm_free(entry);
}

/* maximum number of entries in the hash table */
#define IM_HASH_ENTRIES	(255*4)

/* hash function for ipmatch hash table
 * in case of ipv4:
 *    summarizes the 4 unsigned char values
 * in case of ipv6:
 *    summarizes the 1st, 5th, 9th, and 13th unsigned char values
 */
unsigned int im_hash(struct ip_addr *ip)
{
	int		i, j;
	unsigned int	sum;

	j = ip->len / 4;
	sum = 0;
	for (i = 0; i < 4; i++) {
		sum += ip->u.addr[i * j];
	}
	return sum;
}

/* init global IM_HASH structure */
int init_im_hash(void)
{
	IM_HASH = (im_hash_t *)shm_malloc(sizeof(im_hash_t));
	if (!IM_HASH) {
		LOG(L_ERR, "ERROR: init_im_hash(): not enough shm memory\n");
		return -1;
	}
	IM_HASH->entries = NULL;

	reader_init_imhash_lock();
	writer_init_imhash_lock();

	return 0;
}

/* free memory allocated for the global cache */
void destroy_im_hash(void)
{
	if (!IM_HASH) return;

	if (IM_HASH->entries) free_im_hash(IM_HASH->entries);
	shm_free(IM_HASH);
	IM_HASH = NULL;
}

/* create a new impatch hash table */
im_entry_t **new_im_hash(void)
{
	im_entry_t	**hash;

	hash = (im_entry_t **)shm_malloc(IM_HASH_ENTRIES * sizeof(im_entry_t *));
	if (!hash) {
		LOG(L_ERR, "ERROR: new_im_hash(): not enough shm memory\n");
		return NULL;
	}

	memset(hash, 0, IM_HASH_ENTRIES * sizeof(im_entry_t *));
	return hash;
}

/* free the memory allocated for an ipmatch hash table,
 * and purge out entries
 */
void free_im_hash(im_entry_t **hash)
{
	int	i;

	if (!hash) return;

	for (i = 0; i < IM_HASH_ENTRIES; i++) {
		if (hash[i]) free_im_entry(hash[i]);
	}
	shm_free(hash);
}

/* free the memory allocated for an ipmatch hash table,
 * but do not purge out entries
 */
void delete_im_hash(im_entry_t **hash)
{
	if (hash) shm_free(hash);
}

/* create a new ipmatch entry and insert it into the hash table
 * return value
 *   0: success
 *  -1: error
 */
int insert_im_hash(char *ip, char *avp_val, unsigned int mark,
			im_entry_t **hash)
{
	im_entry_t	*entry;
	unsigned int	i;

	entry = new_im_entry(ip, avp_val, mark);
	if (!entry) {
		LOG(L_ERR, "ERROR: insert_im_hash(): failed to create ipmatch entry\n");
		return -1;
	}

	i = im_hash(&entry->ip);
	if (hash[i]) entry->next = hash[i];
	hash[i] = entry;

	return 0;
}
