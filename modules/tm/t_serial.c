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
 */

#include "../../qvalue.h"
#include "../../mem/mem.h"
#include "../../socket_info.h"
#include "../../usr_avp.h"
#include "../../dset.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "../../xavp.h"
#include "config.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_lookup.h"

/* usr_avp flag for sequential forking */
#define Q_FLAG      (1<<2)

extern str ulattrs_xavp_name;

/* Struture where information regarding contacts is stored */
struct contact {
	str uri;
	qvalue_t q;
	str dst_uri;
	str path;
	struct socket_info* sock;
	str instance;
	str ruid;
	str location_ua;
	unsigned int flags;
	unsigned short q_flag;
	struct contact *next;
	sr_xavp_t *ulattrs;
};

struct instance_list {
	str instance;
	struct instance_list *next;
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

/* 
 * Frees instance list used by next_contacts function
 */
static inline void free_instance_list(struct instance_list *curr) {
	struct instance_list *prev;
	while (curr) {
		pkg_free(curr->instance.s);
		prev = curr;
		curr = curr->next;
		pkg_free(prev);
	}
}

static str uri_name = {"uri", 3};
static str dst_uri_name = {"dst_uri", 7};
static str path_name = {"path", 4};
static str sock_name = {"sock", 4};
static str instance_name = {"instance", 8};
static str flags_name = {"flags", 5};
static str q_flag_name = {"q_flag", 6};
static str ruid_name = {"ruid", 4};
static str ua_name = {"ua", 2};

void add_contacts_avp(str *uri, str *dst_uri, str *path, str *sock_str,
		unsigned int flags, unsigned int q_flag, str *instance,
		str *ruid, str *location_ua, sr_xavp_t *ulattrs_xavp)
{
	sr_xavp_t *record;
	sr_xval_t val;

	record = NULL;

	val.type = SR_XTYPE_STR;
	val.v.s = *uri;
	xavp_add_value(&uri_name, &val, &record);

	if (dst_uri->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *dst_uri;
		xavp_add_value(&dst_uri_name, &val, &record);
	}

	if (path->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *path;
		xavp_add_value(&path_name, &val, &record);
	}

	if (sock_str->len > 0) {
		val.v.s = *sock_str;
		xavp_add_value(&sock_name, &val, &record);
	}

	val.type = SR_XTYPE_INT;
	val.v.i = flags;
	xavp_add_value(&flags_name, &val, &record);

	val.type = SR_XTYPE_INT;
	val.v.i = q_flag;
	xavp_add_value(&q_flag_name, &val, &record);

	if (instance->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *instance;
		xavp_add_value(&instance_name, &val, &record);
	}

	if (ruid->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *ruid;
		xavp_add_value(&ruid_name, &val, &record);
	}

	if (location_ua->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *location_ua;
		xavp_add_value(&ua_name, &val, &record);
	}

	xavp_add(xavp_clone_level_nodata(ulattrs_xavp), &record);

	val.type = SR_XTYPE_XAVP;
	val.v.xavp = record;
	if(xavp_add_value(&contacts_avp, &val, NULL)==NULL) {
		/* failed to add xavps to root list */
		LM_ERR("failed to add xavps to root list\n");
		xavp_destroy_list(&record);
	}
}

/* 
 * Loads contacts in destination set into contacts_avp in reverse
 * priority order and associated each contact with Q_FLAG telling if
 * contact is the last one in its priority class.  Finally, removes
 * all branches from destination set.
 */
int t_load_contacts(struct sip_msg* msg, char* key, char* value)
{
	branch_t *branch;
	str *ruri, sock_str;
	struct contact *contacts, *next, *prev, *curr;
	int first_idx, idx, len;
	char sock_buf[MAX_SOCKET_STR];

	/* Check if contacts_avp has been defined */
	if (contacts_avp.len == 0) {
		LM_ERR("feature has been disabled - "
				"to enable define contacts_avp module parameter");
		return -1;
	}

	/* Check if anything needs to be done */
	LM_DBG("nr_branches is %d\n", nr_branches);

	if ((nr_branches == 0) || ((nr_branches == 1) && !ruri_is_new)) {
		LM_DBG("nothing to do - only one contact!\n");
		return 1;
	}

	/* Allocate memory for first contact */
	contacts = (struct contact *)pkg_malloc(sizeof(struct contact));
	if (!contacts) {
		LM_ERR("no memory for contact info\n");
		return -1;
	}
	memset(contacts, 0, sizeof(struct contact));

	if (ruri_is_new) {
		ruri = GET_RURI(msg);
		if (!ruri) {
			free_contact_list(contacts);
			LM_ERR("no Request-URI found\n");
			return -1;
		}
		/* Insert Request-URI branch to first contact */
		contacts->uri.s = ruri->s;
		contacts->uri.len = ruri->len;
		contacts->dst_uri = msg->dst_uri;
		contacts->sock = msg->force_send_socket;
		getbflagsval(0, &contacts->flags);
		contacts->path = msg->path_vec;
		contacts->q = get_ruri_q();
		contacts->instance = msg->instance;
		contacts->ruid = msg->ruid;
		contacts->location_ua = msg->location_ua;
		if (ulattrs_xavp_name.s != NULL)
		{
			contacts->ulattrs = xavp_get_by_index(&ulattrs_xavp_name, 0, NULL);
		}
		first_idx = 0;
	} else {
		/* Insert first branch to first contact */
		branch = get_sip_branch(0);
		contacts->uri.s = branch->uri;
		contacts->uri.len = branch->len;
		contacts->dst_uri.s = branch->dst_uri;
		contacts->dst_uri.len = branch->dst_uri_len;
		contacts->sock = branch->force_send_socket;
		contacts->flags = branch->flags;
		contacts->path.s = branch->path;
		contacts->path.len = branch->path_len;
		contacts->q = branch->q;
		contacts->instance.s = branch->instance;
		contacts->instance.len = branch->instance_len;
		contacts->ruid.s = branch->ruid;
		contacts->ruid.len = branch->ruid_len;
		contacts->location_ua.s = branch->location_ua;
		contacts->location_ua.len = branch->location_ua_len;
		if (ulattrs_xavp_name.s != NULL)
		{
			contacts->ulattrs = xavp_get_by_index(&ulattrs_xavp_name, 1, NULL);
		}
		first_idx = 1;
	}

	contacts->next = (struct contact *)0;

	/* Insert (remaining) branches to contact list in increasing q order */
	for (idx = first_idx; (branch = get_sip_branch(idx)) != 0; idx++) {

		next = (struct contact *)pkg_malloc(sizeof(struct contact));
		if (!next) {
			LM_ERR("no memory for contact info\n");
			free_contact_list(contacts);
			return -1;
		}

		memset(next, 0, sizeof(struct contact));
		next->uri.s = branch->uri;
		next->uri.len = branch->len;
		next->dst_uri.s = branch->dst_uri;
		next->dst_uri.len = branch->dst_uri_len;
		next->sock = branch->force_send_socket;
		next->flags = branch->flags;
		next->path.s = branch->path;
		next->path.len = branch->path_len;
		next->q = branch->q;
		next->instance.s = branch->instance;
		next->instance.len = branch->instance_len;
		next->ruid.s = branch->ruid;
		next->ruid.len = branch->ruid_len;
		next->location_ua.s = branch->location_ua;
		next->location_ua.len = branch->location_ua_len;
		if (ulattrs_xavp_name.s != NULL)
		{
			next->ulattrs = xavp_get_by_index(&ulattrs_xavp_name, idx + 1, NULL);
		}
		next->next = (struct contact *)0;

		prev = (struct contact *)0;
		curr = contacts;
		while (curr &&
				((curr->q < next->q) ||
				 ((curr->q == next->q) && (next->path.len == 0)))) {
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

		if (curr->sock) {
			len = MAX_SOCKET_STR - 1;
			if (socket2str(sock_buf, &len, curr->sock) < 0) {
				LM_ERR("failed to convert socket to str\n");
				free_contact_list(contacts);
				return -1;
			}
			sock_buf[len] = 0;
			sock_str.s = sock_buf;
			sock_str.len = len + 1;
		} else {
			sock_str.s = 0;
			sock_str.len = 0;
		}

		add_contacts_avp(&(curr->uri), &(curr->dst_uri), &(curr->path),
				&sock_str, curr->flags, curr->q_flag,
				&(curr->instance), &(curr->ruid), &(curr->location_ua),
				curr->ulattrs);

		curr = curr->next;
	}

	/* Clear all branches */
	clear_branches();
	if (ulattrs_xavp_name.s != NULL){
		xavp_rm_by_name(&ulattrs_xavp_name, 1, NULL);
	}

	/* Free contact list */
	free_contact_list(contacts);

	return 1;
}

void add_contact_flows_avp(str *uri, str *dst_uri, str *path, str *sock_str,
		unsigned int flags, str *instance, str *ruid,
		str *location_ua, sr_xavp_t *ulattrs_xavp)
{
	sr_xavp_t *record;
	sr_xval_t val;

	record = NULL;

	val.type = SR_XTYPE_STR;
	val.v.s = *uri;
	xavp_add_value(&uri_name, &val, &record);

	if (dst_uri->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *dst_uri;
		xavp_add_value(&dst_uri_name, &val, &record);
	}

	if (path->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *path;
		xavp_add_value(&path_name, &val, &record);
	}

	if (sock_str->len > 0) {
		val.v.s = *sock_str;
		xavp_add_value(&sock_name, &val, &record);
	}

	if (instance->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *instance;
		xavp_add_value(&instance_name, &val, &record);
	}

	if (ruid->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *ruid;
		xavp_add_value(&ruid_name, &val, &record);
	}

	if (location_ua->len > 0) {
		val.type = SR_XTYPE_STR;
		val.v.s = *location_ua;
		xavp_add_value(&ua_name, &val, &record);
	}

	xavp_add(ulattrs_xavp, &record);

	val.type = SR_XTYPE_INT;
	val.v.i = flags;
	xavp_add_value(&flags_name, &val, &record);

	val.type = SR_XTYPE_XAVP;
	val.v.xavp = record;
	if(xavp_add_value(&contact_flows_avp, &val, NULL)==NULL) {
		/* failed to add xavps to root list */
		LM_ERR("failed to add xavps to root list\n");
		xavp_destroy_list(&record);
	}
}

/*
 * Adds to request a new destination set that includes highest
 * priority class contacts in contacts_avp, but only one contact with same
 * +sip.instance value is included.  Others are added to contact_flows_avp
 * for later consumption by next_contact_flow().
 * Request URI is rewritten with first contact and the remaining contacts
 * (if any) are added as branches. Removes all highest priority contacts
 * from contacts_avp.
 * Returns 1, if contacts_avp was not empty and a destination set was
 * successfully added.  Returns -2, if contacts_avp was empty and thus
 * there was nothing to do. Returns -1 in case of an error. */
int t_next_contacts(struct sip_msg* msg, char* key, char* value)
{
	str uri, dst_uri, path, instance, host, sock_str, ruid, location_ua;
	struct socket_info *sock;
	unsigned int flags, q_flag;
	sr_xavp_t *xavp_list, *xavp, *prev_xavp, *vavp;
	int port, proto;
	struct instance_list *il, *ilp;

	/* Check if contacts_avp has been defined */
	if (contacts_avp.len == 0) {
		LM_ERR("feature has been disabled - "
				"to enable define contacts_avp module parameter");
		return -1;
	}

	/* Load Request-URI and branches */

	/* Find first contacts_avp value */
	xavp_list = xavp_get(&contacts_avp, NULL);
	if (!xavp_list) {
		LM_DBG("no contacts in contacts_avp - we are done!\n");
		return -2;
	}

	xavp = xavp_list;

	vavp = xavp_get(&uri_name, xavp->val.v.xavp);
	uri = vavp->val.v.s;

	vavp = xavp_get(&dst_uri_name, xavp->val.v.xavp);
	if (vavp != NULL) {
		dst_uri = vavp->val.v.s;
	} else {
		dst_uri.s = 0;
		dst_uri.len = 0;
	}

	vavp = xavp_get(&path_name, xavp->val.v.xavp);
	if (vavp != NULL) {
		path = vavp->val.v.s;
	} else {
		path.s = 0;
		path.len = 0;
	}

	vavp = xavp_get(&sock_name, xavp->val.v.xavp);
	if (vavp != NULL) {
		sock_str.s = vavp->val.v.s.s;
		if (parse_phostport(sock_str.s, &host.s, &host.len, &port, &proto)
				!= 0) {
			LM_ERR("parsing of socket info <%s> failed\n", sock_str.s);
			xavp_rm(xavp_list, NULL);
			return -1;
		}
		sock = grep_sock_info(&host, (unsigned short)port,
				(unsigned short)proto);
		if (sock == 0) {
			xavp_rm(xavp_list, NULL);
			return -1;
		}
	} else {
		sock = NULL;
	}

	vavp = xavp_get(&flags_name, xavp->val.v.xavp);
	flags = vavp->val.v.i;

	vavp = xavp_get(&q_flag_name, xavp->val.v.xavp);
	q_flag = vavp->val.v.i;

	vavp = xavp_get(&instance_name, xavp->val.v.xavp);
	il = (struct instance_list *)0;
	if ((vavp != NULL) && !q_flag) {
		instance = vavp->val.v.s;
		il = (struct instance_list *)pkg_malloc(sizeof(struct instance_list));
		if (!il) {
			LM_ERR("no memory for instance list entry\n");
			return -1;
		}
		il->instance.s = pkg_malloc(instance.len);
		if (!il->instance.s) {
			pkg_free(il);
			LM_ERR("no memory for instance list instance\n");
			return -1;
		}
		il->instance.len = instance.len;
		memcpy(il->instance.s, instance.s, instance.len);
		il->next = (struct instance_list *)0;
		set_instance(msg, &instance);
	} else {
		instance.s = 0;
		instance.len = 0;
	}

	vavp = xavp_get(&ruid_name, xavp->val.v.xavp);
	if (vavp != NULL) {
		ruid = vavp->val.v.s;
	} else {
		ruid.s = 0;
		ruid.len = 0;
	}
	vavp = xavp_get(&ua_name, xavp->val.v.xavp);
	if (vavp != NULL) {
		location_ua = vavp->val.v.s;
	} else {
		location_ua.s = 0;
		location_ua.len = 0;
	}

	if (ulattrs_xavp_name.s != NULL)
	{
		vavp = xavp_extract(&ulattrs_xavp_name, &xavp->val.v.xavp);
		xavp_insert(vavp, 0, NULL);
	}

	/* Rewrite Request-URI */
	rewrite_uri(msg, &uri);

	if (dst_uri.len) {
		set_dst_uri(msg, &dst_uri);
	} else {
		reset_dst_uri(msg);
	}

	if (path.len) {
		set_path_vector(msg, &path);
	} else {
		reset_path_vector(msg);
	}

	set_force_socket(msg, sock);

	setbflagsval(0, flags);

	set_ruid(msg, &ruid);

	set_ua(msg, &location_ua);

	/* Check if there was only one contact at this priority */
	if (q_flag) {
		xavp_rm(xavp, NULL);
		return 1;
	}

	/* Append branches until out of branches or Q_FLAG is set */
	/* If a branch has same instance value as some previous branch, */
	/* instead of appending it, add it to contact_flows_avp */

	xavp_rm_by_name(&contact_flows_avp, 1, NULL);
	prev_xavp = xavp;

	while ((xavp = xavp_get_next(prev_xavp)) != NULL) {

		xavp_rm(prev_xavp, NULL);

		vavp = xavp_get(&q_flag_name, xavp->val.v.xavp);
		q_flag = vavp->val.v.i;

		vavp = xavp_get(&uri_name, xavp->val.v.xavp);
		uri = vavp->val.v.s;

		vavp = xavp_get(&dst_uri_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			dst_uri = vavp->val.v.s;
		} else {
			dst_uri.len = 0;
		}

		vavp = xavp_get(&path_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			path = vavp->val.v.s;
		} else {
			path.len = 0;
		}

		vavp = xavp_get(&sock_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			sock_str = vavp->val.v.s;
			if (parse_phostport(sock_str.s, &host.s, &host.len, &port, &proto)
					!= 0) {
				LM_ERR("parsing of socket info <%s> failed\n", sock_str.s);
				free_instance_list(il);
				xavp_rm(xavp_list, NULL);
				return -1;
			}
			sock = grep_sock_info(&host, (unsigned short)port,
					(unsigned short)proto);
			if (sock == 0) {
				free_instance_list(il);
				xavp_rm(xavp_list, NULL);
				return -1;
			}
		} else {
			sock = NULL;
		}

		vavp = xavp_get(&flags_name, xavp->val.v.xavp);
		flags = vavp->val.v.i;

		vavp = xavp_get(&ruid_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			ruid = vavp->val.v.s;
		} else {
			ruid.s = 0;
			ruid.len = 0;
		}

		vavp = xavp_get(&ua_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			location_ua = vavp->val.v.s;
		} else {
			location_ua.s = 0;
			location_ua.len = 0;
		}

		vavp = xavp_get(&instance_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			instance = vavp->val.v.s;
			ilp = il;
			while (ilp) {
				if ((instance.len == ilp->instance.len) &&
						(strncmp(instance.s, ilp->instance.s, instance.len) == 0))
					break;
				ilp = ilp->next;
			}
			if (ilp) {
				vavp = (ulattrs_xavp_name.s != NULL)?xavp_extract(&ulattrs_xavp_name, &xavp->val.v.xavp):NULL;
				add_contact_flows_avp(&uri, &dst_uri, &path, &sock_str,
						flags, &instance, &ruid, &location_ua, vavp);
				goto check_q_flag;
			}
			if (!q_flag) {
				ilp = (struct instance_list *)
					pkg_malloc(sizeof(struct instance_list));
				if (!ilp) {
					LM_ERR("no memory for instance list element\n");
					free_instance_list(il);
					return -1;
				}
				ilp->instance.s = pkg_malloc(instance.len);
				if (!ilp->instance.s) {
					LM_ERR("no memory for instance list instance\n");
					pkg_free(ilp);
					free_instance_list(il);
					return -1;
				}
				ilp->instance.len = instance.len;
				memcpy(ilp->instance.s, instance.s, instance.len);
				ilp->next = il;
				il = ilp;
			}
		} else {
			instance.s = 0;
			instance.len = 0;
		}

		LM_DBG("Appending branch uri-'%.*s' dst-'%.*s' path-'%.*s' inst-'%.*s'"
				" ruid-'%.*s' location_ua-'%.*s'\n",
				uri.len, uri.s,
				dst_uri.len, (dst_uri.len > 0)?dst_uri.s:"",
				path.len, (path.len>0)?path.s:"",
				instance.len, (instance.len>0)?instance.s:"",
				ruid.len, (ruid.len>0)?ruid.s:"",
				location_ua.len, (location_ua.len>0)?location_ua.s:"");
		if (append_branch(msg, &uri, &dst_uri, &path, 0, flags, sock, &instance, 0,
					&ruid, &location_ua) != 1) {
			LM_ERR("appending branch failed\n");
			free_instance_list(il);
			xavp_rm(xavp_list, NULL);
			return -1;
		}

		if (ulattrs_xavp_name.s != NULL)
		{
			vavp = xavp_extract(&ulattrs_xavp_name, &xavp->val.v.xavp);
			xavp_insert(vavp, nr_branches, NULL);
		}

check_q_flag:
		if (q_flag) {
			free_instance_list(il);
			xavp_rm(xavp, NULL);
			return 1;
		}

		prev_xavp = xavp;
	}

	free_instance_list(il);
	xavp_rm(prev_xavp, NULL);

	return 1;
}

/*
 * Adds to request a new destination set that includes contacts
 * from contact_flows_avp.  Only one contact with same +sip.instance
 * value is included.
 * Request URI is rewritten with first contact and the remaining contacts
 * (if any) are added as branches. Removes all used contacts
 * from contacts_avp.
 * Returns 1, if contact_flows_avp was not empty and a destination set was
 * successfully added.  Returns -2, if contact_flows_avp was empty and thus
 * there was nothing to do. Returns -1 in case of an error. */
int t_next_contact_flow(struct sip_msg* msg, char* key, char* value)
{
	str uri, dst_uri, path, instance, host, ruid, location_ua;
	str this_instance;
	struct socket_info *sock;
	unsigned int flags;
	sr_xavp_t *xavp_list, *xavp, *next_xavp, *vavp;
	char *tmp;
	int port, proto;

	/* Check if contact_flows_avp has been defined */
	if (contact_flows_avp.len == 0) {
		LM_ERR("feature has been disabled - "
				"to enable define contact_flows_avp module parameter");
		return -1;
	}

	/* Load Request-URI and branches */
	t_get_this_branch_instance(msg, &this_instance);

	if (this_instance.len == 0)
	{
		LM_DBG("No instance on this branch\n");
		return -2;
	}
	/* Find first contact_flows_avp value */
	xavp_list = xavp_get(&contact_flows_avp, NULL);
	if (!xavp_list) {
		LM_DBG("no contacts in contact_flows_avp - we are done!\n");
		return -2;
	}

	xavp = xavp_list;

	while (xavp) {
		next_xavp = xavp_get_next(xavp);

		vavp = xavp_get(&instance_name, xavp->val.v.xavp);
		if (vavp == NULL)
		{
			/* Does not match this instance */
			goto next_xavp;
		}
		else
		{
			instance = vavp->val.v.s;
			if ((instance.len != this_instance.len) ||
					(strncmp(instance.s, this_instance.s, instance.len) != 0))
				/* Does not match this instance */
				goto next_xavp;
		}

		vavp = xavp_get(&uri_name, xavp->val.v.xavp);
		uri = vavp->val.v.s;

		vavp = xavp_get(&dst_uri_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			dst_uri = vavp->val.v.s;
		} else {
			dst_uri.len = 0;
		}

		vavp = xavp_get(&path_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			path = vavp->val.v.s;
		} else {
			path.len = 0;
		}

		vavp = xavp_get(&sock_name, xavp->val.v.xavp);
		if (vavp != NULL) {
			tmp = vavp->val.v.s.s;
			if (parse_phostport(tmp, &host.s, &host.len, &port, &proto) != 0) {
				LM_ERR("parsing of socket info <%s> failed\n", tmp);
				xavp_rm(xavp, NULL);
				return -1;
			}
			sock = grep_sock_info(&host, (unsigned short)port,
					(unsigned short)proto);
			if (sock == 0) {
				xavp_rm(xavp, NULL);
				return -1;
			}
		} else {
			sock = NULL;
		}

		vavp = xavp_get(&flags_name, xavp->val.v.xavp);
		flags = vavp->val.v.i;

		vavp = xavp_get(&ruid_name, xavp->val.v.xavp);
		ruid = vavp->val.v.s;

		vavp = xavp_get(&ua_name, xavp->val.v.xavp);
		location_ua = vavp->val.v.s;

		LM_DBG("Appending branch uri-'%.*s' dst-'%.*s' path-'%.*s'"
				" inst-'%.*s' ruid-'%.*s' location_ua-'%.*s'\n",
				uri.len, uri.s,
				dst_uri.len, (dst_uri.len > 0)?dst_uri.s:"",
				path.len, (path.len>0)?path.s:"",
				instance.len, (instance.len>0)?instance.s:"",
				ruid.len, ruid.s, location_ua.len, location_ua.s);
		if (append_branch(msg, &uri, &dst_uri, &path, 0, flags, sock, &instance, 0,
					&ruid, &location_ua) != 1) {
			LM_ERR("appending branch failed\n");
			xavp_rm(xavp_list, NULL);
			return -1;
		}

		if (ulattrs_xavp_name.s != NULL)
		{
			vavp = xavp_extract(&ulattrs_xavp_name, &xavp->val.v.xavp);
			xavp_insert(vavp, nr_branches, NULL);
		}

		xavp_rm(xavp, NULL);
		return 1;
next_xavp:
		xavp = next_xavp;
	}

	return -1;
}
