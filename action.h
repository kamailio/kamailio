/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*!
 * \file
 * \brief Kamailio core :: Actions
 * \ingroup core
 * Module: \ref core
 */


#ifndef action_h
#define action_h

#define USE_LONGJMP

#include "route_struct.h"

#include "parser/msg_parser.h"

#ifdef USE_LONGJMP
#include <setjmp.h>
#endif


struct run_act_ctx{
	int rec_lev;
	int run_flags;
	int last_retcode; /* return from last route */
#ifdef USE_LONGJMP
	jmp_buf jmp_env;
#endif
};


#define init_run_actions_ctx(ph) \
	do{\
		(ph)->rec_lev=(ph)->run_flags=(ph)->last_retcode=0; \
	}while(0)

int do_action(struct run_act_ctx* c, struct action* a, struct sip_msg* msg);
int run_actions(struct run_act_ctx* c, struct action* a, struct sip_msg* msg);

int run_top_route(struct action* a, sip_msg_t* msg, struct run_act_ctx* c);

cfg_action_t *get_cfg_crt_action(void);
int get_cfg_crt_line(void);
char *get_cfg_crt_name(void);

void set_max_recursive_level(unsigned int lev);

#ifdef USE_LONGJMP
int run_actions_safe(struct run_act_ctx* c, struct action* a,
						struct sip_msg* msg);
#else /*! USE_LONGJMP */
#define run_actions_safe(c, a, m) run_actions(c, a, m)
#endif /* USE_LONGJMP */

void log_prefix_set(sip_msg_t *msg);

#endif
