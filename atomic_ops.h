/* 
 * $Id$
 * 
 * Copyright (C) 2006 iptelorg GmbH
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
 */
/*
 *  atomic operations and memory barriers
 *  WARNING: atomic ops do not include memory barriers
 *  
 *  memory barriers:
 *  ----------------
 *
 *  void membar();       - memory barrier (load & store)
 *  void membar_read()   - load (read) memory barrier
 *  void membar_write()  - store (write) memory barrier
 *
 *  Note: properly using memory barriers is tricky, in general try not to 
 *        depend on them. Locks include memory barriers, so you don't need
 *        them for writes/load already protected by locks.
 *
 * atomic operations:
 * ------------------
 *  type: atomic_t
 *
 * not including memory barriers:
 *
 *  void atomic_set(atomic_t* v, long i)      -      v->val=i
 *  long atomic_get(atomic_t* v)              -       return v->val
 *  void atomic_inc(atomic_t* v)
 *  void atomic_dec(atomic_t* v)
 *  long atomic_inc_and_test(atomic_t* v)     - returns 1 if the result is 0
 *  long atomic_dec_and_test(atomic_t* v)     - returns 1 if the result is 0
 *  void atomic_or (atomic_t* v, long mask)   - v->val|=mask 
 *  void atomic_and(atomic_t* v, long mask)   - v->val&=mask
 * 
 * same ops, but with builtin memory barriers:
 *
 *  void mb_atomic_set(atomic_t* v, long i)      -      v->val=i
 *  long mb_atomic_get(atomic_t* v)              -       return v->val
 *  void mb_atomic_inc(atomic_t* v)
 *  void mb_atomic_dec(atomic_t* v)
 *  long mb_atomic_inc_and_test(atomic_t* v)  - returns 1 if the result is 0
 *  long mb_atomic_dec_and_test(atomic_t* v)  - returns 1 if the result is 0
 *  void mb_atomic_or(atomic_t* v, long mask - v->val|=mask 
 *  void mb_atomic_and(atomic_t* v, long mask)- v->val&=mask
 * 
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */
#ifndef __atomic_ops
#define __atomic_ops

/* atomic_t defined as a struct to easily catch non atomic ops. on it,
 * e.g.  atomic_t  foo; foo++  will generate a compile error */
typedef struct{ volatile long val; } atomic_t; 


/* store and load operations are atomic on all cpus, note however that they
 * don't include memory barriers so if you want to use atomic_{get,set} 
 * to implement mutexes you must explicitely use the barriers */
#define atomic_set(at_var, value)	((at_var)->val=(value))
#define atomic_get(at_var) ((at_var)->val)

/* init atomic ops */
int atomic_ops_init();



#if defined(__CPU_i386) || defined(__CPU_x86_64)

#define HAVE_ASM_INLINE_ATOMIC_OPS

#ifdef NOSMP
#define __LOCK_PREF 
#else
#define __LOCK_PREF "lock ;"
#endif


/* memory barriers */

#ifdef NOSMP

#define membar()	asm volatile ("" : : : "memory")
#define membar_read()	membar()
#define membar_write()	membar()

#else

/* although most x86 do stores in order, we're playing it safe and use
 *  oostore ready write barriers */
#define X86_OOSTORE 

/* membar: lfence, mfence, sfence available only on newer cpus, so for now
 * stick to lock addl */
#define membar() \
	asm volatile( \
					" lock; addl $0, 0(%%esp) \n\t " \
					: : : "memory" \
				) 

#define membar_read()	membar()

#ifdef X86_OOSTORE
/* out of order store version */
#define membar_write()	membar()
#else
/* no oostore, most x86 cpus => do nothing, just a gcc do_not_cache barrier*/
#define membar_write()	asm volatile ("" : : : "memory")
#endif


#endif /* NOSMP */

#define atomic_inc(var) \
	asm volatile( \
			__LOCK_PREF " incl %0 \n\t"  \
			: "=m"((var)->val) : "m"((var)->val) : "cc" \
			) 

#define atomic_dec(var) \
	asm volatile( \
			__LOCK_PREF " decl %0 \n\t" \
			: "=m"((var)->val) : "m"((var)->val) : "cc" \
			) 

#define atomic_and(var, i) \
	asm volatile( \
			__LOCK_PREF " andl %1, %0 \n\t" \
			: "=m"((var)->val) : "r"((i)), "m"((var)->val) : "cc" \
			)
#define atomic_or(var, i) \
	asm volatile( \
			__LOCK_PREF " orl %1, %0 \n\t" \
			: "=m"((var)->val) : "r"((i)), "m"((var)->val) : "cc" \
			)


/* returns 1 if the result is 0 */
inline static long atomic_inc_and_test(atomic_t* var)
{
	char ret;
	
	asm volatile(
			__LOCK_PREF " incl %0 \n\t"
			"setz  %1 \n\t"
			: "=m"(var->val), "=qm"(ret) : "m" (var->val) : "cc"
			);
	return ret;
}


/* returns 1 if the result is 0 */
inline static long atomic_dec_and_test(atomic_t* var)
{
	char ret;
	
	asm volatile(
			__LOCK_PREF " decl %0 \n\t"
			"setz  %1 \n\t"
			: "=m"(var->val), "=qm"(ret) : "m" (var->val) : "cc"
			);
	return ret;
}



#elif defined __CPU_mips2

#define HAVE_ASM_INLINE_ATOMIC_OPS

#ifdef NOSMP
#define membar() asm volatile ("" : : : memory) /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else

#define membar() \
	asm volatile( \
			".set noreorder \n\t" \
			"    sync\n\t" \
			".set reorder \n\t" \
			: : : "memory" \
			) 

#define membar_read()  membar()
#define membar_write() membar()

#endif /* NOSMP */



/* main asm block */
#define ATOMIC_ASM_OP(op) \
			".set noreorder \n\t" \
			"1:   ll %1, %0 \n\t" \
			"     " op "\n\t" \
			"     sc %2, %0 \n\t" \
			"     beqz %2, 1b \n\t" \
			"     nop \n\t" \
			".set reorder \n\t" 


#define ATOMIC_FUNC_DECL(NAME, OP, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME (atomic_t *var) \
	{ \
		long ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP(OP) \
			: "=m"((var)->val), "=&r"(ret), "=&r"(tmp)  \
			: "m"((var)->val) \
			 \
			); \
		return RET_EXPR; \
	}


/* same as above, but with CT in %3 */
#define ATOMIC_FUNC_DECL_CT(NAME, OP, CT, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME (atomic_t *var) \
	{ \
		long ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP(OP) \
			: "=m"((var)->val), "=&r"(ret), "=&r"(tmp)  \
			: "r"((CT)), "m"((var)->val) \
			 \
			); \
		return RET_EXPR; \
	}


/* takes an extra param, i which goes in %3 */
#define ATOMIC_FUNC_DECL1(NAME, OP, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME (atomic_t *var, long i) \
	{ \
		long ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP(OP) \
			: "=m"((var)->val), "=&r"(ret), "=&r"(tmp)  \
			: "r"((i)), "m"((var)->val) \
			 \
			); \
		return RET_EXPR; \
	}


ATOMIC_FUNC_DECL(inc,      "addiu %2, %1, 1", void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addiu %2, %1, 1", long, (ret+1)==0 )

ATOMIC_FUNC_DECL_CT(dec,   "subu %2, %1, %3", 1,  void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec_and_test, "subu %2, %1, %3", 1, long, (ret-1)==0 )

ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", void,  /* no return */ )

#endif /* __CPU_xxx  => no known cpu */


#ifndef HAVE_ASM_INLINE_ATOMIC_OPS

#include "locking.h"

#define ATOMIC_USE_LOCK

extern gen_lock_t* _atomic_lock;


#define atomic_lock    lock_get(_atomic_lock)
#define atomic_unlock  lock_release(_atomic_lock)


/* memory barriers 
 *  not a known cpu -> fall back lock/unlock: safe but costly  (it should 
 *  include a memory barrier effect) */
#define membar() \
	do{\
		atomic_lock; \
		atomic_unlock; \
	} while(0)


#define membar_write() membar()

#define membar_read()  membar()


/* atomic ops */

#define atomic_inc(var) \
	do{ \
		atomic_lock; \
		(var)->val++;\
		atomic_unlock;\
	}while(0)


#define atomic_dec(var) \
	do{ \
		atomic_lock; \
		(var)->val--; \
		atomic_unlock; \
	}while(0)


#define atomic_and(var, i) \
	do{ \
		atomic_lock; \
		(var)->val&=i; \
		atomic_unlock; \
	}while(0)

#define atomic_or(var, i) \
	do{ \
		atomic_lock; \
		(var)->val|=i; \
		atomic_unlock; \
	}while(0)



/* returns true if result is 0 */
inline static long atomic_inc_and_test(atomic_t* var)
{
	long ret;
	
	atomic_lock;
	var->val++;
	ret=var->val;
	atomic_unlock;
	
	return (ret==0);
}


/* returns true if result is 0 */
inline static long atomic_dec_and_test(atomic_t* var)
{
	long ret;
	
	atomic_lock;
	var->val++;
	ret=var->val;
	atomic_unlock;
	
	return (ret==0);
}


/* memory barrier versions, the same as "normal" versions, except fot
 * the set/get
 * (the * atomic_lock/unlock part should act as a barrier)
 */

#define mb_atomic_set(v, i) \
	do{ \
		membar(); \
		atomic_set(v, i); \
	}while(0)

#define mb_atomic_get(v) \
	do{ \
		membar(); \
		atomic_get(v); \
	}while(0)

#define mb_atomic_inc(v)	atomic_inc(v)
#define mb_atomic_dec(v)	atomic_dec(v)
#define mb_atomic_or(v, m)	atomic_or(v, m)
#define mb_atomic_and(v, m)	atomic_and(v, m)
#define mb_atomic_inc_and_test(v)	atomic_inc_and_test(v)
#define mb_atomic_dec_and_test(v)	atomic_dec_and_test(v)

#endif /* if HAVE_ASM_INLINE_ATOMIC_OPS */

#endif
