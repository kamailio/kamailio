/*
 * Serial forking functions
 *
 * Copyright (C) 2008 Juha Heinanen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * History:
 * -------
 *  2008-10-22: Moved functions from lcr module to tm module (jh)
 */

#include "../../qvalue.h"
#include "../../mem/mem.h"
#include "../../socket_info.h"
#include "../../usr_avp.h"
#include "../../dset.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "config.h"
#include "t_funcs.h"
#include "t_lookup.h"

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<2)

/* module parameter variable */
int fr_inv_timer_next = INV_FR_TIME_OUT_NEXT;

/* Struture where information regarding contacts is stored */
struct contact {
    str uri;
    qvalue_t q;
    str dst_uri;
    str path;
    unsigned int flags;
    struct socket_info* sock;
    unsigned short q_flag;
    struct contact *next;
};

/* 
 * Frees contact list used by load_contacts function
 */
static inline void free_contact_list(struct contact *curr) {
    struct contact *prev;
    while (curr) {
		prev = curr;
		curr = curr->next;
		pkg_free(prev);
    }
}

/* Encode branch info from contact struct to str */
static inline int encode_branch_info(str *info, struct contact *con)
{
    char *at, *s;
    int len;

    info->len = con->uri.len + con->dst_uri.len +
		con->path.len + MAX_SOCKET_STR + INT2STR_MAX_LEN + 5;
    info->s = pkg_malloc(info->len);
    if (!info->s) {
		LM_ERR("no memory left for branch info\n");
		return 0;
    }
    at = info->s;
    append_str(at, con->uri);
    append_chr(at, '\n');
    append_str(at, con->dst_uri);
    append_chr(at, '\n');
    append_str(at, con->path);
    append_chr(at, '\n');
    if (con->sock) {
		len = MAX_SOCKET_STR;
		if (socket2str(at, &len, con->sock) < 0) {
			LM_ERR("failed to convert socket to str\n");
			return 0;
		}
    } else {
		len = 0;
    }
    at = at + len;
    append_chr(at, '\n');
    s = int2str(con->flags, &len);
    append_mem_block(at, s, len);
    append_chr(at, '\n');
    info->len = at - info->s + 1;

    return 1;
}


/* Encode branch info from str */
static inline int decode_branch_info(char *info, str *uri, str *dst, str *path,
									 struct socket_info **sock,
									 unsigned int *flags)
{
    str s, host;
    int port, proto;
    char *pos, *at, *tmp;

    pos = strchr(info, '\n');
    uri->len = pos - info;
    if (uri->len) {
		uri->s = info;
    } else {
		uri->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    dst->len = pos - at;
    if (dst->len) {
		dst->s = at;
    } else {
		dst->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    path->len = pos - at;
    if (path->len) {
		path->s = at;
    } else {
		path->s = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    s.len = pos - at;
    if (s.len) {
		s.s = at;
		if ((tmp = as_asciiz(&s)) == NULL) {
			ERR("No memory left\n");
			return 0;
		}
		if (parse_phostport(tmp, &host.s, &host.len,
							&port, &proto) != 0) {
			LM_ERR("parsing of socket info <%.*s> failed\n",  s.len, s.s);
			pkg_free(tmp);
			return 0;
		}
		pkg_free(tmp);
		*sock = grep_sock_info(&host, (unsigned short)port,
							   (unsigned short)proto);
		if (*sock == 0) {
			LM_ERR("invalid socket <%.*s>\n", s.len, s.s);
			return 0;
		}
    } else {
		*sock = 0;
    }
    at = pos + 1;

    pos = strchr(at, '\n');
    s.len = pos - at;
    if (s.len) {
		s.s = at;
		if (str2int(&s, flags) != 0) {
			LM_ERR("failed to decode flags <%.*s>\n", s.len, s.s);
			return 0;
		}
    } else {
		*flags = 0;
    }

    return 1;
}


/* 
 * Loads contacts in destination set into contacts_avp in reverse
 * priority order and associated each contact with Q_FLAG telling if
 * contact is the last one in its priority class.  Finally, removes
 * all branches from destination set.
 */
int t_load_contacts(struct sip_msg* msg, char* key, char* value)
{
    str uri, tmp, dst_uri, path, branch_info, *ruri;
    qvalue_t first_q, q;
    struct contact *contacts, *next, *prev, *curr;
    int_str val;
    int first_idx, idx;
    struct socket_info* sock;
    unsigned int flags;
    struct cell *t;

    /* Check if contacts_avp has been defined */
    if (contacts_avp.n == 0) {
		LM_ERR("feature has been disabled - "
			   "to enable define contacts_avp module parameter");
		return -1;
    }

    /* Check if anything needs to be done */
    if (nr_branches == 0) {
		LM_DBG("nothing to do - no branches!\n");
		return 1;
    }

    t = get_t();
    ruri = (str *)0;

    if (!t || (t == T_UNDEFINED)) {

		/* No transaction yet - take first q from Request-URI */
		ruri = GET_RURI(msg);
		if (!ruri) {
			LM_ERR("no Request-URI found\n");
			return -1;
		}
		first_q = get_ruri_q();
		first_idx = 0;

    } else {

		/* Transaction exists - take first q from first branch */
	
		uri.s = get_branch(0, &uri.len, &first_q, &dst_uri, &path, &flags,
						   &sock);
		first_idx = 1;

    }

    /* Check if all q values are equal */
    for(idx = first_idx; (tmp.s = get_branch(idx, &tmp.len, &q, 0, 0, 0, 0))
			!= 0; idx++) {
		if (q != first_q) {
			goto rest;
		}
    }

    LM_DBG("nothing to do - all contacts have same q!\n");
    return 1;

rest:

    /* Allocate memory for first contact */
    contacts = (struct contact *)pkg_malloc(sizeof(struct contact));
    if (!contacts) {
		LM_ERR("no memory for contact info\n");
		return -1;
    }

    if (!t || (t == T_UNDEFINED)) {

		/* Insert Request-URI branch to first contact */
		contacts->uri.s = ruri->s;
		contacts->uri.len = ruri->len;
		contacts->dst_uri = msg->dst_uri;
		contacts->sock = msg->force_send_socket;
		getbflagsval(0, &contacts->flags);
		contacts->path = msg->path_vec;

    } else {
	
		/* Insert first branch to first contact */
		contacts->uri = uri;
		contacts->q = first_q;
		contacts->dst_uri = dst_uri;
		contacts->sock = sock;
		contacts->flags = flags;
		contacts->path = path;
    }

    contacts->q = first_q;
    contacts->next = (struct contact *)0;

    /* Insert (remaining) branches to contact list in increasing q order */

    for(idx = first_idx;
		(uri.s = get_branch(idx,&uri.len,&q,&dst_uri,&path,&flags,&sock))
			!= 0;
		idx++ ) {
		next = (struct contact *)pkg_malloc(sizeof(struct contact));
		if (!next) {
			LM_ERR("no memory for contact info\n");
			free_contact_list(contacts);
			return -1;
		}
		next->uri = uri;
		next->q = q;
		next->dst_uri = dst_uri;
		next->path = path;
		next->flags = flags;
		next->sock = sock;
		next->next = (struct contact *)0;
		prev = (struct contact *)0;
		curr = contacts;
		while (curr && (curr->q < q)) {
			prev = curr;
			curr = curr->next;
		}
		if (!curr) {
			next->next = (struct contact *)0;
			prev->next = next;
		} else {
			next->next = curr;
			if (prev) {
				prev->next = next;
			} else {
				contacts = next;
			}
		}    
    }

    /* Assign values for q_flags */
    curr = contacts;
    curr->q_flag = 0;
    while (curr->next) {
		if (curr->q < curr->next->q) {
			curr->next->q_flag = Q_FLAG;
		} else {
			curr->next->q_flag = 0;
		}
		curr = curr->next;
    }

    /* Add contacts to contacts_avp */
    curr = contacts;
    while (curr) {
		if (encode_branch_info(&branch_info, curr) == 0) {
			LM_ERR("encoding of branch info failed\n");
			free_contact_list(contacts);
			if (branch_info.s) pkg_free(branch_info.s);
			return -1;
		}
		val.s = branch_info;
		add_avp(contacts_avp_type|AVP_VAL_STR|(curr->q_flag),
				contacts_avp, val);
		pkg_free(branch_info.s);
		LM_DBG("loaded contact <%.*s> with q_flag <%d>\n",
			   val.s.len, val.s.s, curr->q_flag);
		curr = curr->next;
    }

    /* Clear all branches */
    clear_branches();

    /* Free contact list */
    free_contact_list(contacts);

    return 1;
}


/*
 * Adds to request a destination set that includes all highest priority
 * class contacts in contacts_avp.   If called from a route block,
 * rewrites the request uri with first contact and adds the remaining
 * contacts as branches.  If called from failure route block, adds all
 * contacts as branches.  Removes added contacts from contacts_avp.
 */
int t_next_contacts(struct sip_msg* msg, char* key, char* value)
{
    struct usr_avp *avp, *prev;
    int_str val;
    str uri, dst, path;
    struct socket_info *sock;
    unsigned int flags;
    struct cell *t;
	struct search_state st;

    /* Check if contacts_avp has been defined */
    if (contacts_avp.n == 0) {
		LM_ERR("feature has been disabled - "
			   "to enable define contacts_avp module parameter");
		return -1;
    }

    t = get_t();

    if (!t || (t == T_UNDEFINED)) {

		/* no transaction yet => load Request-URI and branches */

		if (route_type == FAILURE_ROUTE) {
			LM_CRIT("BUG - undefined transaction in failure route\n");
			return -1;
		}

		/* Find first contacts_avp value */
		avp = search_first_avp(contacts_avp_type, contacts_avp, &val, &st);
		if (!avp) {
			LM_DBG("no AVPs - we are done!\n");
			return 1;
		}

		LM_DBG("next contact is <%s>\n", val.s.s);

		if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
			== 0) {
			LM_ERR("decoding of branch info <%.*s> failed\n",
				   val.s.len, val.s.s);
			destroy_avp(avp);
			return -1;
		}

		/* Rewrite Request-URI */
		rewrite_uri(msg, &uri);
		set_dst_uri(msg, &dst);
		set_path_vector(msg, &path);
		msg->force_send_socket = sock;
		setbflagsval(0, flags);

		if (avp->flags & Q_FLAG) {
			destroy_avp(avp);
			/* Set fr_inv_timer */
			val.n = fr_inv_timer_next;
			if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val) != 0) {
				LM_ERR("setting of fr_inv_timer_avp failed\n");
				return -1;
			}
			return 1;
		}

		/* Append branches until out of branches or Q_FLAG is set */
		prev = avp;
		while ((avp = search_next_avp(&st, &val))) {
			destroy_avp(prev);

			LM_DBG("next contact is <%s>\n", val.s.s);

			if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
				== 0) {
				LM_ERR("decoding of branch info <%.*s> failed\n",
					   val.s.len, val.s.s);
				destroy_avp(avp);
				return -1;
			}

			if (km_append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
				LM_ERR("appending branch failed\n");
				destroy_avp(avp);
				return -1;
			}

			if (avp->flags & Q_FLAG) {
				destroy_avp(avp);
				val.n = fr_inv_timer_next;
				if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val)
					!= 0) {
					LM_ERR("setting of fr_inv_timer_avp failed\n");
					return -1;
				}
				return 1;
			}
			prev = avp;
		}
	
    } else {
	
		/* Transaction exists => only load branches */

		/* Find first contacts_avp value */
		avp = search_first_avp(contacts_avp_type, contacts_avp, &val, &st);
		if (!avp) return -1;

		/* Append branches until out of branches or Q_FLAG is set */
		prev = avp;
		do {

			LM_DBG("next contact is <%s>\n", val.s.s);

			if (decode_branch_info(val.s.s, &uri, &dst, &path, &sock, &flags)
				== 0) {
				LM_ERR("decoding of branch info <%.*s> failed\n",
					   val.s.len, val.s.s);
				destroy_avp(avp);
				return -1;
			}
	    
			if (km_append_branch(msg, &uri, &dst, &path, 0, flags, sock) != 1) {
				LM_ERR("appending branch failed\n");
				destroy_avp(avp);
				return -1;
			}

			if (avp->flags & Q_FLAG) {
				destroy_avp(avp);
				return 1;
			}

			prev = avp;
			avp = search_next_avp(&st, &val);
			destroy_avp(prev);

		} while (avp);

		/* Restore fr_inv_timer */
		val.n = default_tm_cfg.fr_inv_timeout;
		if (add_avp(fr_inv_timer_avp_type, fr_inv_timer_avp, val) != 0) {
			LM_ERR("setting of fr_inv_timer_avp failed\n");
			return -1;
		}
	
    }

    return 1;
}
