#ifndef __CDS_H
#define __CDS_H

/** \defgroup cds CDS library - Common Data Structures 
 *
 * This library contains many useful functions and data structures. It is
 possible to use it with Sip Express Router (SER) or without it. In the first
 case it is able to use some internal SER's data types like strings.
 *
 * \section cds_conventions Conventions
 *  - data types (structures, enums, ...) have their names with suffix \c _t
 *  (\c str_t, \c dstring_t, ...)
 *  - many functions have prefix according to data structure on which are
 *  operating (like dstr_append which operates on dstring_t data structure)
 *  - most functions return 0 as successful result and nonzero value as error
 *
 * \section cds_dependencies Dependencies
 * This library depends only on standard C libraries and needs no other
 * external libraries.
 *
 * \section cds_ser_usage Usage with SER
 * There can be problems with using shared libraries on different platforms.
 * Currently supported solution is that user must supply LD_LIBRARY_PATH
 * (or something similar on his OS) with the path to the library before 
 * starting SER with modules needed the library.
 *
 * \section cds_nonser_usage Usage without SER
 * The library can be compiled without dependencies on SER and can be linked
 * to whatever program as any other dynamic library. This style of usage is
 * probably used only when debugging problems which is hard to find with SER
 * (outside of SER is possible to use tools like Valgrind) and for author's
 * little programs and need not to be documented more... ;-)
 *
 * \par History
 * There were following reasons to introduce this library:
 *  - many duplicated functions in modules (copy&pasted between modules)
 *  without touching SER's core
 *  - possibility to debug programs outside of SER due to its simplicity 
 *  and many usefull tools
 *
 * @{ */

#ifdef __cplusplus
extern "C" {
#endif

#if 0
/* not used */

/** \defgroup cds_init Initialization/destruction
 * Library needs to be initialized before using it and
 * un-initialized after it is not used more. Use \ref cds_initialize and 
 * \ref cds_cleanup for this purpose.
 * @{ */

/** Initializes CDS library. 
 *
 * Currently initializes reference counter which is experimental and 
 * probably will be removed in the future because seems to be rather 
 * useless here. */
int cds_initialize();

/** Cleans up after CDS. After calling this function can't be called
 * reference counter functions. */
void cds_cleanup();

#endif

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */

#endif
