/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#ifndef _c_diameter_peer_globals_h
#define _c_diameter_peer_globals_h

#include "utils.h"
#include <sys/types.h>

#define DPNAME "CDiameterPeer"
#define DPVERSION "0.0.2"

extern int process_no;

int init_memory(int show_status);

void destroy_memory(int show_status);


extern unsigned int *listening_socks;

extern int *shutdownx;				/**< whether a shutdown is in progress		*/
extern gen_lock_t *shutdownx_lock; /**< lock used on shutdown				*/

extern pid_t *dp_first_pid;		/**< first pid that we started from		*/

/* ANSI Terminal colors */
#define ANSI_GRAY		"\033[01;30m"
#define ANSI_BLINK_RED 	"\033[00;31m"
#define ANSI_RED 		"\033[01;31m"
#define ANSI_GREEN		"\033[01;32m"
#define ANSI_YELLOW 	"\033[01;33m"
#define ANSI_BLUE 		"\033[01;34m"
#define ANSI_MAGENTA	"\033[01;35m"
#define ANSI_CYAN		"\033[01;36m"
#define ANSI_WHITE		"\033[01;37m"
#endif
