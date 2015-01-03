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
 * \brief Kamailio core :: Handle the signals
 * \ingroup core
 * Module: \ref core
 */

#include "signals.h"
#include "dprint.h"

#include <signal.h>
#include <string.h> /* memset */

#ifdef USE_SIGACTION
void (*set_sig_h(int sig, void (*handler) (int) ))(int)
{
	struct sigaction act;
	struct sigaction old;
	
	memset(&act, 0, sizeof(act));
	act.sa_handler=handler;
	/*
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	*/
	/* sa_sigaction not set, we use sa_hanlder instead */ 
	return (sigaction (sig, &act, &old)==-1)?SIG_ERR:old.sa_handler;
}
#endif
