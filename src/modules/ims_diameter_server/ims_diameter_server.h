/*
 *
 * Copyright (C) 2016-2017 ng-voice GmbH, Carsten Bock, carsten@ng-voice.com
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


#ifndef IMS_DIAMETERSERVER_MOD_H
#define IMS_DIAMETERSERVER_MOD_H

/** callback functions */

extern struct cdp_binds cdpb;
extern cdp_avp_bind_t *cdp_avp;

struct AAAMessage;

extern AAAMessage *request;
extern str responsejson;
extern str requestjson;

AAAMessage *callback_cdp_request(AAAMessage *request, void *param);

#endif /* IMS_DIAMETERSERVER_MOD_H */
