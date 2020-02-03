#if defined(TLSF_MALLOC)

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tlsf_malloc.h"
#include "tlsf_malloc_bits.h"
#include "src_loc.h"
#include "memdbg.h"
#include "memapi.h"
#include "../dprint.h"
#include "../cfg/cfg.h"
#include "../globals.h"

/*
** Constants.
*/

/* Public constants: may be modified. */
enum tlsf_public
{
	/* log2 of number of linear subdivisions of block sizes. */
	SL_INDEX_COUNT_LOG2 = 5,
};

/* Private constants: do not modify. */
enum tlsf_private
{
#if defined (TLSF_64BIT)
	/* All allocation sizes and addresses are aligned to 8 bytes. */
	ALIGN_SIZE_LOG2 = 3,
#else
	/* All allocation sizes and addresses are aligned to 4 bytes. */
	ALIGN_SIZE_LOG2 = 2,
#endif
	ALIGN_SIZE = (1 << ALIGN_SIZE_LOG2),

	/*
	** We support allocations of sizes up to (1 << FL_INDEX_MAX) bits.
	** However, because we linearly subdivide the second-level lists, and
	** our minimum size granularity is 4 bytes, it doesn't make sense to
	** create first-level lists for sizes smaller than SL_INDEX_COUNT * 4,
	** or (1 << (SL_INDEX_COUNT_LOG2 + 2)) bytes, as there we will be
	** trying to split size ranges into more slots than we have available.
	** Instead, we calculate the minimum threshold size, and place all
	** blocks below that size into the 0th first-level list.
	*/

#if defined (TLSF_64BIT)
	/*
	** TODO: We can increase this to support larger sizes, at the expense
	** of more overhead in the TLSF structure.
	*/
	FL_INDEX_MAX = 39,
#else
	FL_INDEX_MAX = 30,
#endif
	SL_INDEX_COUNT = (1 << SL_INDEX_COUNT_LOG2),
	FL_INDEX_SHIFT = (SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2),
	FL_INDEX_COUNT = (FL_INDEX_MAX - FL_INDEX_SHIFT + 1),

	SMALL_BLOCK_SIZE = (1 << FL_INDEX_SHIFT),
};

/*
** Cast and min/max macros.
*/

#define tlsf_cast(t, exp)	((t) (exp))
#define tlsf_min(a, b)		((a) < (b) ? (a) : (b))
#define tlsf_max(a, b)		((a) > (b) ? (a) : (b))

/*
** Set assert macro, if it has not been provided by the user.
*/
#if !defined (tlsf_assert)
#define tlsf_assert assert
#endif

/*
** Static assertion mechanism.
*/

#define _tlsf_glue2(x, y) x ## y
#define _tlsf_glue(x, y) _tlsf_glue2(x, y)
#define tlsf_static_assert(exp) \
	typedef char _tlsf_glue(static_assert, __LINE__) [(exp) ? 1 : -1]

/* This code has been tested on 32- and 64-bit (LP/LLP) architectures. */
tlsf_static_assert(sizeof(int) * CHAR_BIT == 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT >= 32);
tlsf_static_assert(sizeof(size_t) * CHAR_BIT <= 64);

/* SL_INDEX_COUNT must be <= number of bits in sl_bitmap's storage type. */
tlsf_static_assert(sizeof(unsigned int) * CHAR_BIT >= SL_INDEX_COUNT);

/* Ensure we've properly tuned our sizes. */
tlsf_static_assert(ALIGN_SIZE == SMALL_BLOCK_SIZE / SL_INDEX_COUNT);

/*
** Data structures and associated constants.
*/
#ifdef DBG_TLSF_MALLOC
typedef struct {
	const char* file;
	const char* func;
	const char* mname;
	unsigned int line;
} alloc_info_t;
#endif
/*
** Block header structure.
**
** There are several implementation subtleties involved:
** - The prev_phys_block field is only valid if the previous block is free.
** - The prev_phys_block field is actually stored at the end of the
**   previous block. It appears at the beginning of this structure only to
**   simplify the implementation.
** - The next_free / prev_free fields are only valid if the block is free.
*/
typedef struct block_header_t
{
	/* Points to the previous physical block. */
	struct block_header_t* prev_phys_block;

#ifdef DBG_TLSF_MALLOC
	alloc_info_t alloc_info;
#endif

	/* The size of this block, excluding the block header. */
	size_t size;

	/* Next and previous free blocks. */
	struct block_header_t* next_free;
	struct block_header_t* prev_free;
} block_header_t;

/*
** Since block sizes are always at least a multiple of 4, the two least
** significant bits of the size field are used to store the block status:
** - bit 0: whether block is busy or free
** - bit 1: whether previous block is busy or free
*/
static const size_t block_header_free_bit = 1 << 0;
static const size_t block_header_prev_free_bit = 1 << 1;

/*
** The size of the block header exposed to used blocks is the size field.
** The prev_phys_block field is stored *inside* the previous free block.
*/
#ifdef DBG_TLSF_MALLOC
static const size_t block_header_overhead = sizeof(size_t) + sizeof(alloc_info_t);
#else
static const size_t block_header_overhead = sizeof(size_t);
#endif

/* User data starts directly after the size field in a used block. */
static const size_t block_start_offset =
	offsetof(block_header_t, size) + sizeof(size_t);

/*
** A free block must be large enough to store its header minus the size of
** the prev_phys_block field, and no larger than the number of addressable
** bits for FL_INDEX.
*/
static const size_t block_size_min =
	sizeof(block_header_t) - sizeof(block_header_t*);
static const size_t block_size_max = tlsf_cast(size_t, 1) << FL_INDEX_MAX;

#define TLSF_INCREASE_REAL_USED(control, increment) do {control->real_used += (increment) ; control->max_used = tlsf_max(control->real_used, control->max_used);}while(0)
#define TLSF_INCREASE_FRAGMENTS(control) do {control->fragments++ ; control->max_fragments = tlsf_max(control->fragments, control->max_fragments);}while(0)

/* The TLSF control structure. */
typedef struct control_t
{
	/* Empty lists point at this block to indicate they are free. */
	block_header_t block_null;
	size_t total_size;
	size_t allocated;
	size_t real_used;
	size_t max_used;
	size_t fragments;
	size_t max_fragments;
	/* Bitmaps for free lists. */
	unsigned int fl_bitmap;
	unsigned int sl_bitmap[FL_INDEX_COUNT];

	/* Head of free lists. */
	block_header_t* blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
} control_t;

/* A type used for casting when doing pointer arithmetic. */
typedef ptrdiff_t tlsfptr_t;

/*
** block_header_t member functions.
*/

static size_t block_size(const block_header_t* block)
{
	return block->size & ~(block_header_free_bit | block_header_prev_free_bit);
}

static void block_set_size(block_header_t* block, size_t size)
{
	const size_t oldsize = block->size;
	block->size = size | (oldsize & (block_header_free_bit | block_header_prev_free_bit));
}

static int block_is_last(const block_header_t* block)
{
	return 0 == block_size(block);
}

static int block_is_free(const block_header_t* block)
{
	return tlsf_cast(int, block->size & block_header_free_bit);
}

static void block_set_free(block_header_t* block)
{
	block->size |= block_header_free_bit;
}

static void block_set_used(block_header_t* block)
{
	block->size &= ~block_header_free_bit;
}

static int block_is_prev_free(const block_header_t* block)
{
	return tlsf_cast(int, block->size & block_header_prev_free_bit);
}

static void block_set_prev_free(block_header_t* block)
{
	block->size |= block_header_prev_free_bit;
}

static void block_set_prev_used(block_header_t* block)
{
	block->size &= ~block_header_prev_free_bit;
}

static block_header_t* block_from_ptr(const void* ptr)
{
	return tlsf_cast(block_header_t*,
		tlsf_cast(unsigned char*, ptr) - block_start_offset);
}

static void* block_to_ptr(const block_header_t* block)
{
	return tlsf_cast(void*,
		tlsf_cast(unsigned char*, block) + block_start_offset);
}

/* Return location of next block after block of given size. */
static block_header_t* offset_to_block(const void* ptr, size_t size)
{
	return tlsf_cast(block_header_t*, tlsf_cast(tlsfptr_t, ptr) + size);
}

/* Return location of previous block. */
static block_header_t* block_prev(const block_header_t* block)
{
	return block->prev_phys_block;
}

/* Return location of next existing block. */
static block_header_t* block_next(const block_header_t* block)
{
	block_header_t* next = offset_to_block(block_to_ptr(block),
		block_size(block) - sizeof(block_header_t*));
	tlsf_assert(!block_is_last(block));
	return next;
}

/* Link a new block with its physical neighbor, return the neighbor. */
static block_header_t* block_link_next(block_header_t* block)
{
	block_header_t* next = block_next(block);
	next->prev_phys_block = block;
	return next;
}

static void block_mark_as_free(block_header_t* block)
{
	/* Link the block to the next block, first. */
	block_header_t* next = block_link_next(block);
	block_set_prev_free(next);
	block_set_free(block);
}

static void block_mark_as_used(block_header_t* block)
{
	block_header_t* next = block_next(block);
	block_set_prev_used(next);
	block_set_used(block);
}

static size_t align_up(size_t x, size_t align)
{
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return (x + (align - 1)) & ~(align - 1);
}

static size_t align_down(size_t x, size_t align)
{
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return x - (x & (align - 1));
}

static void* align_ptr(const void* ptr, size_t align)
{
	const tlsfptr_t aligned =
		(tlsf_cast(tlsfptr_t, ptr) + (align - 1)) & ~(align - 1);
	tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
	return tlsf_cast(void*, aligned);
}

/*
** Adjust an allocation size to be aligned to word size, and no smaller
** than internal minimum.
*/
static size_t adjust_request_size(size_t size, size_t align)
{
	size_t adjust = 0;
	if (size && size < block_size_max)
	{
		const size_t aligned = align_up(size, align);
		adjust = tlsf_max(aligned, block_size_min);
	}
	return adjust;
}

/*
** TLSF utility functions. In most cases, these are direct translations of
** the documentation found in the white paper.
*/

static void mapping_insert(size_t size, int* fli, int* sli)
{
	int fl, sl;
	if (size < SMALL_BLOCK_SIZE)
	{
		/* Store small blocks in first list. */
		fl = 0;
		sl = tlsf_cast(int, size) / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
	}
	else
	{
		fl = tlsf_fls_sizet(size);
		sl = tlsf_cast(int, size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
		fl -= (FL_INDEX_SHIFT - 1);
	}
	*fli = fl;
	*sli = sl;
}

/* This version rounds up to the next block size (for allocations) */
static void mapping_search(size_t size, int* fli, int* sli)
{
	if (size >= (1 << SL_INDEX_COUNT_LOG2))
	{
		const size_t round = (1 << (tlsf_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
		size += round;
	}
	mapping_insert(size, fli, sli);
}

static block_header_t* search_suitable_block(control_t* control, int* fli, int* sli)
{
	int fl = *fli;
	int sl = *sli;

	/*
	** First, search for a block in the list associated with the given
	** fl/sl index.
	*/
	unsigned int sl_map = control->sl_bitmap[fl] & (~0 << sl);
	if (!sl_map)
	{
		/* No block exists. Search in the next largest first-level list. */
		const unsigned int fl_map = control->fl_bitmap & (~0 << (fl + 1));
		if (!fl_map)
		{
			/* No free blocks available, memory has been exhausted. */
			return 0;
		}

		fl = tlsf_ffs(fl_map);
		*fli = fl;
		sl_map = control->sl_bitmap[fl];
	}
	tlsf_assert(sl_map && "internal error - second level bitmap is null");
	sl = tlsf_ffs(sl_map);
	*sli = sl;

	/* Return the first block in the free list. */
	return control->blocks[fl][sl];
}

/* Remove a free block from the free list.*/
static void remove_free_block(control_t* control, block_header_t* block, int fl, int sl)
{
	block_header_t* prev = block->prev_free;
	block_header_t* next = block->next_free;
	tlsf_assert(prev && "prev_free field can not be null");
	tlsf_assert(next && "next_free field can not be null");
	next->prev_free = prev;
	prev->next_free = next;

	/* If this block is the head of the free list, set new head. */
	if (control->blocks[fl][sl] == block)
	{
		control->blocks[fl][sl] = next;

		/* If the new head is null, clear the bitmap. */
		if (next == &control->block_null)
		{
			control->sl_bitmap[fl] &= ~(1 << sl);

			/* If the second bitmap is now empty, clear the fl bitmap. */
			if (!control->sl_bitmap[fl])
			{
				control->fl_bitmap &= ~(1 << fl);
			}
		}
	}
	control->fragments--;
}

/* Insert a free block into the free block list. */
static void insert_free_block(control_t* control, block_header_t* block, int fl, int sl)
{
	block_header_t* current = control->blocks[fl][sl];
	tlsf_assert(current && "free list cannot have a null entry");
	tlsf_assert(block && "cannot insert a null entry into the free list");
	block->next_free = current;
	block->prev_free = &control->block_null;
	current->prev_free = block;

	tlsf_assert(block_to_ptr(block) == align_ptr(block_to_ptr(block), ALIGN_SIZE)
		&& "block not aligned properly");
	/*
	** Insert the new block at the head of the list, and mark the first-
	** and second-level bitmaps appropriately.
	*/
	control->blocks[fl][sl] = block;
	control->fl_bitmap |= (1 << fl);
	control->sl_bitmap[fl] |= (1 << sl);
	TLSF_INCREASE_FRAGMENTS(control);
}

/* Remove a given block from the free list. */
static void block_remove(control_t* control, block_header_t* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	remove_free_block(control, block, fl, sl);
}

/* Insert a given block into the free list. */
static void block_insert(control_t* control, block_header_t* block)
{
	int fl, sl;
	mapping_insert(block_size(block), &fl, &sl);
	insert_free_block(control, block, fl, sl);
}

static int block_can_split(block_header_t* block, size_t size)
{
	return block_size(block) >= sizeof(block_header_t) + size;
}

/* Split a block into two, the second of which is free. */
static block_header_t* block_split(block_header_t* block, size_t size)
{
	/* Calculate the amount of space left in the remaining block. */
	block_header_t* remaining =
		offset_to_block(block_to_ptr(block), size - sizeof(block_header_t*));

	const size_t remain_size = block_size(block) - (size + block_header_overhead);

	tlsf_assert(block_to_ptr(remaining) == align_ptr(block_to_ptr(remaining), ALIGN_SIZE)
		&& "remaining block not aligned properly");

	tlsf_assert(block_size(block) == remain_size + size + block_header_overhead);
	block_set_size(remaining, remain_size);

	block_set_size(block, size);
	block_mark_as_free(remaining);

	return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block. */
static block_header_t* block_absorb(block_header_t* prev, block_header_t* block)
{
	tlsf_assert(!block_is_last(prev) && "previous block can't be last!");
	/* Note: Leaves flags untouched. */
	prev->size += block_size(block) + block_header_overhead;
	block_link_next(prev);
	return prev;
}

/* Merge a just-freed block with an adjacent previous free block. */
static block_header_t* block_merge_prev(control_t* control, block_header_t* block)
{
	if (block_is_prev_free(block))
	{
		block_header_t* prev = block_prev(block);
		tlsf_assert(prev && "prev physical block can't be null");
		tlsf_assert(block_is_free(prev) && "prev block is not free though marked as such");
		block_remove(control, prev);
		block = block_absorb(prev, block);
	}

	return block;
}

/* Merge a just-freed block with an adjacent free block. */
static block_header_t* block_merge_next(control_t* control, block_header_t* block)
{
	block_header_t* next = block_next(block);
	tlsf_assert(next && "next physical block can't be null");

	if (block_is_free(next))
	{
		tlsf_assert(!block_is_last(block) && "previous block can't be last!");
		block_remove(control, next);
		block = block_absorb(block, next);
	}

	return block;
}

/* Trim any trailing block space off the end of a block, return to pool. */
static void block_trim_free(control_t* control, block_header_t* block, size_t size)
{
	tlsf_assert(block_is_free(block) && "block must be free");
	if (block_can_split(block, size))
	{
		block_header_t* remaining_block = block_split(block, size);
		block_link_next(block);
		block_set_prev_free(remaining_block);
#ifdef DBG_TLSF_MALLOC
		remaining_block->alloc_info.file = _SRC_LOC_;
		remaining_block->alloc_info.func = _SRC_FUNCTION_;
		remaining_block->alloc_info.line = _SRC_LINE_;
#endif
		block_insert(control, remaining_block);
	}
}

/* Trim any trailing block space off the end of a used block, return to pool. */
static void block_trim_used(control_t* control, block_header_t* block, size_t size)
{
	tlsf_assert(!block_is_free(block) && "block must be used");
	if (block_can_split(block, size))
	{
		/* If the next block is free, we must coalesce. */
		block_header_t* remaining_block = block_split(block, size);
		block_set_prev_used(remaining_block);

		remaining_block = block_merge_next(control, remaining_block);
#ifdef DBG_TLSF_MALLOC
		remaining_block->alloc_info.file = _SRC_LOC_;
		remaining_block->alloc_info.func = _SRC_FUNCTION_;
		remaining_block->alloc_info.line = _SRC_LINE_;
#endif
		block_insert(control, remaining_block);
	}
}

static block_header_t* block_locate_free(control_t* control, size_t size)
{
	int fl = 0, sl = 0;
	block_header_t* block = 0;

	if (size)
	{
		mapping_search(size, &fl, &sl);
		block = search_suitable_block(control, &fl, &sl);
	}

	if (block)
	{
		tlsf_assert(block_size(block) >= size);
		remove_free_block(control, block, fl, sl);
	}

	return block;
}
#ifdef DBG_TLSF_MALLOC
static void* block_prepare_used(control_t* control, block_header_t* block, size_t size,
		const char *file, const char *function, unsigned long line, const char *mname)
#else
static void* block_prepare_used(control_t* control, block_header_t* block, size_t size)
#endif
{
	void* p = 0;
	if (block)
	{
		block_trim_free(control, block, size);
		block_mark_as_used(block);
		p = block_to_ptr(block);
		TLSF_INCREASE_REAL_USED(control, block_size(block) + (p - (void *)block
				/* prev_phys_block is melted in the previous block when the current block is used */
				+ sizeof(block->prev_phys_block)));
		control->allocated += block_size(block);
#ifdef DBG_TLSF_MALLOC
		block->alloc_info.file = file;
		block->alloc_info.func = function;
		block->alloc_info.line = line;
		block->alloc_info.mname = mname;
#endif
	}
	return p;
}

/* Clear structure and point all empty lists at the null block. */
static void control_construct(control_t* control)
{
	int i, j;

	control->block_null.next_free = &control->block_null;
	control->block_null.prev_free = &control->block_null;

	control->fl_bitmap = 0;
	for (i = 0; i < FL_INDEX_COUNT; ++i)
	{
		control->sl_bitmap[i] = 0;
		for (j = 0; j < SL_INDEX_COUNT; ++j)
		{
			control->blocks[i][j] = &control->block_null;
		}
	}
}

/*
** Debugging utilities.
*/

typedef struct integrity_t
{
	int prev_status;
	int status;
} integrity_t;

#define tlsf_insist(x) { tlsf_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void* ptr, size_t size, int used, void* user)
{
	block_header_t* block = block_from_ptr(ptr);
	integrity_t* integ = tlsf_cast(integrity_t*, user);
	const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
	const int this_status = block_is_free(block) ? 1 : 0;
	const size_t this_block_size = block_size(block);

	int status = 0;
	tlsf_insist(integ->prev_status == this_prev_status && "prev status incorrect");
	tlsf_insist(size == this_block_size && "block size incorrect");

	integ->prev_status = this_status;
	integ->status += status;
}

int tlsf_check(tlsf_t tlsf)
{
	int i, j;

	control_t* control = tlsf_cast(control_t*, tlsf);
	int status = 0;

	/* Check that the free lists and bitmaps are accurate. */
	for (i = 0; i < FL_INDEX_COUNT; ++i)
	{
		for (j = 0; j < SL_INDEX_COUNT; ++j)
		{
			const int fl_map = control->fl_bitmap & (1 << i);
			const int sl_list = control->sl_bitmap[i];
			const int sl_map = sl_list & (1 << j);
			const block_header_t* block = control->blocks[i][j];

			/* Check that first- and second-level lists agree. */
			if (!fl_map)
			{
				tlsf_insist(!sl_map && "second-level map must be null");
			}

			if (!sl_map)
			{
				tlsf_insist(block == &control->block_null && "block list must be null");
				continue;
			}

			/* Check that there is at least one free block. */
			tlsf_insist(sl_list && "no free blocks in second-level map");
			tlsf_insist(block != &control->block_null && "block should not be null");

			while (block != &control->block_null)
			{
				int fli, sli;
				tlsf_insist(block_is_free(block) && "block should be free");
				tlsf_insist(!block_is_prev_free(block) && "blocks should have coalesced");
				tlsf_insist(!block_is_free(block_next(block)) && "blocks should have coalesced");
				tlsf_insist(block_is_prev_free(block_next(block)) && "block should be free");
				tlsf_insist(block_size(block) >= block_size_min && "block not minimum size");

				mapping_insert(block_size(block), &fli, &sli);
				tlsf_insist(fli == i && sli == j && "block size indexed in wrong list");
				block = block->next_free;
			}
		}
	}

	return status;
}

#undef tlsf_insist

static void default_walker(void* ptr, size_t size, int used, void* user)
{
	(void)user;
	printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, block_from_ptr(ptr));
}

void tlsf_walk_pool(pool_t pool, tlsf_walker walker, void* user)
{
	tlsf_walker pool_walker = walker ? walker : default_walker;
	block_header_t* block = pool + tlsf_size() - sizeof(block_header_t*);

	while (block && !block_is_last(block))
	{
		pool_walker(
			block_to_ptr(block),
			block_size(block),
			!block_is_free(block),
			user);
		block = block_next(block);
	}
}

size_t tlsf_block_size(void* ptr)
{
	size_t size = 0;
	if (ptr)
	{
		const block_header_t* block = block_from_ptr(ptr);
		size = block_size(block);
	}
	return size;
}

int tlsf_check_pool(pool_t pool)
{
	/* Check that the blocks are physically correct. */
	integrity_t integ = { 0, 0 };
	tlsf_walk_pool(pool, integrity_walker, &integ);

	return integ.status;
}

/*
** Size of the TLSF structures in a given memory block passed to
** tlsf_create, equal to the size of a control_t
*/
size_t tlsf_size()
{
	return sizeof(control_t);
}

size_t tlsf_align_size()
{
	return ALIGN_SIZE;
}

size_t tlsf_block_size_min()
{
	return block_size_min;
}

size_t tlsf_block_size_max()
{
	return block_size_max;
}

/*
** Overhead of the TLSF structures in a given memory block passes to
** tlsf_add_pool, equal to the overhead of a free block and the
** sentinel block.
*/
size_t tlsf_pool_overhead()
{
	return 2 * block_header_overhead;
}

size_t tlsf_alloc_overhead()
{
	return block_header_overhead;
}

pool_t tlsf_add_pool(tlsf_t tlsf, void* mem, size_t bytes)
{
	block_header_t* block;
	block_header_t* next;

	const size_t pool_overhead = tlsf_pool_overhead();
	const size_t pool_bytes = align_down(bytes - pool_overhead, ALIGN_SIZE);

	if (((ptrdiff_t)mem % ALIGN_SIZE) != 0)
	{
		printf("tlsf_add_pool: Memory must be aligned by %u bytes.\n",
			(unsigned int)ALIGN_SIZE);
		return 0;
	}

	if (pool_bytes < block_size_min || pool_bytes > block_size_max)
	{
#if defined (TLSF_64BIT)
		printf("tlsf_add_pool: Memory size must be between 0x%x and 0x%x00 bytes.\n",
			(unsigned int)(pool_overhead + block_size_min),
			(unsigned int)((pool_overhead + block_size_max) / 256));
#else
		printf("tlsf_add_pool: Memory size must be between %u and %u bytes.\n",
			(unsigned int)(pool_overhead + block_size_min),
			(unsigned int)(pool_overhead + block_size_max));
#endif
		return 0;
	}

	/*
	** Create the main free block. Offset the start of the block slightly
	** so that the prev_phys_block field falls outside of the pool -
	** it will never be used.
	*/
	block = mem - sizeof (size_t);/*offset_to_block(mem, -(tlsfptr_t)block_header_overhead);*/
	block_set_size(block, pool_bytes);
	block_set_free(block);
	block_set_prev_used(block);
	block_insert(tlsf_cast(control_t*, tlsf), block);
	tlsf_cast(control_t*, tlsf)->total_size += block_size(block);
#ifdef DBG_TLSF_MALLOC
	block->alloc_info.file = _SRC_LOC_;
	block->alloc_info.func = _SRC_FUNCTION_;
	block->alloc_info.line = _SRC_LINE_;
#endif
	/* Split the block to create a zero-size sentinel block. */
	next = block_link_next(block);
	block_set_size(next, 0);
	block_set_used(next);
	block_set_prev_free(next);

	return mem;
}

void tlsf_remove_pool(tlsf_t tlsf, pool_t pool)
{
	control_t* control = tlsf_cast(control_t*, tlsf);
	block_header_t* block = offset_to_block(pool, -(int)block_header_overhead);

	int fl = 0, sl = 0;

	tlsf_assert(block_is_free(block) && "block should be free");
	tlsf_assert(!block_is_free(block_next(block)) && "next block should not be free");
	tlsf_assert(block_size(block_next(block)) == 0 && "next block size should be zero");

	mapping_insert(block_size(block), &fl, &sl);
	remove_free_block(control, block, fl, sl);
	tlsf_cast(control_t*, tlsf)->total_size -= block_size(block);
}

/*
** TLSF main interface.
*/

#if _DEBUG
int test_ffs_fls()
{
	/* Verify ffs/fls work properly. */
	int rv = 0;
	rv += (tlsf_ffs(0) == -1) ? 0 : 0x1;
	rv += (tlsf_fls(0) == -1) ? 0 : 0x2;
	rv += (tlsf_ffs(1) == 0) ? 0 : 0x4;
	rv += (tlsf_fls(1) == 0) ? 0 : 0x8;
	rv += (tlsf_ffs(0x80000000) == 31) ? 0 : 0x10;
	rv += (tlsf_ffs(0x80008000) == 15) ? 0 : 0x20;
	rv += (tlsf_fls(0x80000008) == 31) ? 0 : 0x40;
	rv += (tlsf_fls(0x7FFFFFFF) == 30) ? 0 : 0x80;

#if defined (TLSF_64BIT)
	rv += (tlsf_fls_sizet(0x80000000) == 31) ? 0 : 0x100;
	rv += (tlsf_fls_sizet(0x100000000) == 32) ? 0 : 0x200;
	rv += (tlsf_fls_sizet(0xffffffffffffffff) == 63) ? 0 : 0x400;
#endif

	if (rv)
	{
		printf("tlsf_create: %x ffs/fls tests failed!\n", rv);
	}
	return rv;
}
#endif

tlsf_t tlsf_create(void* mem)
{
#if _DEBUG
	if (test_ffs_fls())
	{
		return 0;
	}
#endif

	if (((tlsfptr_t)mem % ALIGN_SIZE) != 0)
	{
		printf("tlsf_create: Memory must be aligned to %u bytes.\n",
			(unsigned int)ALIGN_SIZE);
		return 0;
	}

	control_construct(tlsf_cast(control_t*, mem));
	tlsf_cast(control_t*, mem)->real_used = tlsf_size();
	tlsf_cast(control_t*, mem)->max_used = tlsf_size();
	tlsf_cast(control_t*, mem)->allocated = 0;
	tlsf_cast(control_t*, mem)->total_size = tlsf_size();
	tlsf_cast(control_t*, mem)->fragments = 0;
	tlsf_cast(control_t*, mem)->max_fragments = 0;
	return tlsf_cast(tlsf_t, mem);
}

tlsf_t tlsf_create_with_pool(void* mem, size_t bytes)
{
	tlsf_t tlsf = tlsf_create(mem);
	tlsf_add_pool(tlsf, (char*)mem + tlsf_size(), bytes - tlsf_size());
	return tlsf;
}

void tlsf_destroy(tlsf_t tlsf)
{
	/* Nothing to do. */
	(void)tlsf;
}

pool_t tlsf_get_pool(tlsf_t tlsf)
{
	return tlsf_cast(pool_t, (char*)tlsf + tlsf_size());
}

#ifdef DBG_TLSF_MALLOC
void* tlsf_malloc(tlsf_t tlsf, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname)
#else
void* tlsf_malloc(tlsf_t tlsf, size_t size)
#endif
{
	control_t* control = tlsf_cast(control_t*, tlsf);
	const size_t adjust = adjust_request_size(size?size:4, ALIGN_SIZE);
	block_header_t* block = block_locate_free(control, adjust);
#ifdef DBG_TLSF_MALLOC
	void *ptr;

	MDBG("tlsf_malloc(%p, %zu) called from %s: %s(%u)\n", tlsf, size, file, function, line);
	ptr = block_prepare_used(control, block, adjust, file, function, line, mname);
	MDBG("tlsf_malloc(%p, %zu) returns address %p \n", tlsf, size,
		ptr);
	return ptr;
#else
	return block_prepare_used(control, block, adjust);
#endif
}


#ifdef DBG_TLSF_MALLOC
void* tlsf_mallocxz(tlsf_t tlsf, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname)
#else
void* tlsf_mallocxz(tlsf_t tlsf, size_t size)
#endif
{
	void *p;

#ifdef DBG_TLSF_MALLOC
	p = tlsf_malloc(tlsf, size, file, function, line, mname);
#else
	p = tlsf_malloc(tlsf, size);
#endif

	if(p) memset(p, 0, size);

	return p;
}


#ifdef DBG_TLSF_MALLOC
void tlsf_free(tlsf_t tlsf, void* ptr,
		const char *file, const char *function, unsigned int line, const char *mname)
#else
void tlsf_free(tlsf_t tlsf, void* ptr)
#endif
{
#ifdef DBG_TLSF_MALLOC
	MDBG("tlsf_free(%p, %p), called from %s: %s(%u)\n", tlsf, ptr, file, function, line);
#endif
	/* Don't attempt to free a NULL pointer. */
	if (ptr)
	{
		control_t* control = tlsf_cast(control_t*, tlsf);
		block_header_t* block = block_from_ptr(ptr);
		if (block_is_free(block)) {
			LOG(L_CRIT, "BUG: tlsf_free: freeing already freed pointer (%p)"
#ifdef DBG_TLSF_MALLOC
					", called from %s: %s(%u)"
					", first free %s: %s(%u)\n",
					ptr, file, function, line,
					block->alloc_info.file, block->alloc_info.func, block->alloc_info.line);
#else
					"\n", ptr);
#endif
			if(likely(cfg_get(core, core_cfg, mem_safety)==0)) {
				abort();
			} else {
				return;
			}

		}
		control->allocated -= block_size(block);
		control->real_used -= (block_size(block) + (ptr - (void *)block
				/* prev_phys_block is melted in the previous block when the current block is used */
				+ sizeof(block->prev_phys_block)));
#ifdef DBG_TLSF_MALLOC
		block->alloc_info.file = file;
		block->alloc_info.func = function;
		block->alloc_info.line = line;
		block->alloc_info.mname = mname;
#endif
		block_mark_as_free(block);
		block = block_merge_prev(control, block);
		block = block_merge_next(control, block);
		block_insert(control, block);
	} else {
#ifdef DBG_TLSF_MALLOC
		LOG(L_WARN, "tlsf_free: free(0) called from %s: %s(%u)\n", file, function, line);
#else
		LOG(L_WARN, "tlsf_free: free(0) called\n");
#endif
	}
}

/*
** The TLSF block information provides us with enough information to
** provide a reasonably intelligent implementation of realloc, growing or
** shrinking the currently allocated block as required.
**
** This routine handles the somewhat esoteric edge cases of realloc:
** - a non-zero size with a null pointer will behave like malloc
** - a zero size with a non-null pointer will behave like free
** - a request that cannot be satisfied will leave the original buffer
**   untouched
** - an extended buffer size will leave the newly-allocated area with
**   contents undefined
*/
#ifdef DBG_TLSF_MALLOC
void* tlsf_realloc(tlsf_t tlsf, void* ptr, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname)
#else
void* tlsf_realloc(tlsf_t tlsf, void* ptr, size_t size)
#endif
{
	control_t* control = tlsf_cast(control_t*, tlsf);
	void* p = 0;

	/* Zero-size requests are treated as free. */
	if (ptr && size == 0)
	{
#ifdef DBG_TLSF_MALLOC
		tlsf_free(tlsf, ptr, file, function, line, mname);
#else
		tlsf_free(tlsf, ptr);
#endif
	}
	/* Requests with NULL pointers are treated as malloc. */
	else if (!ptr)
	{
#ifdef DBG_TLSF_MALLOC
		p = tlsf_malloc(tlsf, size, file, function, line, mname);
#else
		p = tlsf_malloc(tlsf, size);
#endif
	}
	else
	{
		block_header_t* block = block_from_ptr(ptr);
		block_header_t* next = block_next(block);

		const size_t cursize = block_size(block);
		const size_t combined = cursize + block_size(next) + block_header_overhead;
		const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

		tlsf_assert(!block_is_free(block) && "block already marked as free");

		/*
		** If the next block is used, or when combined with the current
		** block, does not offer enough space, we must reallocate and copy.
		*/
		if (adjust > cursize && (!block_is_free(next) || adjust > combined))
		{
#ifdef DBG_TLSF_MALLOC
			p = tlsf_malloc(tlsf, size, file, function, line, mname);
#else
			p = tlsf_malloc(tlsf, size);
#endif
			if (p)
			{
				const size_t minsize = tlsf_min(cursize, size);
				memcpy(p, ptr, minsize);
#ifdef DBG_TLSF_MALLOC
				tlsf_free(tlsf, ptr, file, function, line, mname);
#else
				tlsf_free(tlsf, ptr);
#endif
			}
		}
		else
		{
			control->allocated -= block_size(block);
			control->real_used -= block_size(block);
			/* Do we need to expand to the next block? */
			if (adjust > cursize)
			{
				block_merge_next(control, block);
				block_mark_as_used(block);
			}

			/* Trim the resulting block and return the original pointer. */
			block_trim_used(control, block, adjust);
			p = ptr;
			control->allocated +=block_size(block);
			TLSF_INCREASE_REAL_USED(control, block_size(block));
		}
	}

	return p;
}

#ifdef DBG_TLSF_MALLOC
void* tlsf_reallocxf(tlsf_t tlsf, void* ptr, size_t size,
		const char *file, const char *function, unsigned int line, const char *mname)
#else
void* tlsf_reallocxf(tlsf_t tlsf, void* ptr, size_t size)
#endif
{
	void *r;

#ifdef DBG_TLSF_MALLOC
	r = tlsf_realloc(tlsf, ptr, size, file, function, line, mname);
#else
	r = tlsf_realloc(tlsf, ptr, size);
#endif

	if(!r && ptr) {
	#ifdef DBG_TLSF_MALLOC
		tlsf_free(tlsf, ptr, file, function, line, mname);
	#else
		tlsf_free(tlsf, ptr);
	#endif

	}

	return r;
}

void tlsf_meminfo(tlsf_t pool, struct mem_info *info)
{
	control_t* control = tlsf_cast(control_t*, pool);
	memset(info, 0, sizeof(*info));
	info->free = control->total_size - control->real_used;
	info->max_used = control->max_used;
	info->real_used = control->real_used;
	info->total_frags = control->fragments;
	info->used = control->allocated;
	info->total_size = control->total_size;
}

unsigned long tlsf_available(tlsf_t pool)
{
	control_t* control = tlsf_cast(control_t*, pool);
	return (unsigned long)(control->total_size - control->real_used);
}

void tlsf_status(tlsf_t pool)
{
	int memlog, fl, sl;
	unsigned int len;
	char summary[FL_INDEX_COUNT];
	control_t* control = tlsf_cast(control_t*, pool);
	block_header_t* pb;

	memlog=cfg_get(core, core_cfg, memlog);
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ", "status of pool (%p):\n", pool);
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ", "heap size= %zu\n",
			control->total_size);
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
			"used= %zu, used+overhead=%zu, free=%zu, fragments=%zu\n",
			control->allocated, control->real_used, control->total_size - control->real_used, control->fragments);
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
			"max used (+overhead)=%zu, max fragments=%zu\n", control->max_used, control->max_fragments);

	/* print a summary of the 2 levels bucket list */
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
				"Free blocks matrix ('.': none, 'X': between 2^X and (2^(X+1)-1) free blocks, X=A..Z, A=0, B=1, ...)\n");
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
				"> first-level: %d block list arrays between 2^fl and 2^(fl+1) bytes (fl=%d..%d)\n",
				FL_INDEX_COUNT, FL_INDEX_SHIFT, FL_INDEX_MAX);
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
				"v second-level: %d block lists between 2^fl+sl*2^(fl-%d) and 2^fl+(sl+1)*2^(fl-%d)-1 bytes (sl=0..%d)\n",
				SL_INDEX_COUNT, SL_INDEX_COUNT_LOG2, SL_INDEX_COUNT_LOG2, SL_INDEX_COUNT-1);
	for (sl = 0 ; sl < SL_INDEX_COUNT ; sl++) {
		for (fl = 0 ; fl < FL_INDEX_COUNT ; fl++) {
			if (control->blocks[fl][sl] == &control->block_null) {
				summary[fl] = '.';
			} else {
				/* count free list length */
				len = 1;
				pb = control->blocks[fl][sl];
				while (pb->next_free != &control->block_null) {
					pb = pb->next_free;
					len++;
				}
				summary[fl] = 'A' + tlsf_fls(len);
			}
		}
		LOG_(DEFAULT_FACILITY, memlog, "tlsf_status: ",
					"%2d|%.*s|\n", sl, FL_INDEX_COUNT, summary);
	}
}

#ifdef DBG_TLSF_MALLOC

static mem_counter* get_mem_counter(mem_counter **root, block_header_t* f)
{
	mem_counter *x;
	if (!*root) goto make_new;
	for(x=*root;x;x=x->next)
		if (x->file == f->alloc_info.file && x->func == f->alloc_info.func && x->line == f->alloc_info.line)
			return x;
make_new:
	x = malloc(sizeof(mem_counter));
	x->file = f->alloc_info.file;
	x->func = f->alloc_info.func;
	x->mname= f->alloc_info.mname;
	x->line = f->alloc_info.line;
	x->count = 0;
	x->size = 0;
	x->next = *root;
	*root = x;
	return x;
}


void tlsf_sums(tlsf_t pool)
{
	int memlog;
	block_header_t* block = pool + tlsf_size() - sizeof(block_header_t*);
	mem_counter *root = NULL, *x;

	memlog=cfg_get(core, core_cfg, memlog);

	LOG_(DEFAULT_FACILITY, memlog, "tlsf_sums: ",
			"pool (%p) summarizing all alloc'ed. fragments:\n", pool);


	while (block && !block_is_last(block))
	{
		if(!block_is_free(block)) {
			x = get_mem_counter(&root, block);
			x->count++;
			x->size+=block_size(block);
		}

		block = block_next(block);
	}

	x = root;
	while(x){
		LOG_(DEFAULT_FACILITY, memlog, "tlsf_sums: ",
				" count=%6d size=%10lu bytes from %s: %s(%ld)\n",
			x->count,x->size,
			x->file, x->func, x->line
			);
		root = x->next;
		free(x);
		x = root;
	}
	LOG_(DEFAULT_FACILITY, memlog, "tlsf_sums: ",
			"-----------------------------\n");
}

void tlsf_mod_get_stats(tlsf_t pool, void **rootp)
{
	if (!rootp) {
		return ;
	}

	LM_DBG("get tlsf memory statistics\n");

	mem_counter **root = (mem_counter **) rootp;
	block_header_t* block = pool + tlsf_size() - sizeof(block_header_t*);
	mem_counter *x;

	while (block && !block_is_last(block))
	{
		if(!block_is_free(block)) {
			x = get_mem_counter(root, block);
			x->count++;
			x->size+=block_size(block);
		}

		block = block_next(block);
	}
}

void tlsf_mod_free_stats(void *rootp)
{
	if (!rootp) {
		return ;
	}

	LM_DBG("free tlsf memory statistics\n");

	mem_counter *root = (mem_counter *) rootp;
	mem_counter *x;
	x = root;
	while (x) {
		root = x->next;
		free(x);
		x = root;
	}
}

#else
void tlsf_sums(tlsf_t pool)
{}

void tlsf_mod_get_stats(tlsf_t pool, void **rootp)
{
	LM_WARN("Enable DBG_TLSF_MALLOC for getting statistics\n");
	return ;
}

void tlsf_mod_free_stats(void *rootp)
{
	LM_WARN("Enable DBG_TLSF_MALLOC for freeing statistics\n");
	return ;
}
#endif /* defined DBG_TLSF_MALLOC */

/*memory manager core api*/
static char *_tlsf_mem_name = "tlsf_malloc";

/* PKG - private memory API*/
static void *_tlsf_pkg_pool = 0;
static tlsf_t _tlsf_pkg_block = 0;

/**
 * \brief Destroy memory pool
 */
void tlsf_malloc_destroy_pkg_manager(void)
{
	if (_tlsf_pkg_pool) {
		free(_tlsf_pkg_pool);
		_tlsf_pkg_pool = 0;
	}
	_tlsf_pkg_block = 0;
}

/**
 * \brief Init memory pool
 */
int tlsf_malloc_init_pkg_manager(void)
{
	sr_pkg_api_t ma;
	_tlsf_pkg_pool = malloc(pkg_mem_size);
	if (_tlsf_pkg_pool) {
		_tlsf_pkg_block = tlsf_create_with_pool(_tlsf_pkg_pool, pkg_mem_size);
	} else {
		LOG(L_CRIT, "could not initialize tlsf pkg memory pool\n");
		fprintf(stderr, "Too much tlsf pkg memory demanded: %ld bytes\n",
						pkg_mem_size);
		return -1;
	}

	memset(&ma, 0, sizeof(sr_pkg_api_t));
	ma.mname      = _tlsf_mem_name;
	ma.mem_pool   = _tlsf_pkg_pool;
	ma.mem_block  = _tlsf_pkg_block;
	ma.xmalloc    = tlsf_malloc;
	ma.xmallocxz  = tlsf_mallocxz;
	ma.xfree      = tlsf_free;
	ma.xrealloc   = tlsf_realloc;
	ma.xreallocxf = tlsf_reallocxf;
	ma.xstatus    = tlsf_status;
	ma.xinfo      = tlsf_meminfo;
	ma.xavailable = tlsf_available;
	ma.xsums      = tlsf_sums;
	ma.xdestroy   = tlsf_malloc_destroy_pkg_manager;
	ma.xmodstats  = tlsf_mod_get_stats;
	ma.xfmodstats = tlsf_mod_free_stats;

	return pkg_init_api(&ma);
}

/* SHM - shared memory API*/
static void *_tlsf_shm_pool = 0;
static tlsf_t _tlsf_shm_block = 0;
static gen_lock_t* _tlsf_shm_lock = 0;

#define tlsf_shm_lock()    lock_get(_tlsf_shm_lock)
#define tlsf_shm_unlock()  lock_release(_tlsf_shm_lock)

/**
 *
 */
void tlsf_shm_glock(void* qmp)
{
	lock_get(_tlsf_shm_lock);
}

/**
 *
 */
void tlsf_shm_gunlock(void* qmp)
{
	lock_release(_tlsf_shm_lock);
}

/**
 *
 */
void tlsf_shm_lock_destroy(void)
{
	if (_tlsf_shm_lock){
		DBG("destroying the shared memory lock\n");
		lock_destroy(_tlsf_shm_lock); /* we don't need to dealloc it*/
	}
}

/**
 * init the core lock
 */
int tlsf_shm_lock_init(void)
{
	if (_tlsf_shm_lock) {
		LM_DBG("shared memory lock initialized\n");
		return 0;
	}

#ifdef DBG_TLSF_MALLOC
	_tlsf_shm_lock = tlsf_malloc(_tlsf_shm_block, sizeof(gen_lock_t),
						_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_);
#else
	_tlsf_shm_lock = tlsf_malloc(_tlsf_shm_block, sizeof(gen_lock_t));
#endif

	if (_tlsf_shm_lock==0){
		LOG(L_CRIT, "could not allocate lock\n");
		return -1;
	}
	if (lock_init(_tlsf_shm_lock)==0){
		LOG(L_CRIT, "could not initialize lock\n");
		return -1;
	}
	return 0;
}

/*SHM wrappers to sync the access to memory block*/
#ifdef DBG_TLSF_MALLOC
void* tlsf_shm_malloc(void* tlsfmp, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_malloc(tlsfmp, size, file, func, line, mname);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_mallocxz(void* tlsfmp, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_mallocxz(tlsfmp, size, file, func, line, mname);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_realloc(void* tlsfmp, void* p, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_realloc(tlsfmp, p, size, file, func, line, mname);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_reallocxf(void* tlsfmp, void* p, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_reallocxf(tlsfmp, p, size, file, func, line, mname);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_resize(void* tlsfmp, void* p, size_t size,
					const char* file, const char* func, unsigned int line, const char* mname)
{
	void *r;
	tlsf_shm_lock();
	if(p) tlsf_free(tlsfmp, p, file, func, line, mname);
	r = tlsf_malloc(tlsfmp, size, file, func, line, mname);
	tlsf_shm_unlock();
	return r;
}
void tlsf_shm_free(void* tlsfmp, void* p, const char* file, const char* func,
				unsigned int line, const char* mname)
{
	tlsf_shm_lock();
	tlsf_free(tlsfmp, p, file, func, line, mname);
	tlsf_shm_unlock();
}
#else
void* tlsf_shm_malloc(void* tlsfmp, size_t size)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_malloc(tlsfmp, size);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_mallocxz(void* tlsfmp, size_t size)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_mallocxz(tlsfmp, size);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_realloc(void* tlsfmp, void* p, size_t size)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_realloc(tlsfmp, p, size);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_reallocxf(void* tlsfmp, void* p, size_t size)
{
	void *r;
	tlsf_shm_lock();
	r = tlsf_reallocxf(tlsfmp, p, size);
	tlsf_shm_unlock();
	return r;
}
void* tlsf_shm_resize(void* tlsfmp, void* p, size_t size)
{
	void *r;
	tlsf_shm_lock();
	if(p) tlsf_free(tlsfmp, p);
	r = tlsf_malloc(tlsfmp, size);
	tlsf_shm_unlock();
	return r;
}
void tlsf_shm_free(void* tlsfmp, void* p)
{
	tlsf_shm_lock();
	tlsf_free(tlsfmp, p);
	tlsf_shm_unlock();
}
#endif
void tlsf_shm_status(void* tlsfmp)
{
	tlsf_shm_lock();
	tlsf_status(tlsfmp);
	tlsf_shm_unlock();
}
void tlsf_shm_info(void* tlsfmp, struct mem_info* info)
{
	tlsf_shm_lock();
	tlsf_meminfo(tlsfmp, info);
	tlsf_shm_unlock();
}
unsigned long tlsf_shm_available(void* tlsfmp)
{
	unsigned long r;
	tlsf_shm_lock();
	r = tlsf_available(tlsfmp);
	tlsf_shm_unlock();
	return r;
}
void tlsf_shm_sums(void* tlsfmp)
{
	tlsf_shm_lock();
	tlsf_sums(tlsfmp);
	tlsf_shm_unlock();
}


/**
 * \brief Destroy memory pool
 */
void tlsf_malloc_destroy_shm_manager(void)
{
	tlsf_shm_lock_destroy();
	/*shm pool from core - nothing to do*/
	_tlsf_shm_pool = 0;
	_tlsf_shm_block = 0;
}

/**
 * \brief Init memory pool
 */
int tlsf_malloc_init_shm_manager(void)
{
	sr_shm_api_t ma;
	_tlsf_shm_pool = shm_core_get_pool();
	if (_tlsf_shm_pool) {
		_tlsf_shm_block = tlsf_create_with_pool(_tlsf_shm_pool, shm_mem_size);
	} else {
		LOG(L_CRIT, "could not initialize tlsf shm memory pool\n");
		fprintf(stderr, "Too much tlsf shm memory demanded: %ld bytes\n",
						shm_mem_size);
		return -1;
	}

	memset(&ma, 0, sizeof(sr_shm_api_t));
	ma.mname          = _tlsf_mem_name;
	ma.mem_pool       = _tlsf_shm_pool;
	ma.mem_block      = _tlsf_shm_block;
	ma.xmalloc        = tlsf_shm_malloc;
	ma.xmallocxz      = tlsf_shm_mallocxz;
	ma.xmalloc_unsafe = tlsf_malloc;
	ma.xfree          = tlsf_shm_free;
	ma.xfree_unsafe   = tlsf_free;
	ma.xrealloc       = tlsf_shm_realloc;
	ma.xreallocxf     = tlsf_shm_reallocxf;
	ma.xresize        = tlsf_shm_resize;
	ma.xstatus        = tlsf_shm_status;
	ma.xinfo          = tlsf_shm_info;
	ma.xavailable     = tlsf_shm_available;
	ma.xsums          = tlsf_shm_sums;
	ma.xdestroy       = tlsf_malloc_destroy_shm_manager;
	ma.xmodstats      = tlsf_mod_get_stats;
	ma.xfmodstats     = tlsf_mod_free_stats;
	ma.xglock         = tlsf_shm_glock;
	ma.xgunlock       = tlsf_shm_gunlock;

	if(shm_init_api(&ma)<0) {
		LM_ERR("cannot initialize the core shm api\n");
		return -1;
	}
	if(tlsf_shm_lock_init()<0) {
		LM_ERR("cannot initialize the core shm lock\n");
		return -1;
	}
	return 0;
}

#endif /* TLSF_MALLOC */
