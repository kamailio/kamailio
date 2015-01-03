/*
 * Copyright (C) 2005 iptelorg GmbH
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
/*!
* \file
* \brief Kamailio core :: tcp io wait common stuff used by tcp_main.c & tcp_read.c
* \ingroup core
* Module: \ref core
* \author andrei
*
 * All the functions are inline because of speed reasons and because they are
 * used only from 2 places.
 *
 * You also have to define:
 *     int handle_io(struct fd_map* fm, short events, int idx) (see below)
 *     (this could be trivially replaced by a callback pointer entry attached
 *      to the io_wait handler if more flexibility rather then performance
 *      is needed)
 *      fd_type - define to some enum of you choice and define also
 *                FD_TYPE_DEFINED (if you don't do it fd_type will be defined
 *                to int). 0 has a special not set/not init. meaning
 *                (a lot of sanity checks and the sigio_rt code are based on
 *                 this assumption)
 *     local_malloc (defaults to pkg_malloc)
 *     local_free   (defaults to pkg_free)
 *
 */

#ifndef _io_wait_h
#define _io_wait_h

#include <errno.h>
#include <string.h>
#ifdef HAVE_SIGIO_RT
#define __USE_GNU /* or else F_SETSIG won't be included */
#include <sys/types.h> /* recv */
#include <sys/socket.h> /* recv */
#include <signal.h> /* sigprocmask, sigwait a.s.o */
#endif

#define _GNU_SOURCE  /* for POLLRDHUP on linux */
#include <poll.h>
#include <fcntl.h>

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif
#ifdef HAVE_KQUEUE
#include <sys/types.h> /* needed on freebsd */
#include <sys/event.h>
#include <sys/time.h>
#endif
#ifdef HAVE_DEVPOLL
#include <sys/devpoll.h>
#endif
#ifdef HAVE_SELECT
/* needed on openbsd for select*/
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
/* needed according to POSIX for select*/
#include <sys/select.h>
#endif

#include "dprint.h"

#include "poll_types.h" /* poll_types*/
#ifdef HAVE_SIGIO_RT
#include "pt.h" /* mypid() */
#endif

#include "compiler_opt.h"


#ifdef HAVE_EPOLL
/* fix defines for EPOLL */
#if defined POLLRDHUP && ! defined EPOLLRDHUP
#define EPOLLRDHUP POLLRDHUP  /* should work on all linuxes */
#endif /* POLLRDHUP && EPOLLRDHUP */
#endif /* HAVE_EPOLL */


extern int _os_ver; /* os version number, needed to select bugs workarrounds */


#if 0
enum fd_types; /* this should be defined from the including file,
				  see tcp_main.c for an example,
				  0 has a special meaning: not used/empty*/
#endif

#ifndef FD_TYPE_DEFINED
typedef int fd_type;
#define FD_TYPE_DEFINED
#endif

/* maps a fd to some other structure; used in almost all cases
 * except epoll and maybe kqueue or /dev/poll */
struct fd_map{
	int fd;               /* fd no */
	fd_type type;         /* "data" type */
	void* data;           /* pointer to the corresponding structure */
	short events;         /* events we are interested int */
};


#ifdef HAVE_KQUEUE
#ifndef KQ_CHANGES_ARRAY_SIZE
#define KQ_CHANGES_ARRAY_SIZE 256

#ifdef __OS_netbsd
#define KEV_UDATA_CAST (intptr_t)
#else
#define KEV_UDATA_CAST
#endif

#endif
#endif


/* handler structure */
struct io_wait_handler{
	enum poll_types poll_method;
	int flags;
	struct fd_map* fd_hash;
	int fd_no; /*  current index used in fd_array and the passed size for
				   ep_array (for kq_array at least
				    max(twice the size, kq_changes_size) should be
				   be passed). */
	int max_fd_no; /* maximum fd no, is also the size of fd_array,
						       fd_hash  and ep_array*/
	/* common stuff for POLL, SIGIO_RT and SELECT
	 * since poll support is always compiled => this will always be compiled */
	struct pollfd* fd_array; /* used also by devpoll as devpoll array */
	int crt_fd_array_idx; /*  crt idx for which handle_io is called
							 (updated also by del -> internal optimization) */
	/* end of common stuff */
#ifdef HAVE_EPOLL
	int epfd; /* epoll ctrl fd */
	struct epoll_event* ep_array;
#endif
#ifdef HAVE_SIGIO_RT
	sigset_t sset; /* signal mask for sigio & sigrtmin */
	int signo;     /* real time signal used */
#endif
#ifdef HAVE_KQUEUE
	int kq_fd;
	struct kevent* kq_array;   /* used for the eventlist*/
	struct kevent* kq_changes; /* used for the changelist */
	size_t kq_nchanges;
	size_t kq_array_size;   /* array size */
	size_t kq_changes_size; /* size of the changes array */
#endif
#ifdef HAVE_DEVPOLL
	int dpoll_fd;
#endif
#ifdef HAVE_SELECT
	fd_set master_rset; /* read set */
	fd_set master_wset; /* write set */
	int max_fd_select; /* maximum select used fd */
#endif
};

typedef struct io_wait_handler io_wait_h;


/* get the corresponding fd_map structure pointer */
#define get_fd_map(h, fd)		(&(h)->fd_hash[(fd)])
/* remove a fd_map structure from the hash; the pointer must be returned
 * by get_fd_map or hash_fd_map*/
#define unhash_fd_map(pfm)	\
	do{ \
		(pfm)->type=0 /*F_NONE */; \
		(pfm)->fd=-1; \
	}while(0)

/* add a fd_map structure to the fd hash */
static inline struct fd_map* hash_fd_map(	io_wait_h* h,
											int fd,
											short events,
											fd_type type,
											void* data)
{
	h->fd_hash[fd].fd=fd;
	h->fd_hash[fd].events=events;
	h->fd_hash[fd].type=type;
	h->fd_hash[fd].data=data;
	return &h->fd_hash[fd];
}



#ifdef HANDLE_IO_INLINE
/* generic handle io routine, this must be defined in the including file
 * (faster then registering a callback pointer)
 *
 * params:  fm     - pointer to a fd hash entry
 *          events - combinations of POLLIN, POLLOUT, POLLERR & POLLHUP
 *          idx    - index in the fd_array (or -1 if not known)
 * return: -1 on error
 *          0 on EAGAIN or when by some other way it is known that no more
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 */
inline static int handle_io(struct fd_map* fm, short events, int idx);
#else
int handle_io(struct fd_map* fm, short events, int idx);
#endif



#ifdef HAVE_KQUEUE
/*
 * kqueue specific function: register a change
 * (adds a change to the kevent change array, and if full flushes it first)
 *
 * TODO: check if the event already exists in the change list or if it's
 *       complementary to an event in the list (e.g. EVFILT_WRITE, EV_DELETE
 *       and EVFILT_WRITE, EV_ADD for the same fd).
 * returns: -1 on error, 0 on success
 */
static inline int kq_ev_change(io_wait_h* h, int fd, int filter, int flag,
								void* data)
{
	int n;
	int r;
	struct timespec tspec;

	if (h->kq_nchanges>=h->kq_changes_size){
		/* changes array full ! */
		LM_WARN("kqueue changes array full trying to flush...\n");
		tspec.tv_sec=0;
		tspec.tv_nsec=0;
again:
		n=kevent(h->kq_fd, h->kq_changes, h->kq_nchanges, 0, 0, &tspec);
		if (unlikely(n == -1)){
			if (unlikely(errno == EINTR)) goto again;
			else {
				/* for a detailed explanation of what follows see
				   io_wait_loop_kqueue EV_ERROR case */
				if (unlikely(!(errno == EBADF || errno == ENOENT)))
					BUG("kq_ev_change: kevent flush changes failed"
							" (unexpected error): %s [%d]\n",
							strerror(errno), errno);
					/* ignore error even if it's not a EBADF/ENOENT */
				/* one of the file descriptors is bad, probably already
				   closed => try to apply changes one-by-one */
				for (r = 0; r < h->kq_nchanges; r++) {
retry2:
					n = kevent(h->kq_fd, &h->kq_changes[r], 1, 0, 0, &tspec);
					if (n==-1) {
						if (unlikely(errno == EINTR))
							goto retry2;
					/* for a detailed explanation of what follows see
						io_wait_loop_kqueue EV_ERROR case */
						if (unlikely(!(errno == EBADF || errno == ENOENT)))
							BUG("kq_ev_change: kevent flush changes failed:"
									" (unexpected error) %s [%d] (%d/%lu)\n",
										strerror(errno), errno,
										r, (unsigned long)h->kq_nchanges);
						continue; /* skip over it */
					}
				}
			}
		}
		h->kq_nchanges=0; /* changes array is empty */
	}
	EV_SET(&h->kq_changes[h->kq_nchanges], fd, filter, flag, 0, 0,
			KEV_UDATA_CAST data);
	h->kq_nchanges++;
	return 0;
}
#endif



/* generic io_watch_add function
 * Params:
 *     h      - pointer to initialized io_wait handle
 *     fd     - fd to watch
 *     events - bitmap with the fd events for which the fd should be watched
 *              (combination of POLLIN and POLLOUT)
 *     type   - fd type (non 0 value, returned in the call to handle_io)
 *     data   - pointer/private data returned in the handle_io call
 * returns 0 on success, -1 on error
 *
 * WARNING: handle_io() can be called immediately (from io_watch_add()) so
 *  make sure that any dependent init. (e.g. data stuff) is made before
 *  calling io_watch_add
 *
 * this version should be faster than pointers to poll_method specific
 * functions (it avoids functions calls, the overhead being only an extra
 *  switch())*/
inline static int io_watch_add(	io_wait_h* h,
								int fd,
								short events,
								fd_type type,
								void* data)
{

	/* helper macros */
#define fd_array_setup(ev) \
	do{ \
		h->fd_array[h->fd_no].fd=fd; \
		h->fd_array[h->fd_no].events=(ev); /* useless for select */ \
		h->fd_array[h->fd_no].revents=0;     /* useless for select */ \
	}while(0)
	
#define set_fd_flags(f) \
	do{ \
			flags=fcntl(fd, F_GETFL); \
			if (flags==-1){ \
				LM_ERR("fnctl: GETFL failed: %s [%d]\n", \
					strerror(errno), errno); \
				goto error; \
			} \
			if (fcntl(fd, F_SETFL, flags|(f))==-1){ \
				LM_ERR("fnctl: SETFL failed: %s [%d]\n", \
					strerror(errno), errno); \
				goto error; \
			} \
	}while(0)
	
	
	struct fd_map* e;
	int flags;
#ifdef HAVE_EPOLL
	struct epoll_event ep_event;
#endif
#ifdef HAVE_DEVPOLL
	struct pollfd pfd;
#endif
#if defined(HAVE_SIGIO_RT) || defined (HAVE_EPOLL)
	int n;
#endif
#if defined(HAVE_SIGIO_RT)
	int idx;
	int check_io;
	struct pollfd pf;
	
	check_io=0; /* set to 1 if we need to check for pre-existing queued
				   io/data on the fd */
	idx=-1;
#endif
	e=0;
	/* sanity checks */
	if (unlikely(fd==-1)){
		LM_CRIT("fd is -1!\n");
		goto error;
	}
	if (unlikely((events&(POLLIN|POLLOUT))==0)){
		LM_CRIT("invalid events: 0x%0x\n", events);
		goto error;
	}
	/* check if not too big */
	if (unlikely(h->fd_no>=h->max_fd_no)){
		LM_CRIT("maximum fd number exceeded: %d/%d\n", h->fd_no, h->max_fd_no);
		goto error;
	}
	DBG("DBG: io_watch_add(%p, %d, %d, %p), fd_no=%d\n",
			h, fd, type, data, h->fd_no);
	/*  hash sanity check */
	e=get_fd_map(h, fd);
	if (unlikely(e && (e->type!=0 /*F_NONE*/))){
		LM_ERR("trying to overwrite entry %d"
			" watched for %x in the hash(%d, %d, %p) with (%d, %d, %p)\n",
			fd, events, e->fd, e->type, e->data, fd, type, data);
		e=0;
		goto error;
	}
	
	if (unlikely((e=hash_fd_map(h, fd, events, type, data))==0)){
		LM_ERR("failed to hash the fd %d\n", fd);
		goto error;
	}
	switch(h->poll_method){ /* faster then pointer to functions */
		case POLL_POLL:
#ifdef POLLRDHUP
			/* listen to POLLRDHUP by default (if POLLIN) */
			events|=((int)!(events & POLLIN) - 1) & POLLRDHUP;
#endif /* POLLRDHUP */
			fd_array_setup(events);
			set_fd_flags(O_NONBLOCK);
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			fd_array_setup(events);
			if (likely(events & POLLIN))
				FD_SET(fd, &h->master_rset);
			if (unlikely(events & POLLOUT))
				FD_SET(fd, &h->master_wset);
			if (h->max_fd_select<fd) h->max_fd_select=fd;
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			fd_array_setup(events);
			/* re-set O_ASYNC might be needed, if not done from
			 * io_watch_del (or if somebody wants to add a fd which has
			 * already O_ASYNC/F_SETSIG set on a duplicate)
			 */
			/* set async & signal */
			if (fcntl(fd, F_SETOWN, my_pid())==-1){
				LM_ERR("fnctl: SETOWN failed: %s [%d]\n",
					strerror(errno), errno);
				goto error;
			}
			if (fcntl(fd, F_SETSIG, h->signo)==-1){
				LM_ERR("fnctl: SETSIG failed: %s [%d]\n",
					strerror(errno), errno);
				goto error;
			}
			/* set both non-blocking and async */
			set_fd_flags(O_ASYNC| O_NONBLOCK);
#ifdef EXTRA_DEBUG
			DBG("io_watch_add: sigio_rt on f %d, signal %d to pid %d\n",
					fd,  h->signo, my_pid());
#endif
			/* empty socket receive buffer, if buffer is already full
			 * no more space to put packets
			 * => no more signals are ever generated
			 * also when moving fds, the freshly moved fd might have
			 *  already some bytes queued, we want to get them now
			 *  and not later -- andrei */
			idx=h->fd_no;
			check_io=1;
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
			ep_event.events=
#ifdef POLLRDHUP
						/* listen for EPOLLRDHUP too */
						((EPOLLIN|EPOLLRDHUP) & ((int)!(events & POLLIN)-1) ) |
#else /* POLLRDHUP */
						(EPOLLIN & ((int)!(events & POLLIN)-1) ) |
#endif /* POLLRDHUP */
						(EPOLLOUT & ((int)!(events & POLLOUT)-1) );
			ep_event.data.ptr=e;
again1:
			n=epoll_ctl(h->epfd, EPOLL_CTL_ADD, fd, &ep_event);
			if (unlikely(n==-1)){
				if (errno==EAGAIN) goto again1;
				LM_ERR("epoll_ctl failed: %s [%d]\n", strerror(errno), errno);
				goto error;
			}
			break;
		case POLL_EPOLL_ET:
			set_fd_flags(O_NONBLOCK);
			ep_event.events=
#ifdef POLLRDHUP
						/* listen for EPOLLRDHUP too */
						((EPOLLIN|EPOLLRDHUP) & ((int)!(events & POLLIN)-1) ) |
#else /* POLLRDHUP */
						(EPOLLIN & ((int)!(events & POLLIN)-1) ) |
#endif /* POLLRDHUP */
						(EPOLLOUT & ((int)!(events & POLLOUT)-1) ) |
						EPOLLET;
			ep_event.data.ptr=e;
again2:
			n=epoll_ctl(h->epfd, EPOLL_CTL_ADD, fd, &ep_event);
			if (unlikely(n==-1)){
				if (errno==EAGAIN) goto again2;
				LM_ERR("epoll_ctl failed: %s [%d]\n", strerror(errno), errno);
				goto error;
			}
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			if (likely( events & POLLIN)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_READ, EV_ADD, e)==-1))
					goto error;
			}
			if (unlikely( events & POLLOUT)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_WRITE, EV_ADD, e)==-1))
				{
					if (likely(events & POLLIN)){
						kq_ev_change(h, fd, EVFILT_READ, EV_DELETE, 0);
					}
					goto error;
				}
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
			pfd.fd=fd;
			pfd.events=events;
			pfd.revents=0;
again_devpoll:
			if (write(h->dpoll_fd, &pfd, sizeof(pfd))==-1){
				if (errno==EAGAIN) goto again_devpoll;
				LM_ERR("/dev/poll write failed: %s [%d]\n",
					strerror(errno), errno);
				goto error;
			}
			break;
#endif
			
		default:
			LM_CRIT("no support for poll method  %s (%d)\n",
				poll_method_str[h->poll_method], h->poll_method);
			goto error;
	}
	
	h->fd_no++; /* "activate" changes, for epoll/kqueue/devpoll it
				   has only informative value */
#if defined(HAVE_SIGIO_RT)
	if (check_io){
		/* handle possible pre-existing events */
		pf.fd=fd;
		pf.events=events;
check_io_again:
		n=0;
		while(e->type && ((n=poll(&pf, 1, 0))>0) &&
				(handle_io(e, pf.revents, idx)>0) &&
				(pf.revents & (e->events|POLLERR|POLLHUP)));
		if (unlikely(e->type && (n==-1))){
			if (errno==EINTR) goto check_io_again;
			LM_ERR("check_io poll: %s [%d]\n", strerror(errno), errno);
		}
	}
#endif
	return 0;
error:
	if (e) unhash_fd_map(e);
	return -1;
#undef fd_array_setup
#undef set_fd_flags
}



#define IO_FD_CLOSING 16
/* parameters:    h - handler
 *               fd - file descriptor
 *            index - index in the fd_array if known, -1 if not
 *                    (if index==-1 fd_array will be searched for the
 *                     corresponding fd* entry -- slower but unavoidable in
 *                     some cases). index is not used (no fd_array) for epoll,
 *                     /dev/poll and kqueue
 *            flags - optimization flags, e.g. IO_FD_CLOSING, the fd was
 *                    or will shortly be closed, in some cases we can avoid
 *                    extra remove operations (e.g.: epoll, kqueue, sigio)
 * returns 0 if ok, -1 on error */
inline static int io_watch_del(io_wait_h* h, int fd, int idx, int flags)
{
	
#define fix_fd_array \
	do{\
			if (unlikely(idx==-1)){ \
				/* fix idx if -1 and needed */ \
				for (idx=0; (idx<h->fd_no) && \
							(h->fd_array[idx].fd!=fd); idx++); \
			} \
			if (likely(idx<h->fd_no)){ \
				memmove(&h->fd_array[idx], &h->fd_array[idx+1], \
					(h->fd_no-(idx+1))*sizeof(*(h->fd_array))); \
				if ((idx<=h->crt_fd_array_idx) && (h->crt_fd_array_idx>=0)) \
					h->crt_fd_array_idx--; \
			} \
	}while(0)
	
	struct fd_map* e;
	int events;
#ifdef HAVE_EPOLL
	int n;
	struct epoll_event ep_event;
#endif
#ifdef HAVE_DEVPOLL
	struct pollfd pfd;
#endif
#ifdef HAVE_SIGIO_RT
	int fd_flags;
#endif
	
	if (unlikely((fd<0) || (fd>=h->max_fd_no))){
		LM_CRIT("invalid fd %d, not in [0, %d) \n", fd, h->fd_no);
		goto error;
	}
	DBG("DBG: io_watch_del (%p, %d, %d, 0x%x) fd_no=%d called\n",
			h, fd, idx, flags, h->fd_no);
	e=get_fd_map(h, fd);
	/* more sanity checks */
	if (unlikely(e==0)){
		LM_CRIT("no corresponding hash entry for %d\n", fd);
		goto error;
	}
	if (unlikely(e->type==0 /*F_NONE*/)){
		LM_ERR("trying to delete already erased"
			" entry %d in the hash(%d, %d, %p) flags %x)\n",
			fd, e->fd, e->type, e->data, flags);
		goto error;
	}
	events=e->events;
	
	switch(h->poll_method){
		case POLL_POLL:
			fix_fd_array;
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			if (likely(events & POLLIN))
				FD_CLR(fd, &h->master_rset);
			if (unlikely(events & POLLOUT))
				FD_CLR(fd, &h->master_wset);
			if (unlikely(h->max_fd_select && (h->max_fd_select==fd)))
				/* we don't know the prev. max, so we just decrement it */
				h->max_fd_select--;
			fix_fd_array;
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			/* the O_ASYNC flag must be reset all the time, the fd
			 *  can be changed only if  O_ASYNC is reset (if not and
			 *  the fd is a duplicate, you will get signals from the dup. fd
			 *  and not from the original, even if the dup. fd was closed
			 *  and the signals re-set on the original) -- andrei
			 */
			/*if (!(flags & IO_FD_CLOSING)){*/
				/* reset ASYNC */
				fd_flags=fcntl(fd, F_GETFL);
				if (unlikely(fd_flags==-1)){
					LM_ERR("fnctl: GETFL failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
				if (unlikely(fcntl(fd, F_SETFL, fd_flags&(~O_ASYNC))==-1)){
					LM_ERR("fnctl: SETFL failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
			fix_fd_array; /* only on success */
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
		case POLL_EPOLL_ET:
			/* epoll doesn't seem to automatically remove sockets,
			 * if the socket is a duplicate/moved and the original
			 * is still open. The fd is removed from the epoll set
			 * only when the original (and all the  copies?) is/are
			 * closed. This is probably a bug in epoll. --andrei */
#ifdef EPOLL_NO_CLOSE_BUG
			if (!(flags & IO_FD_CLOSING)){
#endif
again_epoll:
				n=epoll_ctl(h->epfd, EPOLL_CTL_DEL, fd, &ep_event);
				if (unlikely(n==-1)){
					if (errno==EAGAIN) goto again_epoll;
					LM_ERR("removing fd from epoll list failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
#ifdef EPOLL_NO_CLOSE_BUG
			}
#endif
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			if (!(flags & IO_FD_CLOSING)){
				if (likely(events & POLLIN)){
					if (unlikely(kq_ev_change(h, fd, EVFILT_READ,
													EV_DELETE, 0) ==-1)){
						/* try to delete the write filter anyway */
						if (events & POLLOUT){
							kq_ev_change(h, fd, EVFILT_WRITE, EV_DELETE, 0);
						}
						goto error;
					}
				}
				if (unlikely(events & POLLOUT)){
					if (unlikely(kq_ev_change(h, fd, EVFILT_WRITE,
													EV_DELETE, 0) ==-1))
						goto error;
				}
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
				/* for /dev/poll the closed fds _must_ be removed
				   (they are not removed automatically on close()) */
				pfd.fd=fd;
				pfd.events=POLLREMOVE;
				pfd.revents=0;
again_devpoll:
				if (write(h->dpoll_fd, &pfd, sizeof(pfd))==-1){
					if (errno==EINTR) goto again_devpoll;
					LM_ERR("removing fd from /dev/poll failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
				break;
#endif
		default:
			LM_CRIT("no support for poll method  %s (%d)\n",
				poll_method_str[h->poll_method], h->poll_method);
			goto error;
	}
	unhash_fd_map(e); /* only on success */
	h->fd_no--;
	return 0;
error:
	return -1;
#undef fix_fd_array
}



/* parameters:    h - handler
 *               fd - file descriptor
 *           events - new events to watch for
 *              idx - index in the fd_array if known, -1 if not
 *                    (if index==-1 fd_array will be searched for the
 *                     corresponding fd* entry -- slower but unavoidable in
 *                     some cases). index is not used (no fd_array) for epoll,
 *                     /dev/poll and kqueue
 * returns 0 if ok, -1 on error */
inline static int io_watch_chg(io_wait_h* h, int fd, short events, int idx )
{
	
#define fd_array_chg(ev) \
	do{\
			if (unlikely(idx==-1)){ \
				/* fix idx if -1 and needed */ \
				for (idx=0; (idx<h->fd_no) && \
							(h->fd_array[idx].fd!=fd); idx++); \
			} \
			if (likely(idx<h->fd_no)){ \
				h->fd_array[idx].events=(ev); \
			} \
	}while(0)
	
	struct fd_map* e;
	int add_events;
	int del_events;
#ifdef HAVE_DEVPOLL
	struct pollfd pfd;
#endif
#ifdef HAVE_EPOLL
	int n;
	struct epoll_event ep_event;
#endif
	
	if (unlikely((fd<0) || (fd>=h->max_fd_no))){
		LM_CRIT("invalid fd %d, not in [0, %d) \n", fd, h->fd_no);
		goto error;
	}
	if (unlikely((events&(POLLIN|POLLOUT))==0)){
		LM_CRIT("invalid events: 0x%0x\n", events);
		goto error;
	}
	DBG("DBG: io_watch_chg (%p, %d, 0x%x, 0x%x) fd_no=%d called\n",
			h, fd, events, idx, h->fd_no);
	e=get_fd_map(h, fd);
	/* more sanity checks */
	if (unlikely(e==0)){
		LM_CRIT("no corresponding hash entry for %d\n", fd);
		goto error;
	}
	if (unlikely(e->type==0 /*F_NONE*/)){
		LM_ERR("trying to change an already erased"
			" entry %d in the hash(%d, %d, %p) )\n",
			fd, e->fd, e->type, e->data);
		goto error;
	}
	
	add_events=events & ~e->events;
	del_events=e->events & ~events;
	switch(h->poll_method){
		case POLL_POLL:
#ifdef POLLRDHUP
			fd_array_chg(events |
							/* listen to POLLRDHUP by default (if POLLIN) */
							(((int)!(events & POLLIN) - 1) & POLLRDHUP)
						);
#else /* POLLRDHUP */
			fd_array_chg(events);
#endif /* POLLRDHUP */
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			fd_array_chg(events);
			if (unlikely(del_events & POLLIN))
				FD_CLR(fd, &h->master_rset);
			else if (unlikely(add_events & POLLIN))
				FD_SET(fd, &h->master_rset);
			if (likely(del_events & POLLOUT))
				FD_CLR(fd, &h->master_wset);
			else if (likely(add_events & POLLOUT))
				FD_SET(fd, &h->master_wset);
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			fd_array_chg(events);
			/* no need for check_io, since SIGIO_RT listens by default for all
			 * the events */
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
				ep_event.events=
#ifdef POLLRDHUP
						/* listen for EPOLLRDHUP too */
						((EPOLLIN|EPOLLRDHUP) & ((int)!(events & POLLIN)-1) ) |
#else /* POLLRDHUP */
						(EPOLLIN & ((int)!(events & POLLIN)-1) ) |
#endif /* POLLRDHUP */
						(EPOLLOUT & ((int)!(events & POLLOUT)-1) );
				ep_event.data.ptr=e;
again_epoll_lt:
				n=epoll_ctl(h->epfd, EPOLL_CTL_MOD, fd, &ep_event);
				if (unlikely(n==-1)){
					if (errno==EAGAIN) goto again_epoll_lt;
					LM_ERR("modifying epoll events failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
			break;
		case POLL_EPOLL_ET:
				ep_event.events=
#ifdef POLLRDHUP
						/* listen for EPOLLRDHUP too */
						((EPOLLIN|EPOLLRDHUP) & ((int)!(events & POLLIN)-1) ) |
#else /* POLLRDHUP */
						(EPOLLIN & ((int)!(events & POLLIN)-1) ) |
#endif /* POLLRDHUP */
						(EPOLLOUT & ((int)!(events & POLLOUT)-1) ) |
						EPOLLET;
				ep_event.data.ptr=e;
again_epoll_et:
				n=epoll_ctl(h->epfd, EPOLL_CTL_MOD, fd, &ep_event);
				if (unlikely(n==-1)){
					if (errno==EAGAIN) goto again_epoll_et;
					LM_ERR("modifying epoll events failed: %s [%d]\n",
						strerror(errno), errno);
					goto error;
				}
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			if (unlikely(del_events & POLLIN)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_READ,
														EV_DELETE, 0) ==-1))
						goto error;
			}else if (unlikely(add_events & POLLIN)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_READ, EV_ADD, e) ==-1))
					goto error;
			}
			if (likely(del_events & POLLOUT)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_WRITE,
														EV_DELETE, 0) ==-1))
						goto error;
			}else if (likely(add_events & POLLOUT)){
				if (unlikely(kq_ev_change(h, fd, EVFILT_WRITE, EV_ADD, e)==-1))
					goto error;
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
				/* for /dev/poll the closed fds _must_ be removed
				   (they are not removed automatically on close()) */
				pfd.fd=fd;
				pfd.events=POLLREMOVE;
				pfd.revents=0;
again_devpoll1:
				if (unlikely(write(h->dpoll_fd, &pfd, sizeof(pfd))==-1)){
					if (errno==EINTR) goto again_devpoll1;
					LM_ERR("removing fd from /dev/poll failed: %s [%d]\n",
								strerror(errno), errno);
					goto error;
				}
again_devpoll2:
				pfd.events=events;
				pfd.revents=0;
				if (unlikely(write(h->dpoll_fd, &pfd, sizeof(pfd))==-1)){
					if (errno==EINTR) goto again_devpoll2;
					LM_ERR("re-adding fd to /dev/poll failed: %s [%d]\n",
								strerror(errno), errno);
					/* error re-adding the fd => mark it as removed/unhash */
					unhash_fd_map(e);
					goto error;
				}
				break;
#endif
		default:
			LM_CRIT("no support for poll method %s (%d)\n",
				poll_method_str[h->poll_method], h->poll_method);
			goto error;
	}
	e->events=events; /* only on success */
	return 0;
error:
	return -1;
#undef fix_fd_array
}



/* io_wait_loop_x style function.
 * wait for io using poll()
 * params: h      - io_wait handle
 *         t      - timeout in s
 *         repeat - if !=0 handle_io will be called until it returns <=0
 * returns: number of IO events handled on success (can be 0), -1 on error
 */
inline static int io_wait_loop_poll(io_wait_h* h, int t, int repeat)
{
	int n, r;
	int ret;
	struct fd_map* fm;
	
again:
		ret=n=poll(h->fd_array, h->fd_no, t*1000);
		if (n==-1){
			if (errno==EINTR) goto again; /* signal, ignore it */
			else{
				LM_ERR("poll: %s [%d]\n", strerror(errno), errno);
				goto error;
			}
		}
		for (r=0; (r<h->fd_no) && n; r++){
			fm=get_fd_map(h, h->fd_array[r].fd);
			if (h->fd_array[r].revents & (fm->events|POLLERR|POLLHUP)){
				n--;
				/* sanity checks */
				if (unlikely((h->fd_array[r].fd >= h->max_fd_no)||
								(h->fd_array[r].fd < 0))){
					LM_CRIT("bad fd %d (no in the 0 - %d range)\n",
							h->fd_array[r].fd, h->max_fd_no);
					/* try to continue anyway */
					h->fd_array[r].events=0; /* clear the events */
					continue;
				}
				h->crt_fd_array_idx=r;
				/* repeat handle_io if repeat, fd still watched (not deleted
				 *  inside handle_io), handle_io returns that there's still
				 *  IO and the fd is still watched for the triggering event */
				while(fm->type &&
						(handle_io(fm, h->fd_array[r].revents, r) > 0) &&
						repeat && ((fm->events|POLLERR|POLLHUP) &
													h->fd_array[r].revents));
				r=h->crt_fd_array_idx; /* can change due to io_watch_del(fd)
										  array shifting */
			}
		}
error:
	return ret;
}



#ifdef HAVE_SELECT
/* wait for io using select */
inline static int io_wait_loop_select(io_wait_h* h, int t, int repeat)
{
	fd_set sel_rset;
	fd_set sel_wset;
	int n, ret;
	struct timeval timeout;
	int r;
	struct fd_map* fm;
	int revents;
	
again:
		sel_rset=h->master_rset;
		sel_wset=h->master_wset;
		timeout.tv_sec=t;
		timeout.tv_usec=0;
		ret=n=select(h->max_fd_select+1, &sel_rset, &sel_wset, 0, &timeout);
		if (n<0){
			if (errno==EINTR) goto again; /* just a signal */
			LM_ERR("select: %s [%d]\n", strerror(errno), errno);
			n=0;
			/* continue */
		}
		/* use poll fd array */
		for(r=0; (r<h->fd_no) && n; r++){
			revents=0;
			if (likely(FD_ISSET(h->fd_array[r].fd, &sel_rset)))
				revents|=POLLIN;
			if (unlikely(FD_ISSET(h->fd_array[r].fd, &sel_wset)))
				revents|=POLLOUT;
			if (unlikely(revents)){
				h->crt_fd_array_idx=r;
				fm=get_fd_map(h, h->fd_array[r].fd);
				while(fm->type && (fm->events & revents) &&
						(handle_io(fm, revents, r)>0) && repeat);
				r=h->crt_fd_array_idx; /* can change due to io_watch_del(fd)
										  array shifting */
				n--;
			}
		};
	return ret;
}
#endif



#ifdef HAVE_EPOLL
inline static int io_wait_loop_epoll(io_wait_h* h, int t, int repeat)
{
	int n, r;
	struct fd_map* fm;
	int revents;
	
again:
		n=epoll_wait(h->epfd, h->ep_array, h->fd_no, t*1000);
		if (unlikely(n==-1)){
			if (errno==EINTR) goto again; /* signal, ignore it */
			else{
				LM_ERR("epoll_wait(%d, %p, %d, %d): %s [%d]\n",
						h->epfd, h->ep_array, h->fd_no, t*1000,
						strerror(errno), errno);
				goto error;
			}
		}
#if 0
		if (n>1){
			for(r=0; r<n; r++){
				LM_ERR("ep_array[%d]= %x, %p\n",
					r, h->ep_array[r].events, h->ep_array[r].data.ptr);
			}
		}
#endif
		for (r=0; r<n; r++){
			revents= (POLLIN & (!(h->ep_array[r].events & (EPOLLIN|EPOLLPRI))
						-1)) |
					 (POLLOUT & (!(h->ep_array[r].events & EPOLLOUT)-1)) |
					 (POLLERR & (!(h->ep_array[r].events & EPOLLERR)-1)) |
					 (POLLHUP & (!(h->ep_array[r].events & EPOLLHUP)-1))
#ifdef POLLRDHUP
					| (POLLRDHUP & (!(h->ep_array[r].events & EPOLLRDHUP)-1))
#endif
					;
			if (likely(revents)){
				fm=(struct fd_map*)h->ep_array[r].data.ptr;
				while(fm->type && ((fm->events|POLLERR|POLLHUP) & revents) &&
						(handle_io(fm, revents, -1)>0) && repeat);
			}else{
				LM_ERR("unexpected event %x on %d/%d, data=%p\n",
					h->ep_array[r].events, r+1, n, h->ep_array[r].data.ptr);
			}
		}
error:
	return n;
}
#endif



#ifdef HAVE_KQUEUE
inline static int io_wait_loop_kqueue(io_wait_h* h, int t, int repeat)
{
	int n, r;
	struct timespec tspec;
	struct fd_map* fm;
	int orig_changes;
	int apply_changes;
	int revents;
	
	tspec.tv_sec=t;
	tspec.tv_nsec=0;
	orig_changes=h->kq_nchanges;
	apply_changes=orig_changes;
	do {
again:
		n=kevent(h->kq_fd, h->kq_changes, apply_changes,  h->kq_array,
					h->kq_array_size, &tspec);
		if (unlikely(n==-1)){
			if (unlikely(errno==EINTR)) goto again; /* signal, ignore it */
			else {
				/* for a detailed explanation of what follows see below
				   the EV_ERROR case */
				if (unlikely(!(errno==EBADF || errno==ENOENT)))
					BUG("io_wait_loop_kqueue: kevent: unexpected error"
						" %s [%d]\n", strerror(errno), errno);
				/* some of the FDs in kq_changes are bad (already closed)
				   and there is not enough space in kq_array to return all
				   of them back */
				apply_changes = h->kq_array_size;
				goto again;
			}
		}
		/* remove applied changes */
		h->kq_nchanges -= apply_changes;
		if (unlikely(apply_changes < orig_changes)) {
			orig_changes -= apply_changes;
			memmove(&h->kq_changes[0], &h->kq_changes[apply_changes],
									sizeof(h->kq_changes[0])*h->kq_nchanges);
			apply_changes = (orig_changes < h->kq_array_size) ? orig_changes :
								h->kq_array_size;
		} else {
			orig_changes = 0;
			apply_changes = 0;
		}
		for (r=0; r<n; r++){
#ifdef EXTRA_DEBUG
			DBG("DBG: kqueue: event %d/%d: fd=%d, udata=%lx, flags=0x%x\n",
					r, n, h->kq_array[r].ident, (long)h->kq_array[r].udata,
					h->kq_array[r].flags);
#endif
			if (unlikely((h->kq_array[r].flags & EV_ERROR) ||
							 h->kq_array[r].udata == 0)){
				/* error in changes: we ignore it if it has to do with a
				   bad fd or update==0. It can be caused by trying to remove an
				   already closed fd: race between adding something to the
				   changes array, close() and applying the changes (EBADF).
				   E.g. for ser tcp: tcp_main sends a fd to child for reading
				    => deletes it from the watched fds => the changes array
					will contain an EV_DELETE for it. Before the changes
					are applied (they are at the end of the main io_wait loop,
					after all the fd events were processed), a CON_ERR sent
					to tcp_main by a sender (send fail) is processed and causes
					the fd to be closed. When the changes are applied =>
					error for the EV_DELETE attempt of a closed fd.
					Something similar can happen when a fd is scheduled
					for removal, is close()'ed before being removed and
					re-opened(a new sock. get the same fd). When the
					watched fd changes will be applied the fd will be valid
					(so no EBADF), but it's not already watch => ENOENT.
					We report a BUG for the other errors (there's nothing
					constructive we can do if we get an error we don't know
					how to handle), but apart from that we ignore it in the
					idea that it is better apply the rest of the changes,
					rather then dropping all of them.
				*/
				/*
					example EV_ERROR for trying to delete a read watched fd,
					that was already closed:
					{
						ident = 63,  [fd]
						filter = -1, [EVFILT_READ]
						flags = 16384, [EV_ERROR]
						fflags = 0,
						data = 9, [errno = EBADF]
						udata = 0x0
					}
				*/
				if (h->kq_array[r].data != EBADF &&
						h->kq_array[r].data != ENOENT)
					BUG("io_wait_loop_kqueue: kevent unexpected error on "
							"fd %ld udata %lx: %s [%ld]\n",
							(long)h->kq_array[r].ident,
							(long)h->kq_array[r].udata,
							strerror(h->kq_array[r].data),
							(long)h->kq_array[r].data);
			}else{
				fm=(struct fd_map*)h->kq_array[r].udata;
				if (likely(h->kq_array[r].filter==EVFILT_READ)){
					revents=POLLIN |
						(((int)!(h->kq_array[r].flags & EV_EOF)-1)&POLLHUP) |
						(((int)!((h->kq_array[r].flags & EV_EOF) &&
								 	h->kq_array[r].fflags != 0) - 1)&POLLERR);
					while(fm->type && (fm->events & revents) &&
							(handle_io(fm, revents, -1)>0) && repeat);
				}else if (h->kq_array[r].filter==EVFILT_WRITE){
					revents=POLLOUT |
						(((int)!(h->kq_array[r].flags & EV_EOF)-1)&POLLHUP) |
						(((int)!((h->kq_array[r].flags & EV_EOF) &&
								 	h->kq_array[r].fflags != 0) - 1)&POLLERR);
					while(fm->type && (fm->events & revents) &&
							(handle_io(fm, revents, -1)>0) && repeat);
				}else{
					BUG("io_wait_loop_kqueue: unknown filter: kqueue: event "
							"%d/%d: fd=%d, filter=%d, flags=0x%x, fflags=0x%x,"
							" data=%lx, udata=%lx\n",
					r, n, (int)h->kq_array[r].ident, (int)h->kq_array[r].filter,
					h->kq_array[r].flags, h->kq_array[r].fflags,
					(unsigned long)h->kq_array[r].data,
					(unsigned long)h->kq_array[r].udata);
				}
			}
		}
	} while(unlikely(orig_changes));
	return n;
}
#endif



#ifdef HAVE_SIGIO_RT
/* sigio rt version has no repeat (it doesn't make sense)*/
inline static int io_wait_loop_sigio_rt(io_wait_h* h, int t)
{
	int n;
	int ret;
	struct timespec ts;
	siginfo_t siginfo;
	int sigio_band;
	int sigio_fd;
	struct fd_map* fm;
	int revents;
#ifdef SIGINFO64_WORKARROUND
	int* pi;
#endif
	
	
	ret=1; /* 1 event per call normally */
	ts.tv_sec=t;
	ts.tv_nsec=0;
	if (unlikely(!sigismember(&h->sset, h->signo) ||
					!sigismember(&h->sset, SIGIO))) {
		LM_CRIT("the signal mask is not properly set!\n");
		goto error;
	}
again:
	n=sigtimedwait(&h->sset, &siginfo, &ts);
	if (unlikely(n==-1)){
		if (errno==EINTR) goto again; /* some other signal, ignore it */
		else if (errno==EAGAIN){ /* timeout */
			ret=0;
			goto end;
		}else{
			LM_ERR("sigtimed_wait %s [%d]\n", strerror(errno), errno);
			goto error;
		}
	}
	if (likely(n!=SIGIO)){
#ifdef SIGINFO64_WORKARROUND
		/* on linux siginfo.si_band is defined as long in userspace
		 * and as int in kernel (< 2.6.5) => on 64 bits things will break!
		 * (si_band will include si_fd, and si_fd will contain
		 *  garbage).
		 *  see /usr/src/linux/include/asm-generic/siginfo.h and
		 *      /usr/include/bits/siginfo.h
		 *  On newer kernels this is fixed (si_band is long in the kernel too).
		 * -- andrei */
		if  ((_os_ver<0x020605) && (sizeof(siginfo.si_band)>sizeof(int))){
			pi=(int*)(void*)&siginfo.si_band; /* avoid type punning warnings */
			sigio_band=*pi;
			sigio_fd=*(pi+1);
		}else
#endif
		{
			sigio_band=siginfo.si_band;
			sigio_fd=siginfo.si_fd;
		}
		if (unlikely(siginfo.si_code==SI_SIGIO)){
			/* old style, we don't know the event (linux 2.2.?) */
			LM_WARN("old style sigio interface\n");
			fm=get_fd_map(h, sigio_fd);
			/* we can have queued signals generated by fds not watched
			 * any more, or by fds in transition, to a child => ignore them*/
			if (fm->type)
				handle_io(fm, POLLIN|POLLOUT, -1);
		}else{
			/* si_code contains the SIGPOLL reason: POLL_IN, POLL_OUT,
			 *  POLL_MSG, POLL_ERR, POLL_PRI or POLL_HUP
			 * and si_band the translated poll event bitmap:
			 *  POLLIN|POLLRDNORM  (=POLL_IN),
			 *  POLLOUT|POLLWRNORM|POLLWRBAND (=POLL_OUT),
			 *  POLLIN|POLLRDNORM|POLLMSG (=POLL_MSG),
			 *  POLLERR (=POLL_ERR),
			 *  POLLPRI|POLLRDBAND (=POLL_PRI),
			 *  POLLHUP|POLLERR (=POLL_HUP)
			 *  [linux 2.6.22 fs/fcntl.c:447]
			 */
#ifdef EXTRA_DEBUG
			DBG("io_wait_loop_sigio_rt: siginfo: signal=%d (%d),"
					" si_code=%d, si_band=0x%x,"
					" si_fd=%d\n",
					siginfo.si_signo, n, siginfo.si_code,
					(unsigned)sigio_band,
					sigio_fd);
#endif
			/* on some errors (e.g. when receving TCP RST), sigio_band will
			 * be set to 0x08 (POLLERR) or 0x18 (POLLERR|POLLHUP - on stream
			 *  unix socket close) , so better catch all events --andrei */
			if (likely(sigio_band)){
				fm=get_fd_map(h, sigio_fd);
				revents=sigio_band;
				/* fix revents==POLLPRI case */
				revents |= (!(revents & POLLPRI)-1) & POLLIN;
				/* we can have queued signals generated by fds not watched
			 	 * any more, or by fds in transition, to a child
				 * => ignore them */
				if (fm->type && ((fm->events|POLLERR|POLLHUP) & revents))
					handle_io(fm, revents, -1);
				else
					DBG("WARNING: io_wait_loop_sigio_rt: ignoring event"
							" %x on fd %d, watching for %x, si_code=%x "
							"(fm->type=%d, fm->fd=%d, fm->data=%p)\n",
							sigio_band, sigio_fd, fm->events, siginfo.si_code,
							fm->type, fm->fd, fm->data);
			}else{
				LM_ERR("unexpected event on fd %d: %x\n", sigio_fd, sigio_band);
			}
		}
	}else{
		/* signal queue overflow
		 * TODO: increase signal queue size: 2.4x /proc/.., 2.6x -rlimits */
		LM_WARN("signal queue overflowed - falling back to poll\n");
		/* clear real-time signal queue
		 * both SIG_IGN and SIG_DFL are needed , it doesn't work
		 * only with SIG_DFL  */
		if (signal(h->signo, SIG_IGN)==SIG_ERR){
			LM_CRIT("do_poll: couldn't reset signal to IGN\n");
		}
		
		if (signal(h->signo, SIG_DFL)==SIG_ERR){
			LM_CRIT("do_poll: couldn't reset signal to DFL\n");
		}
		/* falling back to normal poll */
		ret=io_wait_loop_poll(h, -1, 1);
	}
end:
	return ret;
error:
	return -1;
}
#endif



#ifdef HAVE_DEVPOLL
inline static int io_wait_loop_devpoll(io_wait_h* h, int t, int repeat)
{
	int n, r;
	int ret;
	struct dvpoll dpoll;
	struct fd_map* fm;

		dpoll.dp_timeout=t*1000;
		dpoll.dp_nfds=h->fd_no;
		dpoll.dp_fds=h->fd_array;
again:
		ret=n=ioctl(h->dpoll_fd, DP_POLL, &dpoll);
		if (unlikely(n==-1)){
			if (errno==EINTR) goto again; /* signal, ignore it */
			else{
				LM_ERR("ioctl: %s [%d]\n", strerror(errno), errno);
				goto error;
			}
		}
		for (r=0; r< n; r++){
			if (h->fd_array[r].revents & (POLLNVAL|POLLERR)){
				LM_ERR("pollinval returned for fd %d, revents=%x\n",
					h->fd_array[r].fd, h->fd_array[r].revents);
			}
			/* POLLIN|POLLHUP just go through */
			fm=get_fd_map(h, h->fd_array[r].fd);
			while(fm->type && (fm->events & h->fd_array[r].revents) &&
				(handle_io(fm, h->fd_array[r].revents, r) > 0) && repeat);
		}
error:
	return ret;
}
#endif



/* init */


/* initializes the static vars/arrays
 * params:      h - pointer to the io_wait_h that will be initialized
 *         max_fd - maximum allowed fd number
 *         poll_m - poll method (0 for automatic best fit)
 */
int init_io_wait(io_wait_h* h, int max_fd, enum poll_types poll_method);

/* destroys everything init_io_wait allocated */
void destroy_io_wait(io_wait_h* h);


#endif
