#ifndef SLEEP_INT_H_INCLUDED
#define SLEEP_INT_H_INCLUDED

#include <string.h>

extern const char * const xmlrpc_strsol;

void
xmlrpc_millisecond_sleep(unsigned int const milliseconds);

void 
xmlrpc_asprintf(const char ** const retvalP, const char * const fmt, ...);

void
xmlrpc_strfree(const char * const string);

static inline unsigned short 
xmlrpc_streq(const char * const a,
             const char * const b) {
    return (strcmp(a, b) == 0);
}

static inline unsigned short
xmlrpc_strcaseeq(const char * const a,
                 const char * const b) {
    return (strcasecmp(a, b) == 0);
}

static inline unsigned short
xmlrpc_strneq(const char * const a,
              const char * const b,
              size_t       const len) {
    return (strncmp(a, b, len) == 0);
}

#define ATTR_UNUSED __attribute__((__unused__))

#define DIRECTORY_SEPARATOR "/"

#endif


#ifndef XMLRPC_C_UTIL_INT_H_INCLUDED
#define XMLRPC_C_UTIL_INT_H_INCLUDED

typedef enum {
    false = 0,
	true = 1
} bool;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)

#endif
