/*
 * Copyright (c) 2009-2012 Petri Lehtinen <petri@digip.org>
 * Copyright (c) 2011-2012 Basile Starynkevitch <basile@starynkevitch.net>
 * Copyright (c) 2012 Rogerz Zhang <rogerz.zhang@gmail.com>
 * Copyright (c) 2013 Flowroute LLC (flowroute.com)
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license.
 *
 * Pulled from https://github.com/rogerz/jansson/blob/json_path/src/path.c
 */

#include <string.h>
#include <assert.h>

#include <jansson.h>

#include "../../mod_fix.h"

/* jansson private helper functions */
void *jsonp_malloc(size_t size);
void jsonp_free(void *ptr);
char *jsonp_strdup(const char *str);

static json_malloc_t do_malloc = malloc;
static json_free_t do_free = free;

json_t *json_path_get(const json_t *json, const char *path)
{
	static const char array_open = '[';
	static const char *path_delims = ".[", *array_close = "]";
	const json_t *cursor;
	char *token, *buf, *peek, *endptr, delim = '\0';
	const char *expect;

	if (!json || !path)
		return NULL;
	else
		buf = jsonp_strdup(path);

	peek = buf;
	token = buf;
	cursor = json;
	expect = path_delims;

	if (*token == array_open) {
		expect = array_close;
		token++;
	}

	while (peek && *peek && cursor)
	{
		char *last_peek = peek;
		peek = strpbrk(peek, expect);
		if (peek) {
			if (!token && peek != last_peek)
				goto fail;
			delim = *peek;
			*peek++ = '\0';
		} else if (expect != path_delims || !token) {
			goto fail;
		}

		if (expect == path_delims) {
			if (token) {
				cursor = json_object_get(cursor, token);
			}
			expect = (delim == array_open ? array_close : path_delims);
			token = peek;
		} else if (expect == array_close) {
			size_t index = strtol(token, &endptr, 0);
			if (*endptr)
				goto fail;
			cursor = json_array_get(cursor, index);
			token = NULL;
			expect = path_delims;
		} else {
			goto fail;
		}
	}

	jsonp_free(buf);
	return (json_t *)cursor;
fail:
	jsonp_free(buf);
	return NULL;
}

int json_path_set(json_t *json, const char *path, json_t *value,
		unsigned int append)
{
	static const char array_open = '[';
	static const char object_delim = '.';
	static const char *path_delims = ".[";
	static const char *array_close = "]";

	json_t *cursor, *parent = NULL;
	char *token, *buf = NULL, *peek, delim = '\0';
	const char *expect;
	int index_saved = -1;

	if (!json || !path || !value) {
		ERR("invalid arguments\n");
		goto fail;
	} else {
		buf = jsonp_strdup(path);
	}

	peek = buf;
	token = buf;
	cursor = json;
	expect = path_delims;

	if (*token == array_open) {
		expect = array_close;
		token++;
	}

	while (peek && *peek && cursor)
	{
		char *last_peek = peek;
		peek = strpbrk(last_peek, expect);

		if (peek) {
			if (!token && peek != last_peek) {
				ERR("unexpected trailing chars in JSON path at pos %zu\n",
						last_peek - buf);
				goto fail;
			}
			delim = *peek;
			*peek++ = '\0';
		} else { // end of path
			if (expect == path_delims) {
				break;
			} else {
				ERR("missing ']' at pos %zu\n", peek - buf);
				goto fail;
			}
		}

		if (expect == path_delims) {
			if (token) {
				if (token[0] == '\0') {
					ERR("empty token at pos %zu\n", peek - buf);
					goto fail;
				}

				parent = cursor;
				cursor = json_object_get(parent, token);

				if (!cursor) {
					if (!json_is_object(parent)) {
						ERR("object expected at pos %zu\n", peek - buf);
						goto fail;
					}
					if (delim == object_delim) {
						cursor = json_object();
						json_object_set_new(parent, token, cursor);
					} else {
						ERR("new array is not allowed at pos %zu\n", peek - buf);
						goto fail;
					}
				}
			}
			expect = (delim == array_open ? array_close : path_delims);
			token = peek;
		} else if (expect == array_close) {
			char *endptr;
			size_t index;

			parent = cursor;
			if (!json_is_array(parent)) {
				ERR("array expected at pos %zu\n", peek - buf);
				goto fail;
			}

			index = strtol(token, &endptr, 0);
			if (*endptr) {
				ERR("invalid array index at pos %zu\n", peek - buf);
				goto fail;
			}

			cursor = json_array_get(parent, index);
			if (!cursor) {
				ERR("array index out of bound at pos %zu\n", peek - buf);
				goto fail;
			}

			index_saved = index;
			token = NULL;
			expect = path_delims;

		} else {
			ERR("fatal JSON error at pos %zu\n", peek - buf);
			goto fail;
		}
	}

	if (token && append) {

		if(strlen(token) > 0) {
			json_t* tmp  = json_object_get(cursor, token);
			if(json_is_array(tmp)) {
				json_array_append(tmp, value);
				json_object_set(cursor, token, tmp);
			} else if(json_is_object(tmp) && json_is_object(value) ) {
				json_object_update(tmp, value);
				json_object_set(cursor, token, tmp);
			} else {
				ERR("JSON array or object expected at pos %zu\n", peek - buf);
				goto fail;
			}
		} else if(json_is_array(cursor)) {
			json_array_append(cursor, value);

		} else if(json_is_object(cursor) && json_is_object(value)) {
			json_object_update(cursor, value);

		} else {
			ERR("JSON array or object expected at pos %zu\n", peek - buf);
			goto fail;
		}

	} else if (token && strlen(token) != 0 ) {

		if (json_is_object(cursor)) {
			json_object_set(cursor, token, value);

		} else {
			ERR("JSON object expected at pos %zu\n", peek - buf);
			goto fail;
		}

		cursor = json_object_get(cursor, token);
	} else if (index_saved != -1 && json_is_array(parent)) {
		json_array_set(parent, index_saved, value);
		cursor = json_array_get(parent, index_saved);

	} else {
		ERR("invalid JSON path at pos %zu\n", peek - buf);
		goto fail;
	}

	json_decref(value);
	jsonp_free(buf);
	return 0;

fail:
	json_decref(value);
	jsonp_free(buf);
	return -1;
}

/* jansson private helper functions */
void *jsonp_malloc(size_t size) {
	if(!size)
		return NULL;

	return (*do_malloc)(size);
}

void jsonp_free(void *ptr) {
	if(!ptr)
		return;

	(*do_free)(ptr);
}

char *jsonp_strdup(const char *str) {
	char *new_str;

	new_str = jsonp_malloc(strlen(str) + 1); 
	if(!new_str)
		return NULL;

	strcpy(new_str, str);
	return new_str;
}
/* end jansson private helpers */
