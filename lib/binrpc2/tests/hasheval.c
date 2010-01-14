#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <math.h>

#define MAX_STR_LEN_ORDER	10
#define MIN_STR_LEN_ORDER	1

#define RANDOM(order)	((unsigned long)random() & ((1 << order) - 1))

#define ERREXIT(msg, fmt...) \
	do { \
		fprintf(stderr, "ERROR(%d): " msg " (%d: %s).\n", __LINE__, ##fmt, \
				errno, strerror(errno)); \
		exit(1); \
	} while (0)

#define rd8(_loc_)	(*(uint8_t *)(_loc_))
#define rd16(_loc_)	(*(uint16_t *)(_loc_))
#define rd32(_loc_)	(*(uint32_t *)(_loc_))
#define rd64(_loc_)	(*(uint64_t *)(_loc_))

typedef uint32_t (*hash_f)(const char *val, size_t len, size_t hcut);

uint32_t hash0(const char *val, size_t len, size_t hcut)
{
    register uint32_t v;
    register uint32_t h;
	register unsigned long k;

#define _bMIX(x)	(x ^ (x >> 3))

	switch (len & 0x03) {
		default: /* make gcc happy */
		case 0: h = 0; break;
		
		do {
		case 1: v = rd8(val + len - 1); break;
		case 2: v = rd16(val + len - 2); break;
		case 3: v = (rd16(val + len - 3) << 8) | rd8(val + len - 1); break;
		} while (0);
			h = _bMIX(v);
			break;
	}
	switch (len >> 2) {
		case 0: break;
		default:
			for (k = 0; k < len; k += 4) {
				v = rd32(val + k);
				h += _bMIX(v);
			}
	}

	h += (h >> 11) + (h >> 13) + (h >> 23);
	return h & hcut;
    //return h % hcut;
    //return h & 31;
#undef _bMIX
}

uint32_t hash1(const char *str, size_t len, size_t hcut)
{
    const char* p;
    register uint32_t v;
    register uint32_t h;

    h=0;
    for (p = str; p <= str + len - 4; p += 4){
        v = rd32(p);
        h += v ^ (v>>3);
    }
    v=0;
    for (; p < str + len; p++) {
        v<<=8;
        v+=*p;
    }
    h += v ^ (v>>3);

    h=((h) + (h>>11)) + ((h>>13) + (h>>23));

    //return h % hcut;
	return h & hcut;
    //return h % 32;
    //return h & 31;
}

uint32_t hash2(const char *data, size_t len, size_t hcut)
{
#define get16bits(d) (*((const uint16_t *) (d)))
	uint32_t  hash, tmp;
	int rem;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (;len > 0; len--) {
		hash  += get16bits (data);
		tmp   = (get16bits (data+2) << 11) ^ hash;
		hash  = (hash << 16) ^ tmp;
		data  += 2 * sizeof (uint16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */ 
	switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= data[sizeof (uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	
	return hash & hcut;
#undef get16bits
}




void seed()
{
	int fd;
	unsigned int seed;

	fd = open("/dev/random", O_RDONLY);
	if (fd < 0)
		ERREXIT("failed to open random device");
	if (sizeof(seed) != read(fd, (char *)&seed, sizeof(seed)))
		ERREXIT("failed to read %zd bytes for seed", sizeof(seed));
	//printf("Seed: 0x%x.\n", seed);
	srandom(seed);
}

typedef struct cell_s {
	char *val;
	size_t len;
	uint32_t hash;
	unsigned long slot;
} cell_t;


char *strNdup(const char *orig, size_t len)
{
	if (! orig)
		return NULL;
	char *new = malloc(len * sizeof(char));
	if (new)
		memcpy(new, orig, len);
	return new;
}

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

cell_t *cell_new()
{
	cell_t *cell;

	cell = (cell_t *)calloc(1, sizeof(cell_t));
	if (! cell)
		ERREXIT("out of mem; requested: %zd", sizeof(cell_t));
	cell->val = string_gen(&cell->len);
	cell->val = strNdup(cell->val, cell->len);
	if (! cell->val)
		ERREXIT("out of mem; requested: %zd", cell->len);
	return cell;
}

cell_t **gen_chain(size_t len)
{
	cell_t **chain;
	int i;

	chain = (cell_t **)malloc(len * sizeof(cell_t*));
	if (! chain)
		ERREXIT("OOM");

	for (i = 0; i < len; i ++) {
		chain[i] = cell_new();
		if (! chain[i])
			ERREXIT("OOM");
	}
	return chain;
}


void hash(cell_t **chain, size_t tsz, hash_f h_func, size_t hcut)
{
	size_t i;
	cell_t *crr;

	for (i = 0; i < tsz; i ++) {
		crr = chain[i];
		crr->slot = h_func(crr->val, crr->len, hcut);
	}
}

double distribution(cell_t **chain, size_t tsz, size_t hsz)
{
	unsigned long *dist, i;
	size_t diff, sum, avg = tsz / hsz;
	double rmsd;

	dist = (unsigned long *)malloc(hsz * sizeof(unsigned long));
	if (! dist)
		ERREXIT("out of mem");
	memset(dist, 0, hsz * sizeof(unsigned long));

	for (i = 0; i < tsz; i ++)
		dist[chain[i]->slot] ++;

	sum = 0;
	for (i = 0; i < hsz; i ++) {
		diff = abs(dist[i] - avg);
		sum += diff*diff;
		//sum += diff;
#if 0
		printf("[%lu]: %lu\n", i, dist[i]);
#endif
	}
	rmsd = sqrt((double)sum / hsz);
	//rmsd = ((double)sum / hsz);
	return rmsd;
}

#define TIMING(op) \
	({ \
		struct timeval t1, t2; \
		unsigned long us1, us2; \
		double delta; \
		gettimeofday(&t1, NULL); \
		op; \
		gettimeofday(&t2, NULL); \
		us1 = t1.tv_sec * 1000000 + t1.tv_usec; \
		us2 = t2.tv_sec * 1000000 + t2.tv_usec; \
		delta = ((double)(us2 - us1)) / 1000000; \
		delta; \
	})

int main(int argc, char **argv)
{
	cell_t **chain;
	size_t tsz, avg, sz, hsz;
	double T0, T1, T2;
	double D0, D1, D2;

	if (argc < 3)
		ERREXIT("USAGE: %s <hashsize> <avg>", argv[0]);
	
	hsz = strtol(argv[1], NULL, 10);
	if (! hsz)
		ERREXIT("invalid hsz argument `%s'", argv[2]);
	avg = strtol(argv[2], NULL, 10);
	if (! avg)
		ERREXIT("invalid avg argument `%s'", argv[1]);

	for (sz = 1; sz < hsz; sz <<= 1)
		;
	if (hsz != sz) {
		hsz = sz;
		printf("Hash size (adjusted): %zd.\n", hsz);
	} else {
		printf("Hash size: %zd.\n", hsz);
	}
	tsz = avg * hsz;
	printf("Test size: %zd.\n", tsz);

	seed();

	chain = gen_chain(tsz);

	T0 = TIMING(hash(chain, tsz, hash0, hsz - 1));
	D0 = distribution(chain, tsz, hsz);
	
	T1 = TIMING(hash(chain, tsz, hash1, hsz-1));
	D1 = distribution(chain, tsz, hsz);
	
	T2 = TIMING(hash(chain, tsz, hash2, hsz - 1));
	D2 = distribution(chain, tsz, hsz);

	printf("case : timing (seconds) : RMSD (%%)\n");
	printf("#0: T=%4.3f; D=%2.3f\n", T0, D0*100/avg);
	printf("#1: T=%4.3f; D=%2.3f\n", T1, D1*100/avg);
	printf("#2: T=%4.3f; D=%2.3f\n", T2, D2*100/avg);

	return 0;
}
