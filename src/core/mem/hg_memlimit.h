/*
 * slab allocator with elastic arenas - host and cgroup memory limits
 *
 * Copyright (C) 2026 Yury Kirsanov
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

#ifndef hg_memlimit_h
#define hg_memlimit_h

/* how much may still be committed: the tighter of the host's room and the
 * memory cgroup's */

/* the floor to keep free on @total_mb MB: a twentieth, at least
 * min(256 MB, an eighth) */
long hg_floor_mb_for(long total_mb);

/* the tightest memory cgroup the calling process is in, for the messages;
 * in MB, which an unsigned long holds on a 32-bit build too */
struct hg_cg_info
{
	int version; /* 1 or 2; 0 = no limited level found */
	unsigned long limit_mb;
	unsigned long used_mb;	/* charged, less the reclaimable file cache */
	unsigned long floor_mb; /* what the floor keeps free */
};

/* room above a floor, in 64-bit bytes on every build; HG_ROOM_ANY: no limit */
#define HG_ROOM_ANY (~0ULL)

/* room left by this process's memory cgroups (tightest level), HG_ROOM_ANY
 * when unlimited or unreadable; @ci describes the binding level */
unsigned long long hg_cgroup_room(struct hg_cg_info *ci);

#endif /* hg_memlimit_h */
