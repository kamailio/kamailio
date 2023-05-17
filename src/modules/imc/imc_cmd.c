/*
 * imc module - instant messaging conferencing implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <sys/types.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/msg_parser.h"

#include "imc.h"
#include "imc_cmd.h"

#define ROOMS "Rooms:\n"
#define MEMBERS "Members:\n"

#define PREFIX "*** "

#define IMC_BUF_SIZE 32768
static char imc_body_buf[IMC_BUF_SIZE];

static str imc_msg_type = {"MESSAGE", 7};

static str msg_room_created = STR_STATIC_INIT(PREFIX "Room was created");
static str msg_room_destroyed =
		STR_STATIC_INIT(PREFIX "Room has been destroyed");
static str msg_room_not_found = STR_STATIC_INIT(PREFIX "Room not found");
static str msg_room_exists = STR_STATIC_INIT(PREFIX "Room already exists");
static str msg_leave_error =
		STR_STATIC_INIT(PREFIX "You are the room's owner and cannot leave. Use "
							   "#destroy if you wish to destroy the room.");
static str msg_room_exists_priv = STR_STATIC_INIT(
		PREFIX "A private room with the same name already exists");
static str msg_room_exists_member =
		STR_STATIC_INIT(PREFIX "Room already exists and you are a member");
static str msg_user_joined = STR_STATIC_INIT(PREFIX "%.*s has joined the room");
static str msg_already_joined =
		STR_STATIC_INIT(PREFIX "You are in the room already");
static str msg_user_left = STR_STATIC_INIT(PREFIX "%.*s has left the room");
static str msg_join_attempt_bcast =
		STR_STATIC_INIT(PREFIX "%.*s attempted to join the room");
static str msg_join_attempt_ucast =
		STR_STATIC_INIT(PREFIX "Private rooms are by invitation only. Room "
							   "owners have been notified.");
static str msg_invite =
		STR_STATIC_INIT(PREFIX "%.*s invites you to join the room (send "
							   "'%.*saccept' or '%.*sreject')");
static str msg_add_reject = STR_STATIC_INIT(
		PREFIX "You don't have the permmission to add members to this room");
static str msg_user_modified = STR_STATIC_INIT(PREFIX "%.*s is now %.*s");
static str msg_modify_reject = STR_STATIC_INIT(
		PREFIX "You don't have the permmission to modify members in this room");
#if 0
static str msg_rejected           = STR_STATIC_INIT(PREFIX "%.*s has rejected invitation");
#endif
static str msg_user_removed =
		STR_STATIC_INIT(PREFIX "You have been removed from the room");
static str msg_invalid_command = STR_STATIC_INIT(
		PREFIX "Invalid command '%.*s' (send '%.*shelp' for help)");

int imc_send_message(str *src, str *dst, str *headers, str *body);
int imc_room_broadcast(imc_room_p room, str *ctype, str *body);
void imc_inv_callback(struct cell *t, int type, struct tmcb_params *ps);


extern imc_hentry_p _imc_htable;
extern int imc_hash_size;


static str *get_callid(struct sip_msg *msg)
{
	if((parse_headers(msg, HDR_CALLID_F, 0) != -1) && msg->callid) {
		return &msg->callid->body;
	}
	return NULL;
}


static str *build_headers(struct sip_msg *msg)
{
	static str ctname = STR_STATIC_INIT("Content-Type: ");
	static str name = STR_STATIC_INIT("In-Reply-To: ");
	static str nl = STR_STATIC_INIT("\r\n");
	static char buf[1024];
	static str rv;
	str *callid;

	rv.s = buf;
	rv.len = all_hdrs.len + ctname.len + msg->content_type->body.len;

	memcpy(buf, all_hdrs.s, all_hdrs.len);
	memcpy(buf + all_hdrs.len, ctname.s, ctname.len);
	memcpy(buf + all_hdrs.len + ctname.len, msg->content_type->body.s,
			msg->content_type->body.len);

	if((callid = get_callid(msg)) == NULL) {
		return &rv;
	}

	rv.len += nl.len + name.len + callid->len;

	if(rv.len > sizeof(buf)) {
		LM_ERR("Header buffer too small for In-Reply-To header\n");
		return &rv;
	}

	memcpy(buf + all_hdrs.len + ctname.len + msg->content_type->body.len, nl.s,
			nl.len);
	memcpy(buf + all_hdrs.len + ctname.len + msg->content_type->body.len
					+ nl.len,
			name.s, name.len);
	memcpy(buf + all_hdrs.len + ctname.len + msg->content_type->body.len
					+ nl.len + name.len,
			callid->s, callid->len);
	return &rv;
}

static str *format_uri(str uri)
{
	static char buf[512];
	static str rv;
	struct sip_uri parsed;

	rv.s = NULL;
	rv.len = 0;

	if(parse_uri(uri.s, uri.len, &parsed) != 0) {
		LM_ERR("bad uri [%.*s]!\n", STR_FMT(&uri));
	} else {
		rv.s = buf;
		rv.len = snprintf(buf, sizeof(buf), "[%.*s]", STR_FMT(&parsed.user));
		if(rv.len >= sizeof(buf)) {
			LM_ERR("Buffer too small\n");
			rv.len = 0;
		}
	}
	return &rv;
}


/*
 * Given string in value and a parsed URI in template, build a full
 * URI as follows:
 * 1) If value has no URI scheme, add sip:
 * 2) If value has no domain, add domain from template
 * 3) Use the string in value for the username portion
 *
 * This function is intended for converting a URI or number provided
 * by the user in a command to a full SIP URI. The caller is
 * responsible for freeing the buffer in res->s which will be
 * allocated with pkg_malloc.
 */
static int build_uri(str *res, str value, struct sip_uri *template)
{
	int len = value.len, add_domain = 0, add_scheme = 0;

	if(memchr(value.s, ':', value.len) == NULL) {
		add_scheme = 1;
		len += 4; /* sip: */
	}

	if(memchr(value.s, '@', value.len) == NULL) {
		add_domain = 1;
		len += 1 + template->host.len;
	}

	if((res->s = (char *)pkg_malloc(len)) == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	res->len = len;
	len = 0;

	if(add_scheme) {
		strcpy(res->s, "sip:");
		len += 4;
	}

	memcpy(res->s + len, value.s, value.len);
	len += value.len;

	if(add_domain) {
		res->s[len++] = '@';
		memcpy(res->s + len, template->host.s, template->host.len);
	}
	return 0;
}


/*
 * Return a struct imc_uri which contains a SIP URI both in string
 * form and parsed to components. Calls build_uri internally and then
 * parses the resulting URI with parse_uri. See the description of
 * build_uri for more detail on arguments.
 *
 * The caller is responsible for pkg_freeing res->uri.s
 */
static int build_imc_uri(
		struct imc_uri *res, str value, struct sip_uri *template)
{
	int rc;

	rc = build_uri(&res->uri, value, template);
	if(rc != 0)
		return rc;

	if(parse_uri(res->uri.s, res->uri.len, &res->parsed) != 0) {
		LM_ERR("bad uri [%.*s]!\n", STR_FMT(&res->uri));
		pkg_free(res->uri.s);
		res->uri.s = NULL;
		res->uri.len = 0;
		return -1;
	}
	return 0;
}


/**
 * parse cmd
 */
int imc_parse_cmd(char *buf, int len, imc_cmd_p cmd)
{
	char *p;
	int i;
	if(buf == NULL || len <= 0 || cmd == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	memset(cmd, 0, sizeof(imc_cmd_t));
	if(buf[0] != imc_cmd_start_char) {
		LM_ERR("invalid command [%.*s]\n", len, buf);
		return -1;
	}
	p = &buf[1];
	cmd->name.s = p;
	while(*p && p < buf + len) {
		if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			break;
		p++;
	}
	if(cmd->name.s == p) {
		LM_ERR("no command in [%.*s]\n", len, buf);
		return -1;
	}
	cmd->name.len = p - cmd->name.s;

	/* identify the command */
	if(cmd->name.len == (sizeof("create") - 1)
			&& !strncasecmp(cmd->name.s, "create", cmd->name.len)) {
		cmd->type = IMC_CMDID_CREATE;
	} else if(cmd->name.len == (sizeof("join") - 1)
			  && !strncasecmp(cmd->name.s, "join", cmd->name.len)) {
		cmd->type = IMC_CMDID_JOIN;
	} else if(cmd->name.len == (sizeof("invite") - 1)
			  && !strncasecmp(cmd->name.s, "invite", cmd->name.len)) {
		cmd->type = IMC_CMDID_INVITE;
	} else if(cmd->name.len == (sizeof("add") - 1)
			  && !strncasecmp(cmd->name.s, "add", cmd->name.len)) {
		cmd->type = IMC_CMDID_ADD;
	} else if(cmd->name.len == (sizeof("accept") - 1)
			  && !strncasecmp(cmd->name.s, "accept", cmd->name.len)) {
		cmd->type = IMC_CMDID_ACCEPT;
	} else if(cmd->name.len == (sizeof("reject") - 1)
			  && !strncasecmp(cmd->name.s, "reject", cmd->name.len)) {
		cmd->type = IMC_CMDID_REJECT;
	} else if(cmd->name.len == (sizeof("deny") - 1)
			  && !strncasecmp(cmd->name.s, "deny", cmd->name.len)) {
		cmd->type = IMC_CMDID_REJECT;
	} else if(cmd->name.len == (sizeof("remove") - 1)
			  && !strncasecmp(cmd->name.s, "remove", cmd->name.len)) {
		cmd->type = IMC_CMDID_REMOVE;
	} else if(cmd->name.len == (sizeof("leave") - 1)
			  && !strncasecmp(cmd->name.s, "leave", cmd->name.len)) {
		cmd->type = IMC_CMDID_LEAVE;
	} else if(cmd->name.len == (sizeof("exit") - 1)
			  && !strncasecmp(cmd->name.s, "exit", cmd->name.len)) {
		cmd->type = IMC_CMDID_LEAVE;
	} else if(cmd->name.len == (sizeof("members") - 1)
			  && !strncasecmp(cmd->name.s, "members", cmd->name.len)) {
		cmd->type = IMC_CMDID_MEMBERS;
	} else if(cmd->name.len == (sizeof("rooms") - 1)
			  && !strncasecmp(cmd->name.s, "rooms", cmd->name.len)) {
		cmd->type = IMC_CMDID_ROOMS;
	} else if(cmd->name.len == (sizeof("list") - 1)
			  && !strncasecmp(cmd->name.s, "list", cmd->name.len)) {
		cmd->type = IMC_CMDID_MEMBERS;
	} else if(cmd->name.len == (sizeof("destroy") - 1)
			  && !strncasecmp(cmd->name.s, "destroy", cmd->name.len)) {
		cmd->type = IMC_CMDID_DESTROY;
	} else if(cmd->name.len == (sizeof("modify") - 1)
			  && !strncasecmp(cmd->name.s, "modify", cmd->name.len)) {
		cmd->type = IMC_CMDID_MODIFY;
	} else if(cmd->name.len == (sizeof("help") - 1)
			  && !strncasecmp(cmd->name.s, "help", cmd->name.len)) {
		cmd->type = IMC_CMDID_HELP;
		goto done;
	} else {
		cmd->type = IMC_CMDID_UNKNOWN;
		goto done;
	}


	if(*p == '\0' || p >= buf + len)
		goto done;

	i = 0;
	do {
		while(p < buf + len && (*p == ' ' || *p == '\t'))
			p++;
		if(p >= buf + len || *p == '\0' || *p == '\r' || *p == '\n')
			goto done;
		cmd->param[i].s = p;
		while(p < buf + len) {
			if(*p == '\0' || *p == ' ' || *p == '\t' || *p == '\r'
					|| *p == '\n')
				break;
			p++;
		}
		cmd->param[i].len = p - cmd->param[i].s;
		i++;
		if(i >= IMC_CMD_MAX_PARAM)
			break;
	} while(1);

done:
	LM_DBG("command: [%.*s]\n", STR_FMT(&cmd->name));
	for(i = 0; i < IMC_CMD_MAX_PARAM; i++) {
		if(cmd->param[i].len <= 0)
			break;
		LM_DBG("parameter %d=[%.*s]\n", i, STR_FMT(&cmd->param[i]));
	}
	return 0;
}


int imc_handle_create(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	int flag_room = 0;
	int flag_member = 0;
	str body;
	struct imc_uri room;
	int params = 0;
	str rs = STR_NULL, ps = STR_NULL;

	memset(&room, '\0', sizeof(room));

	if(cmd->param[0].s) {
		params++;
		if(cmd->param[1].s) {
			params++;
		}
	}

	switch(params) {
		case 0:
			/* With no parameter, use To for the room uri and create a public room */
			break;

		case 1:
			/* With one parameter, if the value is "private", it indicates
		 * a private room, otherwise it is the URI of the room and we
		 * create a public room. */
			if(cmd->param[0].len == IMC_ROOM_PRIVATE_LEN
					&& !strncasecmp(cmd->param[0].s, IMC_ROOM_PRIVATE,
							cmd->param[0].len)) {
				ps = cmd->param[0];
			} else {
				rs = cmd->param[0];
			}
			break;

		case 2:
			/* With two parameters, the first parameter is room URI and
		 * the second parameter must be "private". */
			rs = cmd->param[0];
			ps = cmd->param[1];
			break;

		default:
			LM_ERR("Invalid number of parameters %d\n", params);
			goto error;
	}

	if(build_imc_uri(&room, rs.s ? rs : dst->parsed.user, &dst->parsed) != 0)
		goto error;

	if(ps.s) {
		if(ps.len == IMC_ROOM_PRIVATE_LEN
				&& !strncasecmp(ps.s, IMC_ROOM_PRIVATE, ps.len)) {
			flag_room |= IMC_ROOM_PRIV;
			LM_DBG("Room with private flag on\n");
		} else {
			LM_ERR("Second argument to command 'create' must be string "
				   "'private'\n");
			goto error;
		}
	}

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL) {
		LM_DBG("Creating new room [%.*s]\n", STR_FMT(&room.uri));

		rm = imc_add_room(&room.parsed.user, &room.parsed.host, flag_room);
		if(rm == NULL) {
			LM_ERR("Failed to add new room\n");
			goto error;
		}
		LM_DBG("Added room [%.*s]\n", STR_FMT(&rm->uri));

		if(db_mode == 2) {
			if(add_room_to_db(rm) < 0) {
				LM_ERR("failed to add room to db\n");
				goto error;
			}
			LM_DBG("Add room [%.*s] to db\n", STR_FMT(&rm->uri));
		}

		flag_member |= IMC_MEMBER_OWNER;
		/* adding the owner as the first member*/
		member = imc_add_member(
				rm, &src->parsed.user, &src->parsed.host, flag_member);
		if(member == NULL) {
			LM_ERR("failed to add owner [%.*s]\n", STR_FMT(&src->uri));
			goto error;
		}
		LM_DBG("Added [%.*s] as the first member in room [%.*s]\n",
				STR_FMT(&member->uri), STR_FMT(&rm->uri));

		if(db_mode == 2) {
			if(add_room_member_to_db(member, rm, flag_member) < 0) {
				LM_ERR("failed to add room member [%.*s] to db\n",
						STR_FMT(&member->uri));
				goto error;
			}
		}

		imc_send_message(
				&rm->uri, &member->uri, build_headers(msg), &msg_room_created);
		goto done;
	}

	LM_DBG("Room [%.*s] already exists\n", STR_FMT(&rm->uri));

	if(imc_check_on_create) {
		imc_send_message(
				&dst->uri, &src->uri, build_headers(msg), &msg_room_exists);
		goto done;
	}

	if(rm->flags & IMC_ROOM_PRIV) {
		imc_send_message(&dst->uri, &src->uri, build_headers(msg),
				&msg_room_exists_priv);
		goto done;
	}

	LM_DBG("Checking if user [%.*s] is a member\n", STR_FMT(&src->uri));
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);

	if(member) {
		imc_send_message(&dst->uri, &src->uri, build_headers(msg),
				&msg_room_exists_member);
		goto done;
	}

	member = imc_add_member(
			rm, &src->parsed.user, &src->parsed.host, flag_member);
	if(member == NULL) {
		LM_ERR("Failed to add member [%.*s]\n", STR_FMT(&src->uri));
		goto error;
	}
	LM_DBG("Added [%.*s] as member to room [%.*s]\n", STR_FMT(&member->uri),
			STR_FMT(&rm->uri));

	if(db_mode == 2) {
		if(add_room_member_to_db(member, rm, flag_member) < 0) {
			LM_ERR("failed to add room member [%.*s] to db\n",
					STR_FMT(&member->uri));
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_joined.s,
			STR_FMT(format_uri(member->uri)));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

done:
	rv = 0;
error:
	if(room.uri.s)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_join(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	int flag_room = 0;
	int flag_member = 0;
	str body;
	struct imc_uri room;

	if(cmd == NULL || src == NULL || dst == NULL) {
		return -1;
	}

	memset(&room, '\0', sizeof(room));
	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);

	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_DBG("Room [%.*s] not found\n", STR_FMT(&room.uri));

		if(!imc_create_on_join) {
			imc_send_message(&dst->uri, &src->uri, build_headers(msg),
					&msg_room_not_found);
			goto done;
		}

		LM_DBG("Creating room [%.*s]\n", STR_FMT(&room.uri));
		rm = imc_add_room(&room.parsed.user, &room.parsed.host, flag_room);
		if(rm == NULL) {
			LM_ERR("Failed to add new room [%.*s]\n", STR_FMT(&room.uri));
			goto error;
		}
		LM_DBG("Created a new room [%.*s]\n", STR_FMT(&rm->uri));

		if(db_mode == 2) {
			if(add_room_to_db(rm) < 0) {
				LM_ERR("failed to add room to db\n");
				goto error;
			}
			LM_DBG("Add room [%.*s] to db\n", STR_FMT(&rm->uri));
		}

		flag_member |= IMC_MEMBER_OWNER;
		member = imc_add_member(
				rm, &src->parsed.user, &src->parsed.host, flag_member);
		if(member == NULL) {
			LM_ERR("Failed to add new member [%.*s]\n", STR_FMT(&src->uri));
			goto error;
		}

		if(db_mode == 2) {
			if(add_room_member_to_db(member, rm, flag_member) < 0) {
				LM_ERR("failed to add room member [%.*s] to db\n",
						STR_FMT(&member->uri));
				goto error;
			}
		}
		/* send info message */
		imc_send_message(
				&rm->uri, &member->uri, build_headers(msg), &msg_room_created);
		goto done;
	}

	LM_DBG("Found room [%.*s]\n", STR_FMT(&rm->uri));

	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member && !(member->flags & IMC_MEMBER_DELETED)) {
		LM_DBG("User [%.*s] is already in the room\n", STR_FMT(&member->uri));
		imc_send_message(&rm->uri, &member->uri, build_headers(msg),
				&msg_already_joined);
		goto done;
	}

	body.s = imc_body_buf;
	if(!(rm->flags & IMC_ROOM_PRIV)) {
		LM_DBG("adding new member [%.*s]\n", STR_FMT(&src->uri));
		member = imc_add_member(
				rm, &src->parsed.user, &src->parsed.host, flag_member);
		if(member == NULL) {
			LM_ERR("Failed to add new user [%.*s]\n", STR_FMT(&src->uri));
			goto error;
		}

		if(db_mode == 2) {
			if(add_room_member_to_db(member, rm, flag_member) < 0) {
				LM_ERR("failed to add room member [%.*s] to db\n",
						STR_FMT(&member->uri));
				goto error;
			}
		}

		body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_joined.s,
				STR_FMT(format_uri(src->uri)));
	} else {
		LM_DBG("Attept to join private room [%.*s] by [%.*s]\n",
				STR_FMT(&rm->uri), STR_FMT(&src->uri));

		body.len = snprintf(body.s, sizeof(imc_body_buf),
				msg_join_attempt_bcast.s, STR_FMT(format_uri(src->uri)));
		imc_send_message(&rm->uri, &src->uri, build_headers(msg),
				&msg_join_attempt_ucast);
	}

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

done:
	if(member != NULL && (member->flags & IMC_MEMBER_INVITED))
		member->flags &= ~IMC_MEMBER_INVITED;

	rv = 0;
error:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_invite(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	int flag_member = 0;
	str body;
	del_member_t *cback_param = NULL;
	int result;
	uac_req_t uac_r;
	struct imc_uri user, room;

	memset(&user, '\0', sizeof(user));
	memset(&room, '\0', sizeof(room));

	if(cmd->param[0].s == NULL) {
		LM_INFO("Invite command with missing argument from [%.*s]\n",
				STR_FMT(&src->uri));
		goto error;
	}

	if(build_imc_uri(&user, cmd->param[0], &dst->parsed))
		goto error;

	if(build_imc_uri(&room, cmd->param[1].s ? cmd->param[1] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);

	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&room.uri));
		goto error;
	}

	if(!(member->flags & IMC_MEMBER_OWNER)
			&& !(member->flags & IMC_MEMBER_ADMIN)) {
		LM_ERR("User [%.*s] has no right to invite others!\n",
				STR_FMT(&member->uri));
		goto error;
	}

	member = imc_get_member(rm, &user.parsed.user, &user.parsed.host);
	if(member != NULL) {
		LM_ERR("User [%.*s] is already in room [%.*s]!\n",
				STR_FMT(&member->uri), STR_FMT(&rm->uri));
		goto error;
	}

	flag_member |= IMC_MEMBER_INVITED;
	member = imc_add_member(
			rm, &user.parsed.user, &user.parsed.host, flag_member);
	if(member == NULL) {
		LM_ERR("Adding member [%.*s] failed\n", STR_FMT(&user.uri));
		goto error;
	}

	if(db_mode == 2) {
		if(add_room_member_to_db(member, rm, flag_member) < 0) {
			LM_ERR("failed to add room member [%.*s] to db\n",
					STR_FMT(&member->uri));
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_invite.s,
			STR_FMT(format_uri(src->uri)), STR_FMT(&imc_cmd_start_str),
			STR_FMT(&imc_cmd_start_str));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	LM_DBG("to=[%.*s]\nfrom=[%.*s]\nbody=[%.*s]\n", STR_FMT(&member->uri),
			STR_FMT(&rm->uri), STR_FMT(&body));

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

	if((cback_param = (del_member_t *)shm_malloc(sizeof(del_member_t)))
			== NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(cback_param, 0, sizeof(del_member_t));
	cback_param->room_name = rm->name;
	cback_param->room_domain = rm->domain;
	cback_param->member_name = member->user;
	cback_param->member_domain = member->domain;
	cback_param->inv_uri = member->uri;
	/*?!?! possible race with 'remove user' */

	set_uac_req(&uac_r, &imc_msg_type, build_headers(msg), &body, 0,
			TMCB_LOCAL_COMPLETED, imc_inv_callback, (void *)(cback_param));
	result = tmb.t_request(&uac_r, &member->uri,		/* Request-URI */
			&member->uri,								/* To */
			&rm->uri,									/* From */
			(outbound_proxy.s) ? &outbound_proxy : NULL /* outbound proxy*/
	);
	if(result < 0) {
		LM_ERR("Error in tm send request\n");
		shm_free(cback_param);
		goto error;
	}

	rv = 0;
error:
	if(user.uri.s != NULL)
		pkg_free(user.uri.s);
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_add(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	str body;
	struct imc_uri user, room;

	memset(&user, '\0', sizeof(user));
	memset(&room, '\0', sizeof(room));

	if(cmd->param[0].s == NULL) {
		LM_INFO("Add command with missing argument from [%.*s]\n",
				STR_FMT(&src->uri));
		goto error;
	}

	if(build_imc_uri(&user, cmd->param[0], &dst->parsed))
		goto error;

	if(build_imc_uri(&room, cmd->param[1].s ? cmd->param[1] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);

	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&room.uri));
		goto error;
	}

	if(!(member->flags & IMC_MEMBER_OWNER)
			&& !(member->flags & IMC_MEMBER_ADMIN)) {
		LM_ERR("User [%.*s] has no right to add others!\n",
				STR_FMT(&member->uri));
		imc_send_message(
				&rm->uri, &member->uri, build_headers(msg), &msg_add_reject);
		goto done;
	}

	member = imc_get_member(rm, &user.parsed.user, &user.parsed.host);
	if(member != NULL) {
		LM_ERR("User [%.*s] is already in room [%.*s]!\n",
				STR_FMT(&member->uri), STR_FMT(&rm->uri));
		goto error;
	}

	member = imc_add_member(rm, &user.parsed.user, &user.parsed.host, 0);
	if(member == NULL) {
		LM_ERR("Adding member [%.*s] failed\n", STR_FMT(&user.uri));
		goto error;
	}

	if(db_mode == 2) {
		if(add_room_member_to_db(member, rm, 0) < 0) {
			LM_ERR("failed to add room member [%.*s] to db\n",
					STR_FMT(&member->uri));
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_joined.s,
			STR_FMT(format_uri(member->uri)));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

done:
	rv = 0;
error:
	if(user.uri.s != NULL)
		pkg_free(user.uri.s);
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_accept(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	str body;
	struct imc_uri room;

	memset(&room, '\0', sizeof(room));

	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}

	/* if aready invited add as a member */
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL || !(member->flags & IMC_MEMBER_INVITED)) {
		LM_ERR("User [%.*s] not invited to the room!\n", STR_FMT(&src->uri));
		goto error;
	}

	member->flags &= ~IMC_MEMBER_INVITED;

	if(db_mode == 2) {
		if(modify_room_member_in_db(member, rm, member->flags) < 0) {
			LM_ERR("failed to modify room member [%.*s] in db\n",
					STR_FMT(&member->uri));
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_joined.s,
			STR_FMT(format_uri(member->uri)));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

	rv = 0;
error:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_remove(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	str body;
	struct imc_uri user, room;

	memset(&user, '\0', sizeof(user));
	memset(&room, '\0', sizeof(room));

	if(build_imc_uri(&user, cmd->param[0], &dst->parsed))
		goto error;

	if(build_imc_uri(&room, cmd->param[1].s ? cmd->param[1] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}

	/* verify if the user who sent the request is a member in the room
	 * and has the right to remove other users */
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}

	if(!(member->flags & IMC_MEMBER_OWNER)
			&& !(member->flags & IMC_MEMBER_ADMIN)) {
		LM_ERR("User [%.*s] has no right to remove from room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}

	/* verify if the user that is to be removed is a member of the room */
	member = imc_get_member(rm, &user.parsed.user, &user.parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&user.uri), STR_FMT(&rm->uri));
		goto error;
	}

	if(member->flags & IMC_MEMBER_OWNER) {
		LM_ERR("User [%.*s] is owner of room [%.*s] and cannot be removed!\n",
				STR_FMT(&member->uri), STR_FMT(&rm->uri));
		goto error;
	}

	LM_DBG("to: [%.*s]\nfrom: [%.*s]\nbody: [%.*s]\n", STR_FMT(&member->uri),
			STR_FMT(&rm->uri), STR_FMT(&msg_user_removed));
	imc_send_message(
			&rm->uri, &member->uri, build_headers(msg), &msg_user_removed);

	member->flags |= IMC_MEMBER_DELETED;
	imc_del_member(rm, &user.parsed.user, &user.parsed.host);

	if(db_mode == 2) {
		if(remove_room_member_from_db(member, rm) < 0) {
			LM_ERR("failed to remove room member\n");
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_left.s,
			STR_FMT(format_uri(member->uri)));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

	rv = 0;
error:
	if(user.uri.s != NULL)
		pkg_free(user.uri.s);
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_reject(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	struct imc_uri room;

	memset(&room, '\0', sizeof(room));
	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}

	/* If the user is an invited member, delete it from the list */
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL || !(member->flags & IMC_MEMBER_INVITED)) {
		LM_ERR("User [%.*s] was not invited to room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}

#if 0
	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_rejected.s, STR_FMT(format_uri(src->uri)));
	if (body.len > 0)
	    imc_send_message(&rm->uri, &member->uri, build_headers(msg), &body);
#endif

	LM_DBG("User [%.*s] rejected invitation to room [%.*s]!\n",
			STR_FMT(&src->uri), STR_FMT(&rm->uri));

	imc_del_member(rm, &src->parsed.user, &src->parsed.host);

	if(db_mode == 2) {
		if(remove_room_member_from_db(member, rm) < 0) {
			LM_ERR("failed to remove room member\n");
			goto error;
		}
	}

	rv = 0;
error:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_members(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	imc_member_p imp = 0;
	str body, *name;
	char *p;
	size_t left;
	struct imc_uri room;

	memset(&room, '\0', sizeof(room));
	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed)) {
		goto done;
	}

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto done;
	}

	/* verify if the user is a member of the room */
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto done;
	}

	p = imc_body_buf;
	imc_body_buf[IMC_BUF_SIZE - 1] = '\0';
	left = sizeof(imc_body_buf) - 1;

	memcpy(p, MEMBERS, sizeof(MEMBERS) - 1);
	p += sizeof(MEMBERS) - 1;
	left -= sizeof(MEMBERS) - 1;

	imp = rm->members;
	while(imp) {
		if((imp->flags & IMC_MEMBER_INVITED)
				|| (imp->flags & IMC_MEMBER_DELETED)
				|| (imp->flags & IMC_MEMBER_SKIP)) {
			imp = imp->next;
			continue;
		}

		if(imp->flags & IMC_MEMBER_OWNER) {
			if(left < 2)
				goto overrun;
			*p++ = '*';
			left--;
		} else if(imp->flags & IMC_MEMBER_ADMIN) {
			if(left < 2)
				goto overrun;
			*p++ = '~';
			left--;
		}

		name = format_uri(imp->uri);
		if(left < name->len + 1)
			goto overrun;
		strncpy(p, name->s, name->len);
		p += name->len;
		left -= name->len;

		if(left < 2)
			goto overrun;
		*p++ = '\n';
		left--;

		imp = imp->next;
	}

	/* write over last '\n' */
	*(--p) = 0;
	body.s = imc_body_buf;
	body.len = p - body.s;

	LM_DBG("members = '%.*s'\n", STR_FMT(&body));
	LM_DBG("Message-ID: '%.*s'\n", STR_FMT(get_callid(msg)));
	imc_send_message(&rm->uri, &member->uri, build_headers(msg), &body);

	rv = 0;
	goto done;

overrun:
	LM_ERR("Buffer too small for member list message\n");

done:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_rooms(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int i, rv = -1;
	imc_room_p room;
	str body, *name;
	char *p;
	size_t left;

	p = imc_body_buf;
	left = sizeof(imc_body_buf) - 2;

	memcpy(p, ROOMS, sizeof(ROOMS) - 1);
	p += sizeof(ROOMS) - 1;
	left -= sizeof(ROOMS) - 1;

	for(i = 0; i < imc_hash_size; i++) {
		lock_get(&_imc_htable[i].lock);
		for(room = _imc_htable[i].rooms; room != NULL; room = room->next) {
			if(room->flags & IMC_ROOM_DELETED)
				continue;

			name = format_uri(room->uri);
			if(left < name->len) {
				lock_release(&_imc_htable[i].lock);
				goto error;
			}
			strncpy(p, name->s, name->len);
			p += name->len;
			left -= name->len;

			if(left < 1) {
				lock_release(&_imc_htable[i].lock);
				goto error;
			}
			*p++ = '\n';
			left--;
		}
		lock_release(&_imc_htable[i].lock);
	}

	/* write over last '\n' */
	*(--p) = 0;
	body.s = imc_body_buf;
	body.len = p - body.s;

	LM_DBG("rooms = '%.*s'\n", STR_FMT(&body));
	imc_send_message(&dst->uri, &src->uri, build_headers(msg), &body);

	return 0;

error:
	LM_ERR("Buffer too small for member list message\n");
	return rv;
}


int imc_handle_leave(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	str body;
	struct imc_uri room;

	memset(&room, '\0', sizeof(room));
	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}

	/* verify if the user is a member of the room */
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}

	if(member->flags & IMC_MEMBER_OWNER) {
		imc_send_message(
				&rm->uri, &member->uri, build_headers(msg), &msg_leave_error);
		goto done;
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_left.s,
			STR_FMT(format_uri(member->uri)));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

	member->flags |= IMC_MEMBER_DELETED;
	imc_del_member(rm, &src->parsed.user, &src->parsed.host);

	if(db_mode == 2) {
		if(remove_room_member_from_db(member, rm) < 0) {
			LM_ERR("failed to remove room member\n");
			goto error;
		}
	}

done:
	rv = 0;
error:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_destroy(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	struct imc_uri room;

	memset(&room, '\0', sizeof(room));
	if(build_imc_uri(&room, cmd->param[0].s ? cmd->param[0] : dst->parsed.user,
			   &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}

	/* verify is the user is a member of the room*/
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not a member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}

	if(!(member->flags & IMC_MEMBER_OWNER)) {
		LM_ERR("User [%.*s] is not owner of room [%.*s] and cannot destroy "
			   "it!\n",
				STR_FMT(&src->uri), STR_FMT(&rm->uri));
		goto error;
	}
	rm->flags |= IMC_ROOM_DELETED;

	/* braodcast message */
	imc_room_broadcast(rm, build_headers(msg), &msg_room_destroyed);

	if(db_mode == 2) {
		LM_DBG("Deleting room [%.*s] from db\n", STR_FMT(&room.uri));
		if(remove_room_from_db(rm) < 0) {
			LM_ERR("Failed to delete room [%.*s] from db\n",
					STR_FMT(&room.uri));
			goto error;
		}
	}

	LM_DBG("Deleting room [%.*s] from htable\n", STR_FMT(&room.uri));
	imc_del_room(&room.parsed.user, &room.parsed.host);

	imc_release_room(rm);
	rm = NULL;

	rv = 0;
error:
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}


int imc_handle_help(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	str body;
	uac_req_t uac_r;

	body.s = IMC_HELP_MSG;
	body.len = IMC_HELP_MSG_LEN;

	LM_DBG("to: [%.*s] from: [%.*s]\n", STR_FMT(&src->uri), STR_FMT(&dst->uri));
	set_uac_req(&uac_r, &imc_msg_type, build_headers(msg), &body, 0, 0, 0, 0);
	tmb.t_request(&uac_r, NULL,							/* Request-URI */
			&src->uri,									/* To */
			&dst->uri,									/* From */
			(outbound_proxy.s) ? &outbound_proxy : NULL /* outbound proxy */
	);
	return 0;
}


int imc_handle_unknown(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	str body;
	uac_req_t uac_r;

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_invalid_command.s,
			STR_FMT(&cmd->name), STR_FMT(&imc_cmd_start_str));

	if(body.len < 0 || body.len >= sizeof(imc_body_buf)) {
		LM_ERR("Unable to print message\n");
		return -1;
	}

	LM_DBG("to: [%.*s] from: [%.*s]\n", STR_FMT(&src->uri), STR_FMT(&dst->uri));
	set_uac_req(&uac_r, &imc_msg_type, build_headers(msg), &body, 0, 0, 0, 0);
	tmb.t_request(&uac_r, NULL,							/* Request-URI */
			&src->uri,									/* To */
			&dst->uri,									/* From */
			(outbound_proxy.s) ? &outbound_proxy : NULL /* outbound proxy */
	);
	return 0;
}


int imc_handle_message(struct sip_msg *msg, str *msgbody, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p room = 0;
	imc_member_p member = 0;
	str body, *user;

	room = imc_get_room(&dst->parsed.user, &dst->parsed.host);
	if(room == NULL || (room->flags & IMC_ROOM_DELETED)) {
		LM_DBG("Room [%.*s] does not exist!\n", STR_FMT(&dst->uri));
		goto error;
	}

	member = imc_get_member(room, &src->parsed.user, &src->parsed.host);
	if(member == NULL || (member->flags & IMC_MEMBER_INVITED)) {
		LM_ERR("User [%.*s] has no right to send messages to room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&room->uri));
		goto error;
	}

	LM_DBG("Broadcast to room [%.*s]\n", STR_FMT(&room->uri));

	user = format_uri(member->uri);

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), "%.*s: %.*s",
			STR_FMT(user), STR_FMT(msgbody));

	if(body.len < 0) {
		LM_ERR("Error while printing message\n");
		goto error;
	}

	if(body.len >= sizeof(imc_body_buf)) {
		LM_ERR("Buffer too small for message '%.*s'\n", STR_FMT(&body));
		goto error;
	}

	member->flags |= IMC_MEMBER_SKIP;
	imc_room_broadcast(room, build_headers(msg), &body);
	member->flags &= ~IMC_MEMBER_SKIP;

	rv = 0;
error:
	if(room != NULL)
		imc_release_room(room);
	return rv;
}

int imc_handle_modify(struct sip_msg *msg, imc_cmd_t *cmd, struct imc_uri *src,
		struct imc_uri *dst)
{
	int rv = -1;
	imc_room_p rm = 0;
	imc_member_p member = 0;
	int flag_member = 0;
	str body;
	struct imc_uri user, room;
	int params = 0;

	memset(&user, '\0', sizeof(user));
	memset(&room, '\0', sizeof(room));

	if(cmd->param[0].s) {
		params++;
		if(cmd->param[1].s) {
			params++;
			if(cmd->param[2].s) {
				params++;
			}
		}
	}

	switch(params) {
		case 0:
			LM_INFO("Modify command with missing argument from [%.*s]\n",
					STR_FMT(&src->uri));
			goto error;
		case 1:
			LM_INFO("Modify command with missing argument role\n");
			goto error;
		case 2:
		case 3:
			/* identify the role */
			if(cmd->param[1].len == (sizeof(IMC_MEMBER_OWNER_STR) - 1)
					&& !strncasecmp(cmd->param[1].s, IMC_MEMBER_OWNER_STR,
							cmd->param[1].len)) {
				flag_member |= IMC_MEMBER_OWNER;
			} else if(cmd->param[1].len == (sizeof(IMC_MEMBER_ADMIN_STR) - 1)
					  && !strncasecmp(cmd->param[1].s, IMC_MEMBER_ADMIN_STR,
							  cmd->param[1].len)) {
				flag_member |= IMC_MEMBER_ADMIN;
			} else if(cmd->param[1].len == (sizeof(IMC_MEMBER_STR) - 1)
					  && !strncasecmp(cmd->param[1].s, IMC_MEMBER_STR,
							  cmd->param[1].len)) {
				flag_member = 0;
			} else {
				LM_INFO("Modify command with unknown argument role [%.*s]\n",
						STR_FMT(&cmd->param[1]));
				goto error;
			}

			if(build_imc_uri(&room,
					   cmd->param[3].s ? cmd->param[3] : dst->parsed.user,
					   &dst->parsed))
				goto error;
			break;
		default:
			LM_ERR("Invalid number of parameters %d\n", params);
			goto error;
	}

	if(build_imc_uri(&user, cmd->param[0], &dst->parsed))
		goto error;

	rm = imc_get_room(&room.parsed.user, &room.parsed.host);
	if(rm == NULL || (rm->flags & IMC_ROOM_DELETED)) {
		LM_ERR("Room [%.*s] does not exist!\n", STR_FMT(&room.uri));
		goto error;
	}
	member = imc_get_member(rm, &src->parsed.user, &src->parsed.host);

	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&src->uri), STR_FMT(&room.uri));
		goto error;
	}

	if(!(member->flags & IMC_MEMBER_OWNER)
			&& !(member->flags & IMC_MEMBER_ADMIN)) {
		LM_ERR("User [%.*s] has no right to modify others role!\n",
				STR_FMT(&member->uri));
		imc_send_message(
				&rm->uri, &member->uri, build_headers(msg), &msg_modify_reject);
		goto done;
	}

	member = imc_get_member(rm, &user.parsed.user, &user.parsed.host);
	if(member == NULL) {
		LM_ERR("User [%.*s] is not member of room [%.*s]!\n",
				STR_FMT(&member->uri), STR_FMT(&room.uri));
		goto error;
	}

	rv = imc_modify_member(rm, &member->user, &member->domain, flag_member);

	if(rv == -1) {
		LM_ERR("Failed to modify member [%.*s] role [%.*s]\n",
				STR_FMT(&member->uri), STR_FMT(&cmd->param[1]));
		goto error;
	}

	if(db_mode == 2) {
		if(modify_room_member_in_db(member, rm, flag_member) < 0) {
			LM_ERR("Failed to modify member [%.*s] role [%.*s] in db\n",
					STR_FMT(&member->uri), STR_FMT(&cmd->param[1]));
			goto error;
		}
	}

	body.s = imc_body_buf;
	body.len = snprintf(body.s, sizeof(imc_body_buf), msg_user_modified.s,
			STR_FMT(&member->uri), STR_FMT(&cmd->param[1]));

	if(body.len < 0) {
		LM_ERR("Error while building response\n");
		goto error;
	}

	if(body.len > 0)
		imc_room_broadcast(rm, build_headers(msg), &body);

	if(body.len >= sizeof(imc_body_buf))
		LM_ERR("Truncated message '%.*s'\n", STR_FMT(&body));

done:
	rv = 0;
error:
	if(user.uri.s != NULL)
		pkg_free(user.uri.s);
	if(room.uri.s != NULL)
		pkg_free(room.uri.s);
	if(rm != NULL)
		imc_release_room(rm);
	return rv;
}

int imc_room_broadcast(imc_room_p room, str *ctype, str *body)
{
	imc_member_p imp;

	if(room == NULL || body == NULL)
		return -1;

	imp = room->members;

	LM_DBG("nr = %d\n", room->nr_of_members);

	while(imp) {
		LM_DBG("to uri = %.*s\n", STR_FMT(&imp->uri));
		if((imp->flags & IMC_MEMBER_INVITED)
				|| (imp->flags & IMC_MEMBER_DELETED)
				|| (imp->flags & IMC_MEMBER_SKIP)) {
			imp = imp->next;
			continue;
		}

		/* to-do: callback to remove user if delivery fails */
		imc_send_message(&room->uri, &imp->uri, ctype, body);

		imp = imp->next;
	}
	return 0;
}


int imc_send_message(str *src, str *dst, str *headers, str *body)
{
	uac_req_t uac_r;

	if(src == NULL || dst == NULL || body == NULL)
		return -1;

	/* to-do: callback to remove user if delivery fails */
	set_uac_req(&uac_r, &imc_msg_type, headers, body, 0, 0, 0, 0);
	tmb.t_request(&uac_r, NULL,							/* Request-URI */
			dst,										/* To */
			src,										/* From */
			(outbound_proxy.s) ? &outbound_proxy : NULL /* outbound proxy */
	);
	return 0;
}


void imc_inv_callback(struct cell *t, int type, struct tmcb_params *ps)
{
	str body_final;
	char from_uri_buf[256];
	char to_uri_buf[256];
	char body_buf[256];
	str from_uri_s, to_uri_s;
	imc_member_p member = NULL;
	imc_room_p room = NULL;
	uac_req_t uac_r;

	if(ps->param == NULL || *ps->param == NULL
			|| (del_member_t *)(*ps->param) == NULL) {
		LM_DBG("member not received\n");
		return;
	}

	LM_DBG("completed with status %d [member name domain:"
		   "%p/%.*s/%.*s]\n",
			ps->code, ps->param,
			STR_FMT(&((del_member_t *)(*ps->param))->member_name),
			STR_FMT(&((del_member_t *)(*ps->param))->member_domain));
	if(ps->code < 300) {
		return;
	} else {
		room = imc_get_room(&((del_member_t *)(*ps->param))->room_name,
				&((del_member_t *)(*ps->param))->room_domain);
		if(room == NULL) {
			LM_ERR("The room does not exist!\n");
			goto error;
		}
		/*verify if the user who sent the request is a member in the room
		 * and has the right to remove other users */
		member = imc_get_member(room,
				&((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);

		if(member == NULL) {
			LM_ERR("The user is not a member of the room!\n");
			goto error;
		}
		imc_del_member(room, &((del_member_t *)(*ps->param))->member_name,
				&((del_member_t *)(*ps->param))->member_domain);
		goto build_inform;
	}

build_inform:
	body_final.s = body_buf;
	body_final.len = member->uri.len - 4 /* sip: part of URI */ + 20;
	memcpy(body_final.s, member->uri.s + 4, member->uri.len - 4);
	memcpy(body_final.s + member->uri.len - 4, " is not registered.  ", 21);

	goto send_message;

send_message:

	from_uri_s.s = from_uri_buf;
	from_uri_s.len = room->uri.len;
	strncpy(from_uri_s.s, room->uri.s, room->uri.len);

	LM_DBG("sending message\n");

	to_uri_s.s = to_uri_buf;
	to_uri_s.len = ((del_member_t *)(*ps->param))->inv_uri.len;
	strncpy(to_uri_s.s, ((del_member_t *)(*ps->param))->inv_uri.s,
			((del_member_t *)(*ps->param))->inv_uri.len);

	LM_DBG("to: %.*s\nfrom: %.*s\nbody: %.*s\n", STR_FMT(&to_uri_s),
			STR_FMT(&from_uri_s), STR_FMT(&body_final));
	set_uac_req(&uac_r, &imc_msg_type, &extra_hdrs, &body_final, 0, 0, 0, 0);
	tmb.t_request(&uac_r, NULL,							/* Request-URI */
			&to_uri_s,									/* To */
			&from_uri_s,								/* From */
			(outbound_proxy.s) ? &outbound_proxy : NULL /* outbound proxy*/
	);

error:
	if(room != NULL)
		imc_release_room(room);
	if((del_member_t *)(*ps->param))
		shm_free(*ps->param);
}
