/* $Id$
 *
 * simple & fast malloc library
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
/*
 * History:
 * --------
 *  2003-05-21  on sparc64 roundto 8 even in debugging mode (so malloc'ed
 *               long longs will be 64 bit aligned) (andrei)
 */


#if !defined(q_malloc_h) && !defined(VQ_MALLOC) && !defined(F_MALLOC)
#define q_malloc_h



/* defs*/
#ifdef DBG_QM_MALLOC
#ifdef __CPU_sparc64
/* tricky, on sun in 32 bits mode long long must be 64 bits aligned
 * but long can be 32 bits aligned => malloc should return long long
 * aligned memory */
	#define ROUNDTO		sizeof(long long)
#else
	#define ROUNDTO		sizeof(void*) /* minimum possible ROUNDTO ->heavy 
										 debugging*/
#endif 
#else /* DBG_QM_MALLOC */
	#define ROUNDTO		16 /* size we round to, must be = 2^n  and also
							 sizeof(qm_frag)+sizeof(qm_frag_end)
							 must be mutliple of ROUNDTO!
						   */
#endif
#define MIN_FRAG_SIZE	ROUNDTO



#define QM_MALLOC_OPTIMIZE_FACTOR 11 /*used below */
#define QM_MALLOC_OPTIMIZE  ((unsigned long)(1<<QM_MALLOC_OPTIMIZE_FACTOR))
								/* size to optimize for,
									(most allocs < this size),
									must be 2^k */

#define QM_HASH_SIZE ((unsigned long)(QM_MALLOC_OPTIMIZE/ROUNDTO + \
		(32-QM_MALLOC_OPTIMIZE_FACTOR)+1))

/* hash structure:
 * 0 .... QM_MALLOC_OPTIMIE/ROUNDTO  - small buckets, size increases with
 *                            ROUNDTO from bucket to bucket
 * +1 .... end -  size = 2^k, big buckets */

struct qm_frag{
	unsigned long size;
	union{
		struct qm_frag* nxt_free;
		long is_free;
	}u;
#ifdef DBG_QM_MALLOC
	char* file;
	char* func;
	unsigned long line;
	unsigned long check;
#endif
};

struct qm_frag_end{
#ifdef DBG_QM_MALLOC
	unsigned long check1;
	unsigned long check2;
	unsigned long reserved1;
	unsigned long reserved2;
#endif
	unsigned long size;
	struct qm_frag* prev_free;
};



struct qm_frag_full{
	struct qm_frag head;
	struct qm_frag_end tail;
};


struct qm_block{
	unsigned long size; /* total size */
	unsigned long used; /* alloc'ed size*/
	unsigned long real_used; /* used+malloc overhead*/
	unsigned long max_real_used;
	
	struct qm_frag* first_frag;
	struct qm_frag_end* last_frag_end;
	
	struct qm_frag_full free_hash[QM_HASH_SIZE];
	/*struct qm_frag_end free_lst_end;*/
};



struct qm_block* qm_malloc_init(char* address, unsigned int size);

#ifdef DBG_QM_MALLOC
void* qm_malloc(struct qm_block*, unsigned int size, char* file, char* func, 
					unsigned int line);
#else
void* qm_malloc(struct qm_block*, unsigned int size);
#endif

#ifdef DBG_QM_MALLOC
void  qm_free(struct qm_block*, void* p, char* file, char* func, 
				unsigned int line);
#else
void  qm_free(struct qm_block*, void* p);
#endif
#ifdef DBG_QM_MALLOC
void* qm_realloc(struct qm_block*, void* p, unsigned int size,
				char* file, char* func, unsigned int line);
#else
void* qm_realloc(struct qm_block*, void* p, unsigned int size);
#endif

void  qm_status(struct qm_block*);


#endif
