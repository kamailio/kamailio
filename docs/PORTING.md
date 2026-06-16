# C → Go Porting Guide

You are translating one C file to Go. Read this whole document before writing any code. The goal of Phase A is a draft `.go` next to the `.c` that captures the logic faithfully — it does not need to compile. Phase B makes it compile package-by-package.

## Ground rules

Write the `.go` in the same directory as the `.c`, same basename. Example: `src/core/str.c` → `src/core/str.go`; `src/core/parser/msg_parser.c` → `src/core/parser/msg_parser.go`.

Do not invent package layouts. Cross-area types are referenced as `area.Type` (see package map below). Phase B wires the `go.mod`.

No external Go packages without approval. The project must be self-contained. Standard library only.

No goroutines, channels, or concurrency primitives. C code is single-threaded or uses explicit callback patterns. Keep the same execution model.

`unsafe` is fine when the C was already unsafe. Annotate every block with `// SAFETY: <why>` mirroring the C invariant.

Leave `// TODO(port): <reason>` for anything you can't translate confidently. Don't guess. Flagging is better than wrong code.

Leave `// PERF(port): <C idiom>` — profile in Phase B wherever the C used a perf-specific idiom (inline, preallocated buffers, stack allocation, macros) and the port uses the plain idiomatic form. Phase A optimizes for correctness+idiom; Phase B greps `PERF(port)` and benchmarks.

Match the C's structure. Same function names, same field order, same control flow. Phase B reviewers diff `.c` ↔ `.go` side-by-side.

## Package map

| C path prefix | Go package |
|---------------|------------|
| `src/core/` | `core` |
| `src/core/parser/` | `core/parser` |
| `src/core/mem/` | `core/mem` |
| `src/core/cfg/` | `core/cfg` |
| `src/modules/` | `modules/<name>` |

## Type mapping

| C type | Go type |
|--------|---------|
| `char*`, `str` | `string` |
| `int`, `long` | `int` |
| `unsigned int` | `uint` |
| `uint32_t` | `uint32` |
| `uint64_t` | `uint64` |
| `int32_t` | `int32` |
| `size_t` | `uint64` or `int` |
| `void*` | `unsafe.Pointer` or `[]byte` |
| `struct X*` | `*X` or `X` (by value) |
| `typedef` | `type X struct` or `type X int32` |
| `enum` | `const ( X = iota )` |
| `#define` (constant) | `const` |
| `#define` (macro function) | `func` |
| `static inline` | `inline` comment + direct translation |
| `NULL` | `nil` |
| `func_ptr` | `func(...)` type or `interface` |

## Memory management

C code manually manages memory with `pkg_malloc`, `shm_malloc`, `pkg_free`, `shm_free`. The Go port should use Go's garbage collector but preserve the semantics:

- If the C code used **pkg_malloc/pkg_free**: use Go slices or `new` with the understanding the GC handles it
- If the C code used **shm_malloc/shm_free**: use `sync.Pool` or manually managed pools in Phase B
- If the C code used **stack allocation**: use Go stack variables directly

Leave `// MEM(port): shm_malloc → sync.Pool` markers for Phase B memory model work.

## Control flow

| C pattern | Go translation |
|-----------|----------------|
| `goto label` | `goto label` (rare, use sparingly) |
| `return` at end of `void` func | `return` or omit |
| `switch` | `switch` (same syntax) |
| `for` | `for` (same syntax) |
| `break` / `continue` | same |
| `do { } while(cond)` | `for { ...; if !cond { break }; ... }` |

## Error handling

| C pattern | Go translation |
|-----------|----------------|
| `return -1` on error | `return nil` or `return err` |
| `return 0` on success | `return nil` or `return result` |
| `goto cleanup` | `defer` cleanup + early return |
| `errno` / `strerror()` | `error` type + `errors.New()` / `fmt.Errorf()` |

## Header files

C headers (`.h`) become Go package documentation and type definitions. The `.go` file should contain all implementations. Use `//export` comments sparingly — prefer pure Go.

## SIP-specific types

Kamailio uses `str` type for strings (pointer + length). Map to Go `string` but preserve the pattern where `str` is used as a struct field:

```go
type str struct {
    s []byte
    len int
}
```

Or simpler: use Go `string` directly and add `len()` calls as needed.

## Parser translation

SIP parsers in `src/core/parser/` are critical. Preserve the character-by-character parsing style. Use `for i := 0; i < len(s); i++` pattern instead of iterators.

## Phase A checklist

- [ ] Create `.go` file next to `.c`
- [ ] Translate all `struct` definitions
- [ ] Translate all `typedef` aliases
- [ ] Translate all `#define` constants and macros
- [ ] Translate function signatures (use `error` return for failure)
- [ ] Translate function bodies, preserving control flow
- [ ] Add `// TODO(port):` for uncertain translations
- [ ] Add `// PERF(port):` for performance-sensitive idioms
- [ ] Add `// MEM(port):` for memory management differences

## Phase B checklist

- [ ] Make package compile with `go build`
- [ ] Wire `go.mod` dependencies
- [ ] Replace `// TODO(port):` items
- [ ] Benchmark `// PERF(port):` items
- [ ] Implement memory pool strategy
- [ ] Run existing tests
- [ ] Diff C ↔ Go side-by-side for correctness

## Example translation

**C:**
```c
// src/core/str.c
#include "str.h"

int str_len(str* s) {
    return s->len;
}

str* str_dup(str* src, int flags) {
    str* dst = pkg_malloc(sizeof(str));
    if (!dst) return NULL;
    dst->s = pkg_malloc(src->len);
    if (!dst->s) { pkg_free(dst); return NULL; }
    memcpy(dst->s, src->s, src->len);
    dst->len = src->len;
    return dst;
}
```

**Go:**
```go
// src/core/str.go
package core

// TODO(port): verify pkg_malloc semantics vs Go GC
// MEM(port): pkg_malloc → new + escape analysis or sync.Pool

func strLen(s *str) int {
    return s.len
}

// PERF(port): pkg_malloc preallocation — Go GC may behave differently
func strDup(src *str, flags int) (*str, error) {
    dst := new(str)
    dst.s = make([]byte, len(src.s))
    if dst.s == nil {
        return nil, errors.New("allocation failed")
    }
    copy(dst.s, src.s)
    dst.len = src.len
    return dst, nil
}
```

## Naming conventions

| C | Go |
|---|----|
| `function_name` | `functionName` (camelCase) |
| `STRUCT_NAME` | `StructName` (PascalCase) |
| `MACRO_CONST` | `MacroConst` (PascalCase) |
| `struct member_name` | `memberName` (camelCase) |
| `enum VAL` | `EnumVal` (PascalCase) |
| `typedef oldname` | `Oldname` (PascalCase) |

Preserve the original C function names as comments for reference:
```go
//go:export str_len  // C: str_len()
func strLen(s *str) int { ... }
```
