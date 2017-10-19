#ifndef INCLUDED_tlsf
#define INCLUDED_tlsf

#if defined(TLSF_MALLOC)

/*
** Two Level Segregated Fit memory allocator, version 3.0.
** Written by Matthew Conte, and placed in the Public Domain.
**	http://tlsf.baisoku.org
**
** Based on the original documentation by Miguel Masmano:
**	http://rtportal.upv.es/rtmalloc/allocators/tlsf/index.shtml
**
** Please see the accompanying Readme.txt for implementation
** notes and caveats.
**
** This implementation was written to the specification
** of the document, therefore no GPL restrictions apply.
*/

#include <stddef.h>
#include "meminfo.h"

#ifdef DBG_SR_MEMORY
#define DBG_TLSF_MALLOC
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* tlsf_t: a TLSF structure. Can contain 1 to N pools. */
/* pool_t: a block of memory that TLSF can manage. */
typedef void* tlsf_t;
typedef void* pool_t;

/* Create/destroy a memory pool. */
tlsf_t tlsf_create(void* mem);
tlsf_t tlsf_create_with_pool(void* mem, size_t bytes);
void tlsf_destroy(tlsf_t tlsf);
pool_t tlsf_get_pool(tlsf_t tlsf);

/* Add/remove memory pools. */
pool_t tlsf_add_pool(tlsf_t tlsf, void* mem, size_t bytes);
void tlsf_remove_pool(tlsf_t tlsf, pool_t pool);

/* malloc/memalign/realloc/free replacements. */
#ifdef DBG_TLSF_MALLOC
void* tlsf_malloc(tlsf_t tlsf, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname);
void* tlsf_mallocxz(tlsf_t tlsf, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname);
void* tlsf_realloc(tlsf_t tlsf, void* ptr, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname);
void* tlsf_reallocxf(tlsf_t tlsf, void* ptr, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname);
void tlsf_free(tlsf_t tlsf, void* ptr,
		const char *file, const char *function, unsigned int line, const char *mname);
#else
void* tlsf_malloc(tlsf_t tlsf, size_t bytes);
void* tlsf_mallocxz(tlsf_t tlsf, size_t bytes);
void* tlsf_realloc(tlsf_t tlsf, void* ptr, size_t size);
void* tlsf_reallocxf(tlsf_t tlsf, void* ptr, size_t size);
void tlsf_free(tlsf_t tlsf, void* ptr);
#endif

/* Returns internal block size, not original request size */
size_t tlsf_block_size(void* ptr);

/* Overheads/limits of internal structures. */
size_t tlsf_size();
size_t tlsf_align_size();
size_t tlsf_block_size_min();
size_t tlsf_block_size_max();
size_t tlsf_pool_overhead();
size_t tlsf_alloc_overhead();

/* Debugging. */
typedef void (*tlsf_walker)(void* ptr, size_t size, int used, void* user);
void tlsf_walk_pool(pool_t pool, tlsf_walker walker, void* user);
/* Returns nonzero if any internal consistency check fails. */
int tlsf_check(tlsf_t tlsf);
int tlsf_check_pool(pool_t pool);

void tlsf_meminfo(tlsf_t pool, struct mem_info *info);
void tlsf_status(tlsf_t pool);
void tlsf_sums(tlsf_t pool);
unsigned long tlsf_available(tlsf_t pool);
void tlsf_mod_get_stats(tlsf_t pool, void **root);
void tlsf_mod_free_stats(void *root);

typedef struct _mem_counter{
	const char *file;
	const char *func;
	const char *mname;
	unsigned long line;

	unsigned long size;
	int count;

	struct _mem_counter *next;
} mem_counter;

#if defined(__cplusplus)
};
#endif

#endif /* TLSF_MALLOC */

#endif
