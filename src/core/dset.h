/*
 * Copyright (C) 2001-2004 FhG FOKUS
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
 */

/*!
 * \file
 * \brief Kamailio core :: Destination set handling
 * \ingroup core
 * Module: \ref core
 */

#ifndef _DSET_H
#define _DSET_H

#include "ip_addr.h"
#include "qvalue.h"
#include "flags.h"
#include "parser/msg_parser.h"


extern unsigned int nr_branches;
extern int ruri_is_new;

extern str _ksr_contact_alias;
extern str _ksr_contact_salias;

#define DS_FLAGS 1
#define DS_PATH 2

/*! \brief
 * Structure for storing branch attributes
 */
typedef struct branch
{
	char uri[MAX_URI_SIZE];
	unsigned int len;

	/* Real destination of the request */
	char dst_uri[MAX_URI_SIZE];
	unsigned int dst_uri_len;

	/* Path set */
	char path[MAX_PATH_SIZE];
	unsigned int path_len;

	int q; /* Preference of the contact among
        * contact within the array */
	struct socket_info *force_send_socket;

	/* +sip.instance contact header param value */
	char instance[MAX_INSTANCE_SIZE];
	unsigned int instance_len;

	/* reg-id contact header param value */
	unsigned int reg_id;

	/* ruid value from usrloc */
	char ruid[MAX_RUID_SIZE];
	unsigned int ruid_len;

	char location_ua[MAX_UA_SIZE + 1];
	unsigned int location_ua_len;

	/* tcp connection id */
	int otcpid;

	/* Branch flags */
	flag_t flags;
} branch_t;

typedef struct branch_data
{
	str uri;
	str dst_uri;
	qvalue_t q;
	str path;
	unsigned int flags;
	socket_info_t *force_socket;
	str ruid;
	str instance;
	str location_ua;
	int otcpid;
} branch_data_t;

unsigned int get_nr_branches(void);

/*! \brief
 * Return pointer to branch[idx] structure
 */
branch_t *get_sip_branch(int idx);

int get_all_sip_branches(branch_t **vbranches, unsigned int *nbranches);
int set_all_sip_branches(branch_t *vbranches, unsigned int nbranches);

/*! \brief
 * Drop branch[idx]
 */
int drop_sip_branch(int idx);

/*! \brief
 * Push a new branch to current destination set
 */
branch_t *ksr_push_branch(sip_msg_t *msg, str *uri, str *dst_uri, str *path,
		qvalue_t q, unsigned int flags, struct socket_info *force_socket,
		str *instance, unsigned int reg_id, str *ruid, str *location_ua);

/*! \brief
 * Add a new branch to current destination set
 */
int append_branch(struct sip_msg *msg, str *uri, str *dst_uri, str *path,
		qvalue_t q, unsigned int flags, struct socket_info *force_socket,
		str *instance, unsigned int reg_id, str *ruid, str *location_ua);

/*! \brief kamailio compatible version */
#define km_append_branch(msg, uri, dst_uri, path, q, flags, force_socket) \
	append_branch(msg, uri, dst_uri, path, q, flags, force_socket, 0, 0, 0, 0)

/*! \brief ser compatible append_branch version.
 *  append_branch version compatible with ser: no path or branch flags support
 *  and no str parameters.
 */
static inline int ser_append_branch(struct sip_msg *msg, char *uri, int uri_len,
		char *dst_uri, int dst_uri_len, qvalue_t q,
		struct socket_info *force_socket)
{
	str s_uri, s_dst_uri;
	s_uri.s = uri;
	s_uri.len = uri_len;
	s_dst_uri.s = dst_uri;
	s_dst_uri.len = dst_uri_len;
	return append_branch(
			msg, &s_uri, &s_dst_uri, 0, q, 0, force_socket, 0, 0, 0, 0);
}


/*! \brief
 * Init the index to iterate through the list of transaction branches
 */
void init_branch_iterator(void);

/*! \brief
 * Return branch iterator position
 */
int get_branch_iterator(void);

/*! \brief
 * Set branch iterator position
 */
void set_branch_iterator(int n);

/*! \brief Get the next branch in the current transaction.
 * @return pointer to the uri of the next branch (which the length written in
 *  *len) or 0 if there are no more branches.
 */
char *next_branch(int *len, qvalue_t *q, str *dst_uri, str *path,
		unsigned int *flags, struct socket_info **force_socket, str *ruid,
		str *instance, str *location_ua);

char *get_branch(unsigned int i, int *len, qvalue_t *q, str *dst_uri, str *path,
		unsigned int *flags, struct socket_info **force_socket, str *ruid,
		str *instance, str *location_ua);

int get_branch_data(unsigned int i, branch_data_t *vbranch);
int next_branch_data(branch_data_t *vbranch);

/*! \brief
 * Empty the array of branches
 */
void clear_branches(void);


/*! \brief
 * Create a Contact header field from the
 * list of current branches
 */
char *print_dset(struct sip_msg *msg, int *len, int options);


/*! \brief
 * Set the q value of the Request-URI
 */
void set_ruri_q(qvalue_t q);


/*! \brief
 * Get src ip, port and proto as SIP uri or proxy address
 */
int msg_get_src_addr(sip_msg_t *msg, str *uri, int mode);

/*! \brief
 * Get the q value of the Request-URI
 */
qvalue_t get_ruri_q(void);


/*
 * Get actual Request-URI
 */
inline static int get_request_uri(struct sip_msg *_m, str *_u)
{
	*_u = *GET_RURI(_m);
	return 0;
}


#define ruri_mark_new() (ruri_is_new = 1)

#define ruri_mark_consumed() (ruri_is_new = 0)

/** returns whether or not ruri should be used when forking.
  * (useful for serial forking)
  * @return 0 if already marked as consumed, 1 if not.
 */
#define ruri_get_forking_state() (ruri_is_new)

int rewrite_uri(struct sip_msg *_m, str *_s);

void reset_uri(sip_msg_t *msg);

/*! \brief
 * Set a per-branch flag to 1.
 *
 * This function sets the value of one particular branch flag to 1.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be set (starting with 0)
 * @return 1 on success, -1 on failure.
 */
int setbflag(unsigned int branch, flag_t flag);

/*! \brief
 * Reset a per-branch flag value to 0.
 *
 * This function resets the value of one particular branch flag to 0.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be reset (starting with 0)
 * @return 1 on success, -1 on failure.
 */
int resetbflag(unsigned int branch, flag_t flag);

/*! \brief
 * Determine if a branch flag is set.
 *
 * This function tests the value of one particular per-branch flag.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be tested (starting with 0)
 * @return 1 if the branch flag is set, -1 if not or on failure.
 */
int isbflagset(unsigned int branch, flag_t flag);

/*! \brief
 * Get the value of all branch flags for a branch
 *
 * This function returns the value of all branch flags
 * combined in a single variable.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param res A pointer to a variable to store the result
 * @return 1 on success, -1 on failure
 */
int getbflagsval(unsigned int branch, flag_t *res);

/*! \brief
 * Set the value of all branch flags at once for a given branch.
 *
 * This function sets the value of all branch flags for a given
 * branch at once.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param val All branch flags combined into a single variable
 * @return 1 on success, -1 on failure
 */
int setbflagsval(unsigned int branch, flag_t val);

int ksr_contact_alias_set_name(str *aname);
int uri_add_rcv_alias(sip_msg_t *msg, str *uri, str *nuri);
int uri_restore_rcv_alias(str *uri, str *nuri, str *suri);
int uri_trim_rcv_alias(str *uri, str *nuri);
int ksr_uri_alias_encode(str *iuri, str *ualias);
int ksr_uri_alias_decode(str *ualias, str *ouri);
int ksr_uri_remove_param(str *ouri, str *pname, str *nuri);

int init_dst_set(void);

int set_aor_case_sensitive(int mode);

int get_aor_case_sensitive(void);

#endif /* _DSET_H */
