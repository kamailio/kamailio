/*
 * TLS module
 *
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

#include "../../core/dprint.h"
#include "tls_verify.h"

/*!
 * \file
 * \brief Kamailio TLS support :: Certificate verification
 * \ingroup tls
 * Module: \ref tls
 */


/* FIXME: remove this and use the value in domains instead */
#define VERIFY_DEPTH_S 3

int verify_callback_unconditional_success(
		int pre_verify_ok, WOLFSSL_X509_STORE_CTX *ctx)
{
	LM_NOTICE("Post-verification callback: unconditional success\n");
	return 1;
}
