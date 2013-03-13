/**
 * $Id$
 *
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "../../sr_module.h"

#include <jni.h>

// cast object to pointer
#define FORCE_CAST_O2P(var, type) *(type*)&var

char **split(char *, char *);
void ThrowNewException(JNIEnv *, char *, ...);
struct sip_msg *get_struct_sip_msg(JNIEnv *);

#endif
