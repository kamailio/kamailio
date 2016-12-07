/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _APP_MONO_API_H_
#define _APP_MONO_API_H_

#include <mono/jit/jit.h>
#include <mono/metadata/environment.h>
#include <stdlib.h>

#include "../../parser/msg_parser.h"

typedef struct _sr_mono_env
{
	MonoDomain *domain;
	MonoAssembly *assembly;
	sip_msg_t *msg;
	unsigned int flags;
} sr_mono_env_t;

sr_mono_env_t *sr_mono_env_get(void);

int mono_sr_initialized(void);
int mono_sr_init_mod(void);
int mono_sr_init_child(void);
int mono_sr_init_load(void);
void mono_sr_destroy(void);
int mono_sr_init_probe(void);
int sr_mono_assembly_loaded(void);

int sr_mono_load_script(char *script);
int sr_mono_register_module(char *mname);

int app_mono_exec(struct sip_msg *msg, char *script, char *param);
int app_mono_run(sip_msg_t *msg, char *arg);

typedef struct sr_M_export {
	char *name;
	void* method;
} sr_M_export_t;

#endif

