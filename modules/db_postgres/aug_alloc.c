/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */
/*
** ________________________________________________________________________
**
**
**                      $RCSfile$
**                     $Revision$
**
**             Last change $Date$
**           Last change $Author$
**                        $State$
**                       $Locker$
**
**               Original author: Andrew Fullford
**
**           Copyright (C) August Associates  1995
**
** ________________________________________________________________________
*/

#include "aug_std.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef double MemAlign;
typedef augUInt32 MemMagic;
typedef union MemHead MemHead;
typedef struct MemOpt MemOpt;
typedef struct MemDestructor MemDestructor;	/* not yet implemented */

/*
**  One of these MemHead structs is allocated at the head of
**  each alloc, plus an extra magic number at the end area.
**  This gives an allocation overhead of:
**
**	malloc_overhead + sizeof MemHead + sizeof MemMagic
**
**  "Notes" entry for the man page: the allocation overhead is way
**  too high.  (On a 32bit machine and assuming a malloc overhead
**  of 8 bytes, the total will be 8 + 32 + 4 = 44 bytes).
*/
struct MemHeadStruct
{
	MemHead *parent, *sibling, *child;
	MemOpt *options;
	char *end;
	char *file;
	augUInt32 line;
	MemMagic magic;
};

/*
**  Attempt to guarantee alignment.
*/
union MemHead
{
	struct MemHeadStruct m;
	MemAlign align[1];
};

/*
**  MemOpt holds optional features.  The only current example
**  is the memory destructor state.
*/
struct MemOpt
{
	MemMagic magic;
	MemDestructor *destructor_list;
};

/*
**  These magic numbers are used to validate headers, force memory
**  to a known state, etc.
*/
#define MEM_MAGIC_BOUND	0xC0EDBABE
#define MEM_MAGIC_FILL	0xDEADC0DE

static int mem_bad(MemHead *mem, char *where, char *file, int line)
{
	aug_abort(file, line, "Corrupted memory in %s", where);
}

/*
**  Calculate the MemHead address given an aug_alloc() pointer.
*/
#define MEM_CROWN(alloc) ((MemHead *)(((char *)alloc) - sizeof (MemHead)))
#define MEM_DECAPITATE(mem) ((void *)(((char *)mem) + sizeof (MemHead)))

static MemMagic mem_magic = MEM_MAGIC_BOUND;
#define MEM_TAIL(p) (memcmp((p)->m.end,(char*)&mem_magic,sizeof mem_magic)==0)
#define MEM_CHECK(p,w) ((p) && \
			((p)->m.magic != MEM_MAGIC_BOUND || !MEM_TAIL(p)) && \
			mem_bad((p),(w),file,line))

/*  Initialize stats structure with estimated overhead */
static augAllocStats mem_stats = {sizeof (MemHead) + sizeof (MemMagic) + 8, 0};

static augNoMemFunc *mem_nomem_func = 0;
static void mem_nomem(size_t size, char *func, char *file, int line)
{
	static augBool active = augFALSE;
	char *module;

	if(!func)
		func = "unknown function";

	if(active)
		fprintf(stderr, "\r\n\nPANIC: nomem bounce\r\n\n");
	else
	{
		active = augTRUE;
		if(mem_nomem_func)
			(*mem_nomem_func)(size, func, file, line);
	}

	fprintf(stderr, "\r\n\n");

	module = aug_module();
	if(module && *module)
		fprintf(stderr, "FATAL in %s: ", module);
	else
		fprintf(stderr, "FATAL: ");

	fprintf(stderr, "%s failure allocating %lu bytes ", func, size);

	if(file && *file)
		fprintf(stderr, "from +%d %s \r\n", line, file);
	else
		fprintf(stderr, "(unknown location) \r\n");

	fprintf(stderr, "              Current allocations: %10lu \r\n",
		(mem_stats.alloc_ops - mem_stats.free_ops));
	fprintf(stderr, "                Total allocations: %10lu \r\n",
		mem_stats.alloc_ops);
	fprintf(stderr, "              Total reallocations: %10lu \r\n",
		mem_stats.realloc_ops);
	fprintf(stderr, "                      Total frees: %10lu \r\n",
		mem_stats.free_ops);
	fprintf(stderr, "Estimated total heap use (KBytes): %10lu \r\n",
		(mem_stats.current_bytes_allocated +
		(mem_stats.alloc_ops - mem_stats.free_ops) *
		mem_stats.estimated_overhead_per_alloc + 512)/1024);
	fprintf(stderr, "\n");
		
	aug_exit(augEXIT_NOMEM);
}

static void *mem_alloc(size_t size, void *parent, char *file, int line)
{
	MemHead *mem, *par;
	DABNAME("mem_alloc");

	if(parent)
	{
		par = MEM_CROWN(parent);
		MEM_CHECK(par, "parent");
		MEM_CHECK(par->m.child, "sibling");
		MEM_CHECK(par->m.sibling, "uncle");
	}
	else
		par = 0;

	mem_stats.current_bytes_allocated += size;
	mem_stats.alloc_ops++;

	/*  Adjust for overhead  */
	size += sizeof (MemHead);

	mem = malloc(size + sizeof (MemMagic));
	if(!mem)
		mem_nomem(size, "aug_alloc", file, line);

	if(DABLEVEL(DAB_STD))
	{
		unsigned long *p;
		p = (unsigned long *)mem;
		while((char *)p <= (char *)mem + size)
			*p++ = MEM_MAGIC_FILL;
	}

	mem->m.magic = MEM_MAGIC_BOUND;
	mem->m.file = file;
	mem->m.line = line;
	mem->m.end = (char *)mem + size;
	mem->m.options = 0;
	mem->m.child = 0;
	mem->m.parent = par;

	if(par)
	{
		if(mem->m.sibling = par->m.child)
			mem->m.sibling->m.parent = mem;
		par->m.child = mem;
	}
	else
		mem->m.sibling = 0;

	memcpy(mem->m.end, (char *)&mem_magic, sizeof mem_magic);

	return MEM_DECAPITATE(mem);
}

static void mem_free(MemHead *mem)
{
	size_t size;
	DABNAME("mem_free");

	while(mem)
	{
		MemHead *next = mem->m.sibling;
		if(mem->m.child)
			mem_free(mem->m.child);
		size = (char *)mem->m.end - (char *)mem;
		size -= sizeof (MemHead) + sizeof (MemMagic);
		mem_stats.current_bytes_allocated -= size;
		mem_stats.free_ops++;
		if(DABLEVEL(DAB_STD))
		{
			unsigned long *p = (unsigned long *)(mem+1);

			while((char *)p <= mem->m.end)
				*p++ = MEM_MAGIC_FILL;
			p = (unsigned long *)mem;
			while(p < (unsigned long *)(mem+1))
				*p++ = MEM_MAGIC_FILL;
		}
		free(mem);
		mem = next;
	}
}

static augBool mem_find(MemHead *mem, MemHead *p)
{
	while(mem)
	{
		MemHead *next;

		if(mem == p)
			return augTRUE;
		next = mem->m.sibling;
		if(mem->m.child)
			if(mem_find(mem->m.child, p))
				return augTRUE;
		mem = next;
	}
	return augFALSE;
}

augExport augNoMemFunc *aug_set_nomem_func(augNoMemFunc *new_func)
{
	augNoMemFunc *old = mem_nomem_func;
	DABNAME("aug_set_nomem_func");

	mem_nomem_func = new_func;

	DABTRACE("New nomem func %08lx, previous %08lx",
		(unsigned long)mem_nomem_func, (unsigned long)old);
	return old;
}

augExport augAllocStats *aug_alloc_stats(void)
{
	return &mem_stats;
}

augExport void *aug_alloc_loc(size_t size, void *parent, char *file, int line)
{
	void *alloc;
	DABNAME("aug_alloc");

	DAB("size %lu, parent %08lx [+%d %s]",
		(unsigned long)size, (unsigned long)parent, line, file);

	alloc = mem_alloc(size, parent, file, line);

	DABL(80)("size %lu with header, caller mem at %08lx",
		MEM_CROWN(alloc)->m.end - (char *)MEM_CROWN(alloc),
		(unsigned long)alloc);

	return alloc;
}

augExport void *aug_realloc_loc(size_t size, void *prev, char *file, int line)
{
	void *alloc;
	size_t prev_size;
	MemHead *mem, *par, *kid, *sib, *new;
	DABNAME("aug_realloc");

	if(!prev)
		aug_abort(file, line, "Attempt to realloc a NULL pointer");

	mem = MEM_CROWN(prev);
	MEM_CHECK(mem, "previous alloc");

	par = mem->m.parent;	MEM_CHECK(par, "realloc parent");
	kid = mem->m.child;	MEM_CHECK(kid, "realloc child");
	sib = mem->m.sibling;	MEM_CHECK(sib, "realloc sibling");

	prev_size = (mem->m.end - (char *)mem) - sizeof (MemHead);

	DAB("prior size %lu, new %lu [+%d %s]",
				(unsigned long)prev_size, (unsigned long)size,
				line, file);
	DABL(80)("prior mem %08lx", (unsigned long)mem);

	mem_stats.current_bytes_allocated += size - prev_size;
	mem_stats.realloc_ops++;

	size += sizeof (MemHead);

	new = realloc(mem, size + sizeof (MemMagic));
	if(!new)
		mem_nomem(size, "aug_realloc", file, line);
	new->m.end = (char *)new + size;

	memcpy(new->m.end, (char *)&mem_magic, sizeof mem_magic);

	if(par)
	{
		if(par->m.sibling == mem)
			par->m.sibling = new;
		else
			par->m.child = new;
	}
	if(kid)
		kid->m.parent = new;
	if(sib)
		sib->m.parent = new;

	alloc = MEM_DECAPITATE(new);

	DABL(80)("size %lu with header, caller mem at %08lx",
			new->m.end - (char *)new, (unsigned long)alloc);

	return alloc;
}

augExport void aug_free_loc(void *alloc, char *file, int line)
{
	MemHead *mem, *par;
	DABNAME("aug_free");

	if(!alloc)
		aug_abort(file, line, "Attempt to free a NULL pointer");

	DAB("Freeing %08lx [+%d %s]", (unsigned long)alloc, line, file);

	mem = MEM_CROWN(alloc);
	MEM_CHECK(mem, "alloc to free");

	par = mem->m.parent;
	MEM_CHECK(par, "parent in free");

	if(par)
	{
		if(par->m.sibling == mem)
			par->m.sibling = mem->m.sibling;
		else
			par->m.child = mem->m.sibling;
	}

	if(mem->m.sibling)
	{
		mem->m.sibling->m.parent = par;
		mem->m.sibling = 0;
	}

	mem_free(mem);
}

augExport void aug_foster_loc(void *alloc, void *parent, char *file, int line)
{
	MemHead *mem, *fpar, *ppar, *sib;
	DABNAME("aug_foster");

	DAB("Foster %08lx to %08lx [+%d %s]",
						alloc, parent, line, file);

	if(!alloc)
		aug_abort(file, line, "Attempt to foster a NULL pointer");

	mem = MEM_CROWN(alloc);
	MEM_CHECK(mem, "alloc to foster");

	if(parent)
	{
		fpar = MEM_CROWN(parent);
		MEM_CHECK(fpar, "foster parent");
	}
	else
		fpar = 0;

	ppar = mem->m.parent; MEM_CHECK(ppar, "prior parent");
	sib = mem->m.sibling; MEM_CHECK(ppar, "sibling in foster");

	if(fpar == ppar)
	{
		DABTRACE("No change in parent (%08lx)", (unsigned long)fpar);
		return;
	}

	if(mem == fpar)
		aug_abort(file, line, "Attempt to adopt self");

	/*
	**  Check for incest - isnew parent actually our child?
	*/
	if(mem_find(mem->m.child, fpar))
		aug_abort(file, line, "Attempt to adopt a parent");

	/*
	**  Leave home.
	*/
	if(!ppar)
	{
		DABBULK("Leaving orphanage");
		if(mem->m.sibling)
			mem->m.sibling->m.parent = 0;
	}
	else if(ppar->m.sibling == mem)
	{
		DABBULK("Older child");
		ppar->m.sibling = mem->m.sibling;
		if(ppar->m.sibling)
			ppar->m.sibling->m.parent = ppar;
	}
	else
	{
		DABBULK("Youngest child");
		ppar->m.child = mem->m.sibling;
		if(ppar->m.child)
			ppar->m.child->m.parent = ppar;
	}

	/*
	**  Find new home.
	*/
	mem->m.parent = fpar;
	if(fpar)
	{
		mem->m.sibling = fpar->m.child;
		fpar->m.child = mem;
		if(mem->m.sibling)
			mem->m.sibling->m.parent = mem;
	}
	else
		mem->m.sibling = 0;
}

augExport char *aug_strdup_loc(char *str, void *parent, char *file, int line)
{
	char *new;
	size_t size;
	DABNAME("aug_strdup");

	if(!str)
		aug_abort(file, line, "Attempt to duplicate a NULL string");

	size = strlen(str)+1;
	DAB("string length %lu [+%d %s]", (unsigned long)size, line, file);

	new = mem_alloc(size, parent, file, line);

	DABL(80)("size %lu with header, caller mem at %08lx",
		MEM_CROWN(new)->m.end - (char *)MEM_CROWN(new),
		(unsigned long)new);

	strcpy(new, str);
	return new;
}

augExport char **aug_vecdup_loc(char **vec, void *parent, char *file, int line)
{
	char **nv, **v, *c;
	size_t size;
	int vsize;
	DABNAME("aug_vecdup");

	if(!vec)
		aug_abort(file, line, "Attempt to duplicate a NULL vector");

	size = 0;
	for(v = vec; *v; v++)
		size += strlen(*v) + 1;
	vsize = v - vec;
	DABL(80)("%d elements, total string size %d", vsize, size);

	vsize++;

	nv = (char **)mem_alloc(vsize * sizeof *v + size, parent, file, line);
	c = (char *)(nv + vsize);
	for(v = nv; *vec; v++, vec++)
	{
		strcpy(c, *vec);
		*v = c;
		c += strlen(c) + 1;
	}
	*v = 0;

	return nv;
}

#ifdef TEST
static void nomem(size_t size, char *func, char *file, int line)
{
	fprintf(stderr, "\nNOMEM on %lu bytes via %s, called from %s line %d\n",
		(unsigned long)size, func, file, line);
	/*
	**  Normally would exit from here, but might as well test the
	**  default trap.
	*/
	return;
}

main(int argc, char **argv)
{
	int i;
	void *par;
	char *mem, *m, **v;

	aug_setmodule(argv[0]);

	printf("<MemHead size %lu> should equal <struct MemHead %lu>\n",
		sizeof (MemHead), sizeof (struct MemHead));

	par = aug_alloc(20, 0);
	for(i = 0; i < 20; i++)
	{
		if(i == 10)
			mem = aug_strdup("Hello, world\n", par);
		else
			(void)aug_alloc(3000, par);
	}

	mem = aug_realloc(10000, mem);

	for(i = 0; i < 20; i++)
	{
		if(i == 10)
			m = aug_strdup("Hello, world\n", mem);
		else
			(void)aug_alloc(3000, par);
	}

	v = aug_vecdup(argv, par);

	printf("Program args:");
	while(*v)
	{
		printf(" %s", *v++);
		fflush(stdout);
	}
	printf("\n");

	aug_foster(m, par);

	if(argc > 1)
	{
		printf("Checking anti-incest test ... this should abort\n");
		aug_foster(par, mem);
	}

	for(i = 0; i < 20; i++)
	{
		if(i == 10)
			m = aug_strdup("Hello, world\n", mem);
		else
			(void)aug_alloc(3000, mem);
	}

	mem = aug_realloc(10000, mem);

	aug_foster(m, par);

	aug_free(mem);

	printf("If you can read this, the test completed ok\n");
	printf("Now testing NOMEM func - this should abort after a while ... ");
	fflush(stdout);

	aug_set_nomem_func(nomem);

	while(m = aug_alloc(128*1024, par))
		continue;

	/* Should never get to here */

	aug_free(par);

	aug_exit(augEXIT_YES);
}
#endif
