/* $Id$ */
/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef FLOCK
#include <sys/file.h>

static int lock_fd;
#endif

#ifdef POSIX_SEM
#include <semaphore.h>

static sem_t sem;
#endif

#ifdef PTHREAD_MUTEX
#include <pthread.h>
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef FAST_LOCK
#include "../../fastlock.h"
fl_lock_t lock;
#endif

#ifdef FUTEX
#define USE_FUTEX
#include "../../futexlock.h"
futex_lock_t lock;
#endif

#ifdef SYSV_SEM
#include <sys/ipc.h>
#include <sys/sem.h>


#if (defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)) || \
	defined(__FreeBSD__)
	/* union semun is defined by including <sys/sem.h> */
#else
	/* according to X/OPEN we have to define it ourselves */
	union semun {
		int val;                    /* value for SETVAL */
		struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
		unsigned short int *array;  /* array for GETALL, SETALL */
		struct seminfo *__buf;      /* buffer for IPC_INFO */
	};
#endif

static int semid=-1;

#endif


#ifdef NO_LOCK
	#define LOCK()
	#define UNLOCK()
#elif defined SYSV_SEM
	#define LOCK() \
	{\
		struct sembuf sop; \
		sop.sem_num=0; \
		sop.sem_op=-1; /*down*/ \
		sop.sem_flg=0 /*SEM_UNDO*/; \
		semop(semid, &sop, 1); \
	}

	#define UNLOCK()	\
	{\
		struct sembuf sop;\
		sop.sem_num=0;\
		sop.sem_op=1; /*up*/\
		sop.sem_flg=0 /*SEM_UNDO*/;\
		semop(semid, &sop, 1);\
	}
#elif defined FLOCK

	#define LOCK() \
		flock(lock_fd, LOCK_EX)
	#define  UNLOCK() \
		flock(lock_fd, LOCK_UN)
#elif defined POSIX_SEM
	#define LOCK() \
		sem_wait(&sem)
	#define UNLOCK() \
		sem_post(&sem);
#elif defined PTHREAD_MUTEX
	#define LOCK() \
		pthread_mutex_lock(&mutex)
	#define UNLOCK() \
		pthread_mutex_unlock(&mutex)
#elif defined FAST_LOCK
	#define LOCK() \
		get_lock(&lock)
	#define UNLOCK() \
		release_lock(&lock)
#elif defined FUTEX
	#define LOCK() \
		futex_get(&lock)
	#define UNLOCK() \
		futex_release(&lock)
#endif




static char *id="$Id$";
static char *version="locking_test 0.1-"
#ifdef NO_LOCK
 "nolock"
#elif defined SYSV_SEM
 "sysv_sem"
#elif defined FLOCK
 "flock"
#elif defined POSIX_SEM
 "posix_sem"
#elif defined PTHREAD_MUTEX
 "pthread_mutex"
#elif defined FAST_LOCK
 "fast_lock"
#elif defined FUTEX
 "futex"
#endif
;

static char* help_msg="\
Usage: locking_test -n address [-c count] [-v]\n\
Options:\n\
    -c count      how many times to try lock/unlock \n\
    -v            increase verbosity level\n\
    -V            version number\n\
    -h            this help message\n\
";



int main (int argc, char** argv)
{
	int c;
	int r;
	char *tmp;
	
	int count;
	int verbose;
	char *address;
#ifdef SYSV_SEM
	union semun su;
#endif
	
	/* init */
	count=0;
	verbose=0;
	address=0;



	opterr=0;
	while ((c=getopt(argc,argv, "c:vhV"))!=-1){
		switch(c){
			case 'v':
				verbose++;
				break;
			case 'c':
				count=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n",id);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c´\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x´\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c´ requires an argument.\n",
						optopt);
				goto error;
				break;
			default:
					abort();
		}
	}

	
	/* check if all the required params are present */
	if(count==0){
		fprintf(stderr, "Missing count (-c number)\n");
		exit(-1);
	}else if(count<0){
		fprintf(stderr, "Invalid count (-c %d)\n", count);
		exit(-1);
	}


#ifdef SYSV_SEM
	/*init*/
	puts("Initializing SYS V semaphores\n");
	semid=semget(IPC_PRIVATE,1,0700);
	if(semid==-1){
		fprintf(stderr, "ERROR: could not init semaphore: %s\n",
				strerror(errno));
		goto error;
	}
	/*set init value to 1 (mutex)*/
	su.val=1;
	if (semctl(semid, 0, SETVAL, su)==-1){
		fprintf(stderr, "ERROR: could not set initial semaphore value: %s\n",
				strerror(errno));
		semctl(semid, 0, IPC_RMID, (union semun)0);
		goto error;
	}
#elif defined FLOCK
	puts("Initializing flock\n");
	lock_fd=open("/dev/zero", O_RDONLY);
	if (lock_fd==-1){
		fprintf(stderr, "ERROR: could not open file: %s\n", strerror(errno));
		goto error;
	}
#elif defined POSIX_SEM
	puts("Initializing semaphores\n");
	if (sem_init(&sem, 0, 1)<0){
		fprintf(stderr, "ERROR: could not initialize semaphore: %s\n",
				strerror(errno));
		goto error;
	}
#elif defined PTHREAD_MUTEX
	puts("Initializing mutex -already initialized (statically)\n");
	/*pthread_mutext_init(&mutex, 0 );*/
#elif defined FAST_LOCK
	puts("Initializing fast lock\n");
	init_lock(lock);
#elif defined FUTEX
	puts("Initializing futex lock\n");
	futex_init(&lock);
#endif


	/*  loop */
	for (r=0; r<count; r++){
		LOCK();
		if ((verbose>1)&&(r%1000))  putchar('.');
		UNLOCK();
	}

	printf("%d loops\n", count);

#ifdef SYSV_SEM
	semctl(semid, 0, IPC_RMID, (union semun)0);
#elif defined LIN_SEM
	sem_destroy(&sem);
#endif

	exit(0);

error:
	exit(-1);
}
