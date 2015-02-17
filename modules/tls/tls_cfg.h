/* 
 * TLS module
 * 
 * Copyright (C) 2010 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
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
 * TLS runtime configuration.
 * @file tls_cfg.h
 * @ingroup: tls
 * Module: @ref tls
 */

#ifndef __tls_cfg_h
#define __tls_cfg_h

#include "../../str.h"
#include "../../cfg/cfg.h"


/* maximum accepted lifetime (maximum possible is  ~ MAXINT/2)
 *  (it should be kept in sync w/ MAX_TCP_CON_LIFETIME from tcp_main.c:
 *   MAX_TLS_CON_LIFETIME <= MAX_TCP_CON_LIFETIME )*/
#define MAX_TLS_CON_LIFETIME	(1U<<(sizeof(ticks_t)*8-1))



struct cfg_group_tls {
	int force_run;
	str method;
	str server_name;
	int verify_cert;
	int verify_depth;
	int require_cert;
	str private_key;
	str ca_list;
	str crl;
	str certificate;
	str cipher_list;
	int session_cache;
	str session_id;
	str config_file;
	int log;
	int debug;
	int con_lifetime;
	int disable_compression;
	/* release internal openssl read or write buffer when they are no longer
	 * used (complete read or write that does not have to buffer anything).
	 * Should be used together with tls_free_list_max_len. Might have some
	 * performance impact (and extra *malloc pressure), but has also the
	 * potential of saving a lot of memory (at least 32k/idle connection in the
	 * default config, or ~ 16k+tls_max_send_fragment)) */
	int ssl_release_buffers;
	/* maximum length of free/unused memory buffers/chunks per connection.
	 * Setting it to 0 would cause any unused buffers to be immediately freed
	 * and hence a lower memory footprint (at the cost of a possible
	 * performance decrease and more *malloc pressure).
	 * Too large value would result in extra memory consumption.
	 * The default is 32 in openssl.
	 * For lowest memory usage set it to 0 and tls_mode_release_buffers to 1
	 */
	int ssl_freelist_max;
	/* maximum number of bytes (clear text) sent into one record.
	 * The default and maximum value are ~16k. Lower values would lead to a
	 * lower  memory footprint.
	 * Values lower then the typical  app. write size might decrease
	 * performance (extra write() syscalls), so it should be kept ~2k for ser.
	 */
	int ssl_max_send_fragment;
	/* enable read ahead. Should increase performance (1 less syscall when
	 * enabled, else openssl makes 1 read() for each record header and another
	 * for the content), but might interact with SSL_pending() (not used right
	 * now)
	 */
	int ssl_read_ahead;
	int low_mem_threshold1;
	int low_mem_threshold2;
	int ct_wq_max; /* maximum overall tls write clear text queued bytes */
	int con_ct_wq_max; /* maximum clear text write queued bytes per con */
	int ct_wq_blk_size; /* minimum block size for the clear text write queue */
	int send_close_notify; /* if set try to be nice and send a shutdown alert
						    before closing the tcp connection */
};


extern struct cfg_group_tls default_tls_cfg;
extern volatile void* tls_cfg;
extern cfg_def_t tls_cfg_def[];


extern int fix_tls_cfg(struct cfg_group_tls* cfg);

#endif /*__tls_cfg_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
