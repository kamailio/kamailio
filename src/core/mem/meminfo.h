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
 * \brief Memory manager (malloc) informations and statistics
 *
 * Memory manager (malloc) informations and statistics, like the used memory,
 * fragmentations etc..
 * \ingroup mem
 */

#ifndef meminfo_h
#define meminfo_h

#define MEM_TYPE_SHM	0
#define MEM_TYPE_PKG	1

/** Memory information structure */
struct mem_info{
	unsigned long total_size; /** total size of memory pool */
	unsigned long free_size; /** free memory */
	unsigned long used_size; /** allocated size */
	unsigned long real_used; /** used size plus overhead from malloc */
	unsigned long max_used; /** maximum used size since server start? */
	unsigned long min_frag; /** minimum number of fragmentations? */
	unsigned long total_frags; /** number of total memory fragments */
};

typedef struct _mem_counter{
	const char *file;
	const char *func;
	const char *mname;
	unsigned long line;

	unsigned long size;
	int count;

	struct _mem_counter *next;
} mem_counter;

/** Memory report structure */
typedef struct mem_report {
	unsigned long total_size;  /** total size of memory pool */
	unsigned long free_size_s; /** free memory (stats) */
	unsigned long free_size_m; /** free memory (measured) */
	unsigned long used_size_s; /** allocated size (stats) */
	unsigned long used_size_m; /** allocated size (measured) */
	unsigned long real_used_s; /** used size plus overhead from malloc */
	unsigned long max_used_s;  /** maximum used size since server start? */
	unsigned long free_frags;  /** number of total free memory fragments */
	unsigned long used_frags;  /** number of total used memory fragments */
	unsigned long total_frags; /** number of total memory fragments */

	unsigned long max_free_frag_size;
	const char   *max_free_frag_file;
	const char   *max_free_frag_func;
	const char   *max_free_frag_mname;
	unsigned long max_free_frag_line;

	unsigned long min_free_frag_size;
	const char   *min_free_frag_file;
	const char   *min_free_frag_func;
	const char   *min_free_frag_mname;
	unsigned long min_free_frag_line;

	unsigned long max_used_frag_size;
	const char   *max_used_frag_file;
	const char   *max_used_frag_func;
	const char   *max_used_frag_mname;
	unsigned long max_used_frag_line;

	unsigned long min_used_frag_size;
	const char   *min_used_frag_file;
	const char   *min_used_frag_func;
	const char   *min_used_frag_mname;
	unsigned long min_used_frag_line;
} mem_report_t;

#endif

