/* $Id$*
 *
 * shared mem stuff
 */

#ifdef SHM_MEM

#ifndef shm_mem_h
#define shm_mem_h

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>



#include "q_malloc.h"
#include "dprint.h"

extern struct qm_block* shm_block;
extern int shm_semid;

int shm_mem_init();
void shm_mem_destroy();



inline static void sh_lock()
{
	struct sembuf sop;
	
	sop.sem_num=0;
	sop.sem_op=-1; /*down*/
	sop.sem_flg=0 /*SEM_UNDO*/;
again:
//	semop(shm_semid, &sop, 1);
#if 0
	switch(ret){
		case 0: /*ok*/
			break;
		case EINTR: /*interrupted by signal, try again*/
			DBG("sh_lock: interrupted by signal, trying again...\n");
			goto again;
		default:
			LOG(L_ERR, "ERROR: sh_lock: error waiting on semaphore: %s\n",
					strerror(errno));
	}
#endif
}



inline static void sh_unlock()
{
	struct sembuf sop;
	
	sop.sem_num=0;
	sop.sem_op=1; /*up*/
	sop.sem_flg=0 /*SEM_UNDO*/;
again:
//	semop(shm_semid, &sop, 1);
#if 0
	/*should ret immediately*/
	switch(ret){
		case 0: /*ok*/
			break;
		case EINTR: /*interrupted by signal, try again*/
			DBG("sh_lock: interrupted by signal, trying again...\n");
			goto again;
		default:
			LOG(L_ERR, "ERROR: sh_lock: error waiting on semaphore: %s\n",
					strerror(errno));
	}
#endif
}


inline static void* sh_malloc(unsigned int size)
{
	void *p;
	
	/*if (sh_lock()==0){*/
		sh_lock();
		p=qm_malloc(shm_block, size);
		sh_unlock();
	/*
	}else{
		p=0;
	}*/
	return p;
}



#define sh_free(p) \
do { \
		sh_lock(); \
		qm_free(shm_block, p); \
		sh_unlock(); \
}while(0)



#define sh_status() \
do { \
		sh_lock(); \
		qm_status(shm_block); \
		sh_unlock(); \
}while(0)


	

#endif

#endif

