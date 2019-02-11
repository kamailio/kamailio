#ifndef __DBID_H
#define __DBID_H

/* functions and structures for generating unique database IDs */

#include <string.h>

#define MAX_DBID_LEN	48

#ifdef __cplusplus
extern "C" {
#endif

typedef char dbid_t[MAX_DBID_LEN];

/** generates ID for data in shared memory at address given by data_ptr */
void generate_dbid_ptr(dbid_t dst, void *data_ptr);

#ifdef SER
void generate_dbid(dbid_t dst);
#endif

/* macros for conversion to string representation of DBID
 * (if dbid becomes structure with binary information 
 * these should be removed and replaced by functions) */
#define dbid_strlen(id)	strlen(id)
#define dbid_strptr(id)	((char*)(id))

#define dbid_clear(id)	do { (id)[0] = 0; } while (0)

#define is_dbid_empty(id) (!(id)[0])

/** Copies dbid as string into destination. The destination string
 * data buffer MUST be allocated in needed size! */
#define dbid_strcpy(dst,id,l) do { memcpy((dst)->s,id,l); (dst)->len = l; } while (0)

#ifdef __cplusplus
}
#endif

#endif
