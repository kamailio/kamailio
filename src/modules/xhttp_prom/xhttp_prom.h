/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
 *
 * Copyright (C) 2019 Vicente Hernando (Sonoc)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * @file
 * @brief xHTTP_PROM :: General structures and variables.
 * @ingroup xhttp_prom
 * - Module: @ref xhttp_prom
 */

#ifndef _XHTTP_PROM_H
#define _XHTTP_PROM_H

#include "../../core/str.h"
#include "../../core/parser/msg_parser.h"


#define ERROR_REASON_BUF_LEN 1024
#define PRINT_VALUE_BUF_LEN 256


/**
 * @brief Representation of the xhttp_prom reply being constructed.
 *
 * This data structure describes the xhttp_prom reply that is being constructed
 * and will be sent to the client.
 */
struct xhttp_prom_reply
{
	int code;	/**< Reply code which indicates the type of the reply */
	str reason; /**< Reason phrase text which provides human-readable
			 * description that augments the reply code */
	str body;	/**< The xhttp_prom http body built so far */
	str buf;	/**< The memory buffer allocated for the reply, this is
			 * where the body attribute of the structure points to */
};


/**
 * @brief The context of the xhttp_prom request being processed.
 *
 * This is the data structure that contains all data related to the xhttp_prom
 * request being processed, such as the reply code and reason, data to be sent
 * to the client in the reply, and so on.
 *
 * There is always one context per xhttp_prom request.
 */
typedef struct prom_ctx
{
	sip_msg_t *msg; /**< The SIP/HTTP received message. */
	struct xhttp_prom_reply
			reply; /**< xhttp_prom reply to be sent to the client */
	int reply_sent;
} prom_ctx_t;

/**
 * @brief string for beginning of metrics.
 */
extern str xhttp_prom_beginning;

/**
 * @brief timeout in minutes to delete old metrics.
 */
extern int timeout_minutes;

#endif /* _XHTTP_PROM_H */
