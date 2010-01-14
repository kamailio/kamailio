#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "value.h"
#include "ht.h"

#define MAX_STR_LEN	1024


#define MAX_STR_LEN_ORDER	10
#define MIN_STR_LEN_ORDER	1

#define RANDOM(order)	((unsigned long)random() & ((1 << order) - 1))

char *string_gen(size_t *len)
{
	static char buff[1 << MAX_STR_LEN_ORDER];
	int i;
	unsigned char c;

	do {
		*len = RANDOM(MAX_STR_LEN_ORDER);
	} while (*len < (1 << MIN_STR_LEN_ORDER));
	//0x20 - 0x7E
	for (i = 0; i < *len; i ++) {
		c = RANDOM(7) - 1;
		if (c < 0x20)
			c += 0x20;
		buff[i] = (signed char)c;
	}

	return buff;
}

char *strNdup(const char *orig, size_t len)
{
	if (! orig)
		return NULL;
	assert(orig);
	char *new = brpc_malloc(len * sizeof(char));
	if (new)
		memcpy(new, orig, len);
	return new;
}


typedef struct {
	brpc_str_t val;
	ht_lnk_t link;
} cell_t;


cell_t *cell_new()
{
	cell_t *cell;

	cell = (cell_t *)brpc_calloc(1, sizeof(cell_t));
	assert(cell);
	cell->val.val = string_gen(&cell->val.len);
	cell->val.val = strNdup(cell->val.val, cell->val.len);
	assert(cell->val.val);
	HT_LINK_INIT_STR(&cell->link, &cell->val.val, &cell->val.len);
	return cell;
}

cell_t **gen_chain(size_t len)
{
	cell_t **chain;
	int i;

	chain = (cell_t **)brpc_malloc(len * sizeof(cell_t*));
	assert(chain);

	for (i = 0; i < len; i ++) {
		chain[i] = cell_new();
		assert(chain[i]);
	}
	return chain;
}

void free_chain(cell_t **chain, size_t len)
{
	int i;

	for (i = 0; i < len; i ++) {
		brpc_free(chain[i]->val.val);
		brpc_free(chain[i]);
	}
	brpc_free(chain);
}


void test1()
{
	ht_t *ht;
	cell_t **chain, *cell;
	int i;
	const size_t chain_len = 1289;
	ht_lnk_t *pos;

	ht = ht_new(22);
	assert(ht);

	chain = gen_chain(chain_len);
	assert(chain);
	for (i = 0; i < chain_len; i ++)
		ht_ins(ht, &chain[i]->link);

	for (i = 0; i < chain_len; i ++) {
		pos = ht_lnk_lkup(ht, HT_LINK_HVAL(&chain[i]->link), 
				HT_LINK_LABEL(&chain[i]->link));
		assert(pos);
		ht_rem(pos);
		cell = ht_entry(pos, cell_t, link);
		assert(cell == chain[i]);
		
		pos = ht_lnk_lkup(ht, HT_LINK_HVAL(&chain[i]->link), 
				HT_LINK_LABEL(&chain[i]->link));
		assert(! pos);
	}

	free_chain(chain, chain_len);
	ht_del(ht);
}


void test2()
{
	ht_t *ht;
	cell_t **chain, *cell;
	int i;
	const size_t chain_len = 1289;
	ht_lnk_t *pos;

	ht = ht_new(22);
	assert(ht);

	chain = gen_chain(chain_len);
	assert(chain);
	for (i = 0; i < chain_len; i ++)
		ht_ins_mx(ht, &chain[i]->link);

	for (i = 0; i < chain_len; i ++) {
		pos = ht_lnk_lkup_rem_mx(ht, HT_LINK_HVAL(&chain[i]->link), 
				HT_LINK_LABEL(&chain[i]->link));
		assert(pos);
		cell = ht_entry(pos, cell_t, link);
		assert(cell == chain[i]);
		
		pos = ht_lnk_lkup_rem_mx(ht, HT_LINK_HVAL(&chain[i]->link), 
				HT_LINK_LABEL(&chain[i]->link));
		assert(! pos);
	}

	free_chain(chain, chain_len);
	ht_del(ht);
}

void test3()
{
	ht_t *ht;
	cell_t **chain, *cell;
	int i;
	const size_t chain_len = 4280;
	ht_lnk_t *pos;
	struct list_head *tmp;
	uint32_t hval;
	bool found;

	ht = ht_new(4);
	assert(ht);

	chain = gen_chain(chain_len);
	assert(chain);
	for (i = 0; i < chain_len; i ++)
		ht_ins_mx(ht, &chain[i]->link);

	for (i = chain_len - 1; 0 < i; i --) {
		hval = hash_str(chain[i]->val.val, chain[i]->val.len);
		found = false;
		ht_for_hval(pos, ht, hval, tmp) {
			cell = ht_entry(pos, cell_t, link);
			if (memcmp(chain[i]->val.val, cell->val.val, cell->val.len) == 0) {
				ht_rem(pos);
				pos = ht_lnk_lkup_rem_mx(ht, HT_LINK_HVAL(&cell->link), 
						HT_LINK_LABEL(&cell->link));
				/* hope for no randomly gen'ed string duplicates... */
				assert(! pos);
				found = true;
				break;
			}
		}
		assert(found);
	}

	free_chain(chain, chain_len);
	ht_del(ht);
}


void test4()
{
	ht_t *ht;
	cell_t **chain, *cell;
	int i;
	const size_t chain_len = 4280;
	ht_lnk_t *pos;
	struct list_head *tmp;
	uint32_t hval;
	bool found;

	ht = ht_new(4);
	assert(ht);

	chain = gen_chain(chain_len);
	assert(chain);
	for (i = 0; i < chain_len; i ++) {
		/* force hash so that ht_for_hval is forced to resume iterations */
		HT_LINK_HVAL(&chain[i]->link) = HT_LINK_HVAL(&chain[i]->link) % 4;
		ht_ins_mx(ht, &chain[i]->link);
	}

	for (i = chain_len - 1; 0 < i; i --) {
		hval = hash_str(chain[i]->val.val, chain[i]->val.len) % 4;
		found = false;
		ht_for_hval(pos, ht, hval, tmp) {
			cell = ht_entry(pos, cell_t, link);
			if (memcmp(chain[i]->val.val, cell->val.val, cell->val.len) == 0) {
				/* hope for no randomly gen'ed string duplicates... */
				if (HT_LINK_LABEL(&chain[i]->link) != 
						HT_LINK_LABEL(&cell->link)) {
					printf("WHAT A COINCIDENCE: labels %lu|%lu for same string"
							" <%.*s>\n", HT_LINK_LABEL(&chain[i]->link), 
							HT_LINK_LABEL(&cell->link), cell->val.len, 
							cell->val.val);
					continue;
				}
				assert(HT_LINK_LABEL(&chain[i]->link) == 
						HT_LINK_LABEL(&cell->link));
				ht_rem(pos);
				pos = ht_lnk_lkup_rem_mx(ht, HT_LINK_HVAL(&cell->link), 
						HT_LINK_LABEL(&cell->link));
				assert(! pos);
				found = true;
				break;
			}
		}
		assert(found);
	}

	free_chain(chain, chain_len);
	ht_del(ht);
}


void seed()
{
	int fd;
	unsigned int seed;

	fd = open("/dev/random", O_RDONLY);
	assert(0 < fd);
	assert(sizeof(seed) == read(fd, (char *)&seed, sizeof(seed)));
	//printf("Seed: 0x%x.\n", seed);
	srandom(seed);
}

int main()
{
	seed();
	test1(); printf("1: OK\n");
	test2(); printf("2: OK\n");
	test3(); printf("3: OK\n");
	test4(); printf("4: OK\n");
	return 0;
}
