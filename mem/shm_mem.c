/* $Id$
 *
 * Shared memory functions
 */

#ifdef SHM_MEM

#include "shm_mem.h"
#include "../config.h"

#ifdef  SHM_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h> /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#endif

#ifdef FAST_LOCK
#include "../fastlock.h"
#endif


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



#ifndef SHM_MMAP
static int shm_shmid=-1; /*shared memory id*/
#endif

#ifdef FAST_LOCK
fl_lock_t* mem_lock=0;
#else
int shm_semid=-1; /*semaphore id*/
#endif

static void* shm_mempool=(void*)-1;
#ifdef VQ_MALLOC
	struct vqm_block* shm_block;
#elif F_MALLOC
	struct fm_block* shm_block;
#else
	struct qm_block* shm_block;
#endif

#define sh_realloc(_p, _size) ({ \
		char *_c; \
		shm_lock(); \
		shm_free_unsafe( (_p) ); \
		_c=shm_malloc_unsafe( (_size) ); \
		shm_unlock(); \
		_c; })

/* look at a buffer if there is perhaps enough space for the new size
   (It is benefitial to do so because vq_malloc is pretty stateful
    and if we ask for a new buffer size, we can still make it happy
    with current buffer); if so, we return current buffer again;
    otherwise, we free it, allocate a new one and return it; no
    guarantee for buffer content; if allocation fails, we return
    NULL
*/
#ifdef DBG_QM_MALLOC
void* _shm_resize( void* p, unsigned int s, char* file, char* func, unsigned int line)
#else
void* _shm_resize( void* p , unsigned int s)
#endif
{
	char *c;
#ifdef VQ_MALLOC
	struct vqm_frag *f;
#else
#	warning shm_resize performs suboptimally without VQ_MALLOC!
#endif

	if (p==0) {
		DBG("WARNING:vqm_resize: resize(0) called\n");
		return shm_malloc( s );
	}

#	ifdef VQ_MALLOC
	f=(struct  vqm_frag*) ((char*)p-sizeof(struct vqm_frag));
#	ifdef DBG_QM_MALLOC
	DBG("_shm_resize(%x, %d), called from %s: %s(%d)\n",  p, s, file, func, line);
	VQM_DEBUG_FRAG(shm_block, f);
	if (p>(void *)shm_block->core_end || p<(void*)shm_block->init_core){
		LOG(L_CRIT, "BUG: vqm_free: bad pointer %x (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#	endif
	if (s <= f->size-VQM_OVERHEAD) {
#		ifdef DBG_QM_MALLOC
		DBG("DEBUG: happy guy -- you reused a memory fragment!\n");
#		endif
		return p;
	};
#endif
	/* we can't make the request happy with current size */
	return sh_realloc( p, s ); 
}



int shm_mem_init()
{

	union semun su;
#ifdef SHM_MMAP
	int fd;
#else
	struct shmid_ds shm_info;
#endif
	int ret;

#ifdef SHM_MMAP
	if (shm_mempool && (shm_mempool!=(void*)-1)){
#else
	if ((shm_shmid!=-1)||(shm_semid!=-1)||(shm_mempool!=(void*)-1)){
#endif
		LOG(L_CRIT, "BUG: shm_mem_init: shm already initialized\n");
		return -1;
	}
	
#ifdef SHM_MMAP
	fd=open("/dev/zero", O_RDWR);
	if (fd==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not open /dev/zero: %s\n",
				strerror(errno));
		return -1;
	}
	shm_mempool=mmap(0, /* SHM_MEM_SIZE */ shm_mem_size, PROT_READ|PROT_WRITE, MAP_SHARED,
						fd ,0);
	/* close /dev/zero */
	close(fd);
#else
	
	shm_shmid=shmget(IPC_PRIVATE, /* SHM_MEM_SIZE */ shm_mem_size , 0700);
	if (shm_shmid==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not allocate shared memory"
				" segment: %s\n", strerror(errno));
		return -1;
	}
	shm_mempool=shmat(shm_shmid, 0, 0);
#endif
	if (shm_mempool==(void*)-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not attach shared memory"
				" segment: %s\n", strerror(errno));
		/* destroy segment*/
		shm_mem_destroy();
		return -1;
	}

#ifndef FAST_LOCK
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
#endif
	/* init it for malloc*/
#	ifdef VQ_MALLOC
		shm_block=vqm_malloc_init(shm_mempool, /* SHM_MEM_SIZE */ shm_mem_size );
	#elif defined F_MALLOC
		shm_block=fm_malloc_init(shm_mempool, /* SHM_MEM_SIZE */ shm_mem_size );
#	else
		shm_block=qm_malloc_init(shm_mempool, /* SHM_MEM_SIZE */ shm_mem_size );
#	endif
	if (shm_block==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not initialize shared"
				" malloc\n");
		shm_mem_destroy();
		return -1;
	}
#ifdef FAST_LOCK
	mem_lock=shm_malloc_unsafe(sizeof(fl_lock_t));
	init_lock(*mem_lock);
#endif
	
	DBG("shm_mem_init: success\n");
	
	return 0;
}



void shm_mem_destroy()
{
#ifndef SHM_MMAP
	struct shmid_ds shm_info;
#endif
	
	DBG("shm_mem_destroy\n");
	if (shm_mempool && (shm_mempool!=(void*)-1)) {
#ifdef SHM_MMAP
		munmap(shm_mempool, /* SHM_MEM_SIZE */ shm_mem_size );
#else
		shmdt(shm_mempool);
#endif
		shm_mempool=(void*)-1;
	}
#ifndef SHM_MMAP
	if (shm_shmid!=-1) {
		shmctl(shm_shmid, IPC_RMID, &shm_info);
		shm_shmid=-1;
	}
#endif
#ifndef FAST_LOCK
	if (shm_semid!=-1) {
		semctl(shm_semid, 0, IPC_RMID, (union semun)0);
		shm_semid=-1;
	}
#endif
}


#endif
