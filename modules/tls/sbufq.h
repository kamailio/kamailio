/* 
 * Kamailio TLS module
 *
 * Copyright (C) 2010 iptelorg GmbH
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
/** minimal overhead buffer queue in shm memory.
 * @file modules/tls/sbufq.h
 * @ingroup: tls
 * Module: @ref tls
 */

#ifndef __sbufq_h
#define __sbufq_h

#include "../../compiler_opt.h"
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../timer_ticks.h"
#include "../../timer.h"
#include "../../dprint.h"
#include <string.h>


struct sbuf_elem {
	struct sbuf_elem* next;
	unsigned int b_size; /**< buf size */
	char buf[1]; /**< variable size buffer */
};

struct sbuffer_queue {
	struct sbuf_elem* first;
	struct sbuf_elem* last;
	ticks_t last_chg; /**< last change (creation time or partial flush)*/
	unsigned int queued; /**< total size */
	unsigned int offset; /**< offset in the first buffer where unflushed data
							starts */
	unsigned int last_used; /**< how much of the last buffer is used */
};


/* sbufq_flush() output flags */
#define F_BUFQ_EMPTY 1
#define F_BUFQ_ERROR_FLUSH 2


#define sbufq_empty(bq) ((bq)->first==0)
#define sbufq_non_empty(bq) ((bq)->first!=0)



/** adds/appends data to a buffer queue.
 * WARNING: it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param q - buffer queue
 * @param data
 * @param size
 * @param min_buf_size - min size to allocate for new buffer elements
 * @return 0 on success, -1 on error (mem. allocation)
 */
inline static int sbufq_add(struct sbuffer_queue* q, const void* data,
							unsigned int size, unsigned int min_buf_size)
{
	struct sbuf_elem* b;
	unsigned int last_free;
	unsigned int b_size;
	unsigned int crt_size;
	
	get_ticks_raw();
	
	if (likely(q->last==0)) {
		b_size=MAX_unsigned(min_buf_size, size);
		b=shm_malloc(sizeof(*b)+b_size-sizeof(b->buf));
		if (unlikely(b==0))
			goto error;
		b->b_size=b_size;
		b->next=0;
		q->last=b;
		q->first=b;
		q->last_used=0;
		q->offset=0;
		q->last_chg=get_ticks_raw();
		last_free=b_size;
		crt_size=size;
		goto data_cpy;
	}else{
		b=q->last;
	}
	
	while(size){
		last_free=b->b_size-q->last_used;
		if (last_free==0){
			b_size=MAX_unsigned(min_buf_size, size);
			b=shm_malloc(sizeof(*b)+b_size-sizeof(b->buf));
			if (unlikely(b==0))
				goto error;
			b->b_size=b_size;
			b->next=0;
			q->last->next=b;
			q->last=b;
			q->last_used=0;
			last_free=b->b_size;
		}
		crt_size=MIN_unsigned(last_free, size);
data_cpy:
		memcpy(b->buf+q->last_used, data, crt_size);
		q->last_used+=crt_size;
		size-=crt_size;
		data+=crt_size;
		q->queued+=crt_size;
	}
	return 0;
error:
	return -1;
}



/** inserts data (at the beginning) in a buffer queue.
 * Note: should never be called after sbufq_run().
 * WARNING: it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param q - buffer queue
 * @param data
 * @param size
 * @param min_buf_size - min size to allocate for new buffer elements
 * @return 0 on success, -1 on error (mem. allocation)
 */
inline static int sbufq_insert(struct sbuffer_queue* q, const void* data, 
							unsigned int size, unsigned int min_buf_size)
{
	struct sbuf_elem* b;
	
	if (likely(q->first==0)) /* if empty, use sbufq_add */
		return sbufq_add(q, data, size, min_buf_size);
	
	if (unlikely(q->offset)){
		LOG(L_CRIT, "BUG: non-null offset %d (bad call, should"
				"never be called after sbufq_run())\n", q->offset);
		goto error;
	}
	if ((q->first==q->last) && ((q->last->b_size-q->last_used)>=size)){
		/* one block with enough space in it for size bytes */
		memmove(q->first->buf+size, q->first->buf, size);
		memcpy(q->first->buf, data, size);
		q->last_used+=size;
	}else{
		/* create a size bytes block directly */
		b=shm_malloc(sizeof(*b)+size-sizeof(b->buf));
		if (unlikely(b==0))
			goto error;
		b->b_size=size;
		/* insert it */
		b->next=q->first;
		q->first=b;
		memcpy(b->buf, data, size);
	}
	
	q->queued+=size;
	return 0;
error:
	return -1;
}


/** destroy a buffer queue.
 * Only the content is destroyed (shm_free()'d), the queue head is
 * re-intialized.
 * WARNING: it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param q - buffer queue
 * @return - number of bytes that used to be queued (>=0).
 */
inline static unsigned int sbufq_destroy(struct  sbuffer_queue* q)
{
	struct sbuf_elem* b;
	struct sbuf_elem* next_b;
	int unqueued;
	
	unqueued=0;
	if (likely(q->first)){
		b=q->first;
		do{
			next_b=b->next;
			unqueued+=(b==q->last)?q->last_used:b->b_size;
			if (b==q->first)
				unqueued-=q->offset;
			shm_free(b);
			b=next_b;
		}while(b);
	}
	memset(q, 0, sizeof(*q));
	return unqueued;
}



/** tries to flush the queue.
 * Tries to flush as much as possible from the given queue, using the 
 * given callback.
 * WARNING: it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param q - buffer queue
 * @param *flags - set to:
 *                   F_BUFQ_EMPTY if the queued is completely flushed
 *                   F_BUFQ_ERROR_FLUSH if the flush_f callback returned error.
 * @param flush_f - flush function (callback). modeled after write():
 *                    flush_f(param1, param2, const void* buf, unsigned size).
 *                    It should return the number of bytes "flushed" on
 *                    success, or <0 on error. If the number of bytes
 *                    "flushed" is smaller then the requested size, it
 *                    would be assumed that no more bytes can be flushed
 *                    and sbufq_flush will exit.
 * @param flush_p1 - parameter for the flush function callback.
 * @param flush_p2 - parameter for the flush function callback.
 * @return -1 on internal error, or the number of bytes flushed on
 *            success (>=0). Note that the flags param is
 *            always set and it should be used to check for errors, since
 *            a flush_f() failure will not result in a negative return.
 */
inline static int sbufq_flush(struct sbuffer_queue* q, int* flags,
								int (*flush_f)(void* p1, void* p2,
												const void* buf,
												unsigned size),
								void* flush_p1, void* flush_p2)
{
	struct sbuf_elem *b;
	int n;
	int ret;
	int block_size;
	char* buf;
	
	*flags=0;
	ret=0;
	while(q->first){
		block_size=((q->first==q->last)?q->last_used:q->first->b_size)-
						q->offset;
		buf=q->first->buf+q->offset;
		n=flush_f(flush_p1, flush_p2, buf, block_size);
		if (likely(n>0)){
			ret+=n;
			if (likely(n==block_size)){
				b=q->first;
				q->first=q->first->next; 
				shm_free(b);
				q->offset=0;
				q->queued-=block_size;
			}else{
				q->offset+=n;
				q->queued-=n;
				/* no break: if we are here n < block_size => partial write
				   => the write should be retried */
			}
		}else{
			if (unlikely(n<0))
				*flags|=F_BUFQ_ERROR_FLUSH;
			break;
		}
	}
	if (likely(q->first==0)){
		q->last=0;
		q->last_used=0;
		q->offset=0;
		*flags|=F_BUFQ_EMPTY;
	}
	return ret;
}




#endif /*__sbufq_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
