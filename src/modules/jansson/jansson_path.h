/*
 * Copyright (c) 2012 Rogerz Zhang <rogerz.zhang@gmail.com>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. 
 */


#ifndef _JANSSON_FUNCS_H_
#define _JANSSON_FUNCS_H_
#include <jansson.h>

json_t *json_path_get(const json_t *json, const char *path);
int json_path_set(json_t *json, const char *path, json_t *value, unsigned int append);

#endif

