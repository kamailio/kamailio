/*
 * $Id$
 *
 * destination set
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 */

/** destination set / branches support.
 * @file dset.c
 * @ingroup core
 * Module: @ref core
 */

#include <string.h>
#include "dprint.h"
#include "config.h"
#include "parser/parser_f.h"
#include "parser/msg_parser.h"
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
 * (-1 because of the default branch, #0)
 */
static struct branch branches[MAX_BRANCHES - 1];

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
		if(nr_branches + idx >= 0)
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
	if(idx<0 && nr_branches+idx<0)
		return 0;
	/* last branch */
	if(idx==nr_branches-1)
	{
		nr_branches--;
		return 0;
	}
	if(idx<0)
		idx = nr_branches+idx;
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
	if (unlikely(nr_branches == MAX_BRANCHES - 1)) {
		LOG(L_ERR, "max nr of branches exceeded\n");
		ser_error = E_TOO_MANY_BRANCHES;
		return -1;
	}

	/* if not parameterized, take current uri */
	if (uri==0 || uri->len==0 || uri->s==0) {
		if (msg->new_uri.s)
			luri = msg->new_uri;
		else
			luri = msg->first_line.u.request.uri;
	} else {
		luri = *uri;
	}

	if (unlikely(luri.len > MAX_URI_SIZE - 1)) {
		LOG(L_ERR, "too long uri: %.*s\n", luri.len, luri.s);
		return -1;
	}

	/* copy the dst_uri */
	if (dst_uri && dst_uri->len && dst_uri->s) {
		if (unlikely(dst_uri->len > MAX_URI_SIZE - 1)) {
			LOG(L_ERR, "too long dst_uri: %.*s\n", dst_uri->len, dst_uri->s);
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
			LOG(L_ERR, "too long path: %.*s\n", path->len, path->s);
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
			LOG(L_ERR, "too long instance: %.*s\n",
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
			LOG(L_ERR, "too long ruid: %.*s\n",
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
			LOG(L_ERR, "too long location_ua: %.*s\n",
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
		*len = msg->new_uri.len;
		if (ruri_q != Q_UNSPECIFIED) {
			*len += 1 + Q_PARAM_LEN + len_q(ruri_q);
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
		*len += uri.len;
		if (q != Q_UNSPECIFIED) {
			*len += 1 + Q_PARAM_LEN + len_q(q);
		}
	}

	if (cnt == 0) return 0;	

	*len += CONTACT_LEN + CRLF_LEN + (cnt - 1) * CONTACT_DELIM_LEN;

	if (*len + 1 > MAX_REDIRECTION_LEN) {
		LOG(L_ERR, "ERROR: redirection buffer length exceed\n");
		goto error;
	}

	memcpy(dset, CONTACT, CONTACT_LEN);
	p = dset + CONTACT_LEN;
	if (msg->new_uri.s) {
		if (ruri_q != Q_UNSPECIFIED) {
			*p++ = '<';
		}

		memcpy(p, msg->new_uri.s, msg->new_uri.len);
		p += msg->new_uri.len;

		if (ruri_q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(ruri_q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
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

		if (q != Q_UNSPECIFIED) {
			*p++ = '<';
		}

		memcpy(p, uri.s, uri.len);
		p += uri.len;
		if (q != Q_UNSPECIFIED) {
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
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

