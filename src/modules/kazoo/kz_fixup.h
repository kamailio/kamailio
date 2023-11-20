/*
 * $Id$
 *
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 *
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#ifndef KZ_FIXUP_H_
#define KZ_FIXUP_H_

int fixup_kz_amqp(void **param, int param_no);
int fixup_kz_amqp_free(void **param, int param_no);

int fixup_kz_async_amqp(void **param, int param_no);
int fixup_kz_async_amqp_free(void **param, int param_no);

int fixup_kz_amqp4(void **param, int param_no);
int fixup_kz_amqp4_free(void **param, int param_no);

int fixup_kz_json(void **param, int param_no);
int fixup_kz_json_free(void **param, int param_no);

int fixup_kz_amqp_encode(void **param, int param_no);
int fixup_kz_amqp_encode_free(void **param, int param_no);


#endif /* KZ_FIXUP_H_ */
