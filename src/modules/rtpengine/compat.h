#ifndef __COMPAT__H__
#define __COMPAT__H__

#include "../../core/mem/mem.h"
#include "../../core/str.h"
#ifndef BENCODE_MALLOC
#define BENCODE_MALLOC pkg_malloc
#define BENCODE_FREE pkg_free
#endif
#define INLINE static inline

#endif
