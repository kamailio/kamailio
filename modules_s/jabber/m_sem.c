/*
 * $Id$
 *
 * JABBER module
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
