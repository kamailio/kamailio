#include "semaphore.h"

int create_sem( key_t key , int val)
{
   int id;
   union semun
   {
      int val;
      struct semid_ds *buf;
      ushort *array;
   } argument;

   id = semget( key , 1,  0666|IPC_CREAT );
   argument.val = val;
   if  (id!=-1)
      if ( semctl( id , 0 , SETVAL , argument )==-1 )
         id = -1;
   return id;
}


int change_sem( int id , int val )
{
   struct sembuf pbuf;

   pbuf.sem_num = 0;
   pbuf.sem_op =val;
   pbuf.sem_flg = 0;

   return semop( id , &pbuf , 1);
}



int remove_sem( int id )
{
   return  semctl( id , 0 , IPC_RMID , 0 ) ;
}
