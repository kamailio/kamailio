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
#ifndef _PURPLEPIPE_H
#define _PURPLEPIPE_H

#include <stdlib.h>
#include <glib.h>
#include <libpurple/savedstatuses.h>
#include <libpurple/status.h>

#include "../../str.h"

#include "purple.h"

void purple_free_cmd(struct purple_cmd *cmd);
int purple_send_message_cmd(str *from, str *to, str *body, str *id);
int purple_send_publish_cmd(enum purple_publish_basic basic, PurpleStatusPrimitive primitive, str *from, str *id, str *note);
int purple_send_subscribe_cmd(str *from, str *to, int expires);

#endif
