/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Kamailio core :: Daemonize
 * \author andrei
 *      
 * \ingroup core 
 * Module: \ref core                    
 *  
 * 
 */

#ifndef _daemonize_h
#define _daemonize_h

int daemonize(char* name, int daemon_status_fd_input);
int do_suid(void);
int increase_open_fds(int target);
int set_core_dump(int enable, long unsigned int size);
int mem_lock_pages(void);
int set_rt_prio(int prio, int policy);

void daemon_status_init(void);
void daemon_status_on_fork_cleanup(void);
int daemon_status_send(char status);
void daemon_status_no_wait(void);
void daemon_status_on_fork_cleanup(void);

#endif /*_daemonize_h */

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
