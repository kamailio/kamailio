/** 
 * Callback functions for RTR/PPR from the HSS
 *
 * Copyright (c) 2012 Carsten Bock, ng-voice GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CXDX_CALLBACKS_H
#define CXDX_CALLBACKS_H

/**
 * Check, if a user-agent follows the indicated service-routes
 */
int PPR_RTR_Event(void *parsed_message, int type, void *param);

#endif /* CXDX_CALLBACKS_H */
