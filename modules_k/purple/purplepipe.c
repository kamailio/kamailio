/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "../../mem/shm_mem.h"

#include "purplepipe.h"
#include "purple.h"

extern int pipefds[2];

static char *shm_strdup(str *src) {
	char *res;

	if (!src || !src->s)
		return NULL;
	if (!(res = (char *) shm_malloc(src->len + 1)))
		return NULL;
	strncpy(res, src->s, src->len);
	res[src->len] = 0;
	return res;
}

static struct purple_cmd* purple_new_cmd(enum purple_cmd_type type) {
	struct purple_cmd *cmd;
	
	LM_DBG("allocating cmd\n");
	/* todo: make shm allocation for one big chunk to include all fields */
	cmd = (struct purple_cmd *) shm_malloc(sizeof(struct purple_cmd));
	
	if (cmd == NULL) {
		LM_ERR("error allocating memory for cmd\n");
		return NULL;
	}

	memset(cmd, 0, sizeof(struct purple_cmd));
	cmd->type = type;
	
	return cmd;
}	

void purple_free_cmd(struct purple_cmd *cmd) {
	LM_DBG("freeing cmd\n");
	switch (cmd->type) {
	case PURPLE_MESSAGE_CMD:
		if (cmd->message.from)
			shm_free(cmd->message.from);
		if (cmd->message.to)
			shm_free(cmd->message.to);
		if (cmd->message.body)
			shm_free(cmd->message.body);
		if (cmd->message.id)
			shm_free(cmd->message.id);
		break;
	case PURPLE_PUBLISH_CMD:
		if (cmd->publish.from)
			shm_free(cmd->publish.from);
		if (cmd->publish.id)
			shm_free(cmd->publish.id);
		if (cmd->publish.note)
			shm_free(cmd->publish.note);
		break;
	case PURPLE_SUBSCRIBE_CMD:
		if (cmd->subscribe.from)
			shm_free(cmd->subscribe.from);
		if (cmd->subscribe.to)
			shm_free(cmd->subscribe.to);
		break;
	}
	shm_free(cmd);
}


static int purple_send_cmd(struct purple_cmd **cmd) {
	LM_DBG("writing cmd to pipe\n");
	if (write(pipefds[1], cmd, sizeof(cmd)) != sizeof(cmd)) {
		LM_ERR("failed to write to command pipe: %s\n", strerror(errno));
		purple_free_cmd(*cmd);
		return -1;
	}
	LM_DBG("cmd has been wrote to pipe\n");
	return 0;
}


int purple_send_message_cmd(str *from, str *to, str *body, str *id) {
	LM_DBG("building MESSAGE cmd\n");
	struct purple_cmd *cmd = purple_new_cmd(PURPLE_MESSAGE_CMD);
	if (cmd == NULL)
		return -1;

	cmd->message.from = shm_strdup(from);
	cmd->message.to = shm_strdup(to);
	cmd->message.body = shm_strdup(body);
	cmd->message.id = shm_strdup(id);
	
	return purple_send_cmd(&cmd);
}

int purple_send_publish_cmd(enum purple_publish_basic basic, PurpleStatusPrimitive primitive, str *from, str *id, str *note) {
	LM_DBG("building PUBLISH cmd... %.*s,%.*s,%.*s\n", from->len, from->s, id->len, id->s, note->len, note->s);
	struct purple_cmd *cmd = purple_new_cmd(PURPLE_PUBLISH_CMD);
	if (cmd == NULL)
		return -1;
	
	cmd->publish.from = shm_strdup(from);
	cmd->publish.id = shm_strdup(id);
	cmd->publish.note = shm_strdup(note);
	cmd->publish.primitive = primitive;
	cmd->publish.basic = basic;

	return purple_send_cmd(&cmd);
}

int purple_send_subscribe_cmd(str *from, str *to, int expires) {
	LM_DBG("building SUBSCRIBE cmd\n");
	struct purple_cmd *cmd = purple_new_cmd(PURPLE_SUBSCRIBE_CMD);
	if (cmd == NULL)
		return -1;
	
	cmd->subscribe.from = shm_strdup(from);
	cmd->subscribe.to = shm_strdup(to);
	cmd->subscribe.expires = expires;

	return purple_send_cmd(&cmd);
}


