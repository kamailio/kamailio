/* 
 * Copyright (C) 2009 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core ::  RPC lookup and register functions
 * \ingroup core
 * Module: \ref core
 */

#include "rpc.h"
#include "str_hash.h"
#include "ut.h"
#include "dprint.h"

#define RPC_HASH_SIZE	32
#define RPC_SARRAY_SIZE	32 /* initial size */

#define RPC_COPY_EXPORT

static struct str_hash_table rpc_hash_table;

/* array of pointer to rpc exports, sorted after their name
 *  (used by listMethods) */
rpc_export_t** rpc_sarray;
int rpc_sarray_crt_size; /* used */
static int rpc_sarray_max_size; /* number of entries alloc'ed */

/** init the rpc hash table.
  * @return 0 on success, -1 on error
  */
int init_rpcs(void)
{
	if (str_hash_alloc(&rpc_hash_table, RPC_HASH_SIZE)<0)
		return -1;
	str_hash_init(&rpc_hash_table);
	rpc_sarray_max_size=RPC_SARRAY_SIZE;
	rpc_sarray=pkg_malloc(sizeof(*rpc_sarray)* rpc_sarray_max_size);
	rpc_sarray_crt_size=0;
	return 0;
}



void destroy_rpcs(void)
{
	int r;
	struct str_hash_entry* e;
	struct str_hash_entry* bak;
	for (r=0; r<rpc_hash_table.size; r++){
		clist_foreach_safe(&rpc_hash_table.table[r], e, bak, next){
			pkg_free(e);
		}
	}
	if (rpc_hash_table.table) pkg_free(rpc_hash_table.table);
	if (rpc_sarray) pkg_free(rpc_sarray);
	rpc_hash_table.table=0;
	rpc_hash_table.size=0;
	rpc_sarray=0;
	rpc_sarray_crt_size=0;
	rpc_sarray_max_size=0;
}



/** adds a new rpc to the hash table (no checks).
 * @return 0 on success, -1 on error, 1 on duplicate
 */
static int rpc_hash_add(struct rpc_export* rpc)
{
	struct str_hash_entry* e;
	int name_len;
	int doc0_len, doc1_len;
	struct rpc_export* n_rpc;
	struct rpc_export** r;
	
	name_len=strlen(rpc->name);
	doc0_len=rpc->doc_str[0]?strlen(rpc->doc_str[0]):0;
	doc1_len=rpc->doc_str[1]?strlen(rpc->doc_str[1]):0;
	/* alloc everything into one block */
	
#ifdef RPC_COPY_EXPORT
	e=pkg_malloc(ROUND_POINTER(sizeof(struct str_hash_entry))
								+ROUND_POINTER(sizeof(*rpc))+2*sizeof(char*)+
								+name_len+1+doc0_len+(rpc->doc_str[0]!=0)
								+doc1_len+(rpc->doc_str[1]!=0)
								);
#else /* RPC_COPY_EXPORT */
	e=pkg_malloc(ROUND_POINTER(sizeof(struct str_hash_entry)));
#endif /* RPC_COPY_EXPORT */
	
	if (e==0){
		ERR("out of memory\n");
		goto error;
	}
#ifdef RPC_COPY_EXPORT
	n_rpc=(rpc_export_t*)((char*)e+
			ROUND_POINTER(sizeof(struct str_hash_entry)));
	/* copy rpc into n_rpc */
	*n_rpc=*rpc;
	n_rpc->doc_str=(const char**)((char*)n_rpc+ROUND_POINTER(sizeof(*rpc)));
	n_rpc->name=(char*)n_rpc->doc_str+2*sizeof(char*);
	memcpy((char*)n_rpc->name, rpc->name, name_len);
	*((char*)&n_rpc->name[name_len])=0;
	if (rpc->doc_str[0]){
		n_rpc->doc_str[0]=&n_rpc->name[name_len+1];
		memcpy((char*)n_rpc->doc_str[0], rpc->doc_str[0], doc0_len);
		*(char*)&(n_rpc->doc_str[0][doc0_len])=0;
	}else{
		n_rpc->doc_str[0]=0;
	}
	if (rpc->doc_str[1]){
		n_rpc->doc_str[1]=n_rpc->doc_str[0]?&n_rpc->doc_str[0][doc0_len+1]:
							&n_rpc->name[name_len+1];;
		memcpy((char*)n_rpc->doc_str[1], rpc->doc_str[1], doc1_len);
		*(char*)&(n_rpc->doc_str[1][doc1_len])=0;
	}else{
		n_rpc->doc_str[1]=0;
	}
#else /* RPC_COPY_EXPORT */
	n_rpc=rpc;
#endif /* RPC_COPY_EXPORT */
	
	e->key.s=(char*)n_rpc->name;
	e->key.len=name_len;
	e->flags=0;
	e->u.p=n_rpc;
	str_hash_add(&rpc_hash_table, e);
	
	/* insert it into the sorted array */
	if (rpc_sarray_max_size<=rpc_sarray_crt_size){
		/* array must be increased */
		r=pkg_realloc(rpc_sarray, 2*rpc_sarray_max_size*sizeof(*rpc_sarray));
		if (r==0){
			ERR("out of memory while adding RPC to the sorted list\n");
			goto error;
		}
		rpc_sarray=r;
		rpc_sarray_max_size*=2;
	};
	/* insert into array, sorted */
	for (r=rpc_sarray;r<(rpc_sarray+rpc_sarray_crt_size); r++){
		if (strcmp(n_rpc->name, (*r)->name)<0)
			break;
	}
	if (r!=(rpc_sarray+rpc_sarray_crt_size))
		memmove(r+1, r, (int)(long)((char*)(rpc_sarray+rpc_sarray_crt_size)-
											(char*)r));
	rpc_sarray_crt_size++;
	*r=n_rpc;
	return 0;
error:
	return -1;
}



/** lookup an rpc export after its name.
 * @return pointer to rpc export on success, 0 on error
 */
rpc_export_t* rpc_lookup(const char* name, int len)
{
	struct str_hash_entry* e;
	
	e=str_hash_get(&rpc_hash_table, (char*)name, len);
	return e?(rpc_export_t*)e->u.p:0;
}



/** register a new rpc.
 * @return 0 on success, -1 on error, 1 on duplicate
 */
int rpc_register(rpc_export_t* rpc)
{
	
	/* check if the entry is already registered */
	if (rpc_lookup(rpc->name, strlen(rpc->name))){
		WARN("duplicate rpc \"%s\"\n", rpc->name);
		return 1;
	}
	if (rpc_hash_add(rpc)!=0) return -1;
	return 0;
}



/** register all the rpc in a null-terminated array.
  * @return 0 on success, >0 if duplicates were found (number of 
  * duplicates), -1 on error
  */
int rpc_register_array(rpc_export_t* rpc_array)
{
	rpc_export_t* rpc;
	int ret,i;
	
	ret=0;
	for (rpc=rpc_array; rpc && rpc->name; rpc++){
		i=rpc_register(rpc);
		if (i!=0){
			if (i<0) goto error;
			ret++;
		}
	}
	return ret;
error:
	return -1;
}


/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
