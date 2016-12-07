/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 *  
 *  2/27/00:3am, random plans by jer
 *  
 *  ok based on gprof, we really need some innovation here... my thoughs are this:
 *  
 *  most things are strings, so have a string-based true-blue garbage collector
 *  one big global hash containing all the strings created by any pstrdup, returning const char *
 *  a refcount on each string block
 *  when a pool is freed, it moves down the refcount
 *  garbage collector collects pools on the free stack, and runs through the hash for unused strings
 *  j_strcmp can check for == (if they are both from a pstrdup)
 *  
 *  let's see... this would change:
 *  pstrdup: do a hash lookup, success=return, fail=pmalloc & hash put
 *  pool_free: 
 *  
 */

/*! \file
 * \ingroup xmpp
 */


#include "xode.h"
//#include "config.h"

#define _xode_pool__malloc malloc
#define _xode_pool__free   free

/* xode_pfree - a linked list node which stores an
   allocation chunk, plus a callback */
struct xode_pool_free
{
    xode_pool_cleaner f;
    void *arg;
    struct xode_pool_heap *heap;
    struct xode_pool_free *next;
};

/* make an empty pool */
xode_pool _xode_pool_new(void)
{
    xode_pool p;
    while((p = _xode_pool__malloc(sizeof(_xode_pool))) == NULL) sleep(1);
    p->cleanup = NULL;
    p->heap = NULL;
    p->size = 0;

    return p;
}

/* free a heap */
void _xode_pool_heapfree(void *arg)
{
    struct xode_pool_heap *h = (struct xode_pool_heap *)arg;

    _xode_pool__free(h->block);
    _xode_pool__free(h);
}

/* mem should always be freed last */
void _xode_pool_cleanup_append(xode_pool p, struct xode_pool_free *pf)
{
    struct xode_pool_free *cur;

    if(p->cleanup == NULL)
    {
        p->cleanup = pf;
        return;
    }

    /* fast forward to end of list */
    for(cur = p->cleanup; cur->next != NULL; cur = cur->next);

    cur->next = pf;
}

/* create a cleanup tracker */
struct xode_pool_free *_xode_pool_free(xode_pool p, xode_pool_cleaner f, void *arg)
{
    struct xode_pool_free *ret;

    /* make the storage for the tracker */
    while((ret = _xode_pool__malloc(sizeof(struct xode_pool_free))) == NULL) sleep(1);
    ret->f = f;
    ret->arg = arg;
    ret->next = NULL;

    return ret;
}

/* create a heap and make sure it get's cleaned up */
struct xode_pool_heap *_xode_pool_heap(xode_pool p, int size)
{
    struct xode_pool_heap *ret;
    struct xode_pool_free *clean;

    /* make the return heap */
    while((ret = _xode_pool__malloc(sizeof(struct xode_pool_heap))) == NULL) sleep(1);
    while((ret->block = _xode_pool__malloc(size)) == NULL) sleep(1);
    ret->size = size;
    p->size += size;
    ret->used = 0;

    /* append to the cleanup list */
    clean = _xode_pool_free(p, _xode_pool_heapfree, (void *)ret);
    clean->heap = ret; /* for future use in finding used mem for pstrdup */
    _xode_pool_cleanup_append(p, clean);

    return ret;
}

xode_pool _xode_pool_newheap(int bytes)
{
    xode_pool p;
    p = _xode_pool_new();
    p->heap = _xode_pool_heap(p,bytes);
    return p;
}

void *xode_pool_malloc(xode_pool p, int size)
{
    void *block;

    if(p == NULL)
    {
        fprintf(stderr,"Memory Leak! xode_pmalloc received NULL pool, unable to track allocation, exiting]\n");
        abort();
    }

    /* if there is no heap for this pool or it's a big request, just raw, I like how we clean this :) */
    if(p->heap == NULL || size > (p->heap->size / 2))
    {
        while((block = _xode_pool__malloc(size)) == NULL) sleep(1);
        p->size += size;
        _xode_pool_cleanup_append(p, _xode_pool_free(p, _xode_pool__free, block));
        return block;
    }

    /* we have to preserve boundaries, long story :) */
    if(size >= 4)
        while(p->heap->used&7) p->heap->used++;

    /* if we don't fit in the old heap, replace it */
    if(size > (p->heap->size - p->heap->used))
        p->heap = _xode_pool_heap(p, p->heap->size);

    /* the current heap has room */
    block = (char *)p->heap->block + p->heap->used;
    p->heap->used += size;
    return block;
}

void *xode_pool_mallocx(xode_pool p, int size, char c)
{
   void* result = xode_pool_malloc(p, size);
   if (result != NULL)
           memset(result, c, size);
   return result;
}  

/* easy safety utility (for creating blank mem for structs, etc) */
void *xode_pool_malloco(xode_pool p, int size)
{
    void *block = xode_pool_malloc(p, size);
    memset(block, 0, size);
    return block;
}  

/* XXX efficient: move this to const char * and then loop through the existing heaps to see if src is within a block in this pool */
char *xode_pool_strdup(xode_pool p, const char *src)
{
    char *ret;

    if(src == NULL)
        return NULL;

    ret = xode_pool_malloc(p,strlen(src) + 1);
    strcpy(ret,src);

    return ret;
}

/* when move above, this one would actually return a new block */
char *xode_pool_strdupx(xode_pool p, const char *src)
{
    return xode_pool_strdup(p, src);
}

int xode_pool_size(xode_pool p)
{
    if(p == NULL) return 0;

    return p->size;
}

void xode_pool_free(xode_pool p)
{
    struct xode_pool_free *cur, *stub;

    if(p == NULL) return;

    cur = p->cleanup;
    while(cur != NULL)
    {
        (*cur->f)(cur->arg);
        stub = cur->next;
        _xode_pool__free(cur);
        cur = stub;
    }

    _xode_pool__free(p);
}

/* public cleanup utils, insert in a way that they are run FIFO, before mem frees */
void xode_pool_cleanup(xode_pool p, xode_pool_cleaner f, void *arg)
{
    struct xode_pool_free *clean;

    clean = _xode_pool_free(p, f, arg);
    clean->next = p->cleanup;
    p->cleanup = clean;
}

xode_pool xode_pool_new(void)
{
    return _xode_pool_new();
}

xode_pool xode_pool_heap(const int bytes)
{
    return _xode_pool_newheap(bytes);
}
