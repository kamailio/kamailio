/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * \defgroup mem Kamailio memory manager
 * \brief  Kamailio internal memory manager
 *
 * Kamailio internal memory manager for private (per process) and shared
 * memory pools. It provides several different strategies for the memory
 * management, like really fast, with extended debugging and also plain system
 * memory management.
 */

/**
 * \file
 * \brief Main definitions for memory manager
 *
 * \brief Main definitions for memory manager, like malloc, free and realloc
 * \ingroup mem
 */


#ifndef mem_h
#define mem_h
#include "../config.h"
#include "../dprint.h"

#include "pkg.h"


/** generic logging helper for allocation errors in private memory pool/ system */
#ifdef SYS_MALLOC
#define PKG_MEM_ERROR LM_ERR("could not allocate private memory from sys pool\n")
#define PKG_MEM_CRITICAL LM_CRIT("could not allocate private memory from sys pool\n")
#else
#define PKG_MEM_ERROR LM_ERR("could not allocate private memory from pkg pool\n")
#define PKG_MEM_CRITICAL LM_CRIT("could not allocate private memory from pkg pool\n")
#endif

/** generic logging helper for allocation errors in system memory */
#define SYS_MEM_ERROR LM_ERR("could not allocate memory from system\n")
#define SYS_MEM_CRITICAL LM_CRIT("could not allocate memory from system\n")

/** generic logging helper for allocation errors in shared memory pool */
#define SHM_MEM_ERROR LM_ERR("could not allocate shared memory from shm pool\n")
#define SHM_MEM_CRITICAL LM_CRIT("could not allocate shared memory from shm pool\n")

#endif
