/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * \file
 * \brief Simple & fast malloc library
 * \ingroup mem
 */

#if defined(Q_MALLOC)

#if !defined(q_malloc_h)
#define q_malloc_h

#include "meminfo.h"

#ifdef DBG_SR_MEMORY
#define DBG_QM_MALLOC
#endif

/* defs*/
#ifdef DBG_QM_MALLOC
#if defined(__CPU_sparc64) || defined(__CPU_sparc)
/* tricky, on sun in 32 bits mode long long must be 64 bits aligned
 * but long can be 32 bits aligned => malloc should return long long
 * aligned memory */
	#define ROUNDTO		sizeof(long long)
#else
	#define ROUNDTO		sizeof(void*) /* minimum possible ROUNDTO
										* ->heavy debugging*/
#endif
#else /* DBG_QM_MALLOC */
	#define ROUNDTO		16UL /* size we round to, must be = 2^n  and also
							* sizeof(qm_frag)+sizeof(qm_frag_end)
							* must be multiple of ROUNDTO!
							*/
#endif
#define MIN_FRAG_SIZE	ROUNDTO



#define QM_MALLOC_OPTIMIZE_FACTOR 14UL /*used below */
#define QM_MALLOC_OPTIMIZE  ((unsigned long)(1UL<<QM_MALLOC_OPTIMIZE_FACTOR))
								/* size to optimize for,
									(most allocs <= this size),
									must be 2^k */

#define QM_HASH_SIZE ((unsigned long)(QM_MALLOC_OPTIMIZE/ROUNDTO + \
		(sizeof(long)*8-QM_MALLOC_OPTIMIZE_FACTOR)+1))

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
	const char* file;
	const char* func;
	const char* mname;
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



struct qm_frag_lnk{
	struct qm_frag head;
	struct qm_frag_end tail;
	unsigned long no;
};


/**
 * \brief Block of memory for Q_MALLOC memory manager
 * \see mem_info
 */

struct qm_block{
	int type; /* type of memory */
	unsigned long size; /* total size */
	unsigned long used; /* alloc'ed size*/
	unsigned long real_used; /* used+malloc overhead*/
	unsigned long max_real_used;
	unsigned long ffrags;

	struct qm_frag* first_frag;
	struct qm_frag_end* last_frag_end;

	struct qm_frag_lnk free_hash[QM_HASH_SIZE];
	/*struct qm_frag_end free_lst_end;*/
};


struct qm_block* qm_malloc_init(char* address, unsigned long size, int type);

#ifdef DBG_QM_MALLOC
void* qm_malloc(void*, size_t size, const char* file,
					const char* func, unsigned int line, const char* mname);
#else
void* qm_malloc(void*, size_t size);
#endif

#ifdef DBG_QM_MALLOC
void* qm_mallocxz(void*, size_t size, const char* file,
					const char* func, unsigned int line, const char* mname);
#else
void* qm_mallocxz(void*, size_t size);
#endif

#ifdef DBG_QM_MALLOC
void  qm_free(void*, void* p, const char* file, const char* func,
				unsigned int line, const char* mname);
#else
void  qm_free(void*, void* p);
#endif

#ifdef DBG_QM_MALLOC
void* qm_realloc(void*, void* p, size_t size,
					const char* file, const char* func, unsigned int line,
					const char *mname);
#else
void* qm_realloc(void*, void* p, size_t size);
#endif

#ifdef DBG_QM_MALLOC
void* qm_reallocxf(void*, void* p, size_t size,
					const char* file, const char* func, unsigned int line,
					const char *mname);
#else
void* qm_reallocxf(void*, void* p, size_t size);
#endif

void  qm_check(struct qm_block*);

void  qm_status(void*);
void  qm_info(void*, struct mem_info*);
void qm_report(void* qmp, mem_report_t *mrep);

unsigned long qm_available(void* qm);

void qm_sums(void* qm);
void qm_mod_get_stats(void *qm, void **qm_root);
void qm_mod_free_stats(void *root);

#endif
#endif
