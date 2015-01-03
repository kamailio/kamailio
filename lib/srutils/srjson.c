/*
   Copyright (c) 2009 Dave Gamble

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
   */

/*!
 * \file
 * \brief srutils :: SRjson - JSON parser in C - MIT License
 * \ingroup srutils
 * Module: \ref srutils
 * - addapted from cJSON to fit better within Kamailio/SER environment
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "srjson.h"

static const char *ep;

const char *srjson_GetErrorPtr()
{
	return ep;
}

static int srjson_strcasecmp(const char *s1, const char *s2)
{
	if (!s1)
		return (s1 == s2) ? 0 : 1;
	if (!s2)
		return 1;
	for (; tolower(*s1) == tolower(*s2); ++s1, ++s2)
		if (*s1 == 0)
			return 0;
	return tolower(*(const unsigned char *) s1) - tolower(*(const unsigned char *) s2);
}


srjson_doc_t *srjson_NewDoc(srjson_Hooks *hooks)
{
	srjson_doc_t *d;
	if(hooks && hooks->malloc_fn)
	{
		d = (srjson_doc_t*)hooks->malloc_fn(sizeof(srjson_doc_t));
	} else {
		d = (srjson_doc_t*)malloc(sizeof(srjson_doc_t));
	}
	if(d)
	{
		memset(d, 0, sizeof(srjson_doc_t));
		d->malloc_fn = (hooks && hooks->malloc_fn) ? hooks->malloc_fn : malloc;
		d->free_fn   = (hooks && hooks->free_fn) ? hooks->free_fn : free;
	}
	return d;
}

int srjson_InitDoc(srjson_doc_t *doc, srjson_Hooks *hooks)
{
	if(!doc)
		return -1;
	memset(doc, 0, sizeof(srjson_doc_t));
	doc->malloc_fn = (hooks && hooks->malloc_fn) ? hooks->malloc_fn : malloc;
	doc->free_fn   = (hooks && hooks->free_fn) ? hooks->free_fn : free;
	return 0;
}

void srjson_DeleteDoc(srjson_doc_t *doc)
{
	void (*f) (void *);

	if(!doc)
		return;
	srjson_Delete(doc, doc->root);
	f = doc->free_fn;
	f(doc);
}

void srjson_DestroyDoc(srjson_doc_t *doc)
{
	if(!doc)
		return;
	srjson_Delete(doc, doc->root);
	memset(doc, 0, sizeof(srjson_doc_t));
}

static char *srjson_strdup(srjson_doc_t *doc, const char *str)
{
	size_t len;
	char   *copy;

	len = strlen(str) + 1;
	if (!(copy = (char *) doc->malloc_fn(len)))
		return 0;
	memcpy(copy, str, len);
	return copy;
}

static char *srjson_strndupz(srjson_doc_t *doc, const char *str, int len)
{
	char *copy;

	if (!(copy = (char *) doc->malloc_fn(len+1)))
		return 0;
	memcpy(copy, str, len);
	copy[len] = '\0';
	return copy;
}


/* Internal constructor. */
static srjson_t *srjson_New_Item(srjson_doc_t *doc)
{
	srjson_t *node = (srjson_t*)doc->malloc_fn(sizeof(srjson_t));
	if (node)
		memset(node, 0, sizeof(srjson_t));
	return node;
}

/* Delete a srjson structure. */
void srjson_Delete(srjson_doc_t *doc, srjson_t *c)
{
	srjson_t *next;
	while (c) {
		next = c->next;
		if (!(c->type & srjson_IsReference) && c->child)
			srjson_Delete(doc, c->child);
		if (!(c->type & srjson_IsReference) && c->valuestring)
			doc->free_fn(c->valuestring);
		if (c->string)
			doc->free_fn(c->string);
		doc->free_fn(c);
		c = next;
	}
}

/*
 * Parse the input text to generate a number, and populate the result into
 * item.
 */
static const char *parse_number(srjson_doc_t *doc, srjson_t *item, const char *num)
{
	double n = 0, sign = 1, scale = 0;
	int subscale = 0, signsubscale = 1;

	/* Could use sscanf for this? */
	if (*num == '-')
		sign = -1, num++;	/* Has sign? */
	if (*num == '0')
		num++;		/* is zero */
	if (*num >= '1' && *num <= '9')
		do
			n = (n * 10.0) + (*num++ - '0');
		while (*num >= '0' && *num <= '9');	/* Number? */
	if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
		num++;
		do
			n = (n * 10.0) + (*num++ - '0'), scale--;
		while (*num >= '0' && *num <= '9');
	}			/* Fractional part? */
	if (*num == 'e' || *num == 'E') {	/* Exponent? */
		num++;
		if (*num == '+')
			num++;
		else if (*num == '-')
			signsubscale = -1, num++;	/* With sign? */
		while (*num >= '0' && *num <= '9')
			subscale = (subscale * 10) + (*num++ - '0');	/* Number? */
	}
	n = sign * n * pow(10.0, (scale + subscale * signsubscale));	/* number = +/-
									 * number.fraction *
									 * 10^+/- exponent */

	item->valuedouble = n;
	item->valueint = (int) n;
	item->type = srjson_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static char *print_number(srjson_doc_t *doc, srjson_t *item)
{
	char    *str;
	double  d = item->valuedouble;
	if (fabs(((double) item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
		str = (char *) doc->malloc_fn(21);	/* 2^64+1 can be
							 * represented in 21
							 * chars. */
		if (str)
			sprintf(str, "%d", item->valueint);
	} else {
		str = (char *) doc->malloc_fn(64);	/* This is a nice
							 * tradeoff. */
		if (str) {
			if (fabs(floor(d) - d) <= DBL_EPSILON)
				sprintf(str, "%.0f", d);
			else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
				sprintf(str, "%e", d);
			else
				sprintf(str, "%f", d);
		}
	}
	return str;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
static const char *parse_string(srjson_doc_t *doc, srjson_t *item, const char *str)
{
	const char     *ptr = str + 1;
	char           *ptr2;
	char           *out;
	int             len = 0;
	unsigned        uc, uc2;
	if (*str != '\"') {
		ep = str;
		return 0;
	}			/* not a string! */
	while (*ptr != '\"' && *ptr && ++len)
		if (*ptr++ == '\\')
			ptr++;	/* Skip escaped quotes. */

	out = (char *) doc->malloc_fn(len + 1);	/* This is how long we need
						 * for the string, roughly. */
	if (!out)
		return 0;

	ptr = str + 1;
	ptr2 = out;
	while (*ptr != '\"' && *ptr) {
		if (*ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u':	/* transcode utf16 to utf8. */
				sscanf(ptr + 1, "%4x", &uc);
				ptr += 4;	/* get the unicode char. */

				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
					break;
				//check for invalid
				if (uc >= 0xD800 && uc <= 0xDBFF)
					//UTF16 surrogate pairs.
				{
					if (ptr[1] != '\\' || ptr[2] != 'u')
						break;
					//missing second - half of surrogate.
					sscanf(ptr + 3, "%4x", &uc2);
					ptr += 6;
					if (uc2 < 0xDC00 || uc2 > 0xDFFF)
						break;
					//invalid second - half of surrogate.
					uc = 0x10000 | ((uc & 0x3FF) << 10) | (uc2 & 0x3FF);
				}
				len = 4;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				else if (uc < 0x10000)
					len = 3;
				ptr2 += len;

				switch (len) {
				case 4:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 3:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 1:
					*--ptr2 = (uc | firstByteMark[len]);
				}
				ptr2 += len;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"')
		ptr++;
	item->valuestring = out;
	item->type = srjson_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(srjson_doc_t *doc, const char *str)
{
	const char     *ptr;
	char           *ptr2, *out;
	int             len = 0;
	unsigned char   token;

	if (!str)
		return srjson_strdup(doc, "");
	ptr = str;
	while ((token = *ptr) && ++len) {
		if (strchr("\"\\\b\f\n\r\t", token))
			len++;
		else if (token < 32)
			len += 5;
		ptr++;
	}

	out = (char *) doc->malloc_fn(len + 3);
	if (!out)
		return 0;

	ptr2 = out;
	ptr = str;
	*ptr2++ = '\"';
	while (*ptr) {
		if ((unsigned char) *ptr > 31 && *ptr != '\"' && *ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			*ptr2++ = '\\';
			switch (token = *ptr++) {
			case '\\':
				*ptr2++ = '\\';
				break;
			case '\"':
				*ptr2++ = '\"';
				break;
			case '\b':
				*ptr2++ = 'b';
				break;
			case '\f':
				*ptr2++ = 'f';
				break;
			case '\n':
				*ptr2++ = 'n';
				break;
			case '\r':
				*ptr2++ = 'r';
				break;
			case '\t':
				*ptr2++ = 't';
				break;
			default:
				sprintf(ptr2, "u%04x", token);
				ptr2 += 5;
				break;	/* escape and print */
			}
		}
	}
	*ptr2++ = '\"';
	*ptr2++ = 0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static char *print_string(srjson_doc_t *doc, srjson_t *item) {
	return print_string_ptr(doc, item->valuestring);
}

/* Predeclare these prototypes. */
static const char *parse_value(srjson_doc_t *doc, srjson_t *item, const char *value);
static char *print_value(srjson_doc_t *doc, srjson_t *item, int depth, int fmt);
static const char *parse_array(srjson_doc_t *doc, srjson_t *item, const char *value);
static char *print_array(srjson_doc_t *doc, srjson_t *item, int depth, int fmt);
static const char *parse_object(srjson_doc_t *doc, srjson_t *item, const char *value);
static char *print_object(srjson_doc_t *doc, srjson_t *item, int depth, int fmt);

/* Utility to jump whitespace and cr/lf */
static const char *skip(const char *in)
{
	while (in && *in && (unsigned char) *in <= 32)
		in++;
	return in;
}

/* Parse an object - create a new root, and populate. */
srjson_t *srjson_Parse(srjson_doc_t *doc, const char *value)
{
	srjson_t *c = srjson_New_Item(doc);
	ep = 0;
	if (!c)
		return 0;	/* memory fail */

	if (!parse_value(doc, c, skip(value))) {
		srjson_Delete(doc, c);
		return 0;
	}
	return c;
}

/* Render a srjson item/entity/structure to text. */
char *srjson_Print(srjson_doc_t *doc, srjson_t *item) {
	return print_value(doc, item, 0, 1);
}

char *srjson_PrintUnformatted(srjson_doc_t *doc, srjson_t *item) {
	return print_value(doc, item, 0, 0);
}

/* Parser core - when encountering text, process appropriately. */
static const char *parse_value(srjson_doc_t *doc, srjson_t *item, const char *value)
{
	if (!value)
		return 0;	/* Fail on null. */
	if (!strncmp(value, "null", 4)) {
		item->type = srjson_NULL;
		return value + 4;
	}
	if (!strncmp(value, "false", 5)) {
		item->type = srjson_False;
		return value + 5;
	}
	if (!strncmp(value, "true", 4)) {
		item->type = srjson_True;
		item->valueint = 1;
		return value + 4;
	}
	if (*value == '\"') {
		return parse_string(doc, item, value);
	}
	if (*value == '-' || (*value >= '0' && *value <= '9')) {
		return parse_number(doc, item, value);
	}
	if (*value == '[') {
		return parse_array(doc, item, value);
	}
	if (*value == '{') {
		return parse_object(doc, item, value);
	}
	ep = value;
	return 0;		/* failure. */
}

/* Render a value to text. */
static char *print_value(srjson_doc_t *doc, srjson_t *item, int depth, int fmt)
{
	char *out = 0;
	if (!item)
		return 0;
	switch ((item->type) & 255) {
	case srjson_NULL:
		out = srjson_strdup(doc, "null");
		break;
	case srjson_False:
		out = srjson_strdup(doc, "false");
		break;
	case srjson_True:
		out = srjson_strdup(doc, "true");
		break;
	case srjson_Number:
		out = print_number(doc, item);
		break;
	case srjson_String:
		out = print_string(doc, item);
		break;
	case srjson_Array:
		out = print_array(doc, item, depth, fmt);
		break;
	case srjson_Object:
		out = print_object(doc, item, depth, fmt);
		break;
	}
	return out;
}

/* Build an array from input text. */
static const char *parse_array(srjson_doc_t *doc, srjson_t *item, const char *value)
{
	srjson_t *child;
	if (*value != '[') {
		ep = value;
		return 0;
	}			/* not an array! */
	item->type = srjson_Array;
	value = skip(value + 1);
	if (*value == ']')
		return value + 1;	/* empty array. */

	item->child = child = srjson_New_Item(doc);
	if (!item->child)
		return 0;	/* memory fail */
	value = skip(parse_value(doc, child, skip(value)));	/* skip any spacing, get
							 * the value. */
	if (!value)
		return 0;

	while (*value == ',') {
		srjson_t *new_item;
		if (!(new_item = srjson_New_Item(doc)))
			return 0;	/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_value(doc, child, skip(value + 1)));
		if (!value)
			return 0;	/* memory fail */
	}

	if (*value == ']')
		return value + 1;	/* end of array */
	ep = value;
	return 0;		/* malformed. */
}

/* Render an array to text */
static char *print_array(srjson_doc_t *doc, srjson_t *item, int depth, int fmt)
{
	char     **entries;
	char     *out = 0, *ptr, *ret;
	int       len = 5;
	srjson_t *child = item->child;
	int       numentries = 0, i = 0, fail = 0;

	/* How many entries in the array? */
	while (child)
		numentries++, child = child->next;
	/* Allocate an array to hold the values for each */
	entries = (char **) doc->malloc_fn(numentries * sizeof(char *));
	if (!entries)
		return 0;
	memset(entries, 0, numentries * sizeof(char *));
	/* Retrieve all the results: */
	child = item->child;
	while (child && !fail) {
		ret = print_value(doc, child, depth + 1, fmt);
		entries[i++] = ret;
		if (ret)
			len += strlen(ret) + 2 + (fmt ? 1 : 0);
		else
			fail = 1;
		child = child->next;
	}

	/* If we didn't fail, try to malloc the output string */
	if (!fail)
		out = (char *) doc->malloc_fn(len);
	/* If that fails, we fail. */
	if (!out)
		fail = 1;

	/* Handle failure. */
	if (fail) {
		for (i = 0; i < numentries; i++)
			if (entries[i])
				doc->free_fn(entries[i]);
		doc->free_fn(entries);
		return 0;
	}
	/* Compose the output array. */
	*out = '[';
	ptr = out + 1;
	*ptr = 0;
	for (i = 0; i < numentries; i++) {
		strcpy(ptr, entries[i]);
		ptr += strlen(entries[i]);
		if (i != numentries - 1) {
			*ptr++ = ',';
			if (fmt)
				*ptr++ = ' ';
			*ptr = 0;
		}
		doc->free_fn(entries[i]);
	}
	doc->free_fn(entries);
	*ptr++ = ']';
	*ptr++ = 0;
	return out;
}

/* Build an object from the text. */
static const char *parse_object(srjson_doc_t *doc, srjson_t *item, const char *value)
{
	srjson_t *child;
	if (*value != '{') {
		ep = value;
		return 0;
	}			/* not an object! */
	item->type = srjson_Object;
	value = skip(value + 1);
	if (*value == '}')
		return value + 1;	/* empty array. */

	item->child = child = srjson_New_Item(doc);
	if (!item->child)
		return 0;
	value = skip(parse_string(doc, child, skip(value)));
	if (!value)
		return 0;
	child->string = child->valuestring;
	child->valuestring = 0;
	if (*value != ':') {
		ep = value;
		return 0;
	}			/* fail! */
	value = skip(parse_value(doc, child, skip(value + 1)));	/* skip any spacing, get
								 * the value. */
	if (!value)
		return 0;

	while (*value == ',') {
		srjson_t *new_item;
		if (!(new_item = srjson_New_Item(doc)))
			return 0;	/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_string(doc, child, skip(value + 1)));
		if (!value)
			return 0;
		child->string = child->valuestring;
		child->valuestring = 0;
		if (*value != ':') {
			ep = value;
			return 0;
		}		/* fail! */
		value = skip(parse_value(doc, child, skip(value + 1)));	/* skip any spacing, get
									 * the value. */
		if (!value)
			return 0;
	}

	if (*value == '}')
		return value + 1;	/* end of array */
	ep = value;
	return 0;		/* malformed. */
}

/* Render an object to text. */
static char *print_object(srjson_doc_t *doc, srjson_t *item, int depth, int fmt)
{
	char      **entries = 0, **names = 0;
	char      *out = 0, *ptr, *ret, *str;
	int        len = 7, i = 0, j;
	srjson_t  *child = item->child;
	int        numentries = 0, fail = 0;
	/* Count the number of entries. */
	while (child)
		numentries++, child = child->next;
	/* Allocate space for the names and the objects */
	entries = (char **) doc->malloc_fn(numentries * sizeof(char *));
	if (!entries)
		return 0;
	names = (char **)doc->malloc_fn(numentries * sizeof(char *));
	if (!names) {
		doc->free_fn(entries);
		return 0;
	}
	memset(entries, 0, sizeof(char *) * numentries);
	memset(names, 0, sizeof(char *) * numentries);

	/* Collect all the results into our arrays: */
	child = item->child;
	depth++;
	if (fmt)
		len += depth;
	while (child) {
		names[i] = str = print_string_ptr(doc, child->string);
		entries[i++] = ret = print_value(doc, child, depth, fmt);
		if (str && ret)
			len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
		else
			fail = 1;
		child = child->next;
	}

	/* Try to allocate the output string */
	if (!fail)
		out = (char *) doc->malloc_fn(len);
	if (!out)
		fail = 1;

	/* Handle failure */
	if (fail) {
		for (i = 0; i < numentries; i++) {
			if (names[i])
				doc->free_fn(names[i]);
			if (entries[i])
				doc->free_fn(entries[i]);
		}
		doc->free_fn(names);
		doc->free_fn(entries);
		return 0;
	}
	/* Compose the output: */
	*out = '{';
	ptr = out + 1;
	if (fmt)
		*ptr++ = '\n';
	*ptr = 0;
	for (i = 0; i < numentries; i++) {
		if (fmt)
			for (j = 0; j < depth; j++)
				*ptr++ = '\t';
		strcpy(ptr, names[i]);
		ptr += strlen(names[i]);
		*ptr++ = ':';
		if (fmt)
			*ptr++ = '\t';
		strcpy(ptr, entries[i]);
		ptr += strlen(entries[i]);
		if (i != numentries - 1)
			*ptr++ = ',';
		if (fmt)
			*ptr++ = '\n';
		*ptr = 0;
		doc->free_fn(names[i]);
		doc->free_fn(entries[i]);
	}

	doc->free_fn(names);
	doc->free_fn(entries);
	if (fmt)
		for (i = 0; i < depth - 1; i++)
			*ptr++ = '\t';
	*ptr++ = '}';
	*ptr++ = 0;
	return out;
}

/* Get Array size/item / object item. */
int srjson_GetArraySize(srjson_doc_t *doc, srjson_t *array)
{
	srjson_t *c = array->child;
	int i = 0;
	while (c)
		i++, c = c->next;
	return i;
}

srjson_t *srjson_GetArrayItem(srjson_doc_t *doc, srjson_t *array, int item)
{
	srjson_t *c = array->child;
	while (c && item > 0)
		item--, c = c->next;
	return c;
}

srjson_t *srjson_GetObjectItem(srjson_doc_t *doc, srjson_t *object, const char *string)
{
	srjson_t *c = object->child;
	while (c && srjson_strcasecmp(c->string, string))
		c = c->next;
	return c;
}

/* Utility for array list handling. */
static void suffix_object(srjson_t *prev, srjson_t *item)
{
	prev->next = item;
	item->prev = prev;
}

/* Utility for handling references. */
static srjson_t *create_reference(srjson_doc_t *doc, srjson_t *item) {
	srjson_t *ref = srjson_New_Item(doc);
	if (!ref)
		return 0;
	memcpy(ref, item, sizeof(srjson_t));
	ref->string = 0;
	ref->type |= srjson_IsReference;
	ref->next = ref->prev = 0;
	return ref;
}

/* Add item to array/object. */
void srjson_AddItemToArray(srjson_doc_t *doc, srjson_t *array, srjson_t *item) {
	srjson_t *c = array->child;
	if (!item)
		return;
	if (!c) {
		array->child = item;
	} else {
		while (c && c->next)
			c = c->next;
		suffix_object(c, item);
	}
}

void srjson_AddItemToObject(srjson_doc_t *doc, srjson_t *object, const char *string, srjson_t *item) {
	if (!item)
		return;
	if (item->string)
		doc->free_fn(item->string);
	item->string = srjson_strdup(doc, string);
	srjson_AddItemToArray(doc, object, item);
}

void srjson_AddStrItemToObject(srjson_doc_t *doc, srjson_t *object, const char *string, int len, srjson_t *item) {
	if (!item)
		return;
	if (item->string)
		doc->free_fn(item->string);
	item->string = srjson_strndupz(doc, string, len);
	srjson_AddItemToArray(doc, object, item);
}

void srjson_AddItemReferenceToArray(srjson_doc_t *doc, srjson_t *array, srjson_t *item) {
	srjson_AddItemToArray(doc, array, create_reference(doc, item));
}

void srjson_AddItemReferenceToObject(srjson_doc_t *doc, srjson_t *object, const char *string, srjson_t *item) {
	srjson_AddItemToObject(doc, object, string, create_reference(doc, item));
}

srjson_t *srjson_UnlinkItemFromObj(srjson_doc_t *doc, srjson_t *obj, srjson_t *c)
{
	if (!c)
		return 0;
	if (c->prev)
		c->prev->next = c->next;
	if (c->next)
		c->next->prev = c->prev;
	if (c == obj->child)
		obj->child = c->next;
	c->prev = c->next = 0;
	return c;
}

srjson_t *srjson_DetachItemFromArray(srjson_doc_t *doc, srjson_t *array, int which)
{
	srjson_t *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return 0;
	if (c->prev)
		c->prev->next = c->next;
	if (c->next)
		c->next->prev = c->prev;
	if (c == array->child)
		array->child = c->next;
	c->prev = c->next = 0;
	return c;
}

void srjson_DeleteItemFromArray(srjson_doc_t *doc, srjson_t *array, int which) {
	srjson_Delete(doc, srjson_DetachItemFromArray(doc, array, which));
}

srjson_t *srjson_DetachItemFromObject(srjson_doc_t *doc, srjson_t *object, const char *string) {
	int i = 0;
	srjson_t *c = object->child;
	while (c && srjson_strcasecmp(c->string, string))
		i++, c = c->next;
	if (c)
		return srjson_DetachItemFromArray(doc, object, i);
	return 0;
}

void srjson_DeleteItemFromObject(srjson_doc_t *doc, srjson_t *object, const char *string) {
	srjson_Delete(doc, srjson_DetachItemFromObject(doc, object, string));
}

/* Replace array/object items with new ones. */
void srjson_ReplaceItemInArray(srjson_doc_t *doc, srjson_t *array, int which, srjson_t *newitem)
{
	srjson_t *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return;
	newitem->next = c->next;
	newitem->prev = c->prev;
	if (newitem->next)
		newitem->next->prev = newitem;
	if (c == array->child)
		array->child = newitem;
	else
		newitem->prev->next = newitem;
	c->next = c->prev = 0;
	srjson_Delete(doc, c);
}

void srjson_ReplaceItemInObject(srjson_doc_t *doc, srjson_t * object, const char *string, srjson_t *newitem) {
	int i = 0;
	srjson_t *c = object->child;
	while (c && srjson_strcasecmp(c->string, string))
		i++, c = c->next;
	if (c) {
		newitem->string = srjson_strdup(doc, string);
		srjson_ReplaceItemInArray(doc, object, i, newitem);
	}
}

/* Create basic types: */
srjson_t *srjson_CreateNull(srjson_doc_t *doc) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = srjson_NULL;
	return item;
}

srjson_t *srjson_CreateTrue(srjson_doc_t *doc) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = srjson_True;
	return item;
}

srjson_t *srjson_CreateFalse(srjson_doc_t *doc) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = srjson_False;
	return item;
}

srjson_t *srjson_CreateBool(srjson_doc_t *doc, int b) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = b ? srjson_True : srjson_False;
	return item;
}

srjson_t *srjson_CreateNumber(srjson_doc_t *doc, double num) {
	srjson_t *item = srjson_New_Item(doc);
	if (item) {
		item->type = srjson_Number;
		item->valuedouble = num;
		item->valueint = (int) num;
	} return item;
}

srjson_t *srjson_CreateString(srjson_doc_t *doc, const char *string) {
	srjson_t *item = srjson_New_Item(doc);
	if (item) {
		item->type = srjson_String;
		item->valuestring = srjson_strdup(doc, string);
	} return item;
}


srjson_t *srjson_CreateStr(srjson_doc_t *doc, const char *string, int len) {
	srjson_t *item = srjson_New_Item(doc);
	if (item) {
		item->type = srjson_String;
		item->valuestring = srjson_strndupz(doc, string, len);
	} return item;
}

srjson_t *srjson_CreateArray(srjson_doc_t *doc) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = srjson_Array;
	return item;
}

srjson_t *srjson_CreateObject(srjson_doc_t *doc) {
	srjson_t *item = srjson_New_Item(doc);
	if (item)
		item->type = srjson_Object;
	return item;
}

/* Create Arrays: */
srjson_t *srjson_CreateIntArray(srjson_doc_t *doc, int *numbers, int count) {
	int i;
	srjson_t *n = 0, *p = 0, *a = srjson_CreateArray(doc);
	for (i = 0; a && i < count; i++) {
		n = srjson_CreateNumber(doc, numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	} return a;
}

srjson_t *srjson_CreateFloatArray(srjson_doc_t *doc, float *numbers, int count) {
	int i;
	srjson_t *n = 0, *p = 0, *a = srjson_CreateArray(doc);
	for (i = 0; a && i < count; i++) {
		n = srjson_CreateNumber(doc, numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	} return a;
}

srjson_t *srjson_CreateDoubleArray(srjson_doc_t *doc, double *numbers, int count) {
	int i;
	srjson_t *n = 0, *p = 0, *a = srjson_CreateArray(doc);
	for (i = 0; a && i < count; i++) {
		n = srjson_CreateNumber(doc, numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	} return a;
}

srjson_t *srjson_CreateStringArray(srjson_doc_t *doc, const char **strings, int count) {
	int i;
	srjson_t *n = 0, *p = 0, *a = srjson_CreateArray(doc);
	for (i = 0; a && i < count; i++) {
		n = srjson_CreateString(doc, strings[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	} return a;
}
