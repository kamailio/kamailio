#ifndef __TRACE_H
#define __TRACE_H

#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/ser_profile.h>

#define mem_alloc	cds_malloc
#define mem_free	cds_free

#define TRACE(...)		TRACE_LOG("presence_b2b: " __VA_ARGS__)
/* #define TRACE(args...) */

#endif
