/*
 * $Id$
 *
 * JABBER module
 *
 */

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "m_sem.h"

void semcall(int semid,int op)
{
	struct sembuf pbuf;

	pbuf.sem_num=0;
	pbuf.sem_op=op;
	pbuf.sem_flg=0;
	if(semop(semid,&pbuf,1)<0)
		printf("Error semop\n");
}

void P(int semid)
{
	semcall(semid,-1);
}

void V(int semid)
{
	semcall(semid,1);
}

void mutex_lock(int semid)
{
	semcall(semid,-1);
}

void mutex_unlock(int semid)
{
	semcall(semid,1);
}


int init_sem(key_t semkey, int val)
{
	int semid;
	
	if((semid=semget(semkey, 1, 0600 | IPC_CREAT)) == -1)
		return -1;
	if(semctl(semid, 0, SETVAL, val)<0)
	{
		printf("Error semctl\n");
		rm_sem(semid);
		return -1;
	}
	return semid;
}

int init_mutex(key_t semkey)
{
	int semid;
	
	if((semid=semget(semkey, 1, 0600 | IPC_CREAT)) == -1)
		return -1;
	if(semctl(semid, 0, SETVAL, 1)<0)
	{
		printf("Error semctl\n");
		rm_sem(semid);
		return -1;
	}
	return semid;
}

void rm_sem(int semid)
{
	semctl(semid, 0, IPC_RMID, 0);
}
