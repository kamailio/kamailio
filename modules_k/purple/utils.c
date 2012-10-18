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
#include "../../dprint.h"
#include "utils.h"


void primitive_parse(PurpleStatusPrimitive primitive, enum purple_publish_basic *basic, enum purple_publish_activity *activity) {
	*basic = PURPLE_BASIC_OPEN;
	*activity = 0;
		
	switch (primitive) {
		case PURPLE_STATUS_OFFLINE:
			LM_DBG("primitive: OFFLINE\n");
			*basic = PURPLE_BASIC_CLOSED;
			break;
		case PURPLE_STATUS_AVAILABLE:
			LM_DBG("primitive: AVAILABLE\n");
			*activity = PURPLE_ACTIVITY_AVAILABLE;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			LM_DBG("primitive: UNAVAILABLE\n");
			*activity = PURPLE_ACTIVITY_BUSY;
			break;
		case PURPLE_STATUS_INVISIBLE:
			LM_DBG("primitive: INVISIBLE\n");
			*basic = PURPLE_BASIC_CLOSED;
			break;
		case PURPLE_STATUS_AWAY:
			LM_DBG("primitive: AWAY\n");
			*activity = PURPLE_ACTIVITY_AWAY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			LM_DBG("primitive: EXTENDED AWAY\n");
			*activity = PURPLE_ACTIVITY_AWAY;
			break;
		default:
			LM_DBG("primitive: [unknown]\n");
			break;
	}
}
