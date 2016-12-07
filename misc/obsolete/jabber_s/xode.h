/*
 * $Id$
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include "expat.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/*
**  Arrange to use either varargs or stdargs
*/

#define MAXSHORTSTR	203		/* max short string length */
#define QUAD_T	unsigned long long

#ifdef __STDC__

#include <stdarg.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap, f)
# define VA_END		va_end(ap)

#else /* __STDC__ */

# include <varargs.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap)
# define VA_END		va_end(ap)

#endif /* __STDC__ */


#ifndef INCL_LIBXODE_H
#define INCL_LIBXODE_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_SNPRINTF
extern int ap_snprintf(char *, size_t, const char *, ...);
#define snprintf ap_snprintf
#endif

#ifndef HAVE_VSNPRINTF
extern int ap_vsnprintf(char *, size_t, const char *, va_list ap);
#define vsnprintf ap_vsnprintf
#endif

/* --------------------------------------------------------- */
/*                                                           */
/* Pool-based memory management routines                     */
/*                                                           */
/* --------------------------------------------------------- */


/* xode_pool_cleaner - callback type which is associated
   with a pool entry; invoked when the pool entry is 
   free'd */
typedef void (*xode_pool_cleaner)(void *arg);


/* pheap - singular allocation of memory */
struct xode_pool_heap
{
    void *block;
    int size, used;
};

/* pool - base node for a pool. Maintains a linked list
   of pool entries (pool_free) */
typedef struct xode_pool_struct
{
    int size;
    struct xode_pool_free *cleanup;
    struct xode_pool_heap *heap;
} _xode_pool, *xode_pool;

/* pool creation routines */
xode_pool xode_pool_heap(int bytes);
xode_pool xode_pool_new(void);

/* pool wrappers for malloc */
void *xode_pool_malloc  (xode_pool p, int size);
void *xode_pool_mallocx (xode_pool p, int size, char c); 
void *xode_pool_malloco (xode_pool p, int size); 

/* wrapper around strdup, gains mem from pool */
char *xode_pool_strdup  (xode_pool p, const char *src); 

/* calls f(arg) before the pool is freed during cleanup */
void xode_pool_cleanup  (xode_pool p, xode_pool_cleaner f, void *arg); 

/* pool wrapper for free, called on a pool */
void xode_pool_free     (xode_pool p); 

/* returns total bytes allocated in this pool */
int  xode_pool_size     (xode_pool p); 

/* --------------------------------------------------------- */
/*                                                           */
/* XML escaping utils                                        */
/*                                                           */
/* --------------------------------------------------------- */
char *xode_strescape(xode_pool p, char *buf); /* Escape <>&'" chars */
char *xode_strunescape(xode_pool p, char *buf);


/* --------------------------------------------------------- */
/*                                                           */
/* String pools (spool) functions                            */
/*                                                           */
/* --------------------------------------------------------- */
struct xode_spool_node
{
    char *c;
    struct xode_spool_node *next;
};

typedef struct xode_spool_struct
{
    xode_pool p;
    int len;
    struct xode_spool_node *last;
    struct xode_spool_node *first;
} *xode_spool;

xode_spool xode_spool_new         ( void                          ); /* create a string pool on a new pool */
xode_spool xode_spool_newfrompool ( xode_pool        p            ); /* create a string pool from an existing pool */
xode_pool  xode_spool_getpool     ( const xode_spool s            ); /* returns the xode_pool used by this xode_spool */
void       xode_spooler           ( xode_spool       s, ...       ); /* append all the char * args to the pool, terminate args with s again */
char       *xode_spool_tostr      ( xode_spool       s            ); /* return a big string */
void       xode_spool_add         ( xode_spool       s, char *str ); /* add a single char to the pool */
char       *xode_spool_str        ( xode_pool        p, ...       ); /* wrap all the spooler stuff in one function, the happy fun ball! */
int        xode_spool_getlen      ( const xode_spool s            ); /* returns the total length of the string contained in the pool */
void       xode_spool_free        ( xode_spool       s            ); /* Free's the pool associated with the xode_spool */


/* --------------------------------------------------------- */
/*                                                           */
/* xodes - Document Object Model                          */
/*                                                           */
/* --------------------------------------------------------- */
#define XODE_TYPE_TAG    0
#define XODE_TYPE_ATTRIB 1
#define XODE_TYPE_CDATA  2

#define XODE_TYPE_LAST   2
#define XODE_TYPE_UNDEF  -1

/* -------------------------------------------------------------------------- 
   Node structure. Do not use directly! Always use accessors macros 
   and methods!
   -------------------------------------------------------------------------- */
typedef struct xode_struct
{
     char*                name;
     unsigned short       type;
     char*                data;
     int                  data_sz;
     int                  complete;
     xode_pool            p;
     struct xode_struct*  parent;
     struct xode_struct*  firstchild; 
     struct xode_struct*  lastchild;
     struct xode_struct*  prev; 
     struct xode_struct*  next;
     struct xode_struct*  firstattrib;
     struct xode_struct*  lastattrib;
} _xode, *xode;

/* Node creation routines */
xode  xode_wrap(xode x,const char* wrapper);
xode  xode_new(const char* name);
xode  xode_new_tag(const char* name);
xode  xode_new_frompool(xode_pool p, const char* name);
xode  xode_insert_tag(xode parent, const char* name); 
xode  xode_insert_cdata(xode parent, const char* CDATA, unsigned int size);
xode  xode_insert_tagnode(xode parent, xode node);
void  xode_insert_node(xode parent, xode node);
xode  xode_from_str(char *str, int len);
xode  xode_from_strx(char *str, int len, int *err, int *pos);
xode  xode_from_file(char *file);
xode  xode_dup(xode x); /* duplicate x */
xode  xode_dup_frompool(xode_pool p, xode x);

/* Node Memory Pool */
xode_pool xode_get_pool(xode node);

/* Node editing */
void xode_hide(xode child);
void xode_hide_attrib(xode parent, const char *name);

/* Node deletion routine, also frees the node pool! */
void xode_free(xode node);

/* Locates a child tag by name and returns it */
xode  xode_get_tag(xode parent, const char* name);
char* xode_get_tagdata(xode parent, const char* name);

/* Attribute accessors */
void     xode_put_attrib(xode owner, const char* name, const char* value);
char*    xode_get_attrib(xode owner, const char* name);

/* Bastard am I, but these are fun for internal use ;-) */
void     xode_put_vattrib(xode owner, const char* name, void *value);
void*    xode_get_vattrib(xode owner, const char* name);

/* Node traversal routines */
xode  xode_get_firstattrib(xode parent);
xode  xode_get_firstchild(xode parent);
xode  xode_get_lastchild(xode parent);
xode  xode_get_nextsibling(xode sibling);
xode  xode_get_prevsibling(xode sibling);
xode  xode_get_parent(xode node);

/* Node information routines */
char*    xode_get_name(xode node);
char*    xode_get_data(xode node);
int      xode_get_datasz(xode node);
int      xode_get_type(xode node);

int      xode_has_children(xode node);
int      xode_has_attribs(xode node);

/* Node-to-string translation */
char*    xode_to_str(xode node);
char*    xode_to_prettystr(xode node);  /* Puts \t and \n to make a human-easily readable string */

int      xode_cmp(xode a, xode b); /* compares a and b for equality */

int      xode_to_file(char *file, xode node); /* writes node to file */


/***********************
 * XSTREAM Section
 ***********************/

#define XODE_STREAM_MAXNODE 1000000
#define XODE_STREAM_MAXDEPTH 100

#define XODE_STREAM_ROOT        0 /* root element */
#define XODE_STREAM_NODE        1 /* normal node */
#define XODE_STREAM_CLOSE       2 /* closed root node */
#define XODE_STREAM_ERROR       4 /* parser error */

typedef void (*xode_stream_onNode)(int type, xode x, void *arg); /* xstream event handler */

typedef struct xode_stream_struct
{
    XML_Parser parser;
    xode node;
    char *cdata;
    int cdata_len;
    xode_pool p;
    xode_stream_onNode f;
    void *arg;
    int status;
    int depth;
} *xode_stream, _xode_stream;

xode_stream xode_stream_new(xode_pool p, xode_stream_onNode f, void *arg); /* create a new xstream */
int xode_stream_eat(xode_stream xs, char *buff, int len); /* parse new data for this xstream, returns last XSTREAM_* status */

/* convenience functions */

#ifdef __cplusplus
}
#endif

#endif /* INCL_LIBXODE_H */
