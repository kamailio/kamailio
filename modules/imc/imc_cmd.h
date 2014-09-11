/*
 * $Id$
 *
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
 * History:
 * ---------
 */




#ifndef _IMC_CMD_H_
#define _IMC_CMD_H_

#include "../../parser/parse_uri.h"
#include "../../str.h"
#include "imc_mng.h"

#define IMC_CMD_START		'#'
#define IMC_CMD_START_STR	"#"

#define IMC_CMDID_CREATE	1
#define IMC_CMDID_INVITE	2
#define IMC_CMDID_JOIN		3
#define IMC_CMDID_EXIT		4
#define IMC_CMDID_ACCEPT	5
#define IMC_CMDID_DENY		6
#define IMC_CMDID_REMOVE	7
#define IMC_CMDID_DESTROY	8
#define IMC_CMDID_HELP		9
#define IMC_CMDID_LIST		10
#define IMC_CMDID_UNKNOWN	11

#define IMC_CMD_CREATE	"create"
#define IMC_CMD_INVITE	"invite"
#define IMC_CMD_JOIN	"join"
#define IMC_CMD_EXIT	"exit"
#define IMC_CMD_ACCEPT	"accept"
#define IMC_CMD_DENY	"deny"
#define IMC_CMD_REMOVE	"remove"
#define IMC_CMD_DESTROY	"destroy"
#define IMC_CMD_LIST	"list"

#define IMC_ROOM_PRIVATE		"private"
#define IMC_ROOM_PRIVATE_LEN	(sizeof(IMC_ROOM_PRIVATE)-1)

#define IMC_HELP_MSG	"\r\n"IMC_CMD_START_STR IMC_CMD_CREATE" <room_name> - \
create new conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_JOIN" [<room_name>] - \
join the conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_INVITE" <user_name> [<room_name>] - \
invite a user to join a conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_ACCEPT" - \
accept invitation to join a conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_DENY" - \
deny invitation to join a conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_REMOVE" <user_name> [<room_name>] - \
remove an user from the conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_LIST" - \
list members is a conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_EXIT" [<room_name>] - \
exit from a conference room\r\n\
"IMC_CMD_START_STR IMC_CMD_DESTROY" [<room_name>] - \
destroy conference room\r\n"

#define IMC_HELP_MSG_LEN (sizeof(IMC_HELP_MSG)-1)


#define IMC_CMD_MAX_PARAM   5
typedef struct _imc_cmd
{
	str name;
	int type;
	str param[IMC_CMD_MAX_PARAM];
} imc_cmd_t, *imc_cmd_p;

int imc_parse_cmd(char *buf, int len, imc_cmd_p cmd);

int imc_handle_create(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_join(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_invite(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_accept(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_deny(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_remove(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_list(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_exit(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_destroy(struct sip_msg* msg, imc_cmd_t *cmd,
		struct sip_uri *src, struct sip_uri *dst);
int imc_handle_unknown(struct sip_msg* msg, imc_cmd_t *cmd,
		str *src, str *dst);
int imc_handle_help(struct sip_msg* msg, imc_cmd_t *cmd,
		str *src, str *dst);
int imc_handle_message(struct sip_msg* msg, str *msgbody,
		struct sip_uri *src, struct sip_uri *dst);

#endif
