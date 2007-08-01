/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file route_func.h
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief routing and balancing functions
 *
 */

#ifndef SP_ROUTE_ROUTE_FUNC_H
#define SP_ROUTE_ROUTE_FUNC_H

#include "../../parser/msg_parser.h"

/**
 * initialises the hash functions
 *
 * @return 0 on success, -1 on failure
 */
int init_route_funcs();

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * crc32 for hashing. The request URI is used to determine tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_uri(struct sip_msg * msg, char * level_param, char * hash);

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * crc32 for hashing. The request URI is used to determine tree node
 * the given _user is used to determine the routing tree.
 *
 * @param msg the current SIP message
 * @param _user the user to determine the route tree (Request-URI|from_uri|to_uri|avpname)
 * @param _level the requested routing level
 *
 * @return 1 on success, -1 on failure
 */
int user_route_uri(struct sip_msg * msg, char * _user, char * _level);

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * crc32 for hashing. The request URI is used to determine tree node
 * the given _tree is the used routing tree
 *
 * @param msg the current SIP message
 * @param _tree the routing tree to be used
 * @param _level the requested routing level
 *
 * @return 1 on success, -1 on failure
 */
int tree_route_uri(struct sip_msg * msg, char * _tree, char * _level);

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The request URI is used to determine 
 * tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_uri(struct sip_msg * msg, char * level_param, char * hash);

/**
 * rewrites the request URI of msg by calculating a rule, 
 * using crc32 for hashing. The to URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_by_to(struct sip_msg * msg, char * level_param, char * hash);

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The to URI is used to determine 
 * tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_by_to(struct sip_msg * msg, char * level_param, char * hash);

/**
 * rewrites the request URI of msg by calculating a rule, 
 * using crc32 for hashing. The from URI is used to determine
 * tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int route_by_from(struct sip_msg * msg, char * level_param, char * hash);

/**
 * rewrites the request URI of msg by calculating a rule, using 
 * prime number algorithm for hashing, only from_user or to_user
 * are possible values for hash. The from URI is used to determine 
 * tree node
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int prime_balance_by_from(struct sip_msg * msg, char * level_param, char * hash);


// this function is only needed for the 0700 functionality, and obselete
// needs the add-rewrite-branches patch for the rewrite_branches function in core
#ifdef SP_ROUTE2_0700
/**
 * rewrites the request URI and all branches of msg by calculating a rule, using 
 * crc32 for hashing, only The request URI and the branch routes are used to 
 * determine tree node, respectively
 *
 * @param msg the current SIP message
 * @param level_param the requested routing level
 * @param hash the message header used for hashing
 *
 * @return 1 on success, -1 on failure
 */
int rewrite_branches(struct sip_msg * msg, char * level_param, char * unused);
#endif



#endif
