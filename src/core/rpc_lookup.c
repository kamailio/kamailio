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
#include "mem/shm.h"

#define RPC_HASH_SIZE	32
#define RPC_SARRAY_SIZE	32 /* initial size */

static struct str_hash_table rpc_hash_table;

/* array of pointer to rpc exports, sorted after their name
 *  (used by listMethods) */
rpc_exportx_t** rpc_sarray;
int rpc_sarray_crt_size; /* used */
static int rpc_sarray_max_size; /* number of entries alloc'ed */

int ksr_rpc_exec_delta = 0;

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
	if (rpc_sarray) {
		rpc_sarray_crt_size=0;
	} else {
		PKG_MEM_ERROR;
		return -1;
	}
	return 0;
}



void destroy_rpcs(void)
{
	int r;
	struct str_hash_entry* e;
	struct str_hash_entry* bak;
	rpc_exportx_t* rx;
	for (r=0; r<rpc_hash_table.size; r++){
		clist_foreach_safe(&rpc_hash_table.table[r], e, bak, next) {
			rx = (rpc_exportx_t*)e->u.p;
			if(rx->xdata != NULL) {
				lock_destroy(&rx->xdata->elock);
				shm_free(rx->xdata);
			}
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
	struct str_hash_entry *e;
	int name_len;
	int doc0_len, doc1_len;
	rpc_exportx_t *n_rpc;
	rpc_exportx_t **r;
	rpc_xdata_t* xd = NULL;

	name_len=strlen(rpc->name);
	doc0_len=rpc->doc_str[0]?strlen(rpc->doc_str[0]):0;
	doc1_len=rpc->doc_str[1]?strlen(rpc->doc_str[1]):0;
	/* alloc everything into one block */

	e=pkg_malloc(ROUND_POINTER(sizeof(struct str_hash_entry))
								+ROUND_POINTER(sizeof(rpc_exportx_t))+2*sizeof(char*)+
								+name_len+1+doc0_len+(rpc->doc_str[0]!=0)
								+doc1_len+(rpc->doc_str[1]!=0)
								);
	if (e==0){
		PKG_MEM_ERROR;
		goto error;
	}
	if (rpc->flags & RPC_EXEC_DELTA) {
		xd = (rpc_xdata_t*)shm_mallocxz(sizeof(rpc_xdata_t));
		if (xd==NULL) {
			pkg_free(e);
			goto error;
		}
		lock_init(&xd->elock);
	}
	n_rpc=(rpc_exportx_t*)((char*)e+
			ROUND_POINTER(sizeof(struct str_hash_entry)));
	/* copy rpc into n_rpc */
	n_rpc->r=*rpc;
	n_rpc->xdata = xd;
	n_rpc->r.doc_str=(const char**)((char*)n_rpc+ROUND_POINTER(sizeof(rpc_exportx_t)));
	n_rpc->r.name=(char*)n_rpc->r.doc_str+2*sizeof(char*);
	memcpy((char*)n_rpc->r.name, rpc->name, name_len);
	*((char*)&n_rpc->r.name[name_len])=0;
	if (rpc->doc_str[0]){
		n_rpc->r.doc_str[0]=&n_rpc->r.name[name_len+1];
		memcpy((char*)n_rpc->r.doc_str[0], rpc->doc_str[0], doc0_len);
		*(char*)&(n_rpc->r.doc_str[0][doc0_len])=0;
	}else{
		n_rpc->r.doc_str[0]=0;
	}
	if (rpc->doc_str[1]){
		n_rpc->r.doc_str[1]=n_rpc->r.doc_str[0]?&n_rpc->r.doc_str[0][doc0_len+1]:
							&n_rpc->r.name[name_len+1];
		memcpy((char*)n_rpc->r.doc_str[1], rpc->doc_str[1], doc1_len);
		*(char*)&(n_rpc->r.doc_str[1][doc1_len])=0;
	}else{
		n_rpc->r.doc_str[1]=0;
	}

	e->key.s=(char*)n_rpc->r.name;
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
		if (strcmp(n_rpc->r.name, (*r)->r.name)<0)
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
	return e?&(((rpc_exportx_t*)e->u.p)->r):0;
}


/** lookup an extended rpc export after its name.
 * @return pointer to rpc export on success, 0 on error
 */
rpc_exportx_t* rpc_lookupx(const char* name, int len, unsigned int *rdata)
{
	rpc_exportx_t *rx;
	time_t tnow;

	struct str_hash_entry* e;

	e=str_hash_get(&rpc_hash_table, (char*)name, len);
	*rdata = 0;
	if(e != NULL) {
		rx = (rpc_exportx_t*)e->u.p;
		if(ksr_rpc_exec_delta > 0 && rx->xdata != NULL) {
			tnow = time(NULL);
			lock_get(&rx->xdata->elock);
			if(rx->xdata->etime > 0) {
				if(rx->xdata->etime > tnow - ksr_rpc_exec_delta) {
					*rdata = RPC_EXEC_DELTA;
				} else {
					rx->xdata->etime = tnow;
				}
			} else {
				rx->xdata->etime = tnow;
			}
			lock_release(&rx->xdata->elock);
		}
		return rx;
	}
	return NULL;
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
