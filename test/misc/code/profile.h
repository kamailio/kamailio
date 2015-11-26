/*
 * Copyright (C) 2007 iptelorg GmbH
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
/*
 * Basic profile using the cpu cycle counter
 *
 * cycles_t - an unsigned interger type used for storing the cpu cycles
 *            (unsigned long long for now)
 *
 * cycles_t get_cpu_cycles() - returns the current cpu cycles counter
 *
 * void     get_cpu_cycles_uint(unsigned* u1, unsigned* u2) 
 *                            - sets u1 and u2 to the least significant, 
 *                              respective most significant 32 bit word of
 *                              the cpu cycles counter
 * struct profile_data;            - holds all the profile results
 *                               (last call cycles, max cycles, total cycles,
 *                                no. of profile_start calls, no. of 
 *                                profile_end calls, name use in profile_init)
 * void     profile_init(pd, name) - intialize a profile structure
 * void     profile_start(pd)      - starts profiling (call before calling
 *                               the target function)
 * void     profile_end(pd)        - stops profiling (call after the target
 *                               function returns)
 * 
 */
 /*
 * Config defines:   CC_GCC_LIKE_ASM  - the compiler support gcc style
 *                     inline asm,
 *                  __CPU_x86, __CPU_x86_64, __CPU_sparc64
 */
/* 
 * History:
 * --------
 *  2007-06-23  created by andrei
 */




#ifndef _profile_h
#define _profile_h

#include <string.h>

/*
 * cycles_t - an unsigned interger type used for storing the cpu cycles
 *            (unsigned long long for now)
 *
 * cycles_t get_cpu_cycles() - returns the current cpu cycles counter
 * void     get_cpu_cycles_uint(unsigned* u1, unsigned* u2) 
 *                            - sets u1 and u2 to the least significant, 
 *                              respective most significant 32 bit word of
 *                              the cpu cycles counter
 */

#if defined __CPU_i386 && ! defined __CPU_x86
#define __CPU_x86
#endif

#ifdef __CPU_x86
typedef unsigned long long cycles_t;

inline static cycles_t get_cpu_cycles()
{
	cycles_t r;
	asm volatile( "rdtsc \n\t" : "=A"(r));
	return r;
}

#define get_cpu_cycles_uint(u1, u2) \
	do{ \
		/* result in edx:eax */ \
		asm volatile( "rdtsc \n\t" : "=a"(*(u1)), "=d"(*(u2))); \
	}while(0)

#elif defined __CPU_x86_64
typedef unsigned long long cycles_t;

inline static cycles_t get_cpu_cycles()
{
	unsigned int u1, u2;
	asm volatile( "rdtsc \n\t" : "=a"(u1), "=d"(u2));
	return ((cycles_t)u2<<32ULL)|u1;
}


#define get_cpu_cycles_uint(u1, u2) \
	do{ \
		/* result in edx:eax */ \
		asm volatile( "rdtsc \n\t" : "=a"(*(u1)), "=d"(*(u2))); \
	}while(0)

#elif defined __CPU_sparc64

typedef unsigned long long cycles_t;

inline static cycles_t get_cpu_cycles()
{
#if ! defined(_LP64)
#warning "ilp32 mode "
	struct uint_64{
		unsigned int u2;
		unsigned int u1;
	};
	union{
		cycles_t c;
		struct uint_64 u;
	}r;
	
	asm volatile("rd %%tick, %0 \n\t"
				 "srlx %0, 32, %1 \n\t"
				: "=r"(r.u.u1), "=r"(r.u.u2));
	return r.c;
#else
	cycles_t r;
	/* normal 64 bit mode (e.g. gcc -m64) */
	asm volatile("rd %%tick, %0" : "=r"(r));
	return r;
#endif
}
inline static void  get_cpu_cycles_uint(unsigned int* u1, unsigned int* u2)
{
	cycles_t r;
	asm volatile("rd %%tick, %0" : "=r"(r));
	*u1=(unsigned int)r;
	*u2=(unsigned int)(r>>32);
}

#else /* __CPU_xxx */
#error "no get_cycles support for this CPU"
#endif /* __CPU_xxx */


union profile_cycles{
	cycles_t c;
	struct{
		unsigned int u1;
		unsigned int u2;
	}uint;
};

struct profile_data{
	cycles_t cycles;  /* last call */
	cycles_t total_cycles;
	cycles_t max_cycles;
	unsigned long entries; /* no. profile_start calls */
	unsigned long exits;   /* no. profile_end calls */
	char * name;
	
	/* private stuff */
	union profile_cycles init_rdtsc;
};

inline static void profile_init(struct profile_data* pd, char *name)
{
	memset(pd, 0, sizeof(*pd));
	pd->name=name;
}


inline static void profile_start(struct profile_data* pd)
{
	pd->entries++;
	pd->init_rdtsc.c=get_cpu_cycles();
}


inline static void profile_end(struct profile_data* pd)
{
	pd->cycles=get_cpu_cycles()-pd->init_rdtsc.c;
	if (pd->max_cycles<pd->cycles) pd->max_cycles=pd->cycles;
	pd->total_cycles+=pd->cycles;
	pd->exits++;
}


#endif
