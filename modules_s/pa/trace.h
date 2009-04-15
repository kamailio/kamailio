#ifndef __TRACE_H
#define __TRACE_H

#define DO_TRACE

#ifdef DO_TRACE

#include <cds/memory.h>
#include <cds/logger.h>

#include <cds/ser_profile.h>

#define mem_alloc	cds_malloc
#define mem_free	cds_free

#define TRACE(...)		TRACE_LOG("PA: " __VA_ARGS__)

#else

#define TRACE(args...)

#endif

#endif
