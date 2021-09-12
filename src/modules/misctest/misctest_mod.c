/**
 *
 * Memory allocators debugging/test sip-router module.
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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/mem/mem.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/locking.h"
#include "../../core/atomic_ops.h"
#include "../../core/cfg/cfg.h"
#include "../../core/rpc.h"
#include "../../core/rand/fastrand.h"
#include "../../core/timer.h"
#include "../../core/mod_fix.h"

#include "../../core/parser/sdp/sdp.h"
#include "../../core/parser/parse_uri.c"
#include "../../core/parser/parse_hname2.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_refer_to.h"
#include "../../core/parser/parse_ppi_pai.h"
#include "../../core/parser/parse_privacy.h"
#include "../../core/parser/parse_diversion.h"

MODULE_VERSION

static int mt_mem_alloc_f(struct sip_msg *, char *, char *);
static int mt_mem_free_f(struct sip_msg *, char *, char *);
static int mod_init(void);
static void mod_destroy(void);

static int misctest_memory_init(void);
static int misctest_message_init(void);
int misctest_hexprint(void *data, size_t length, int linelen, int split);

static int misctest_memory = 0;
static int misctest_message = 0;
static str misctest_message_data = STR_NULL;
static str misctest_message_file = STR_NULL;

/* clang-format off */
static cmd_export_t cmds[]={
	{"mt_mem_alloc", mt_mem_alloc_f, 1, fixup_var_int_1, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"mt_mem_free", mt_mem_free_f, 1, fixup_var_int_1, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{0, 0, 0, 0, 0}
};
/* clang-format on */


/* clang-format off */
struct cfg_group_misctest {
	int mem_check_content;
	int mem_realloc_p; /* realloc probability */
};
/* clang-format on */


/* clang-format off */
static struct cfg_group_misctest default_mt_cfg = {
	0, /* mem_check_content, off by default */
	0  /* mem realloc probability, 0 by default */
};
/* clang-format on */

static void *mt_cfg = &default_mt_cfg;


/* clang-format off */
static cfg_def_t misctest_cfg_def[] = {
	{"mem_check_content", CFG_VAR_INT | CFG_ATOMIC, 0, 1, 0, 0,
		"check if allocated memory was overwritten by filling it with "
		"a special pattern and checking it on free."},
	{"mem_realloc_p", CFG_VAR_INT | CFG_ATOMIC, 0, 90, 0, 0,
		"realloc probability in percents. During tests and mem_rnd_alloc"
		" mem_realloc_p percents of the allocations will be made by realloc'ing"
		" and existing chunk. The maximum value is limited to 90, to avoid"
		" very long mem_rnd_alloc runs (a realloc might also free memory)." },
	{0, 0, 0, 0, 0, 0}
};
/* clang-format on */


static rpc_export_t mt_rpc[];


/* clang-format off */
static param_export_t params[]={
	{"memory", PARAM_INT, &misctest_memory},
	{"message", PARAM_INT, &misctest_message},
	{"message_data", PARAM_STR, &misctest_message_data},
	{"message_file", PARAM_STR, &misctest_message_file},
	{"mem_check_content", PARAM_INT, &default_mt_cfg.mem_check_content},
	{0,0,0}
};
/* clang-format on */


/* clang-format off */
struct module_exports exports = {
	"misctest",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	mt_rpc,        	/* RPC methods */
	0,       	/* pseudo-variables exports */
	0,        	/* response function*/
	mod_init, 	/* module initialization function */
	0,         	/* per-child init function */
	mod_destroy 	/* destroy function */
};
/* clang-format on */


#define MC_F_CHECK_CONTENTS 1

struct mem_chunk
{
	struct mem_chunk *next;
	void *addr;
	unsigned long size;
	unsigned long flags;
};

struct allocated_list
{
	struct mem_chunk *chunks;
	gen_lock_t lock;
	volatile long size;
	volatile int no;
};

struct allocated_list *alloc_lst = NULL;


struct rnd_time_test
{
	unsigned long min;
	unsigned long max;
	unsigned long total;
	unsigned long crt;
	ticks_t min_intvrl;
	ticks_t max_intvrl;
	ticks_t stop_time;
	ticks_t start_time;
	unsigned long calls;
	unsigned long reallocs;
	unsigned int errs;
	unsigned int overfl;
	struct rnd_time_test *next;
	struct timer_ln timer;
	int id;
};

struct rnd_time_test_lst
{
	struct rnd_time_test *tests;
	gen_lock_t lock;
	volatile int last_id;
};


struct rnd_time_test_lst *rndt_lst = NULL;

static unsigned long mem_unleak(unsigned long size);
static void mem_destroy_all_tests();

static int mod_init(void)
{
	WARN("This is a test/debugging module, don't use it in production\n");
	/* declare configuration */
	if(cfg_declare("misctest", misctest_cfg_def, &default_mt_cfg,
			   cfg_sizeof(misctest), &mt_cfg)) {
		ERR("failed to register the configuration\n");
		goto error;
	}

	if(misctest_memory!=0) {
		if(misctest_memory_init()<0) {
			goto error;
		}
	}

	if(misctest_message!=0) {
		if(misctest_message_init()<0) {
			goto error;
		}
		return -1;
	}

	return 0;
error:
	return -1;
}


static void mod_destroy()
{
	if(misctest_memory!=0) {
		if(rndt_lst) {
			mem_destroy_all_tests();
			lock_destroy(&rndt_lst->lock);
			shm_free(rndt_lst);
			rndt_lst = 0;
		}
		if(alloc_lst) {
			mem_unleak(-1);
			lock_destroy(&alloc_lst->lock);
			shm_free(alloc_lst);
			alloc_lst = 0;
		}
	}
}

static int misctest_memory_init(void)
{
	alloc_lst = shm_malloc(sizeof(*alloc_lst));
	if(alloc_lst == 0)
		goto error;
	alloc_lst->chunks = 0;
	atomic_set_long(&alloc_lst->size, 0);
	atomic_set_int(&alloc_lst->no, 0);
	if(lock_init(&alloc_lst->lock) == 0)
		goto error;
	rndt_lst = shm_malloc(sizeof(*rndt_lst));
	if(rndt_lst == 0)
		goto error;
	rndt_lst->tests = 0;
	atomic_set_int(&rndt_lst->last_id, 0);
	if(lock_init(&rndt_lst->lock) == 0)
		goto error;

	return 0;
error:
	return -1;
}

static int misctest_message_init(void)
{
	char tbuf[BUF_SIZE];
	FILE *f;
	long fsize;
    sip_msg_t tmsg = { };

	if(misctest_message_data.s!=0 && misctest_message_data.len>0) {
		if(misctest_message_data.len>=BUF_SIZE-2) {
			LM_ERR("the data is too big\n");
			return -1;
		}
		memcpy(tbuf, misctest_message_data.s, misctest_message_data.len);
		tbuf[misctest_message_data.len] = '\0';
		tmsg.len = misctest_message_data.len;
	} else if(misctest_message_file.s!=0 && misctest_message_file.len>0) {
		LM_DBG("reading data from file: %.*s\n", misctest_message_file.len,
				misctest_message_file.s);
		f = fopen(misctest_message_file.s, "r");
		if(f==NULL) {
			LM_ERR("cannot open file: %.*s\n", misctest_message_file.len,
					misctest_message_file.s);
			return -1;
		}
		fseek(f, 0, SEEK_END);
		fsize = ftell(f);
		if(fsize<0) {
			LM_ERR("ftell failed on file: %.*s\n", misctest_message_file.len,
					misctest_message_file.s);
			fclose(f);
			return -1;
		}
		fseek(f, 0, SEEK_SET);

		if(fsize>=BUF_SIZE-2) {
			LM_ERR("the file data is too big\n");
			fclose(f);
			return -1;

		}
		if(fread(tbuf, fsize, 1, f) != fsize) {
			if(ferror(f)) {
				LM_ERR("error reading from file: %.*s\n",
					misctest_message_file.len, misctest_message_file.s);
			}
		}
		fclose(f);

		tbuf[fsize] = 0;
		tmsg.len = (int)fsize;
	} else {
		LM_ERR("no input data\n");
		return -1;
	}

	tmsg.buf = tbuf;

	LM_INFO("data - start: %p - end: %p - len: %d\n", tmsg.buf,
			tmsg.buf + tmsg.len, tmsg.len);
	LM_INFO("data - content: [[%.*s]]\n", tmsg.len, tmsg.buf);

	misctest_hexprint(tmsg.buf, tmsg.len, 20, 10);

	if (parse_msg(tmsg.buf, tmsg.len, &tmsg) < 0) {
		goto cleanup;
	}

	parse_sdp(&tmsg);

	parse_headers(&tmsg, HDR_TO_F, 0);

	parse_contact_header(&tmsg);

	parse_refer_to_header(&tmsg);

	parse_to_header(&tmsg);

	parse_pai_header(&tmsg);

	parse_diversion_header(&tmsg);

	parse_privacy(&tmsg);

cleanup:
	free_sip_msg(&tmsg);

	return 0;
}

/**
 *	misctest_hexprint - output a hex dump of a buffer
 *
 *	data - pointer to the buffer
 *	length - length of buffer to write
 *	linelen - number of chars to output per line
 *	split - number of chars in each chunk on a line
 */

int misctest_hexprint(void *data, size_t length, int linelen, int split)
{
	char buffer[512];
	char *ptr;
	const void *inptr;
	int pos;
	int remaining = length;

	inptr = data;

	if(sizeof(buffer) <= (3 + (4 * (linelen / split)) + (linelen * 4))) {
		LM_ERR("buffer size is too small\n");
		return -1;
	}

	while (remaining > 0) {
		int lrem;
		int splitcount;
		ptr = buffer;

		lrem = remaining;
		splitcount = 0;
		for (pos = 0; pos < linelen; pos++) {

			if (split == splitcount++) {
				sprintf(ptr, "  ");
				ptr += 2;
				splitcount = 1;
			}

			if (lrem) {
				sprintf(ptr, "%0.2x ", *((unsigned char *) inptr + pos));
				lrem--;
			} else {
				sprintf(ptr, "   ");
			}
			ptr += 3;
		}

		*ptr++ = ' ';
		*ptr++ = ' ';

		lrem = remaining;
		splitcount = 0;
		for (pos = 0; pos < linelen; pos++) {
			unsigned char c;

			if (split == splitcount++) {
				sprintf(ptr, "  ");
				ptr += 2;
				splitcount = 1;
			}

			if (lrem) {
				c = *((unsigned char *) inptr + pos);
				if (c > 31 && c < 127) {
					sprintf(ptr, "%c", c);
				} else {
					sprintf(ptr, ".");
				}
				lrem--;
			}
			ptr++;
		}

		*ptr = '\0';
		LM_INFO("%s\n", buffer);

		inptr += linelen;
		remaining -= linelen;
	}

	return 0;
}

/** record a memory chunk list entry.
 * @param addr - address of the newly allocated memory
 * @oaram size - size
 * @return 0 on success, -1 on error (no more mem).
 */
static int mem_track(void *addr, unsigned long size)
{
	struct mem_chunk *mc;
	unsigned long *d;
	unsigned long r, i;

	mc = shm_malloc(sizeof(*mc));
	if(mc == 0)
		goto error;
	mc->addr = addr;
	mc->size = size;
	mc->flags = 0;
	if(cfg_get(misctest, mt_cfg, mem_check_content)) {
		mc->flags |= MC_F_CHECK_CONTENTS;
		d = addr;
		for(r = 0; r < size / sizeof(*d); r++) {
			d[r] = ~(unsigned long)&d[r];
		}
		for(i = 0; i < size % sizeof(*d); i++) {
			((char *)&d[r])[i] = ~((unsigned long)&d[r] >> i * 8);
		}
	}
	lock_get(&alloc_lst->lock);
	mc->next = alloc_lst->chunks;
	alloc_lst->chunks = mc;
	lock_release(&alloc_lst->lock);
	atomic_add_long(&alloc_lst->size, size);
	atomic_inc_int(&alloc_lst->no);
	return 0;
error:
	return -1;
}


/** allocate memory.
 * Allocates memory, but keeps track of it, so that mem_unleak() can
 * free it.
 * @param size - how many bytes
 * @return 0 on success, -1 on error
 */
static int mem_leak(unsigned long size)
{
	void *d;

	d = shm_malloc(size);
	if(d) {
		if(mem_track(d, size) < 0) {
			shm_free(d);
		} else
			return 0;
	}
	return -1;
}


/* realloc a chunk, unsafe (requires external locking) version.
 * @return 0 on success, -1 on error
 */
static int _mem_chunk_realloc_unsafe(struct mem_chunk *c, unsigned long size)
{
	unsigned long *d;
	int r, i;

	d = shm_realloc(c->addr, size);
	if(d) {
		if(cfg_get(misctest, mt_cfg, mem_check_content)
				&& c->flags & MC_F_CHECK_CONTENTS) {
			/* re-fill the test patterns (the address might have changed
			   and they depend on it) */
			for(r = 0; r < size / sizeof(*d); r++) {
				d[r] = ~(unsigned long)&d[r];
			}
			for(i = 0; i < size % sizeof(*d); i++) {
				((char *)&d[r])[i] = ~((unsigned long)&d[r] >> i * 8);
			}
		}
		c->addr = d;
		c->size = size;
		return 0;
	}
	return -1;
}


static void mem_chunk_free(struct mem_chunk *c)
{
	unsigned long *d;
	unsigned long r, i;
	int err;

	if(cfg_get(misctest, mt_cfg, mem_check_content)
			&& c->flags & MC_F_CHECK_CONTENTS) {
		d = c->addr;
		err = 0;
		for(r = 0; r < c->size / sizeof(*d); r++) {
			if(d[r] != ~(unsigned long)&d[r])
				err++;
			d[r] = (unsigned long)&d[r]; /* fill it with something else */
		}
		for(i = 0; i < c->size % sizeof(*d); i++) {
			if(((unsigned char *)&d[r])[i]
					!= (unsigned char)~((unsigned long)&d[r] >> i * 8))
				err++;
			((char *)&d[r])[i] = (unsigned char)((unsigned long)&d[r] >> i * 8);
		}
		if(err)
			ERR("%d errors while checking %ld bytes at %p\n", err, c->size, d);
	}
	shm_free(c->addr);
	c->addr = 0;
	c->flags = 0;
}


/** free memory.
 * Frees previously allocated memory chunks until at least size bytes are
 * released. Use -1 to free all,
 * @param size - at least free size bytes.
 * @return  bytes_freed (>=0)
 */
static unsigned long mem_unleak(unsigned long size)
{
	struct mem_chunk **mc;
	struct mem_chunk *t;
	struct mem_chunk **min_chunk;
	unsigned long freed;
	unsigned int no;

	freed = 0;
	no = 0;
	min_chunk = 0;
	lock_get(&alloc_lst->lock);
	if(size >= atomic_get_long(&alloc_lst->size)) {
		/* free all */
		for(mc = &alloc_lst->chunks; *mc;) {
			t = *mc;
			mem_chunk_free(t);
			freed += t->size;
			no++;
			*mc = t->next;
			shm_free(t);
		}
		alloc_lst->chunks = 0;
	} else {
		/* free at least size bytes, trying smaller chunks first */
		for(mc = &alloc_lst->chunks; *mc && (freed < size);) {
			if((*mc)->size <= (size - freed)) {
				t = *mc;
				mem_chunk_free(t);
				freed += t->size;
				no++;
				*mc = t->next;
				shm_free(t);
				continue;
			} else if(min_chunk == 0 || (*min_chunk)->size > (*mc)->size) {
				/* find minimum remaining chunk  */
				min_chunk = mc;
			}
			mc = &(*mc)->next;
		}
		if(size > freed && min_chunk) {
			mc = min_chunk;
			t = *mc;
			mem_chunk_free(t);
			freed += t->size;
			no++;
			*mc = (*mc)->next;
			shm_free(t);
		}
	}
	lock_release(&alloc_lst->lock);
	atomic_add_long(&alloc_lst->size, -freed);
	atomic_add_int(&alloc_lst->no, -no);
	return freed;
}


/** realloc randomly size bytes.
 * Chooses randomly a previously allocated chunk and realloc')s it.
 * @param size - size.
 * @param diff - filled with difference, >= 0 means more bytes were alloc.,
 *               < 0 means bytes were freed.
 * @return  >= 0 on success, -1 on error/ not found
 * (empty list is a valid error reason)
 */
static int mem_rnd_realloc(unsigned long size, long *diff)
{
	struct mem_chunk *t;
	int ret;
	int target, i;

	*diff = 0;
	ret = -1;
	lock_get(&alloc_lst->lock);
	target = fastrand_max(atomic_get_int(&alloc_lst->no));
	for(t = alloc_lst->chunks, i = 0; t; t = t->next, i++) {
		if(target == i) {
			*diff = (long)size - (long)t->size;
			if((ret = _mem_chunk_realloc_unsafe(t, size)) < 0)
				*diff = 0;
			break;
		}
	}
	lock_release(&alloc_lst->lock);
	atomic_add_long(&alloc_lst->size, *diff);
	return ret;
}


#define MIN_ulong(a, b) \
	(unsigned long)((unsigned long)(a) < (unsigned long)(b) ? (a) : (b))

/*
 * Randomly alloc. total_size bytes, in chunks of size between
 * min & max. max - min should be smaller then 4G.
 * @return < 0 if there were some alloc errors, 0 on success.
 */
static int mem_rnd_leak(
		unsigned long min, unsigned long max, unsigned long total_size)
{
	unsigned long size;
	unsigned long crt_size, crt_min;
	long diff;
	int err, p;

	size = total_size;
	err = 0;
	while(size) {
		crt_min = MIN_ulong(min, size);
		crt_size = fastrand_max(MIN_ulong(max, size) - crt_min) + crt_min;
		p = cfg_get(misctest, mt_cfg, mem_realloc_p);
		if(p && ((fastrand_max(99) + 1) <= p)) {
			if(mem_rnd_realloc(crt_size, &diff) == 0) {
				size -= diff;
				continue;
			} /* else fallback to normal alloc. */
		}
		size -= crt_size;
		err += mem_leak(crt_size) < 0;
	}
	return -err;
}


/* test timer */
static ticks_t tst_timer(ticks_t ticks, struct timer_ln *tl, void *data)
{
	struct rnd_time_test *tst;
	ticks_t next_int;
	ticks_t max_int;
	unsigned long crt_size, crt_min, remaining;
	long diff;
	int p;

	tst = data;

	next_int = 0;
	max_int = 0;

	if(tst->total <= tst->crt) {
		mem_unleak(tst->crt);
		tst->crt = 0;
		tst->overfl++;
	}
	remaining = tst->total - tst->crt;
	crt_min = MIN_ulong(tst->min, remaining);
	crt_size = fastrand_max(MIN_ulong(tst->max, remaining) - crt_min) + crt_min;
	p = cfg_get(misctest, mt_cfg, mem_realloc_p);
	if(p && ((fastrand_max(99) + 1) <= p)) {
		if(mem_rnd_realloc(crt_size, &diff) == 0) {
			tst->crt -= diff;
			tst->reallocs++;
			goto skip_alloc;
		}
	}
	if(mem_leak(crt_size) >= 0)
		tst->crt += crt_size;
	else
		tst->errs++;
skip_alloc:
	tst->calls++;

	if(TICKS_GT(tst->stop_time, ticks)) {
		next_int = fastrand_max(tst->max_intvrl - tst->min_intvrl)
				   + tst->min_intvrl;
		max_int = tst->stop_time - ticks;
	} else {
		/* stop test */
		WARN("test %d time expired, stopping"
			 " (%d s runtime, %ld calls, %d overfl, %d errors,"
			 " crt %ld bytes)\n",
				tst->id, TICKS_TO_S(ticks - tst->start_time), tst->calls,
				tst->overfl, tst->errs, tst->crt);
		mem_unleak(tst->crt);
		/* tst->crt = 0 */;
	}

	/* 0 means stop stop, so if next_int == 0 => stop */
	return MIN_unsigned(next_int, max_int);
}


/*
 * start a malloc test of a test_time length:
 *  - randomly between min_intvrl and max_intvrl, alloc.
 *    a random number of bytes, between min & max.
 *  - if total_size is reached, free everything.
 *
 * @returns test id (>=0) on success, -1 on error.
 */
static int mem_leak_time_test(unsigned long min, unsigned long max,
		unsigned long total_size, ticks_t min_intvrl, ticks_t max_intvrl,
		ticks_t test_time)
{
	struct rnd_time_test *tst;
	struct rnd_time_test *l;
	ticks_t first_int;
	int id;

	tst = shm_malloc(sizeof(*tst));
	if(tst == 0)
		goto error;
	memset(tst, 0, sizeof(*tst));
	id = tst->id = atomic_add_int(&rndt_lst->last_id, 1);
	tst->min = min;
	tst->max = max;
	tst->total = total_size;
	tst->min_intvrl = min_intvrl;
	tst->max_intvrl = max_intvrl;
	tst->start_time = get_ticks_raw();
	tst->stop_time = get_ticks_raw() + test_time;
	first_int = fastrand_max(max_intvrl - min_intvrl) + min_intvrl;
	timer_init(&tst->timer, tst_timer, tst, 0);
	lock_get(&rndt_lst->lock);
	tst->next = rndt_lst->tests;
	rndt_lst->tests = tst;
	lock_release(&rndt_lst->lock);
	if(timer_add(&tst->timer, MIN_unsigned(first_int, test_time)) < 0)
		goto error;
	return id;
error:
	if(tst) {
		lock_get(&rndt_lst->lock);
		for(l = rndt_lst->tests; l; l = l->next)
			if(l->next == tst) {
				l->next = tst->next;
				break;
			}
		lock_release(&rndt_lst->lock);
		shm_free(tst);
	}
	return -1;
}


static int is_mem_test_stopped(struct rnd_time_test *tst)
{
	return TICKS_LE(tst->stop_time, get_ticks_raw());
}

/** stops test tst.
 * @return 0 on success, -1 on error (test already stopped)
 */
static int mem_test_stop_tst(struct rnd_time_test *tst)
{
	if(!is_mem_test_stopped(tst)) {
		if(timer_del(&tst->timer) == 0) {
			tst->stop_time = get_ticks_raw();
			return 0;
		}
	}
	return -1;
}


/** stops test id.
 * @return 0 on success, -1 on error (not found).
 */
static int mem_test_stop(int id)
{
	struct rnd_time_test *tst;

	lock_get(&rndt_lst->lock);
	for(tst = rndt_lst->tests; tst; tst = tst->next)
		if(tst->id == id) {
			mem_test_stop_tst(tst);
			break;
		}
	lock_release(&rndt_lst->lock);
	return -(tst == 0);
}


static void mem_destroy_all_tests()
{
	struct rnd_time_test *tst;
	struct rnd_time_test *nxt;

	lock_get(&rndt_lst->lock);
	for(tst = rndt_lst->tests; tst;) {
		nxt = tst->next;
		mem_test_stop_tst(tst);
		shm_free(tst);
		tst = nxt;
	}
	rndt_lst->tests = 0;
	lock_release(&rndt_lst->lock);
}


static int mem_test_destroy(int id)
{
	struct rnd_time_test *tst;
	struct rnd_time_test **crt_lnk;

	lock_get(&rndt_lst->lock);
	for(tst = 0, crt_lnk = &rndt_lst->tests; *crt_lnk;
			crt_lnk = &(*crt_lnk)->next)
		if((*crt_lnk)->id == id) {
			tst = *crt_lnk;
			mem_test_stop_tst(tst);
			*crt_lnk = tst->next;
			shm_free(tst);
			break;
		}
	lock_release(&rndt_lst->lock);
	return -(tst == 0);
}

/* script functions: */


static int mt_mem_alloc_f(struct sip_msg *msg, char *sz, char *foo)
{
	int size;

	if(sz == 0 || get_int_fparam(&size, msg, (fparam_t *)sz) < 0)
		return -1;
	return mem_leak(size) >= 0 ? 1 : -1;
}


static int mt_mem_free_f(struct sip_msg *msg, char *sz, char *foo)
{
	int size;
	unsigned long freed;

	size = -1;
	if(sz != 0 && get_int_fparam(&size, msg, (fparam_t *)sz) < 0)
		return -1;
	freed = mem_unleak(size);
	return (freed == 0) ? 1 : freed;
}


/* RPC exports: */


/* helper functions, parses an optional b[ytes]|k|m|g to a numeric shift value
   (e.g. b -> 0, k -> 10, ...)
   returns bit shift value on success, -1 on error
*/
static int rpc_get_size_mod(rpc_t *rpc, void *c)
{
	char *m;

	if(rpc->scan(c, "*s", &m) > 0) {
		switch(*m) {
			case 'b':
			case 'B':
				return 0;
			case 'k':
			case 'K':
				return 10;
			case 'm':
			case 'M':
				return 20;
			case 'g':
			case 'G':
				return 30;
			default:
				rpc->fault(c, 500, "bad param use b|k|m|g");
				return -1;
		}
	}
	return 0;
}


static const char *rpc_mt_alloc_doc[2] = {
		"Allocates the specified number of bytes (debugging/test function)."
		"Use b|k|m|g to specify the desired size unit",
		0};

static void rpc_mt_alloc(rpc_t *rpc, void *c)
{
	int size;
	int rs;

	if(rpc->scan(c, "d", &size) < 1) {
		return;
	}
	rs = rpc_get_size_mod(rpc, c);
	if(rs < 0)
		/* fault already generated on rpc_get_size_mod() error */
		return;
	if(mem_leak((unsigned long)size << rs) < 0) {
		rpc->fault(c, 400, "memory allocation failed");
	}
	return;
}


static const char *rpc_mt_realloc_doc[2] = {
		"Reallocates the specified number of bytes from a pre-allocated"
		" randomly selected memory chunk. If no pre-allocated memory"
		" chunks exists, it will fail."
		" Make sure mt.mem_used is non 0 or call mt.mem_alloc prior to calling"
		" this function."
		" Returns the difference in bytes (<0 if bytes were freed, >0 if more"
		" bytes were allocated)."
		"Use b|k|m|g to specify the desired size unit",
		0};

static void rpc_mt_realloc(rpc_t *rpc, void *c)
{
	int size;
	int rs;
	long diff;

	if(rpc->scan(c, "d", &size) < 1) {
		return;
	}
	rs = rpc_get_size_mod(rpc, c);
	if(rs < 0)
		/* fault already generated on rpc_get_size_mod() error */
		return;
	if(mem_rnd_realloc((unsigned long)size << rs, &diff) < 0) {
		rpc->fault(c, 400, "memory allocation failed");
	}
	rpc->add(c, "d", diff >> rs);
	return;
}


static const char *rpc_mt_free_doc[2] = {
		"Frees the specified number of bytes, previously allocated by one of "
		"the"
		" other misctest functions (e.g. mt.mem_alloc or the script "
		"mt_mem_alloc). Use b|k|m|g to specify the desired size unit."
		"Returns the number of bytes freed (can be higher or"
		" smaller then the requested size)",
		0};


static void rpc_mt_free(rpc_t *rpc, void *c)
{
	int size;
	int rs;

	size = -1;
	rs = 0;
	if(rpc->scan(c, "*d", &size) > 0) {
		/* found size, look if a size modifier is present */
		rs = rpc_get_size_mod(rpc, c);
		if(rs < 0)
			/* fault already generated on rpc_get_size_mod() error */
			return;
	}
	rpc->add(c, "d", (int)(mem_unleak((unsigned long)size << rs) >> rs));
	return;
}


static const char *rpc_mt_used_doc[2] = {
		"Returns how many memory chunks and how many bytes are currently"
		" allocated via the mem_alloc module functions."
		" Use b|k|m|g to specify the desired size unit.",
		0};


static void rpc_mt_used(rpc_t *rpc, void *c)
{
	int rs;

	rs = 0;
	rs = rpc_get_size_mod(rpc, c);
	if(rs < 0)
		/* fault already generated on rpc_get_size_mod() error */
		return;
	rpc->add(c, "d", atomic_get_int(&alloc_lst->no));
	rpc->add(c, "d", (int)(atomic_get_long(&alloc_lst->size) >> rs));
	return;
}


static const char *rpc_mt_rnd_alloc_doc[2] = {
		"Takes 4 parameters: min, max, total_size and an optional unit "
		"(b|k|m|g)."
		" It will allocate total_size memory, in pieces of random size between"
		"min .. max (inclusive).",
		0};


static void rpc_mt_rnd_alloc(rpc_t *rpc, void *c)
{
	int min, max, total_size;
	int rs;
	int err;

	if(rpc->scan(c, "ddd", &min, &max, &total_size) < 3) {
		return;
	}
	rs = rpc_get_size_mod(rpc, c);
	if(rs < 0)
		/* fault already generated on rpc_get_size_mod() error */
		return;
	if(min > max || min < 0 || max > total_size) {
		rpc->fault(c, 400, "invalid parameter values");
		return;
	}
	if((err = mem_rnd_leak((unsigned long)min << rs, (unsigned long)max << rs,
				(unsigned long)total_size << rs))
			< 0) {
		rpc->fault(c, 400, "memory allocation failed (%d errors)", -err);
	}
	return;
}


static const char *rpc_mt_test_start_doc[2] = {
		"Takes 7 parameters: min, max, total_size, min_interval, max_interval, "
		"test_time and an optional size unit (b|k|m|g). All the time units are "
		"ms."
		" It will run a memory allocation test for test_time ms. At a random"
		" interval between min_interval and max_interval ms. it will allocate a"
		" memory chunk with random size, between min and max. Each time "
		"total_size"
		" is reached, it will free all the memory allocated and start again."
		"Returns the test id (integer)",
		0};


static void rpc_mt_test_start(rpc_t *rpc, void *c)
{
	int min, max, total_size;
	int min_intvrl, max_intvrl, total_time;
	int rs;
	int id;

	if(rpc->scan(c, "dddddd", &min, &max, &total_size, &min_intvrl, &max_intvrl,
			   &total_time)
			< 6) {
		return;
	}
	rs = rpc_get_size_mod(rpc, c);
	if(rs < 0)
		/* fault already generated on rpc_get_size_mod() error */
		return;
	if(min > max || min < 0 || max > total_size) {
		rpc->fault(c, 400, "invalid size parameters values");
		return;
	}
	if(min_intvrl > max_intvrl || min_intvrl <= 0 || max_intvrl > total_time) {
		rpc->fault(c, 400, "invalid time intervals values");
		return;
	}
	if((id = mem_leak_time_test((unsigned long)min << rs,
				(unsigned long)max << rs, (unsigned long)total_size << rs,
				MS_TO_TICKS(min_intvrl), MS_TO_TICKS(max_intvrl),
				MS_TO_TICKS(total_time)))
			< 0) {
		rpc->fault(c, 400, "memory allocation failed");
	} else {
		rpc->add(c, "d", id);
	}
	return;
}


static const char *rpc_mt_test_stop_doc[2] = {
		"Takes 1 parameter: the test id. It will stop the corresponding test."
		"Note: the test is stopped, but not destroyed.",
		0};


static void rpc_mt_test_stop(rpc_t *rpc, void *c)
{
	int id;

	if(rpc->scan(c, "d", &id) < 1) {
		return;
	}
	if(mem_test_stop(id) < 0) {
		rpc->fault(c, 400, "test %d not found", id);
	}
	return;
}


static const char *rpc_mt_test_destroy_doc[2] = {
		"Takes 1 parameter: the test id. It will destroy the corresponding "
		"test.",
		0};


static void rpc_mt_test_destroy(rpc_t *rpc, void *c)
{
	int id;

	if(rpc->scan(c, "*d", &id) > 0 && id != -1) {
		if(mem_test_destroy(id) < 0)
			rpc->fault(c, 400, "test %d not found", id);
	} else {
		mem_destroy_all_tests();
	}
	return;
}


static const char *rpc_mt_test_destroy_all_doc[2] = {
		"It will destroy all the tests (running or stopped).", 0};


static void rpc_mt_test_destroy_all(rpc_t *rpc, void *c)
{
	mem_destroy_all_tests();
	return;
}


static const char *rpc_mt_test_list_doc[2] = {
		"If a test id parameter is provided it will list the corresponding "
		"test,"
		" else it will list all of them. Use b |k | m | g as a second parameter"
		" for the size units (default bytes)",
		0};


static void rpc_mt_test_list(rpc_t *rpc, void *c)
{
	int id, rs;
	struct rnd_time_test *tst;
	void *h;

	rs = 0;
	if(rpc->scan(c, "*d", &id) < 1) {
		id = -1;
	} else {
		rs = rpc_get_size_mod(rpc, c);
		if(rs < 0)
			return;
	}
	lock_get(&rndt_lst->lock);
	for(tst = rndt_lst->tests; tst; tst = tst->next)
		if(tst->id == id || id == -1) {
			rpc->add(c, "{", &h);
			rpc->struct_add(h, "ddddddddddd", "ID           ", tst->id,
					"run time (s) ",
					(int)TICKS_TO_S((TICKS_LE(tst->stop_time, get_ticks_raw())
													? tst->stop_time
													: get_ticks_raw())
									- tst->start_time),
					"remaining (s)",
					TICKS_LE(tst->stop_time, get_ticks_raw())
							? 0
							: (int)TICKS_TO_S(tst->stop_time - get_ticks_raw()),
					"total calls  ", (int)tst->calls, "reallocs     ",
					(int)tst->reallocs, "errors       ", (int)tst->errs,
					"overflows    ", (int)tst->overfl, "total alloc  ",
					(int)((tst->crt + tst->overfl * tst->total) >> rs),
					"min          ", (int)(tst->min >> rs), "max          ",
					(int)(tst->max >> rs), "total        ",
					(int)(tst->total >> rs));
			if(id != -1)
				break;
		}
	lock_release(&rndt_lst->lock);

	return;
}


/* clang-format off */
static rpc_export_t mt_rpc[] = {
	{"mt.mem_alloc", rpc_mt_alloc, rpc_mt_alloc_doc, 0},
	{"mt.mem_free", rpc_mt_free, rpc_mt_free_doc, 0},
	{"mt.mem_realloc", rpc_mt_realloc, rpc_mt_realloc_doc, 0},
	{"mt.mem_used", rpc_mt_used, rpc_mt_used_doc, 0},
	{"mt.mem_rnd_alloc", rpc_mt_rnd_alloc, rpc_mt_rnd_alloc_doc, 0},
	{"mt.mem_test_start", rpc_mt_test_start, rpc_mt_test_start_doc, 0},
	{"mt.mem_test_stop", rpc_mt_test_stop, rpc_mt_test_stop_doc, 0},
	{"mt.mem_test_destroy", rpc_mt_test_destroy, rpc_mt_test_destroy_doc, 0},
	{"mt.mem_test_destroy_all", rpc_mt_test_destroy_all,
								rpc_mt_test_destroy_all_doc, 0},
	{"mt.mem_test_list", rpc_mt_test_list, rpc_mt_test_list_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */
