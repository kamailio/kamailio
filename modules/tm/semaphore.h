#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>


int create_sem( key_t key , int val);

int change_sem( int id , int val );

int remove_sem( int id );

#endif

