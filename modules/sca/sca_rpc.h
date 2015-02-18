/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */
#ifndef SCA_RPC_H
#define SCA_RPC_H

#include "../../rpc.h"
#include "../../rpc_lookup.h"

extern const char	*sca_rpc_show_all_subscriptions_doc[];
extern const char	*sca_rpc_subscription_count_doc[];
extern const char	*sca_rpc_show_subscription_doc[];
extern const char	*sca_rpc_show_subscribers_doc[];
extern const char	*sca_rpc_deactivate_all_subscriptions_doc[];
extern const char	*sca_rpc_deactivate_subscription_doc[];
extern const char	*sca_rpc_show_all_appearances_doc[];
extern const char	*sca_rpc_show_appearance_doc[];
extern const char	*sca_rpc_seize_appearance_doc[];
extern const char	*sca_rpc_update_appearance_doc[];
extern const char	*sca_rpc_release_appearance_doc[];

void	sca_rpc_show_all_subscriptions( rpc_t *, void * );
void	sca_rpc_subscription_count( rpc_t *, void * );
void	sca_rpc_show_subscription( rpc_t *, void * );
void	sca_rpc_show_subscribers( rpc_t *, void * );
void	sca_rpc_deactivate_all_subscriptions( rpc_t *, void * );
void	sca_rpc_deactivate_subscription( rpc_t *, void * );
void	sca_rpc_show_all_appearances( rpc_t *, void * );
void	sca_rpc_show_appearance( rpc_t *, void * );
void	sca_rpc_seize_appearance( rpc_t *, void * );
void	sca_rpc_update_appearance( rpc_t *, void * );
void	sca_rpc_release_appearance( rpc_t *, void * );

#endif /* SCA_RPC_H */
