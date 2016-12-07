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
#ifndef _PURPLE_H
#define _PURPLE_H

#include <glib.h>
#include <libpurple/status.h>

enum purple_cmd_type {
	PURPLE_MESSAGE_CMD		= 1,
	PURPLE_PUBLISH_CMD		= 2,
	PURPLE_SUBSCRIBE_CMD		= 3
};

enum purple_publish_basic {
	PURPLE_BASIC_OPEN			= 1,
	PURPLE_BASIC_CLOSED		= 2
};

enum purple_publish_activity {
	PURPLE_ACTIVITY_AVAILABLE = 1,
	PURPLE_ACTIVITY_BUSY = 2,
	PURPLE_ACTIVITY_AWAY = 3,
};

struct purple_message {
	char *from, *to, *body, *id;
};

struct purple_publish {
	char *from, *id;
	enum purple_publish_basic basic;
	PurpleStatusPrimitive primitive;
	char *note;
};

struct purple_subscribe {
	char *from, *to;
	int expires;
	
};

struct purple_cmd {
	enum purple_cmd_type type;
	union {
		struct purple_message message;
		struct purple_publish publish;
		struct purple_subscribe subscribe;
	};
};

#endif /*PURPLE_H_*/
