/* $Id$
 *
 * Shared memory functions
 */

#ifdef SHM_MEM

#include "shm_mem.h"
#include "config.h"


/* define semun */
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
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




static int shm_shmid=-1; /*shared memory id*/
int shm_semid=-1; /*semaphore id*/
static void* shm_mempool=(void*)-1;
struct qm_block* shm_block;



/* ret -1 on erro*/
int shm_mem_init()
{

	struct shmid_ds shm_info;
	union semun su;
	int ret;

	if ((shm_shmid!=-1)||(shm_semid!=-1)||(shm_mempool!=(void*)-1)){
		LOG(L_CRIT, "BUG: shm_mem_init: shm already initialized\n");
		return -1;
	}
	
	shm_shmid=shmget(IPC_PRIVATE, SHM_MEM_SIZE, 0700);
	if (shm_shmid==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not allocate shared memory"
				" segment: %s\n", strerror(errno));
		return -1;
	}
	shm_mempool=shmat(shm_shmid, 0, 0);
	if (shm_mempool==(void*)-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not attach shared memory"
				" segment: %s\n", strerror(errno));
		/* destroy segment*/
		shm_mem_destroy();
		return -1;
	}
	/* alloc a semaphore (for malloc)*/
	shm_semid=semget(IPC_PRIVATE, 1, 0700);
	if (shm_semid==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not allocate semaphore: %s\n",
				strerror(errno));
		shm_mem_destroy();
		return -1;
	}
	/* set its value to 1 (mutex)*/
	su.val=1;
	ret=semctl(shm_semid, 0, SETVAL, su);
	if (ret==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not set initial semaphore"
				" value: %s\n", strerror(errno));
		shm_mem_destroy();
		return -1;
	}
	/* init it for malloc*/
	shm_block=qm_malloc_init(shm_mempool, SHM_MEM_SIZE);
	if (shm_block==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not initialize shared"
				" malloc\n");
		shm_mem_destroy();
		return -1;
	}
	DBG("shm_mem_init: success\n");
	
	return 0;
}



void shm_mem_destroy()
{
	struct shmid_ds shm_info;
	
	DBG("shm_mem_destroy\n");
	if (shm_mempool && (shm_mempool!=(void*)-1)) {
		shmdt(shm_mempool);
		shm_mempool=(void*)-1;
	}
	if (shm_shmid!=-1) {
		shmctl(shm_shmid, IPC_RMID, &shm_info);
		shm_shmid=-1;
	}
	if (shm_semid!=-1) {
		semctl(shm_semid, 0, IPC_RMID, (union semun)0);
		shm_semid=-1;
	}
}


#endif
