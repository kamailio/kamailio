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

#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>

#include "utils.h"
#include "diameter_peer.h"

#include "config.h"
#include "acceptor.h"
#include "timer.h"
#include "peermanager.h"
#include "worker.h"
#include "receiver.h"
#include "api_process.h"
#include "transaction.h"
#include "session.h"

#include "../../pt.h"
#include "../../cfg/cfg_struct.h"

dp_config *config=0;		/**< Configuration for this diameter peer 	*/

int *shutdownx=0;			/**< whether a shutdown is in progress		*/
gen_lock_t *shutdownx_lock; /**< lock used on shutdown				*/

pid_t *dp_first_pid;		/**< first pid that we started from		*/

pid_list_head_t *pid_list;	/**< list of local processes			*/
gen_lock_t *pid_list_lock;	/**< lock for list of local processes	*/

extern handler_list *handlers; 		/**< list of handlers */
extern gen_lock_t *handlers_lock;	/**< lock for list of handlers */

extern peer_list_t *peer_list;		/**< list of peers */
extern gen_lock_t *peer_list_lock;	/**< lock for the list of peers */


/**
 * Add a pid to the local process list.
 * @param pid newly forked pid
 * @returns 1 on success or 0 on error
 */
inline int dp_add_pid(pid_t pid)
{
	pid_list_t *n;
	lock_get(pid_list_lock);
	n = shm_malloc(sizeof(pid_list_t));
	if (!n){
		LOG_NO_MEM("shm",sizeof(pid_list_t));
		lock_release(pid_list_lock);
		return 0;
	}
	n->pid = pid;
	n->next = 0;
	n->prev = pid_list->tail;
	if (!pid_list->head) pid_list->head = n;
	if (pid_list->tail) pid_list->tail->next = n;
	pid_list->tail = n;
	lock_release(pid_list_lock);
	return 1;
}

/**
 * Returns the last pid in the local process list.
 */
inline int dp_last_pid()
{
	int pid;
	lock_get(pid_list_lock);
	if (pid_list->tail)	pid = pid_list->tail->pid;
	else pid = -1;
	lock_release(pid_list_lock);
	return pid;
}

/**
 * Delete a pid from the process list
 * @param pid - the pid to remove
 */
inline void dp_del_pid(pid_t pid)
{
	pid_list_t *i;
	lock_get(pid_list_lock);
	i = pid_list->head;
	if (!i) {
		lock_release(pid_list_lock);
		return;
	}
	while(i && i->pid!=pid) i = i->next;
	if (i){
		if (i->prev) i->prev->next = i->next;
		else pid_list->head = i->next;
		if (i->next) i->next->prev = i->prev;
		else pid_list->tail = i->prev;
		shm_free(i);
	}
	lock_release(pid_list_lock);
}


/**
 * Real initialization, called after the config is parsed
 */
int diameter_peer_init_real()
{
	pid_list_t *i,*j;

	if (!config) {
		LM_ERR("diameter_peer_init_real(): Configuration was not parsed yet. Aborting...\n");
		goto error;
	}
	log_dp_config(config);

	dp_first_pid = shm_malloc(sizeof(pid_t));
	if (!dp_first_pid){
		LOG_NO_MEM("shm",sizeof(pid_t));
		goto error;
	}
	*dp_first_pid = getpid();

	shutdownx = shm_malloc(sizeof(int));
	if (!shutdownx){
		LOG_NO_MEM("shm",sizeof(int));
		goto error;
	}
	*shutdownx = 0;

	shutdownx_lock = lock_alloc();
	if (!shutdownx_lock){
		LOG_NO_MEM("shm",sizeof(gen_lock_t));
		goto error;
	}
	shutdownx_lock = lock_init(shutdownx_lock);

	handlers_lock = lock_alloc();
	if (!handlers_lock){
		LOG_NO_MEM("shm",sizeof(gen_lock_t));
		goto error;
	}
	handlers_lock = lock_init(handlers_lock);

	handlers = shm_malloc(sizeof(handler_list));
	if (!handlers){
		LOG_NO_MEM("shm",sizeof(handler_list));
		goto error;
	}
	handlers->head=0;
	handlers->tail=0;

	/* init the pid list */
	pid_list = shm_malloc(sizeof(pid_list_head_t));
	if (!pid_list){
		LOG_NO_MEM("shm",sizeof(pid_list_head_t));
		goto error;
	}
	bzero(pid_list,sizeof(pid_list_head_t));
	pid_list_lock = lock_alloc();
	pid_list_lock = lock_init(pid_list_lock);

	/* init shared mem pointers before forking */
	timer_cdp_init();
	worker_init();

	/* init the peer manager */
	peer_manager_init(config);

	/* init diameter transactions */
	cdp_trans_init();

	/* init the session */
	if (!cdp_sessions_init(config->sessions_hash_size)) goto error;


	/* add callback for messages - used to implement the API */
	cb_add(api_callback,0);

	return 1;

error:
	if (shutdownx) shm_free(shutdownx);
	if (config) free_dp_config(config);
	i = pid_list->head;
	while(i){
		j = i->next;
		shm_free(i);
		i = j;
	}
	shm_free(pid_list);
	lock_get(pid_list_lock);
	lock_destroy(pid_list_lock);
	lock_dealloc((void*)pid_list_lock);
	return 0;

}


/**
 * Initialize the CDiameterPeer from a configuration file.
 * The file is kept as dtd. See configdtd.h for the DTD and ConfigExample.xml.
 * @param cfg_filename - file with the configuration
 * @returns 1 on success, 0 on error
 */
int diameter_peer_init(char *cfg_filename)
{
	xmlDocPtr doc = parse_dp_config_file(cfg_filename);
	config = parse_dp_config(doc);
	if (!config) {
		LM_ERR("init_diameter_peer(): Error loading configuration file. Aborting...\n");
		goto error;
	}

	return diameter_peer_init_real();
error:
	return 0;
}

/**
 * Initialize the CDiameterPeer from a configuration string
 * The file is kept as dtd. See configdtd.h for the DTD and ConfigExample.xml.
 * @param cfg_filename - file with the configuration
 * @returns 1 on success, 0 on error
 */
int diameter_peer_init_str(str config_str)
{
	xmlDocPtr doc = parse_dp_config_str(config_str);
	config = parse_dp_config(doc);
	if (!config) {
		LM_ERR("init_diameter_peer(): Error loading configuration file. Aborting...\n");
		goto error;
	}

	return diameter_peer_init_real();
error:
	return 0;
}



/**
 * Start the CDiameterPeer operations.
 * It forks all the processes required.
 * @param blocking - if this is set, use the calling processes for the timer and never
 * return; else fork a new one for the timer and return
 * @returns 1 on success, 0 on error, never if blocking
 */
int diameter_peer_start(int blocking)
{
	int pid;
	int k=0;
	peer *p;

	/* fork workers */
	for(k=0;k<config->workers;k++){
		pid = fork_process(1001+k,"cdp_worker",1);
		if (pid==-1){
			LM_CRIT("init_diameter_peer(): Error on fork() for worker!\n");
			return 0;
		}
		if (pid==0) {
			srandom(time(0)*k);
			snprintf(pt[process_no].desc, MAX_PT_DESC,"cdp worker child=%d", k );
			if (cfg_child_init()) return 0;
			worker_process(k);
			LM_CRIT("init_diameter_peer(): worker_process finished without exit!\n");
			exit(-1);
		}else{
			dp_add_pid(pid);
		}
	}

	/* init the fd_exchange pipes */
	receiver_init(NULL);
	for(p = peer_list->head,k=0;p;p=p->next,k++)
		receiver_init(p);


	/* fork receiver for unknown peers */

	pid = fork_process(1001+k,"cdp_receiver_peer_unkown",1);

	if (pid==-1){
		LM_CRIT("init_diameter_peer(): Error on fork() for unknown peer receiver!\n");
		return 0;
	}
	if (pid==0) {
		srandom(time(0)*k);
		snprintf(pt[process_no].desc, MAX_PT_DESC,
				"cdp receiver peer unknown");
		if (cfg_child_init()) return 0;
		receiver_process(NULL);
		LM_CRIT("init_diameter_peer(): receiver_process finished without exit!\n");
		exit(-1);
	}else{
		dp_add_pid(pid);
	}

	/* fork receivers for each pre-configured peers */
	lock_get(peer_list_lock);
	for(p = peer_list->head,k=-1;p;p = p->next,k--){
		pid = fork_process(1001+k,"cdp_receiver_peer",1);
		if (pid==-1){
			LM_CRIT("init_diameter_peer(): Error on fork() for peer receiver!\n");
			return 0;
		}
		if (pid==0) {
			srandom(time(0)*k);
				snprintf(pt[process_no].desc, MAX_PT_DESC,
					"cdp_receiver_peer=%.*s", p->fqdn.len,p->fqdn.s );
			if (cfg_child_init()) return 0;
			receiver_process(p);
			LM_CRIT("init_diameter_peer(): receiver_process finished without exit!\n");
			exit(-1);
		}else{
			dp_add_pid(pid);
		}
	}
	lock_release(peer_list_lock);


	/* Fork the acceptor process (after receivers, so it inherits all the right sockets) */
	pid = fork_process(1000,"cdp_acceptor",1);

	if (pid==-1){
		LM_CRIT("init_diameter_peer(): Error on fork() for acceptor!\n");
		return 0;
	}
	if (pid==0) {
		if (cfg_child_init()) return 0;
		acceptor_process(config);
		LM_CRIT("init_diameter_peer(): acceptor_process finished without exit!\n");
		exit(-1);
	}else{
		dp_add_pid(pid);
	}

	/* fork/become timer */
	if (blocking) {
		dp_add_pid(getpid());
		if (cfg_child_init()) return 0;
		timer_process(1);
	}
	else{
		pid = fork_process(1001,"cdp_timer",1);
		if (pid==-1){
			LM_CRIT("init_diameter_peer(): Error on fork() for timer!\n");
			return 0;
		}
		if (pid==0) {
			if (cfg_child_init()) return 0;
			timer_process(0);
			LM_CRIT("init_diameter_peer(): timer_process finished without exit!\n");
			exit(-1);
		}else{
			dp_add_pid(pid);
		}
	}

	return 1;
}

/**
 * Shutdown the CDiameterPeer nicely.
 * It stops the workers, disconnects peers, drops timers and wait for all processes to exit.
 */
void diameter_peer_destroy()
{
	int pid,status;
	handler *h;

	lock_get(shutdownx_lock);
	if (*shutdownx) {
		/* already other process is cleaning stuff */
		lock_release(shutdownx_lock);
		return;
	}else {
		/* indicating that we are shuting down */
		*shutdownx = 1;
		lock_release(shutdownx_lock);
	}

	/* wait for all children to clean up nicely (acceptor, receiver, timer, workers) */
	LM_INFO("destroy_diameter_peer(): Terminating all children...\n");
	while(pid_list->tail){
		pid = dp_last_pid();
		if (pid<=0||pid==getpid()){
			dp_del_pid(pid);
			continue;
		}
		LM_INFO("destroy_diameter_peer(): Waiting for child [%d] to terminate...\n",pid);
		if (waitpid(pid,&status,0)<0){
			dp_del_pid(pid);
			continue;
		}
		if (!WIFEXITED(status) /*|| WIFSIGNALED(status)*/){
			sleep(1);
		} else {
			dp_del_pid(pid);
		}

	}
	LM_INFO("destroy_diameter_peer(): All processes terminated. Cleaning up.\n");

	/* clean upt the timer */
	timer_cdp_destroy();

	/* cleaning up workers */
	worker_destroy();

	/* cleaning peer_manager */
	peer_manager_destroy();

	/* cleaning up sessions */
	cdp_sessions_destroy();

	/* cleaning up transactions */
	cdp_trans_destroy();

	/* cleaning up global vars */
/*	lock_get(pid_list_lock);*/
	shm_free(dp_first_pid);
	shm_free(pid_list);
	lock_destroy(pid_list_lock);
	lock_dealloc((void*)pid_list_lock);

	shm_free(shutdownx);

	lock_destroy(shutdownx_lock);
	lock_dealloc((void*)shutdownx_lock);

	lock_get(handlers_lock);
	while(handlers->head){
		h = handlers->head->next;
		shm_free(handlers->head);
		handlers->head = h;
	}
	lock_destroy(handlers_lock);
	lock_dealloc((void*)handlers_lock);
	shm_free(handlers);

	free_dp_config(config);
	LM_CRIT("destroy_diameter_peer(): Bye Bye from C Diameter Peer test\n");

}


