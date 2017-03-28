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

#ifndef AVP_HELPER_H
#define AVP_HELPER_H

int pv_get_request(struct sip_msg *, pv_param_t *, pv_value_t *);
int pv_get_response(struct sip_msg *, pv_param_t *, pv_value_t *);
int pv_set_response(struct sip_msg *, pv_param_t *, int, pv_value_t *);
int pv_get_command(struct sip_msg *, pv_param_t *, pv_value_t *);
int pv_get_application(struct sip_msg *, pv_param_t *, pv_value_t *);
int addAVPsfromJSON(AAAMessage *, str * json);
int AAAmsg2json(AAAMessage *, str *);

#endif /* AVP_HELPER_H */
