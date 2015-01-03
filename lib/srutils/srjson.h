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
* \brief srutils :: SRjson
* \ingroup srutils
* Module: \ref srutils
*/

#ifndef _srjson__h_
#define _srjson__h_

#ifdef __cplusplus
extern          "C"
{
#endif

#include "../../str.h"

/* srjson Types: */
#define srjson_False   0
#define srjson_True    1
#define srjson_NULL    2
#define srjson_Number  3
#define srjson_String  4
#define srjson_Array   5
#define srjson_Object  6

#define srjson_IsReference 256

/* The srjson node structure: */
typedef struct srjson {
	struct srjson *parent;
	struct srjson *next;
	struct srjson *prev;	/* next/prev allow you to
						 * walk array/object chains.
						 * Alternatively, use
						 * GetArraySize/GetArrayItem/G
						 * etObjectItem */
	struct srjson *child;	/* An array or object item will have
					 * a child pointer pointing to a
					 * chain of the items in the
					 * array/object. */

	int             type;	/* The type of the item, as above. */
	char           *valuestring;	/* The item's string, if
						 * type==srjson_String */
	int             valueint;	/* The item's number, if
						 * type==srjson_Number */
	double          valuedouble;	/* The item's number, if
						 * type==srjson_Number */
	char           *string;	/* The item's name string, if this
					 * item is the child of, or is in the
					 * list of subitems of an object. */
} srjson_t;

typedef struct srjson_doc {
	srjson_t  *root;
	int flags;
	str buf;
	void           *(*malloc_fn) (size_t sz);
	void            (*free_fn) (void *ptr);
} srjson_doc_t;

typedef struct srjson_Hooks {
	void           *(*malloc_fn) (size_t sz);
	void            (*free_fn) (void *ptr);
} srjson_Hooks;


extern srjson_doc_t *srjson_NewDoc(srjson_Hooks *hooks);
extern int srjson_InitDoc(srjson_doc_t *doc, srjson_Hooks *hooks);

extern void srjson_DeleteDoc(srjson_doc_t *doc);
extern void srjson_DestroyDoc(srjson_doc_t *doc);

/*
 * Supply a block of JSON, and this returns a srjson object you can
 * interrogate. Call srjson_Delete when finished.
 */
extern srjson_t *srjson_Parse(srjson_doc_t *doc, const char *value);

/*
 * Render a srjson entity to text for transfer/storage. Free the char*
 * when finished.
 */
extern char *srjson_Print(srjson_doc_t *doc, srjson_t *item);

/*
 * Render a srjson entity to text for transfer/storage without any
 * formatting. Free the char* when finished.
 */
extern char *srjson_PrintUnformatted(srjson_doc_t *doc, srjson_t *item);

/* Delete a srjson entity and all subentities. */
extern void srjson_Delete(srjson_doc_t *doc, srjson_t *c);

/* Returns the number of items in an array (or object). */
extern int srjson_GetArraySize(srjson_doc_t *doc, srjson_t *array);

/*
 * Retrieve item number "item" from array "array". Returns NULL if
 * unsuccessful.
 */
extern srjson_t *srjson_GetArrayItem(srjson_doc_t *doc, srjson_t *array, int item);

/* Get item "string" from object. Case insensitive. */
extern srjson_t *srjson_GetObjectItem(srjson_doc_t *doc, srjson_t *object, const char *string);

/*
 * For analysing failed parses. This returns a pointer to the parse
 * error. You'll probably need to look a few chars back to make sense
 * of it. Defined when srjson_Parse() returns 0. 0 when srjson_Parse()
 * succeeds.
 */
extern const char *srjson_GetErrorPtr();

/* These calls create a srjson item of the appropriate type. */
extern srjson_t *srjson_CreateNull(srjson_doc_t *doc);
extern srjson_t *srjson_CreateTrue(srjson_doc_t *doc);
extern srjson_t *srjson_CreateFalse(srjson_doc_t *doc);
extern srjson_t *srjson_CreateBool(srjson_doc_t *doc, int b);
extern srjson_t *srjson_CreateNumber(srjson_doc_t *doc, double num);
extern srjson_t *srjson_CreateString(srjson_doc_t *doc, const char *string);
extern srjson_t *srjson_CreateStr(srjson_doc_t *doc, const char *string, int len);
extern srjson_t *srjson_CreateArray(srjson_doc_t *doc);
extern srjson_t *srjson_CreateObject(srjson_doc_t *doc);

/* These utilities create an Array of count items. */
extern srjson_t *srjson_CreateIntArray(srjson_doc_t *doc, int *numbers, int count);
extern srjson_t *srjson_CreateFloatArray(srjson_doc_t *doc, float *numbers, int count);
extern srjson_t *srjson_CreateDoubleArray(srjson_doc_t *doc, double *numbers, int count);
extern srjson_t *srjson_CreateStringArray(srjson_doc_t *doc, const char **strings, int count);

/* Append item to the specified array/object. */
extern void srjson_AddItemToArray(srjson_doc_t *doc, srjson_t *array, srjson_t *item);
extern void srjson_AddItemToObject(srjson_doc_t *doc, srjson_t *object, const char *string, srjson_t *item);
extern void srjson_AddStrItemToObject(srjson_doc_t *doc, srjson_t *object, const char *string, int len, srjson_t *item);

/*
 * Append reference to item to the specified array/object. Use this
 * when you want to add an existing srjson to a new srjson, but don't
 * want to corrupt your existing srjson.
 */
extern void srjson_AddItemReferenceToArray(srjson_doc_t *doc, srjson_t *array, srjson_t *item);
extern void srjson_AddItemReferenceToObject(srjson_doc_t *doc, srjson_t *object, const char *string, srjson_t *item);

/* Remove/Detatch items from Arrays/Objects. */
extern srjson_t *srjson_UnlinkItemFromObj(srjson_doc_t *doc, srjson_t *obj, srjson_t *item);
extern srjson_t *srjson_DetachItemFromArray(srjson_doc_t *doc, srjson_t *array, int which);
extern void srjson_DeleteItemFromArray(srjson_doc_t *doc, srjson_t *array, int which);
extern srjson_t *srjson_DetachItemFromObject(srjson_doc_t *doc, srjson_t *object, const char *string);
extern void srjson_DeleteItemFromObject(srjson_doc_t *doc, srjson_t *object, const char *string);

/* Update array items. */
extern void srjson_ReplaceItemInArray(srjson_doc_t *doc, srjson_t *array, int which, srjson_t *newitem);
extern void srjson_ReplaceItemInObject(srjson_doc_t *doc, srjson_t *object, const char *string, srjson_t *newitem);

#define srjson_AddNullToObject(doc, object,name)		srjson_AddItemToObject(doc, object, name, srjson_CreateNull(doc))
#define srjson_AddTrueToObject(doc, object,name)		srjson_AddItemToObject(doc, object, name, srjson_CreateTrue(doc))
#define srjson_AddFalseToObject(doc, object,name)		srjson_AddItemToObject(doc, object, name, srjson_CreateFalse(doc))
#define srjson_AddNumberToObject(doc, object,name,n)	srjson_AddItemToObject(doc, object, name, srjson_CreateNumber(doc,n))
#define srjson_AddStringToObject(doc, object,name,s)	srjson_AddItemToObject(doc, object, name, srjson_CreateString(doc,s))
#define srjson_AddStrToObject(doc, object,name,s,l)		srjson_AddItemToObject(doc, object, name, srjson_CreateStr(doc,s,l))
#define srjson_AddStrStrToObject(doc, object,name,ln,s,l)	srjson_AddStrItemToObject(doc, object, name, ln, srjson_CreateStr(doc,s,l))

#ifdef __cplusplus
}
#endif

#endif
