/*
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/action.h"
#include "../../core/mem/mem.h"
#include "../../core/sr_module.h"
#include "../../core/dset.h"
#include "../../core/parser/msg_parser.h"

#include <jni.h>

#define APP_NAME "app_java"

extern JavaVM *_aj_jvm;
extern JNIEnv *_aj_env;
extern jclass KamailioClass;
extern jclass KamailioClassRef;
extern jclass KamailioClassInstanceRef;
extern jobject KamailioClassInstance;
extern jmethodID KamailioID;

extern sip_msg_t *_aj_msg;

#endif
