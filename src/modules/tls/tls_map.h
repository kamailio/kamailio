/**
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLS_MAP_H
#define _TLS_MAP_H

#include <string.h>

#define MAP_VERSION "0.1.0"

struct map_node_t;
typedef struct map_node_t map_node_t;

typedef struct
{
	map_node_t **buckets;
	unsigned nbuckets, nnodes;
} map_base_t;

typedef struct
{
	unsigned bucketidx;
	map_node_t *node;
} map_iter_t;


#define map_t(T)         \
	struct               \
	{                    \
		map_base_t base; \
		T *ref;          \
		T tmp;           \
	}


#define map_init(m) memset(m, 0, sizeof(*(m)))


#define map_deinit(m) map_deinit_(&(m)->base)


#define map_get(m, key) ((m)->ref = map_get_(&(m)->base, key))


#define map_set(m, key, value) \
	((m)->tmp = (value), map_set_(&(m)->base, key, &(m)->tmp, sizeof((m)->tmp)))


#define map_remove(m, key) map_remove_(&(m)->base, key)


#define map_iter(m) map_iter_()


#define map_next(m, iter) map_next_(&(m)->base, iter)


void map_deinit_(map_base_t *m);
void *map_get_(map_base_t *m, const char *key);
int map_set_(map_base_t *m, const char *key, void *value, int vsize);
void map_remove_(map_base_t *m, const char *key);
map_iter_t map_iter_(void);
const char *map_next_(map_base_t *m, map_iter_t *iter);


typedef map_t(void *) map_void_t;
typedef map_t(char *) map_str_t;
typedef map_t(int) map_int_t;
typedef map_t(char) map_char_t;
typedef map_t(float) map_float_t;
typedef map_t(double) map_double_t;

#endif /* _TLS_MAP_H */
