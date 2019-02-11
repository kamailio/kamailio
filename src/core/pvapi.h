/* 
 * Copyright (C) 2009 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*!
 * \file
 * \brief Kamailio core :: pvapi init and destroy functions.
 * \ingroup core
 * Module: \ref core
 */

#ifndef __pvapi_h__
#define __pvapi_h__

int  pv_init_api(void);
void pv_destroy_api(void);

int   pv_init_buffer(void);
int   pv_reinit_buffer(void);
void  pv_destroy_buffer(void);
char* pv_get_buffer(void);
int   pv_get_buffer_size(void);
int   pv_get_buffer_slots(void);
void  pv_set_buffer_size(int n);
void  pv_set_buffer_slots(int n);

#endif /*__pvapi_h__*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
