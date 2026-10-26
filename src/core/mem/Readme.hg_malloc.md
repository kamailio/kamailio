# hg_malloc - size classes with per-thread caches

A memory manager for shm and pkg, selected with `-x hg` (shm) and
`-X hg` (pkg). One file, `hg_malloc.c` (cells, caches, the shared pool,
page runs, the page bitmap, the memapi entry points), declared in
`memcore.h`. Built when `HG_MALLOC` is defined, which `Makefile.defs` and
the cmake option `HG_MALLOC` do by default, like `TLSF_MALLOC`;
`-DHG_MALLOC=OFF` leaves it out.

## How it works

Requests up to 65,464 bytes are cells of 21 size classes (64 B to
65,472 B, about x1.5 apart, 8-byte aligned payloads; from 8 KB up the
powers of two are 64 B short, so their cells fill a block). Cells are
carved from blocks of 2^k pages (8 KB to 256 KB, 32 cells per block as a
target) placed at 2^k-aligned indices of a page bitmap, lowest first: a
buddy allocator's placement without its bookkeeping. The last 32 bytes of
a block stay unused, so a short overrun of its last cell cannot reach the
next block's header. An aligned hole left by one block fits any block of
its order or below, and frees merge into aligned holes of the next order
by themselves.

A thread's frees go onto its own lists and its allocations come from
them, so the request path takes no lock and touches no cache line another
thread writes. Cells move between threads only through a shared pool,
under the arena lock, a batch at a time: a refill of 32 cells when a list
is empty, a donation of half the list when one is full. The cache budget
is 25% of the arena: for shm split over the processes (over the CPUs, 4
to 64, until the process count is known at the first fork), for pkg kept
by its one owner; at most 256 and at least one cell per class and thread.
A refill takes 32 cells, or the class's bound if that is lower.

A block whose every cell is back in the pool is a drained block. A class
keeps as many drained blocks as it carved or took back into use in the
last second (at least 2); the rest return to the bitmap, where any class
or a page run can take them. Returning blocks the moment they drain would
cut and give back the same blocks thousands of times a second under a
steady load. When no block or page run fits a request, every class's
drained blocks go back first and the request is tried again.

A new block is the carving thread's to hand out. Once a second, on its
next slow path, a thread puts the cells it has not handed out of the
blocks of classes it did not use that second into the pool, so those
blocks can drain.

Larger requests are page runs, exact sizes placed from the top of the
arena down, so they never fragment the block area. realloc keeps a cell or
run while the new size fits it; a run gives back the pages it no longer
needs.

The cell header is 8 bytes: class and state in one word, which a free
claims LIVE to FREE with one compare-and-swap, and the block's offset (or
the page count of a run) in the other. There are no page descriptors: a
cell finds its block through that word.

The memory is the pool the core hands in: the one shm block
(`shm_core_get_pool(0)`) for shm, `malloc()` for pkg, as for f_malloc.
Its processes already share one arena, so hg refuses `-z` above 1. It
registers with `xlocked` set, and `shm_init_api()` then keeps hg's own
entry points instead of wrapping every call in the block lock. Nothing
goes back to the OS. The resident set is what the load has touched, as
with the other managers.

Before a fork the parent returns its cached shm cells to the pool and the
child starts with empty shm caches; the pkg pool is copied with its
caches, which stay valid, and its lock is held over the fork, so a child
never inherits it taken. A thread that exits returns its caches through a
`pthread_key` destructor, armed when it first caches a cell. The TCP
reactor's pkg lock is not installed for hg, since it locks internally. At
shutdown, cleanup() force-unlocks shm once every child has exited; for hg
that releases the arena lock, which a child that got SIGTERM inside it
still holds. tm's paired shm_global_lock()/unlock() calls do nothing. A
status dump gives up if the arena lock stays taken, as it does when a
signal handler interrupted a locked section.

## What it checks

A free of memory the arena does not hold, a pointer that is not aligned
as an allocation or not at the start of one (the block its header names
does not hold a cell there), a double free (the header is already FREE,
or two frees race for it), a header overwritten (the class or state word
is not a live cell's), a page run whose header disagrees with the bitmap
(a second bitmap marks the first page of every block and run), a realloc
of a non-live pointer, a cell on a free list whose header is not FREE,
and a block header whose tag (its class and place) or list links disagree
are refused and logged with LM_CRIT. With `mem_safety=0` the process
aborts on the first of them; otherwise the error is counted (`errors=` in
the status output) and ignored, and the cell or chain in question leaks
rather than being reused; a broken block list is dropped and its blocks
stay in use.

## What it does not do

- No per-site accounting: the memory debugging signature (`DBG_SR_MEMORY`)
  is accepted and ignored, and the summaries say "not available".
- It asks for no huge pages itself (no madvise, no hugetlb pool); with
  the kernel's shmem THP on, shm gets them anyway (see "Huge pages").
- No returning memory to the OS, no sweep of idle threads' caches (the
  budget above bounds them).
- The counters are block-granular: `core.shmmem` reports `real_used` as
  the bytes in blocks and page runs and `used` as that minus the cells in
  the shared pool, so cells in private caches count as used. `free` is the
  free pages. `pkg.stats` figures refresh once per pool batch, not on
  every request.

## Status output

The status dump Kamailio prints at shutdown and on its memory dump
triggers (`mem_dump_pkg`, `mem_dump_shm`, at `memlog` level) shows the
heap size, the bytes carved and their peak, the free bytes, the number of
blocks cut and given back, the page runs, the cells in the shared pool,
the error count, the requests refused for lack of memory, and per class
the partial and drained blocks, the keep count and the cells in the pool.
`core.shmmem` reports the figures described under "What it does not do".

## Measurements

A bare-metal server (2 x Xeon E5-2690 v2, Debian 13, kernel 6.12),
Kamailio built as the Debian package is (-O2, hardening, MEMDBG off) and
pinned to one NUMA node: 8 UDP workers on 8 cores, the other processes
and the NIC interrupts on 4 more. Every manager ran from the same binary
and config, selected with `-x`/`-X`, with 64 GB of shm and 64 MB of pkg so
that memory never limits, on plain 4K pages. sipp load came from four
other hosts, none over 55% CPU. A point is 30 s of load and holds when
every call completes; the highest holding point ran twice. CPU is that
of all kamailio processes over 15 s inside the load; resident is the PSS
of all of them after the load, so a shared page counts once.

### Transaction mode

INVITE answered with 486 by tm, its ACK absorbed. Each ladder started
high and stepped down to the first rate that held.

| manager | holds, calls/s | fails at, calls/s (failed calls) |
|---|---|---|
| q_malloc | 60,000 | 75,000 (3.2%) |
| f_malloc | 60,000 | 75,000 (51.9%) |
| tlsf | 60,000 | 75,000 (0.48%) |
| hg | 120,000 | 150,000 (4.2%) |

At 150,000 calls/s hg's eight workers run 8.2 cores and the socket
drops: the ceiling there is their SIP work, not the allocator. At 75,000
f_malloc completes fewer than half the calls on 1.65 cores: its workers
wait for its lock.

At 60,000 calls/s, where every manager completes every call:

| manager | cores | sipp retransmissions | p99 response, ms | resident after the load, MiB |
|---|---|---|---|---|
| q_malloc | 4.64 | 498,912 | over 1,000 | 2,587 |
| f_malloc | 3.07 | 72,881 | under 1,000 | 2,298 |
| tlsf | 2.91 | 117,201 | over 1,000 | 2,707 |
| hg | 2.67 | 0 | under 2 | 2,227 |

### Load-balancer mode

record_route, dialog with dlg_manage, dispatcher round-robin over 32 sipp
UAS, in-dialog ACK and BYE loose-routed, every call held so that 50,000
are up at once.

| manager | holds, calls/s | fails at, calls/s (failed calls) | CPU at 10,000 calls/s, cores | resident at 10,000 calls/s, MiB |
|---|---|---|---|---|
| f_malloc | 15,000 | 20,000 (33.3%) | 2.41 | 879 |
| hg | 30,000 | 40,000 (20.0%) | 2.09 | 921 |

hg fails at 40,000 on CPU (8.8 cores); f_malloc at 20,000 runs 5.7.

### Huge pages

The kernel backs shared memory with transparent huge pages when
`/sys/kernel/mm/transparent_hugepage/shmem_enabled` is `always`;
Kamailio's shm pool then sits on 2 MB pages with no code change (checked
at every point in the processes' memory map). Same binary, config and
rule as above, that setting the only change:

| manager | holds on 4K pages, calls/s | holds on huge pages, calls/s | gain |
|---|---|---|---|
| q_malloc | 60,000 | 90,000 | 50% |
| f_malloc | 60,000 | 135,000 | 125% |
| tlsf | 60,000 | 150,000 | 150% |
| hg | 120,000 | 180,000 | 50% |

On huge pages the first failures are at 105,000 (q_malloc), 150,000
(f_malloc), 165,000 (tlsf) and 190,000 calls/s (hg, on 8.0 cores). At
150,000 calls/s, where tlsf and hg both complete every call, tlsf runs
7.5 to 8.0 cores with 450,000 to 690,000 sipp retransmissions and hg 6.1
to 6.2 cores with none; at 60,000 hg uses 2.48 cores on huge pages
against 2.67 on 4K pages.

## Tests

A load-time test module (not part of the tree) drives the public shm and
pkg API from RPCs: oversize requests near SIZE_MAX refused with the live
allocation intact; double free refused; realloc from 32 B to 300 KB with
the contents checked; 300 page runs of random sizes freed in random order
with cells interleaved, three cycles, no drift; a raw fork() child not
seeing the parent's cached cells; and 10 s of random churn in 4,096 slots
with patterns checked, for shm and for pkg. Four more corrupt memory on
purpose: a free through a copy of a live header inside another
allocation, the same at an odd address, a page run whose length is raised
over its neighbour, and a pooled block whose header tag is damaged. Each
is refused and logged, nothing is handed out twice and the neighbouring
memory stays intact; the code before these checks fails the first three.
All of them pass with MEMDBG on as well.
