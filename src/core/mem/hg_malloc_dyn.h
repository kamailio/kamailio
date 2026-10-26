/*
 * slab allocator with elastic arenas - public entry points
 *
 * Copyright (C) 2026 Yury Kirsanov
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

/* entry points with the memapi.h signatures; locking is internal, so the
 * shm unsafe and locked variants are the same functions */

#ifdef DBG_HG_MALLOC
void *hg_malloc(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname)
#else
void *hg_malloc(void *hbp, size_t size)
#endif
{
	struct hg_block *hb = (struct hg_block *)hbp;
	void *p;

#ifdef DBG_HG_MALLOC
	MDBG("%s_malloc(%lu), called from %s: %s(%d)\n", hb->name,
			(unsigned long)size, file, func, line);
	p = hg_cell_alloc(hb, size, file, func, line, mname);
	MDBG("%s_malloc(%lu), returns address %p\n", hb->name, (unsigned long)size,
			p);
#else
	p = hg_cell_alloc(hb, size);
#endif
	return p;
}

#ifdef DBG_HG_MALLOC
void hg_free(void *hbp, void *p, const char *file, const char *func,
		unsigned int line, const char *mname)
#else
void hg_free(void *hbp, void *p)
#endif
{
	struct hg_block *hb = (struct hg_block *)hbp;

#ifdef DBG_HG_MALLOC
	MDBG("%s_free(%p), called from %s: %s(%d)\n", hb->name, p, file, func,
			line);
#endif
	if(p == 0) {
		MDBG("WARNING: %s_free: free(0) called\n", hb->name);
		return;
	}
#ifdef DBG_HG_MALLOC
	/* any live arena: a child may free a pre-fork pkg pointer, which
	 * hg_cell_free() hands back to its owner */
	if(!hg_owns_any(HG_HDR(p))) {
		if(likely(cfg_get(core, core_cfg, mem_safety) == 0)) {
			LM_CRIT("BUG: bad pointer %p (out of memory block (%p)!),"
					" called from %s: %s(%d) - aborting\n",
					p, hb, file, func, line);
			abort();
		}
		LM_CRIT("BUG: bad pointer %p (out of memory block (%p)!),"
				" called from %s: %s(%d) - ignoring\n",
				p, hb, file, func, line);
		return;
	}
	hg_cell_free(hb, p, file, func, line, mname);
#else
	hg_cell_free(hb, p);
#endif
}

#ifdef DBG_HG_MALLOC
void *hg_realloc(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void *hg_realloc(void *hbp, void *p, size_t size)
#endif
{
	struct hg_block *hb = (struct hg_block *)hbp;
	unsigned char cls;
	unsigned long cur_total = 0, need_total, cur_payload = 0, copy, asked;
	union hg_hdr_w hw;
	unsigned short hsum;
	int st;
	void *ptr;

#ifdef DBG_HG_MALLOC
	MDBG("%s_realloc(%p, ->%lu), called from %s: %s(%d)\n", hb->name, p,
			(unsigned long)size, file, func, line);
#endif

	if(size == 0) {
		if(p)
#ifdef DBG_HG_MALLOC
			hg_free(hb, p, file, func, line, mname);
#else
			hg_free(hb, p);
#endif
		return NULL;
	}

	if(!p)
#ifdef DBG_HG_MALLOC
		return hg_malloc(hb, size, file, func, line, mname);
#else
		return hg_malloc(hb, size);
#endif

	/* same size class still fits: keep the cell; large objects always move.
	 * Ownership is checked first, so a foreign pointer is refused, not read,
	 * and so is one into memory its arena has released */
	{
		struct hg_block *own = hg_owner(HG_HDR(p));

		if(!own || !hg_backed(own, HG_HDR(p))) {
			LM_CRIT("%s: realloc of %p, which is %s - refusing it\n", hb->name,
					p,
					own ? "in memory its arena has released"
						: "from no live arena");
			hg_corrupt(hb, HG_C_FOREIGN_PTR);
			return NULL;
		}
	}

	/* and only a LIVE or LIVE_G cell with an intact header: its class byte is
	 * what the size below is read from */
	st = hg_hdr_read(HG_HDR(p), &hw, &hsum);
	if(!hg_st_live(st)) {
		LM_CRIT("%s: realloc of %p, which is %s - refusing it\n", hb->name, p,
				st < 0 ? "damaged or not a cell" : "free");
		hg_corrupt(hb, st < 0 ? HG_C_HEADER : HG_C_DOUBLE_FREE);
		return NULL;
	}
	cls = hw.f.cls;
	/* payload the caller can read: a large cell carries HG_LFRAG_HDR_SIZE
	 * before its cell header */
	if(cls == HG_LARGE_MARKER) {
		cur_total = hg_large_frag_size_at(HG_HDR(p) - HG_LFRAG_HDR_SIZE);
		cur_payload = cur_total > HG_LFRAG_HDR_SIZE + HG_CELL_HDR
							  ? cur_total - HG_LFRAG_HDR_SIZE - HG_CELL_HDR
							  : 0;
	} else if(cls >= HG_NCLASSES) {
		LM_CRIT("%s: cell %p carries invalid class %u - aborting "
				"realloc\n",
				hb->name, p, cls);
		hg_corrupt(hb, HG_C_BAD_CLASS);
		return NULL;
	} else {
		cur_total = hg_cell_total_size(cls);
		cur_payload = cur_total > HG_CELL_HDR ? cur_total - HG_CELL_HDR : 0;
		/* a guarded cell stays guarded, an unguarded one unguarded */
		need_total = ((size + (st == HG_ST_LIVE_G ? HG_GUARD_ROOM : 0)
							  + HG_ROUNDTO - 1)
							 / HG_ROUNDTO)
							 * HG_ROUNDTO
					 + HG_CELL_HDR;
		if(hb->dbg)
			need_total += HG_DBG_TAIL;
		if(need_total <= cur_total) {
			/* the red zone (debug arena) or the guard moves to the new
			 * size, checked first */
			struct hg_block *own = hb->dbg || HG_SAN_ON || st == HG_ST_LIVE_G
										   ? hg_owner(HG_HDR(p))
										   : NULL;

			if(own) {
				hg_san_open(own, HG_HDR(p), cur_payload);
				if(own->dbg) {
					hg_dbg_tail_check(own, HG_HDR(p), cur_payload);
					hg_dbg_tail_set(HG_HDR(p), cur_payload, size);
				} else if(st == HG_ST_LIVE_G) {
					if(!hg_guard_ok(HG_HDR(p), hsum, cur_payload, &asked))
						hg_guard_bad(own, HG_HDR(p), cur_payload, asked);
					hg_guard_set(HG_HDR(p), hsum, cur_payload, size);
				}
				hg_san_give(own, HG_HDR(p), cur_payload, size);
			}
			return p;
		}
	}

#ifdef DBG_HG_MALLOC
	ptr = hg_malloc(hb, size, file, func, line, mname);
#else
	ptr = hg_malloc(hb, size);
#endif

	if(ptr) {
		/* copy min(old, new) payload: a large-tier shrink must not overrun */
		copy = cur_payload < size ? cur_payload : size;
		/* the copy reads past what the caller asked for: a sanitizer
		 * must see the old payload open first (hg_san.h) */
		if(HG_SAN_ON) {
			struct hg_block *own = hg_owner(HG_HDR(p));

			if(own)
				hg_san_open(own, HG_HDR(p), cur_payload);
		}
		memcpy(ptr, p, copy);
#ifdef DBG_HG_MALLOC
		hg_free(hb, p, file, func, line, mname);
#else
		hg_free(hb, p);
#endif
	}

	return ptr;
}

/* zero-filled */
#ifdef DBG_HG_MALLOC
void *hg_mallocxz(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname)
#else
void *hg_mallocxz(void *hbp, size_t size)
#endif
{
	void *p;

#ifdef DBG_HG_MALLOC
	p = hg_malloc(hbp, size, file, func, line, mname);
#else
	p = hg_malloc(hbp, size);
#endif
	if(p)
		memset(p, 0, size);
	return p;
}

/* last byte set to 0 */
#ifdef DBG_HG_MALLOC
void *hg_mallocxn(void *hbp, size_t size, const char *file, const char *func,
		unsigned int line, const char *mname)
#else
void *hg_mallocxn(void *hbp, size_t size)
#endif
{
	void *p;

#ifdef DBG_HG_MALLOC
	p = hg_malloc(hbp, size, file, func, line, mname);
#else
	p = hg_malloc(hbp, size);
#endif
	if(p && size > 0)
		((char *)p)[size - 1] = 0;
	return p;
}

/* the old buffer is freed when the realloc fails */
#ifdef DBG_HG_MALLOC
void *hg_reallocxf(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void *hg_reallocxf(void *hbp, void *p, size_t size)
#endif
{
	void *r;

#ifdef DBG_HG_MALLOC
	r = hg_realloc(hbp, p, size, file, func, line, mname);
	if(!r && p)
		hg_free(hbp, p, file, func, line, mname);
#else
	r = hg_realloc(hbp, p, size);
	if(!r && p)
		hg_free(hbp, p);
#endif
	return r;
}

/* free, then allocate: the contents are not kept */
#ifdef DBG_HG_MALLOC
void *hg_resize(void *hbp, void *p, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void *hg_resize(void *hbp, void *p, size_t size)
#endif
{
#ifdef DBG_HG_MALLOC
	if(p)
		hg_free(hbp, p, file, func, line, mname);
	return hg_malloc(hbp, size, file, func, line, mname);
#else
	if(p)
		hg_free(hbp, p);
	return hg_malloc(hbp, size);
#endif
}
