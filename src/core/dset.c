/*
 * destination set
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 */

/** Kamailio core :: destination set / branches support.
 * @file dset.c
 * @ingroup core
 * Module: @ref core
 */

#include <string.h>
#include "dprint.h"
#include "config.h"
#include "parser/parser_f.h"
#include "parser/parse_uri.h"
#include "parser/msg_parser.h"
#include "globals.h"
#include "ut.h"
#include "hash_func.h"
#include "error.h"
#include "dset.h"
#include "mem/mem.h"
#include "ip_addr.h"

#define CONTACT "Contact: "
#define CONTACT_LEN (sizeof(CONTACT) - 1)

#define CONTACT_DELIM ", "
#define CONTACT_DELIM_LEN (sizeof(CONTACT_DELIM) - 1)

#define Q_PARAM ">;q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)


/* 
 * Where we store URIs of additional transaction branches
 * (sr_dst_max_branches - 1 : because of the default branch for r-uri, #0 in tm)
 */
static struct branch *branches = NULL;

/* how many of them we have */
unsigned int nr_branches = 0;

/* branch iterator */
static int branch_iterator = 0;

/* used to mark ruris "consumed" when branching (1 new, 0 consumed) */
int ruri_is_new = 0;

/* The q parameter of the Request-URI */
static qvalue_t ruri_q = Q_UNSPECIFIED;

/* Branch flags of the Request-URI */
static flag_t ruri_bflags;


int init_dst_set(void)
{
	if(sr_dst_max_branches<=0 || sr_dst_max_branches>=MAX_BRANCHES_LIMIT) {
		LM_ERR("invalid value for max branches parameter: %u\n",
				sr_dst_max_branches);
		return -1;
	}
	/* sr_dst_max_branches - 1 : because of the default branch for r-uri, #0 in tm */
	branches = (branch_t*)pkg_malloc((sr_dst_max_branches-1)*sizeof(branch_t));
	if(branches==NULL) {
		LM_ERR("not enough memory to initialize destination branches\n");
		return -1;
	}
	memset(branches, 0, (sr_dst_max_branches-1)*sizeof(branch_t));
	return 0;
}

/*! \brief
 * Return pointer to branch[idx] structure
 * @param idx - branch index
 *
 * @return  pointer to branch or NULL if invalid branch
 */
branch_t *get_sip_branch(int idx)
{
	if(nr_branches==0)
		return NULL;
	if(idx<0)
	{
		if((int)nr_branches + idx >= 0)
			return &branches[nr_branches+idx];
		return NULL;
	}
	if(idx < nr_branches)
		return &branches[idx];
	return 0;
}

/*! \brief
 * Drop branch[idx]
 * @param idx - branch index
 *
 * @return  0 on success, -1 on error
 */
int drop_sip_branch(int idx)
{
	if(nr_branches==0 || idx>=nr_branches)
		return 0;
	if(idx<0 && (int)nr_branches+idx<0)
		return 0;
	if(idx<0)
		idx += nr_branches;
	/* last branch */
	if(idx==nr_branches-1)
	{
		nr_branches--;
		return 0;
	}
	/* shift back one position */
	for(; idx<nr_branches-1; idx++)
		memcpy(&branches[idx], &branches[idx+1], sizeof(branch_t));
	nr_branches--;
	return 0;
}

static inline flag_t* get_bflags_ptr(unsigned int branch)
{
	if (branch == 0) return &ruri_bflags;
	if (branch - 1 < nr_branches) return &branches[branch - 1].flags;
	return NULL;
}


int setbflag(unsigned int branch, flag_t flag)
{
	flag_t* flags;

	if ((flags = get_bflags_ptr(branch)) == NULL) return -1;
	(*flags) |= 1 << flag;
	return 1;
}


int isbflagset(unsigned int branch, flag_t flag)
{
	flag_t* flags;

	if ((flags = get_bflags_ptr(branch)) == NULL) return -1;
	return ((*flags) & (1 << flag)) ? 1 : -1;
}


int resetbflag(unsigned int branch, flag_t flag)
{
	flag_t* flags;

	if ((flags = get_bflags_ptr(branch)) == NULL) return -1;
	(*flags) &= ~ (1 << flag);
	return 1;
}


int getbflagsval(unsigned int branch, flag_t* res)
{
	flag_t* flags;
	if (res == NULL) return -1;
	if ((flags = get_bflags_ptr(branch)) == NULL) return -1;
	*res = *flags;
	return 1;
}


int setbflagsval(unsigned int branch, flag_t val)
{
	flag_t* flags;
	if ((flags = get_bflags_ptr(branch)) == NULL) return -1;
	*flags = val;
	return 1;
}


/*
 * Initialize the branch iterator, the next
 * call to next_branch will return the first
 * contact from the dset array
 */
void init_branch_iterator(void)
{
	branch_iterator = 0;
}

/**
 * return the value of current branch iterator
 */
int get_branch_iterator(void)
{
	return branch_iterator;
}

/**
 * set the value of current branch interator
 */
void set_branch_iterator(int n)
{
	branch_iterator = n;
}


/** \brief Get a branch from the destination set
 * \return Return the 'i' branch from the dset
 * array, 0 is returned if there are no
 * more branches
 */
char* get_branch(unsigned int i, int* len, qvalue_t* q, str* dst_uri,
		 str* path, unsigned int *flags,
		 struct socket_info** force_socket,
		 str *ruid, str *instance, str *location_ua)
{
	if (i < nr_branches) {
		*len = branches[i].len;
		*q = branches[i].q;
		if (dst_uri) {
			dst_uri->len = branches[i].dst_uri_len;
			dst_uri->s = (dst_uri->len)?branches[i].dst_uri:0;
		}
		if (path) {
			path->len = branches[i].path_len;
			path->s = (path->len)?branches[i].path:0;
		}
		if (force_socket)
			*force_socket = branches[i].force_send_socket;
		if (flags)
			*flags = branches[i].flags;
		if (ruid) {
			ruid->len = branches[i].ruid_len;
			ruid->s = (ruid->len)?branches[i].ruid:0;
		}
		if (instance) {
			instance->len = branches[i].instance_len;
			instance->s = (instance->len)?branches[i].instance:0;
		}
		if (location_ua) {
			location_ua->len = branches[i].location_ua_len;
			location_ua->s
				= (location_ua->len)?branches[i].location_ua:0;
		}
		return branches[i].uri;
	} else {
		*len = 0;
		*q = Q_UNSPECIFIED;
		if (dst_uri) {
			dst_uri->s = 0;
			dst_uri->len = 0;
		}
		if (path) {
			path->s = 0;
			path->len = 0;
		}
		if (force_socket)
			*force_socket = 0;
		if (flags)
			*flags = 0;
		if (ruid) {
			ruid->s = 0;
			ruid->len = 0;
		}
		if (instance) {
			instance->s = 0;
			instance->len = 0;
		}
		if (location_ua) {
			location_ua->s = 0;
			location_ua->len = 0;
		}
		return 0;
	}
}



/** Return the next branch from the dset array.
 * 0 is returned if there are no more branches
 */
char* next_branch(int* len, qvalue_t* q, str* dst_uri, str* path,
		  unsigned int* flags, struct socket_info** force_socket,
		  str* ruid, str *instance, str *location_ua)
{
	char* ret;
	
	ret=get_branch(branch_iterator, len, q, dst_uri, path, flags,
		       force_socket, ruid, instance, location_ua);
	if (likely(ret))
		branch_iterator++;
	return ret;
}


/*
 * Empty the dset array
 */
void clear_branches(void)
{
	nr_branches = 0;
	ruri_q = Q_UNSPECIFIED;
	ruri_bflags = 0;
	ruri_mark_consumed();
}



/**  Add a new branch to the current transaction.
 * @param msg - sip message, used for getting the uri if not specified (0).
 * @param uri - uri, can be 0 (in which case the uri is taken from msg)
 * @param dst_uri - destination uri, can be 0.
 * @param path - path vector (passed in a string), can be 0.
 * @param q  - q value.
 * @param flags - per branch flags.
 * @param force_socket - socket that should be used when sending.
 *
 * @return  <0 (-1) on failure, 1 on success (script convention).
 */
int append_branch(struct sip_msg* msg, str* uri, str* dst_uri, str* path,
		  qvalue_t q, unsigned int flags,
		  struct socket_info* force_socket,
		  str* instance, unsigned int reg_id,
		  str* ruid, str* location_ua)
{
	str luri;

	/* if we have already set up the maximum number
	 * of branches, don't try new ones 
	 */
	if (unlikely(nr_branches == sr_dst_max_branches - 1)) {
		LM_ERR("max nr of branches exceeded\n");
		ser_error = E_TOO_MANY_BRANCHES;
		return -1;
	}

	/* if not parameterized, take current uri */
	if (uri==0 || uri->len==0 || uri->s==0) {
		if(msg==NULL) {
			LM_ERR("no new uri and no msg to take r-uri\n");
			ser_error = E_INVALID_PARAMS;
			return -1;
		}
		if (msg->new_uri.s)
			luri = msg->new_uri;
		else
			luri = msg->first_line.u.request.uri;
	} else {
		luri = *uri;
	}

	if (unlikely(luri.len > MAX_URI_SIZE - 1)) {
		LM_ERR("too long uri: %.*s\n", luri.len, luri.s);
		return -1;
	}

	/* copy the dst_uri */
	if (dst_uri && dst_uri->len && dst_uri->s) {
		if (unlikely(dst_uri->len > MAX_URI_SIZE - 1)) {
			LM_ERR("too long dst_uri: %.*s\n", dst_uri->len, dst_uri->s);
			return -1;
		}
		memcpy(branches[nr_branches].dst_uri, dst_uri->s, dst_uri->len);
		branches[nr_branches].dst_uri[dst_uri->len] = 0;
		branches[nr_branches].dst_uri_len = dst_uri->len;
	} else {
		branches[nr_branches].dst_uri[0] = '\0';
		branches[nr_branches].dst_uri_len = 0;
	}

	/* copy the path string */
	if (unlikely(path && path->len && path->s)) {
		if (unlikely(path->len > MAX_PATH_SIZE - 1)) {
			LM_ERR("too long path: %.*s\n", path->len, path->s);
			return -1;
		}
		memcpy(branches[nr_branches].path, path->s, path->len);
		branches[nr_branches].path[path->len] = 0;
		branches[nr_branches].path_len = path->len;
	} else {
		branches[nr_branches].path[0] = '\0';
		branches[nr_branches].path_len = 0;
	}

	/* copy the ruri */
	memcpy(branches[nr_branches].uri, luri.s, luri.len);
	branches[nr_branches].uri[luri.len] = 0;
	branches[nr_branches].len = luri.len;
	branches[nr_branches].q = q;

	branches[nr_branches].force_send_socket = force_socket;
	branches[nr_branches].flags = flags;

	/* copy instance string */
	if (unlikely(instance && instance->len && instance->s)) {
		if (unlikely(instance->len > MAX_INSTANCE_SIZE - 1)) {
			LM_ERR("too long instance: %.*s\n",
			    instance->len, instance->s);
			return -1;
		}
		memcpy(branches[nr_branches].instance, instance->s,
		       instance->len);
		branches[nr_branches].instance[instance->len] = 0;
		branches[nr_branches].instance_len = instance->len;
	} else {
		branches[nr_branches].instance[0] = '\0';
		branches[nr_branches].instance_len = 0;
	}

	/* copy reg_id */
	branches[nr_branches].reg_id = reg_id;

	/* copy ruid string */
	if (unlikely(ruid && ruid->len && ruid->s)) {
		if (unlikely(ruid->len > MAX_RUID_SIZE - 1)) {
			LM_ERR("too long ruid: %.*s\n",
			    ruid->len, ruid->s);
			return -1;
		}
		memcpy(branches[nr_branches].ruid, ruid->s,
		       ruid->len);
		branches[nr_branches].ruid[ruid->len] = 0;
		branches[nr_branches].ruid_len = ruid->len;
	} else {
		branches[nr_branches].ruid[0] = '\0';
		branches[nr_branches].ruid_len = 0;
	}

	if (unlikely(location_ua && location_ua->len && location_ua->s)) {
		if (unlikely(location_ua->len > MAX_UA_SIZE)) {
			LM_ERR("too long location_ua: %.*s\n",
			    location_ua->len, location_ua->s);
			return -1;
		}
		memcpy(branches[nr_branches].location_ua, location_ua->s,
		       location_ua->len);
		branches[nr_branches].location_ua[location_ua->len] = 0;
		branches[nr_branches].location_ua_len = location_ua->len;
	} else {
		branches[nr_branches].location_ua[0] = '\0';
		branches[nr_branches].location_ua_len = 0;
	}
	
	nr_branches++;
	return 1;
}


/*
 * Create a Contact header field from the dset
 * array
 */
char* print_dset(struct sip_msg* msg, int* len) 
{
	int cnt, i;
	unsigned int qlen;
	qvalue_t q;
	str uri;
	char* p, *qbuf;
	int crt_branch;
	static char dset[MAX_REDIRECTION_LEN];

	if (msg->new_uri.s) {
		cnt = 1;
		*len = msg->new_uri.len + 1 /*'<'*/;
		if (ruri_q != Q_UNSPECIFIED) {
			*len += Q_PARAM_LEN + len_q(ruri_q);
		} else {
			*len += 1 /*'>'*/;
		}
	} else {
		cnt = 0;
		*len = 0;
	}

	/* backup current branch index to restore it later */
	crt_branch = get_branch_iterator();

	init_branch_iterator();
	while ((uri.s = next_branch(&uri.len, &q, 0, 0, 0, 0, 0, 0, 0))) {
		cnt++;
		*len += uri.len + 1 /*'<'*/;
		if (q != Q_UNSPECIFIED) {
			*len += Q_PARAM_LEN + len_q(q);
		} else {
			*len += 1 /*'>'*/;
		}
	}

	if (cnt == 0) return 0;	

	*len += CONTACT_LEN + CRLF_LEN + (cnt - 1) * CONTACT_DELIM_LEN;

	if (*len + 1 > MAX_REDIRECTION_LEN) {
		LM_ERR("redirection buffer length exceed\n");
		goto error;
	}

	memcpy(dset, CONTACT, CONTACT_LEN);
	p = dset + CONTACT_LEN;
	if (msg->new_uri.s) {
		*p++ = '<';

		memcpy(p, msg->new_uri.s, msg->new_uri.len);
		p += msg->new_uri.len;

		if (ruri_q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(ruri_q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		} else {
			*p++ = '>';
		}
		i = 1;
	} else {
		i = 0;
	}

	init_branch_iterator();
	while ((uri.s = next_branch(&uri.len, &q, 0, 0, 0, 0, 0, 0, 0))) {
		if (i) {
			memcpy(p, CONTACT_DELIM, CONTACT_DELIM_LEN);
			p += CONTACT_DELIM_LEN;
		}

		*p++ = '<';

		memcpy(p, uri.s, uri.len);
		p += uri.len;
		if (q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		} else {
			*p++ = '>';
		}
		i++;
	}

	memcpy(p, CRLF " ", CRLF_LEN + 1);
	set_branch_iterator(crt_branch);
	return dset;

error:
	set_branch_iterator(crt_branch);
	return 0;
}


/*
 * Sets the q parameter of the Request-URI
 */
void set_ruri_q(qvalue_t q)
{
	ruri_q = q;
}


/*
 * Return the q value of the Request-URI
 */
qvalue_t get_ruri_q(void)
{
	return ruri_q;
}



/*
 * Rewrite Request-URI
 */
int rewrite_uri(struct sip_msg* _m, str* _s)
{
	char *buf = NULL;

	if(_m->new_uri.s==NULL || _m->new_uri.len<_s->len) {
		buf = (char*)pkg_malloc(_s->len + 1);
		if (!buf) {
			LM_ERR("No memory left to rewrite r-uri\n");
			return -1;
		}
	}
	if(buf!=NULL) {
		if(_m->new_uri.s)
			pkg_free(_m->new_uri.s);
	} else {
		buf = _m->new_uri.s;
	}

	memcpy(buf, _s->s, _s->len);
	buf[_s->len] = '\0';

	_m->parsed_uri_ok = 0;

	_m->new_uri.s = buf;
	_m->new_uri.len = _s->len;
	/* mark ruri as new and available for forking */
	ruri_mark_new();

	return 1;
}

/**
 * return src ip, port and proto as a SIP uri or proxy address
 * - value stored in a static buffer
 * - mode=0 return uri, mode=1 return proxy address
 */
int msg_get_src_addr(sip_msg_t *msg, str *uri, int mode)
{
	static char buf[80];
	char* p;
	str ip, port;
	int len;
	str proto;

	if (msg==NULL || uri==NULL) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ip.s = ip_addr2a(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);

	port.s = int2str(msg->rcv.src_port, &port.len);

	switch(msg->rcv.proto) {
		case PROTO_NONE:
		case PROTO_UDP:
			if(mode==0) {
				proto.s = 0; /* Do not add transport parameter, UDP is default */
				proto.len = 0;
			} else {
				proto.s = "udp";
				proto.len = 3;
			}
		break;

		case PROTO_TCP:
			proto.s = "tcp";
			proto.len = 3;
		break;

		case PROTO_TLS:
			proto.s = "tls";
			proto.len = 3;
		break;

		case PROTO_SCTP:
			proto.s = "sctp";
			proto.len = 4;
		break;

		case PROTO_WS:
		case PROTO_WSS:
			proto.s = "ws";
			proto.len = 2;
		break;

		default:
			LM_ERR("unknown transport protocol\n");
		return -1;
	}

	len = ip.len + 2*(msg->rcv.src_ip.af==AF_INET6)+ 1 + port.len;
	if (mode==0) {
		len += 4;
		if(proto.s) {
			len += TRANSPORT_PARAM_LEN;
			len += proto.len;
		}
	} else {
		len += proto.len + 1;
	}

	if (len > 79) {
		LM_ERR("buffer too small\n");
		return -1;
	}

	p = buf;
	if(mode==0) {
		memcpy(p, "sip:", 4);
		p += 4;
	} else {
		memcpy(p, proto.s, proto.len);
		p += proto.len;
		*p++ = ':';
	}

	if (msg->rcv.src_ip.af==AF_INET6)
		*p++ = '[';
	memcpy(p, ip.s, ip.len);
	p += ip.len;
	if (msg->rcv.src_ip.af==AF_INET6)
		*p++ = ']';

	*p++ = ':';

	memcpy(p, port.s, port.len);
	p += port.len;

	if (mode==0 && proto.s) {
		memcpy(p, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
		p += TRANSPORT_PARAM_LEN;

		memcpy(p, proto.s, proto.len);
		p += proto.len;
	}

	uri->s = buf;
	uri->len = len;
	uri->s[uri->len] = '\0';

	return 0;
}

/**
 * add alias parameter with encoding of source address
 * - nuri->s must point to a buffer of nuri->len size
 */
int uri_add_rcv_alias(sip_msg_t *msg, str *uri, str *nuri)
{
	char* p;
	str ip, port;
	int len;

	if (msg==NULL || uri==NULL || nuri==NULL) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ip.s = ip_addr2a(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);

	port.s = int2str(msg->rcv.src_port, &port.len);

	/*uri;alias=[ip]~port~proto*/
	len = uri->len+ip.len+port.len+12;
	if(len>=nuri->len) {
		LM_ERR("not enough space for new uri: %d\n", len);
		return -1;
	}
	p = nuri->s;
	memcpy(p, uri->s, uri->len);
	p += uri->len;
	memcpy(p, ";alias=", 7);
	p += 7;
	if (msg->rcv.src_ip.af == AF_INET6)
		*p++ = '[';
	memcpy(p, ip.s, ip.len);
	p += ip.len;
	if (msg->rcv.src_ip.af == AF_INET6)
		*p++ = ']';
	*p++ = '~';
	memcpy(p, port.s, port.len);
	p += port.len;
	*p++ = '~';
	*p++ = msg->rcv.proto + '0';
	nuri->len = p - nuri->s;
	nuri->s[nuri->len] = '\0';

	LM_DBG("encoded <%.*s> => [%.*s]\n",
			uri->len, uri->s, nuri->len, nuri->s);
	return 0;
}

/**
 * restore from alias parameter with encoding of source address
 * - nuri->s must point to a buffer of nuri->len size
 * - suri->s must point to a buffer of suri->len size
 */
int uri_restore_rcv_alias(str *uri, str *nuri, str *suri)
{
	char* p;
	str skip;
	str ip, port, sproto;
	int proto;

	if (uri==NULL || nuri==NULL || suri==NULL) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* sip:x;alias=1.1.1.1~0~0 */
	if(uri->len < 23) {
		/* no alias possible */
		return -2;
	}
	p = uri->s + uri->len-18;
	skip.s = 0;
	while(p>uri->s+5) {
		if(strncmp(p, ";alias=", 7)==0) {
			skip.s = p;
			break;
		}
		p--;
	}
	if(skip.s==0) {
		/* alias parameter not found */
		return -2;
	}
	p += 7;
	ip.s = p;
	p = (char*)memchr(ip.s, '~', (size_t)(uri->s+uri->len-ip.s));
	if(p==NULL) {
		/* proper alias parameter not found */
		return -2;
	}
	ip.len = p - ip.s;
	p++;
	if(p>=uri->s+uri->len) {
		/* proper alias parameter not found */
		return -2;
	}
	port.s = p;
	p = (char*)memchr(port.s, '~', (size_t)(uri->s+uri->len-port.s));
	if(p==NULL) {
		/* proper alias parameter not found */
		return -2;
	}
	port.len = p - port.s;
	p++;
	if(p>=uri->s+uri->len) {
		/* proper alias parameter not found */
		return -2;
	}
	proto = (int)(*p - '0');
	p++;

	if(p!=uri->s+uri->len && *p!=';') {
		/* proper alias parameter not found */
		return -2;
	}
	skip.len = (int)(p - skip.s);

	if(suri->len<=4+ip.len+1+port.len+11/*;transport=*/+4) {
		LM_ERR("address buffer too small\n");
		return -1;
	}
	if(nuri->len<=uri->len - skip.len) {
		LM_ERR("uri buffer too small\n");
		return -1;
	}

	p = nuri->s;
	memcpy(p, uri->s, (size_t)(skip.s-uri->s));
	p += skip.s-uri->s;
	memcpy(p, skip.s+skip.len, (size_t)(uri->s+uri->len - skip.s - skip.len));
	p += uri->s+uri->len - skip.s - skip.len;
	nuri->len = p - nuri->s;

	p = suri->s;
	strncpy(p, "sip:", 4);
	p += 4;
	strncpy(p, ip.s, ip.len);
	p += ip.len;
	*p++ = ':';
	strncpy(p, port.s, port.len);
	p += port.len;
	proto_type_to_str((unsigned short)proto, &sproto);
	if(sproto.len>0 && proto!=PROTO_UDP) {
		strncpy(p, ";transport=", 11);
		p += 11;
		strncpy(p, sproto.s, sproto.len);
		p += sproto.len;
	}
	suri->len = p - suri->s;

	LM_DBG("decoded <%.*s> => [%.*s] [%.*s]\n",
			uri->len, uri->s, nuri->len, nuri->s, suri->len, suri->s);

	return 0;
}

/* address of record (aor) management */

/* address of record considered case sensitive
 * - 0 = no; 1 = yes */
static int aor_case_sensitive=0;

int set_aor_case_sensitive(int mode)
{
	int r;
	r = aor_case_sensitive;
	aor_case_sensitive = mode;
	return r;
}

int get_aor_case_sensitive(void)
{
	return aor_case_sensitive;
}
