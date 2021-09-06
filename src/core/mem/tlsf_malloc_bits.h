#ifndef INCLUDED_tlsfbits
#define INCLUDED_tlsfbits

#if defined(__cplusplus)
#define tlsf_decl inline
#else
#define tlsf_decl static
#endif

/*
** Architecture-specific bit manipulation routines.
**
** TLSF achieves O(1) cost for malloc and free operations by limiting
** the search for a free block to a free list of guaranteed size
** adequate to fulfill the request, combined with efficient free list
** queries using bitmasks and architecture-specific bit-manipulation
** routines.
**
** Most modern processors provide instructions to count leading zeroes
** in a word, find the lowest and highest set bit, etc. These
** specific implementations will be used when available, falling back
** to a reasonably efficient generic implementation.
**
** NOTE: TLSF spec relies on ffs/fls returning value 0..31.
** ffs/fls return 1-32 by default, returning 0 for error.
*/

/*
** Detect whether or not we are building for a 32- or 64-bit (LP/LLP)
** architecture. There is no reliable portable method at compile-time.
*/
#if defined (__alpha__) || defined (__ia64__) || defined (__x86_64__) \
	|| defined (_WIN64) || defined (__LP64__) || defined (__LLP64__)
#define TLSF_64BIT
#endif

/*
** gcc 3.4 and above have builtin support, specialized for architecture.
** Some compilers masquerade as gcc; patchlevel test filters them out.
**
** Note: clang is compatible with GCC builtins and will also define those macros
*/
#if defined (__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)) \
	&& defined (__GNUC_PATCHLEVEL__)

tlsf_decl int tlsf_ffs(unsigned int word)
{
	return __builtin_ffs(word) - 1;
}

tlsf_decl int tlsf_fls(unsigned int word)
{
	const int bit = word ? 32 - __builtin_clz(word) : 0;
	return bit - 1;
}

#if defined (TLSF_64BIT)
tlsf_decl int tlsf_fls_sizet(size_t size)
{
	const int bit = size ? 64 - __builtin_clzl(size) : 0;
	return bit - 1;
}
#endif
#else
/* Fall back to generic implementation. */

tlsf_decl int tlsf_fls_generic(unsigned int word)
{
	int bit = 32;

	if (!word) bit -= 1;
	if (!(word & 0xffff0000)) { word <<= 16; bit -= 16; }
	if (!(word & 0xff000000)) { word <<= 8; bit -= 8; }
	if (!(word & 0xf0000000)) { word <<= 4; bit -= 4; }
	if (!(word & 0xc0000000)) { word <<= 2; bit -= 2; }
	if (!(word & 0x80000000)) { word <<= 1; bit -= 1; }

	return bit;
}

/* Implement ffs in terms of fls. */
tlsf_decl int tlsf_ffs(unsigned int word)
{
	return tlsf_fls_generic(word & (~word + 1)) - 1;
}

tlsf_decl int tlsf_fls(unsigned int word)
{
	return tlsf_fls_generic(word) - 1;
}

#if defined (TLSF_64BIT)
tlsf_decl int tlsf_fls_sizet(size_t size)
{
	int high = (int)(size >> 32);
	int bits = 0;
	if (high)
	{
		bits = 32 + tlsf_fls(high);
	}
	else
	{
		bits = tlsf_fls((int)size & 0xffffffff);

	}
	return bits;
}
#endif /* defined (TLSF_64BIT) */

#endif /* GNUC */


#if !defined (TLSF_64BIT)
#define tlsf_fls_sizet tlsf_fls
#endif

#undef tlsf_decl

#endif
