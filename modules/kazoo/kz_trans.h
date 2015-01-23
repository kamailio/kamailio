/*
 * $Id$
 *
 * Copyright (C) 2007 voice-system.ro
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
 */

/*! \file
 * \brief Transformations support
 */

#ifndef _KZ_TRANS_H_
#define _KZ_TRANS_H_

#include "../../pvar.h"



enum _kz_tr_type { TR_NONE=0, TR_KAZOO };
enum _kz_tr_subtype { TR_KAZOO_NONE=0, TR_KAZOO_ENCODE, TR_KAZOO_JSON };

char* kz_tr_parse(str *in, trans_t *tr);

int kz_tr_init_buffers(void);
void kz_tr_clear_buffers(void);


#endif
