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

#include "globals.h"
#include "utils.h"

//str aaa_fqdn={"unset_fqdn",10};
//str aaa_realm={"unset_realm",11};
//str aaa_identity={"unset_identity",14};

/** initialized the pkg and shm memory */
//int init_memory(int show_status)
//{
//#ifdef PKG_MALLOC
//	if (init_pkg_mallocs()==-1)
//		goto error;
//	if (show_status){
//		LM_DBG( "Memory status (pkg):\n");
//		pkg_status();
//	}
//#endif
//
//#ifdef SHM_MEM
//
//	if (init_shm_mallocs(
//#ifdef SER_MOD_INTERFACE
//				1
//#endif
//		)==-1)
//		goto error;
//	if (show_status){
//		LM_DBG( "Memory status (shm):\n");
//		shm_status();
//	}
//#endif
//	return 1;
//error:
//	return 0;
//}

/** call it before exiting; if show_status==1, mem status is displayed */
void destroy_memory(int show_status)
{
	/*clean-up*/
	if (mem_lock)
	    shm_unlock(); /* hack: force-unlock the shared memory lock in case
	                             some process crashed and let it locked; this will
	                             allow an almost gracious shutdown */
#ifdef SHM_MEM
	if (show_status){
		LM_DBG( "Memory status (shm):\n");
		//shm_status();
#ifndef SER_MOD_INTERFACE
		shm_sums();
#endif		
	}
	/* zero all shmem alloc vars that we still use */
	shm_mem_destroy();
#endif
#ifdef PKG_MALLOC
	if (show_status){
		LM_DBG( "Memory status (pkg):\n");
		//pkg_status();
#ifndef SER_MOD_INTERFACE
		pkg_sums();
#endif
	}
#endif
}


