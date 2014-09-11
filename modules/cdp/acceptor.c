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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>

#include "acceptor.h"
#include "utils.h"
#include "globals.h"
#include "tcp_accept.h"
#include "config.h"
#include "worker.h"


/* defined in ../diameter_peer.c */
int dp_add_pid(pid_t pid);
void dp_del_pid(pid_t pid);

/**
 * Called from diameter_peer_start, after a process was forked for this.
 * - opens the listening sockets and binds them to the addresses.
 * @param cfg - the dp_config file, which contains the list of acceptors
 * @returns - never. As this is forked, when finished it is expected to exit on itself.
 */
void acceptor_process(dp_config *cfg)
{
	int i,k;
	unsigned int sock;
	
	LM_INFO("Acceptor process starting up...\n");
	listening_socks = pkg_malloc((cfg->acceptors_cnt+1)*sizeof(int));
	if (!listening_socks){
		LOG_NO_MEM("pkg",(cfg->acceptors_cnt+1)*sizeof(int));
		goto done;
	}
	memset(listening_socks,0,(cfg->acceptors_cnt+1)*sizeof(int));	
	k=0;
	for(i=0;i<cfg->acceptors_cnt;i++)
		if (create_socket(cfg->acceptors[i].port,cfg->acceptors[i].bind,&sock)){
			listening_socks[k++]=sock;			
		}	

	
	LM_INFO("Acceptor opened sockets. Entering accept loop ...\n");		
	accept_loop();

	for(i=0;listening_socks[i];i++)
		close(listening_socks[i]);
	
	if (listening_socks) pkg_free(listening_socks);
#ifdef CDP_FOR_SER
#else
#ifdef PKG_MALLOC
	#ifdef PKG_MALLOC
		LM_DBG("Acceptor Memory status (pkg):\n");
		//pkg_status();
		#ifdef pkg_sums
			pkg_sums();
		#endif 
	#endif
#endif
		dp_del_pid(getpid());		
#endif		
done:		
	LM_INFO("Acceptor process finished\n");
	exit(0);
}
