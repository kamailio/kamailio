#ifndef _TREX_H_
#define _TREX_H_
/***************************************************************
	T-Rex a tiny regular expression library

	Copyright (C) 2003-2006 Alberto Demichelis

	This software is provided 'as-is', without any express 
	or implied warranty. In no event will the authors be held 
	liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for 
	any purpose, including commercial applications, and to alter
	it and redistribute it freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		you must not claim that you wrote the original software.
		If you use this software in a product, an acknowledgment
		in the product documentation would be appreciated but
		is not required.

		2. Altered source versions must be plainly marked as such,
		and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any
		source distribution.

****************************************************************/

/****************************************************************
 * NOTE:
 * The code below is no longer the original T-Rex code as it was 
 * modified to meet the needs of integration into the OpenSER
 * code.
 *****************************************************************/

#include <setjmp.h>
#include <ctype.h>
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../re.h"
#include "../../dprint.h"
#include "../../str.h"

#ifndef TREX_API
#define TREX_API extern
#endif

#define TRex_True 1
#define TRex_False 0


#define OP_GREEDY		MAX_CHAR+1 // * + ? {n}
#define OP_OR			MAX_CHAR+2
#define OP_EXPR			MAX_CHAR+3 //parentesis ()
#define OP_DOT			MAX_CHAR+4
#define OP_CLASS		MAX_CHAR+5
#define OP_CCLASS		MAX_CHAR+6
#define OP_NCLASS		MAX_CHAR+7 //negates class the [^
#define OP_RANGE		MAX_CHAR+8
#define OP_CHAR			MAX_CHAR+9
#define OP_EOL			MAX_CHAR+10
#define OP_BOL			MAX_CHAR+11

#define TREX_SYMBOL_ANY_CHAR '.'
#define TREX_SYMBOL_GREEDY_ONE_OR_MORE '+'
#define TREX_SYMBOL_GREEDY_ZERO_OR_MORE '*'
#define TREX_SYMBOL_GREEDY_ZERO_OR_ONE '?'
#define TREX_SYMBOL_BRANCH '|'
#define TREX_SYMBOL_END_OF_STRING '$'
#define TREX_SYMBOL_BEGINNING_OF_STRING '^'
#define TREX_SYMBOL_ESCAPE_CHAR '\\'


#define trex_shm_alloc 		shm_malloc
#define trex_shm_realloc	shm_realloc
#define trex_shm_free		shm_free


#define trex_pkg_alloc 		pkg_malloc
#define trex_pkg_realloc 	pkg_realloc
#define trex_pkg_free 		pkg_free

#define MATCH_SIZE			sizeof(TRexMatch)
#define NODE_SIZE			sizeof(TRexNode)

#ifdef _UNICODE
#define TRexChar unsigned short
#define MAX_CHAR 0xFFFF
#define _TREXC(c) L##c 
#define trex_strlen wcslen
#define trex_printf wprintf
#else
#define TRexChar char
#define MAX_CHAR 0xFF
#define _TREXC(c) (c) 
#define trex_strlen strlen
#define trex_printf printf
#endif

typedef unsigned int TRexBool;
typedef struct TRex TRex;

typedef struct {
	const TRexChar *begin;
	int len;
} TRexMatch;

typedef int TRexNodeType;

typedef struct tagTRexNode{
	TRexNodeType type;
	long left;
	long right;
	int next;
}TRexNode;

struct TRex{
	const TRexChar *_eol;
	const TRexChar *_bol;
	const TRexChar *_p;
	int _first;
	int _op;
	TRexNode *_nodes;
	int _nallocated;
	int _nsize;
	int _nsubexpr;
	TRexMatch *_matches;
	int _currsubexp;
	void *_jmpbuf;
	const TRexChar **_error;
};

TREX_API TRex *trex_compile(const TRexChar *pattern,const TRexChar **error);
TREX_API TRexBool trex_match(TRex* exp,const TRexChar* text);
TREX_API TRexBool trex_search(TRex* exp,const TRexChar* text, const TRexChar** out_begin, const TRexChar** out_end);
TREX_API TRexBool trex_searchrange(TRex* exp,const TRexChar* text_begin,const TRexChar* text_end,const TRexChar** out_begin, const TRexChar** out_end);
TREX_API int trex_getsubexpcount(TRex* exp);
TREX_API TRexBool trex_getsubexp(TRex* exp, int n, TRexMatch *subexp);

TREX_API void trex_destroy(TRex *exp);
#endif

