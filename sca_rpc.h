#ifndef SCA_RPC_H
#define SCA_RPC_H

#include "../../rpc.h"
#include "../../rpc_lookup.h"

extern const char	*sca_rpc_show_all_subscriptions_doc[];
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
