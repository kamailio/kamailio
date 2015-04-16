/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * \file cr_func.c
 * \brief Routing and balancing functions.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include "cr_func.h"
#include "cr_db.h"
#include "../../sr_module.h"
#include "../../action.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../mem/mem.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "cr_map.h"
#include "cr_rule.h"
#include "cr_domain.h"
#include "cr_carrier.h"
#include "carrierroute.h"
#include "config.h"

#define MAX_DESTINATIONS 64

enum hash_algorithm {
	alg_crc32 = 1, /*!< hashing algorithm is CRC32 */
	alg_crc32_nofallback, /*!< same algorithm as alg_crc32, with only a backup rule, but no fallback tree is chosen
                           if there is something wrong. */
	alg_error
};


static const str SIP_URI  = { .s="sip:",  .len=4 };
static const str SIPS_URI = { .s="sips:", .len=5 };
static const str AT_SIGN  = { .s="@",     .len=1 };
static char g_rewrite_uri[MAX_URI_SIZE+1];

/**
 * Get the id that belongs to a string name from gparam_t structure.
 *
 * Get the id that belongs to a string name from gparam_t structure, use the
 * search_id function for the lookup.
 * @param _msg SIP message
 * @param gp id as integer, pseudo-variable or AVP name of carrier
 * @param map lookup function
 * @param size size of the list
 * @return id on success, -1 otherwise
 */
static inline int cr_gp2id(struct sip_msg *_msg, gparam_t *gp, struct name_map_t *map, int size) {
	int id;
	struct usr_avp *avp;
	int_str avp_val;
	str tmp;

	switch (gp->type) {
	case GPARAM_TYPE_INT:
		return gp->v.i;
		break;
	case GPARAM_TYPE_PVE:
		/* does this PV hold an AVP? */
		if (gp->v.pve->spec->type==PVT_AVP) {
			avp = search_first_avp(gp->v.pve->spec->pvp.pvn.u.isname.type,
						gp->v.pve->spec->pvp.pvn.u.isname.name, &avp_val, 0);
			if (!avp) {
				if(gp->v.pve->spec->pvp.pvn.u.isname.type & AVP_NAME_STR)
					LM_ERR("cannot find AVP '%.*s'\n", gp->v.pve->spec->pvp.pvn.u.isname.name.s.len,
						gp->v.pve->spec->pvp.pvn.u.isname.name.s.s);
				else if(gp->v.pve->spec->pvp.pvn.u.isname.type & AVP_NAME_RE)
					LM_ERR("cannot find AVP regex\n");
				else 	LM_ERR("cannot find AVP '%d'\n", gp->v.pve->spec->pvp.pvn.u.isname.name.n);
				return -1;
			}
			if ((avp->flags&AVP_VAL_STR)==0) {
				return avp_val.n;
			} else {
				id = map_name2id(map, size, &avp_val.s);
				if (id < 0) {
					if(gp->v.pve->spec->pvp.pvn.u.isname.type & AVP_NAME_STR)
						LM_ERR("cannot map carrier with id %.*s from  AVP '%.*s'\n", avp_val.s.len, avp_val.s.s, gp->v.pve->spec->pvp.pvn.u.isname.name.s.len,
							gp->v.pve->spec->pvp.pvn.u.isname.name.s.s);
					else if(gp->v.pve->spec->pvp.pvn.u.isname.type & AVP_NAME_RE)
						LM_ERR("cannot map carrier with id %.*s from  AVP regex\n", avp_val.s.len, avp_val.s.s);
					else 	LM_ERR("cannot map carrier with id %.*s from  AVP '%d'\n", avp_val.s.len, avp_val.s.s, gp->v.pve->spec->pvp.pvn.u.isname.name.n);
					return -1;
				}
				return id;
			}
		} else {
			/* retrieve name from parameter */
			if (fixup_get_svalue(_msg, gp, &tmp)<0) {
				LM_ERR("cannot print the name from PV\n");
				return -1;
			}
			id = map_name2id(map, size, &tmp);
			if (id < 0) {
				LM_ERR("could not find id '%.*s' from PV\n", tmp.len, tmp.s);
				return -1;
			}
			return id;
		}
	default:
		LM_ERR("invalid parameter type\n");
		return -1;
	}
}


/**
 * Try to match the reply code rc to the reply code with wildcards.
 *
 * @param rcw reply code specifier with wildcards
 * @param rc the current reply code
 *
 * @return 0 on match, -1 otherwise
 */
static inline int reply_code_matcher(const str *rcw, const str *rc) {
	int i;
	
	if (rcw->len==0) return 0;
	
	if (rcw->len != rc->len) return -1;
	
	for (i=0; i<rc->len; i++) {
		if (rcw->s[i]!='.' && rcw->s[i]!=rc->s[i]) return -1;
	}
	
	return 0;
}


/**
 * writes the next_domain avp using the rule list of failure_tree
 *
 * @param frr_head the head of the failure route rule list
 * @param host last tried host
 * @param reply_code the last reply code
 * @param flags flags for the failure route rule
 * @param dstavp the name of the AVP where to store the next domain
 *
 * @return 0 on success, -1 on failure
 */
static int set_next_domain_on_rule(struct failure_route_rule *frr_head,
		const str *host, const str *reply_code, const flag_t flags,
		const gparam_t *dstavp) {
	struct failure_route_rule * rr;
	int_str avp_val;
	
	assert(frr_head != NULL);
	
	LM_DBG("searching for matching routing rules");
	for (rr = frr_head; rr != NULL; rr = rr->next) {
		/*
		LM_DBG("rr.flags=%d rr.mask=%d flags=%d\n", rr->flags, rr->mask, flags);
		LM_DBG("rr.host.len=%d host.len=%d\n", rr->host.len, host->len);
		LM_DBG("rr.host.s='%.*s' host.s='%.*s'\n", rr->host.len, rr->host.s, host->len, host->s);
		LM_DBG("rr.reply_code.len=%d reply_code.len=%d\n", rr->reply_code.len, reply_code->len);
		LM_DBG("rr.reply_code.s='%.*s' reply_code.s='%.*s'\n", rr->reply_code.len, rr->reply_code.s, reply_code->len, reply_code->s);
		*/
		if (((rr->mask & flags) == rr->flags) &&
				((rr->host.len == 0) || (str_strcmp(host, &rr->host)==0)) &&
				(reply_code_matcher(&(rr->reply_code), reply_code)==0)) {
			avp_val.n = rr->next_domain;
			if (add_avp(dstavp->v.pve->spec->pvp.pvn.u.isname.type,
					dstavp->v.pve->spec->pvp.pvn.u.isname.name, avp_val)<0) {
				LM_ERR("set AVP failed\n");
				return -1;
			}
			
			LM_INFO("next_domain is %d\n", rr->next_domain);
			return 0;
		}
	}
	
	LM_INFO("no matching rule for (flags=%d, host='%.*s', reply_code='%.*s') found\n", flags, host->len, host->s, reply_code->len, reply_code->s);
	return -1;
}


/**
 * traverses the failure routing tree until a matching rule is found.
 * The longest match is taken, so it is possible to define
 * failure route rules for a single number
 *
 * @param failure_node the current routing tree node
 * @param uri the uri to be rewritten at the current position
 * @param host last tried host
 * @param reply_code the last reply code
 * @param flags flags for the failure route rule
 * @param dstavp the name of the AVP where to store the next domain
 *
 * @return 0 on success, -1 on failure, 1 on no more matching child node and no rule list
 */
static int set_next_domain_recursor(struct dtrie_node_t *failure_node,
		const str *uri, const str *host, const str *reply_code, const flag_t flags,
		const gparam_t *dstavp) {
	str re_uri = *uri;
	void **ret;
	
	/* Skip over non-digits.  */
	while (re_uri.len > 0 && (!isdigit(*re_uri.s) && cr_match_mode == 10)) {
		++re_uri.s;
		--re_uri.len;
	}
	ret = dtrie_longest_match(failure_node, re_uri.s, re_uri.len, NULL, cr_match_mode);

	if (ret == NULL) {
		LM_INFO("URI or prefix tree nodes empty, empty rule list\n");
		return 1;
	}
	else return set_next_domain_on_rule(*ret, host, reply_code, flags, dstavp);
}


/**
 * searches for a rule int rt with hash_index prob - 1
 * If the rule with the desired hash index is deactivated,
 * the backup rule is used if available.
 *
 * @param rf the route_flags node to search for rule
 * @param prob the hash index
 *
 * @return pointer to route rule on success, NULL on failure
 */
static struct route_rule * get_rule_by_hash(const struct route_flags * rf,
		const int prob) {
	struct route_rule * act_hash = NULL;

	if (prob > rf->rule_num) {
		LM_WARN("too large desired hash, taking highest\n");
		act_hash = rf->rules[rf->rule_num - 1];
	}
	else act_hash = rf->rules[prob - 1];

	if (!act_hash->status) {
		if (act_hash->backup && act_hash->backup->rr) {
			act_hash = act_hash->backup->rr;
		} else {
			act_hash = NULL;
		}
	}
	LM_INFO("desired hash was %i, return %i\n", prob, act_hash ? act_hash->hash_index : -1);
	return act_hash;
}

// debug functions for cr_uri_avp
/*
static void print_cr_uri_avp(){
	struct search_state st;
	int_str val;
	int elem = 0;

	if (!search_first_avp( AVP_VAL_STR | AVP_NAME_STR, cr_uris_avp, &val, &st)) {
		LM_DBG("no AVPs - we are done!\n");
		return;
	}

	LM_DBG("	cr_uri_avp[%d]=%.*s\n", elem++, val.s.len, val.s.s);

	while (  search_next_avp(&st, &val) ) {
		LM_DBG("	cr_uri_avp[%d]=%.*s\n", elem++, val.s.len, val.s.s);
	}
}
*/

static void build_used_uris_list(avp_value_t* used_dests, int* no_dests){
	struct search_state st;
	int_str val;
	*no_dests = 0;

	if (!search_first_avp( AVP_VAL_STR | AVP_NAME_STR, cr_uris_avp, &val, &st)) {
		//LM_DBG("no AVPs - we are done!\n");
		return;
	}

	used_dests[(*no_dests)++] = val;
	//LM_DBG("	used_dests[%d]=%.*s \n", (*no_dests)-1, used_dests[(*no_dests)-1].s.len, used_dests[(*no_dests)-1].s.s);

	while ( search_next_avp(&st, &val) ) {
		if ( MAX_DESTINATIONS == *no_dests ) {
			LM_ERR("Too many  AVPs - we are done!\n");
			return;
		}
		used_dests[(*no_dests)++] = val;
		//LM_DBG("	used_dests[%d]=%.*s \n", (*no_dests)-1, used_dests[(*no_dests)-1].s.len, used_dests[(*no_dests)-1].s.s);
	}

	//LM_DBG("sucessfully built used_uris list!\n");
}

int cr_uri_already_used(str dest , avp_value_t* used_dests, int no_dests){
	int i;
	for (i=0; i<no_dests; i++){
		if ( (dest.len == used_dests[i].s.len) &&
				(memcmp(dest.s, used_dests[i].s.s, dest.len)==0)){
			LM_NOTICE("Candidate destination <%.*s> was previously used.\n", dest.len, dest.s);
			return 1;
		}

	}
	//LM_DBG("cr_uri_already_used: Candidate destination <%.*s> was NEVER USED.\n", dest.len, dest.s);
	return 0;
}


/**
 * does the work for rewrite_on_rule, writes the new URI into dest
 *
 * @param rs the route rule used for rewriting, not NULL
 * @param dest the returned new destination URI, not NULL
 * @param msg the sip message, not NULL
 * @param user the localpart of the uri to be rewritten, not NULL
 *
 * @return 0 on success, -1 on failure
 *
 * @see rewrite_on_rule()
 */
static int actually_rewrite(const struct route_rule *rs, str *dest,
		const struct sip_msg *msg, const str * user, gparam_t *descavp) {
	size_t len;
	char *p;
	int_str avp_val;
	int strip = 0;
	str l_user;
	
	if( !rs || !dest || !msg || !user) {
		LM_ERR("NULL parameter\n");
		return -1;
	}
	
	l_user = *user;

	strip = (rs->strip > user->len ? user->len : rs->strip);
	strip = (strip < 0 ? 0 : strip);

	if ( strcmp(user->s, "<null>") == 0 || user->len == 0)
	{
		l_user.s = NULL;
		l_user.len = 0;
		len = rs->host.len;
		strip = 0;
	}
	else{
		len = rs->local_prefix.len + l_user.len + rs->local_suffix.len +
                       AT_SIGN.len + rs->host.len - strip;
	}
	
	if (msg->parsed_uri.type == SIPS_URI_T) {
		len += SIPS_URI.len;
	} else {
		len += SIP_URI.len;
	}
	if ( len > MAX_URI_SIZE ) {
		LM_ERR("Calculated uri size too large: %lu\n", (unsigned long)len);
		return -1;
	}

	dest->s = g_rewrite_uri;
	dest->len = len;
	p = dest->s;
	if (msg->parsed_uri.type == SIPS_URI_T) {
		memcpy(p, SIPS_URI.s, SIPS_URI.len);
		p += SIPS_URI.len;
	} else {
		memcpy(p, SIP_URI.s, SIP_URI.len);
		p += SIP_URI.len;
	}
	if (l_user.len) {
		memcpy(p, rs->local_prefix.s, rs->local_prefix.len);
		p += rs->local_prefix.len;
		memcpy(p, l_user.s + strip, l_user.len - strip);
		p += l_user.len - strip;
		memcpy(p, rs->local_suffix.s, rs->local_suffix.len);
		p += rs->local_suffix.len;
		memcpy(p, AT_SIGN.s, AT_SIGN.len);
		p += AT_SIGN.len;
	}
	/* this could be an error, or a blacklisted destination */
	if (rs->host.len == 0) {
		*p = '\0';
		return -1;
	}
	memcpy(p, rs->host.s, rs->host.len);
	p += rs->host.len;
	*p = '\0';

	if (descavp) {
		avp_val.s = rs->comment;
		if (add_avp(AVP_VAL_STR | descavp->v.pve->spec->pvp.pvn.u.isname.type,
					descavp->v.pve->spec->pvp.pvn.u.isname.name, avp_val)<0) {
			LM_ERR("set AVP failed\n");
			return -1;
		}
	}

	return 0;
}


/**
 * writes the uri dest using the flags and rule list of rf_head
 *
 * @param rf_head the head of the route flags list
 * @param flags user defined flags
 * @param dest the returned new destination URI
 * @param msg the sip message
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 * @param descavp the name of the AVP where the description is stored
 *
 * @return 0 on success, -1 on failure, 1 on empty rule list
 */
static int rewrite_on_rule(struct route_flags *rf_head, flag_t flags, str * dest,
		struct sip_msg * msg, const str * user, const enum hash_source hash_source,
		const enum hash_algorithm alg, gparam_t *descavp) {
	struct route_flags * rf;
	struct route_rule * rr;
	int prob;

	assert(rf_head != NULL);

	LM_DBG("searching for matching routing rules");
	for (rf = rf_head; rf != NULL; rf = rf->next) {
		/* LM_DBG("actual flags %i, searched flags %i, mask %i and match %i", rf->flags, flags, rf->mask, flags&rf->mask); */
		if ((flags&rf->mask) == rf->flags) break;
	}

	if (rf==NULL) {
		LM_INFO("did not find a match for flags %d\n", flags);
		return -1;
	}

	if (rf->rule_list == NULL) {
		LM_INFO("empty rule list\n");
		return 1;
	}

	switch (alg) {
		case alg_crc32:
		{
			static avp_value_t used_dests[MAX_DESTINATIONS];
			static int no_dests = 0;
			avp_value_t cr_new_uri;

			if(rf->dice_max == 0) {
				LM_ERR("invalid dice_max value\n");
				return -1;
			}
			if ((prob = hash_func(msg, hash_source, rf->dice_max)) < 0) {
				LM_ERR("could not hash message with CRC32");
				return -1;
			}

			/* This auto-magically takes the last rule if anything is broken.
			 * Sometimes the hash result is zero. If the first rule is off
			 * (has a probablility of zero) then it has also a dice_to of
			 * zero and the message could not be routed at all if we use
			 * '<' here. Thus the '<=' is necessary.
			 *
			 * cr_uri_already_used is a function that checks that the selected
			 * rule has not been previously used as a failed destinatin
			 */

			for (rr = rf->rule_list;
				rr->next!= NULL && rr->dice_to <= prob ; rr = rr->next) {}

			//LM_DBG("CR: candidate hashed destination is: <%.*s>\n", rr->host.len, rr->host.s);
			if (cr_avoid_failed_dests) {
				if (is_route_type(FAILURE_ROUTE) && (mode == CARRIERROUTE_MODE_DB) ){
					build_used_uris_list(used_dests, &no_dests);

					if (cr_uri_already_used(rr->host, used_dests, no_dests) ) {
						//LM_DBG("CR: selecting new destination !!! \n");
						for (rr = rf->rule_list;
									rr!= NULL && cr_uri_already_used(rr->host, used_dests, no_dests); rr = rr->next) {}
						/* are there any destinations that were not already used? */
						if (rr == NULL) {
							LM_NOTICE("All gateways from this group were already used\n");
							return -1;
						}

						/* this is a hack: we do not take probabilities into consideration if first destination
						 * was previously tried */

						do {
							int rule_no = rand() % rf->rule_num;
							//LM_DBG("CR: trying rule_no=%d \n", rule_no);
							for (rr = rf->rule_list; (rule_no > 0) && (rr->next!=NULL) ; rule_no-- , rr = rr->next) {}
						} while (cr_uri_already_used(rr->host, used_dests, no_dests));
						LM_DBG("CR: candidate selected destination is: <%.*s>\n", rr->host.len, rr->host.s);
					}
				}
			}
			/*This should be regarded as an ELSE branch for the if above
			 * ( status exists for mode == CARRIERROUTE_MODE_FILE */
			if (!rr->status) {
				if (!rr->backup) {
					LM_ERR("all routes are off\n");
					return -1;
				} else {
					if (!rr->backup->rr) {
						LM_ERR("all routes are off\n");
						return -1;
					}
					rr = rr->backup->rr;
				}
			}

			if (cr_avoid_failed_dests) {
				//LM_DBG("CR: destination is: <%.*s>\n", rr->host.len, rr->host.s);
				cr_new_uri.s = rr->host;
				/* insert used destination into avp, in case corresponding request fails and
				 * another destination has to be used; this new destination must not be one
				 * that failed before
				 */

				if (mode == CARRIERROUTE_MODE_DB){
					if ( add_avp( AVP_VAL_STR | AVP_NAME_STR, cr_uris_avp, cr_new_uri) < 0){
						LM_ERR("set AVP failed\n");
						return -1;
					}
					//print_cr_uri_avp();
				}
			}
			break;
		}
		case alg_crc32_nofallback:
			if ((prob = (hash_func(msg, hash_source, rf->max_targets))) < 0) {
				LM_ERR("could not hash message with CRC32");
				return -1;
			}
			/* Instead of search the whole rule_list if there is something broken
			 * this function just tries only a backup rule and otherwise
			 * returns -1. This way we get an error
			 */
			if ((rr = get_rule_by_hash(rf, prob + 1)) == NULL) {
				LM_CRIT("no route found\n");
				return -1;
			}
			break;
		default:
			LM_ERR("invalid hash algorithm\n");
			return -1;
	}
	return actually_rewrite(rr, dest, msg, user, descavp);
}


/**
 * traverses the routing tree until a matching rule is found
 * The longest match is taken, so it is possible to define
 * route rules for a single number
 *
 * @param node the current routing tree node
 * @param pm the user to be used for prefix matching
 * @param flags user defined flags
 * @param dest the returned new destination URI
 * @param msg the sip message
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 * @param descavp the name of the AVP where the description is stored
 *
 * @return 0 on success, -1 on failure, 1 on no more matching child node and no rule list
 */
static int rewrite_uri_recursor(struct dtrie_node_t * node,
		const str * pm, flag_t flags, str * dest, struct sip_msg * msg, const str * user,
		const enum hash_source hash_source, const enum hash_algorithm alg,
		gparam_t *descavp) {
	str re_pm = *pm;
	void **ret;
	
	/* Skip over non-digits.  */
	while (re_pm.len > 0 && (!isdigit(*re_pm.s) && cr_match_mode == 10)) {
		++re_pm.s;
		--re_pm.len;
	}
	ret = dtrie_longest_match(node, re_pm.s, re_pm.len, NULL, cr_match_mode);

	if (ret == NULL) {
		LM_INFO("URI or prefix tree nodes empty, empty rule list\n");
		return 1;
	}
	else return rewrite_on_rule(*ret, flags, dest, msg, user, hash_source, alg, descavp);
}


/**
 * rewrites the request URI of msg after determining the
 * new destination URI
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _rewrite_user the localpart of the URI to be rewritten
 * @param _hsrc the SIP header used for hashing
 * @param _halg the hash algorithm used for hashing
 * @param _dstavp the name of the destination AVP where the used host name is stored
 *
 * @return 1 on success, -1 on failure
 */
int cr_do_route(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc,
		enum hash_algorithm _halg, gparam_t *_dstavp) {

	int carrier_id, domain_id, ret = -1;
	str rewrite_user, prefix_matching, dest;
	flag_t flags;
	struct route_data_t * rd;
	struct carrier_data_t * carrier_data;
	struct domain_data_t * domain_data;
	struct action act;
	struct run_act_ctx ra_ctx;

	if (fixup_get_svalue(_msg, _rewrite_user, &rewrite_user)<0) {
		LM_ERR("cannot print the rewrite_user\n");
		return -1;
	}

	if (fixup_get_svalue(_msg, _prefix_matching, &prefix_matching)<0) {
		LM_ERR("cannot print the prefix_matching\n");
		return -1;
	}

	flags = _msg->flags;

	do {
		rd = get_data();
	} while (rd == NULL);

	carrier_id = cr_gp2id(_msg, _carrier, rd->carrier_map, rd->carrier_num);
	if (carrier_id < 0) {
		LM_ERR("invalid carrier id %d\n", carrier_id);
		release_data(rd);
		return -1;
	}

	domain_id = cr_gp2id(_msg, _domain, rd->domain_map, rd->domain_num);
	if (domain_id < 0) {
		LM_ERR("invalid domain id %d\n", domain_id);
		release_data(rd);
		return -1;
	}
	
	carrier_data=NULL;
	if (carrier_id < 0) {
		if (cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
			LM_NOTICE("invalid tree id %i specified, using default tree\n", carrier_id);
			carrier_data = get_carrier_data(rd, rd->default_carrier_id);
		}
	} else if (carrier_id == 0) {
		carrier_data = get_carrier_data(rd, rd->default_carrier_id);
	} else {
		carrier_data = get_carrier_data(rd, carrier_id);
		if (carrier_data == NULL) {
			if (cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
				LM_NOTICE("invalid tree id %i specified, using default tree\n", carrier_id);
				carrier_data = get_carrier_data(rd, rd->default_carrier_id);
			}
		}
	}
	if (carrier_data == NULL) {
		LM_ERR("cannot get carrier data\n");
		goto unlock_and_out;
	}

	domain_data = get_domain_data(carrier_data, domain_id);
	if (domain_data == NULL) {
		LM_ERR("desired routing domain doesn't exist, prefix %.*s, carrier %d, domain %d\n",
			prefix_matching.len, prefix_matching.s, carrier_id, domain_id);
		goto unlock_and_out;
	}

	if (rewrite_uri_recursor(domain_data->tree, &prefix_matching, flags, &dest, _msg, &rewrite_user, _hsrc, _halg, _dstavp) != 0) {
		/* this is not necessarily an error, rewrite_recursor does already some error logging */
		LM_INFO("rewrite_uri_recursor doesn't complete, uri %.*s, carrier %d, domain %d\n", prefix_matching.len,
			prefix_matching.s, carrier_id, domain_id);
		goto unlock_and_out;
	}

	LM_INFO("uri %.*s was rewritten to %.*s, carrier %d, domain %d\n", rewrite_user.len, rewrite_user.s, dest.len, dest.s, carrier_id, domain_id);

	memset(&act, 0, sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = dest.s;
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, &act, _msg);
	if (ret < 0) {
		LM_ERR("Error in do_action()\n");
	}

unlock_and_out:
	release_data(rd);
	return ret;
}


/**
 * Loads user carrier from subscriber table and stores it in an AVP.
 *
 * @param _msg the current SIP message
 * @param _user the user to determine the carrier data
 * @param _domain the domain to determine the domain data
 * @param _dstavp the name of the AVP where to store the carrier id
 *
 * @return 1 on success, -1 on failure
 */
int cr_load_user_carrier(struct sip_msg * _msg, gparam_t *_user, gparam_t *_domain, gparam_t *_dstavp) {
	str user, domain;
	int_str avp_val;
	
	if (fixup_get_svalue(_msg, _user, &user)<0) {
		LM_ERR("cannot print the user\n");
		return -1;
	}

	if (fixup_get_svalue(_msg, _domain, &domain)<0) {
		LM_ERR("cannot print the domain\n");
		return -1;
	}
	/* get carrier id */
	if ((avp_val.n = load_user_carrier(&user, &domain)) < 0) {
		LM_ERR("error in load user carrier");
		return -1;
	} else {
		/* set avp */
		if (add_avp(_dstavp->v.pve->spec->pvp.pvn.u.isname.type,
					_dstavp->v.pve->spec->pvp.pvn.u.isname.name, avp_val)<0) {
			LM_ERR("add AVP failed\n");
			return -1;
		}
	}
	return 1;
}


/**
 * rewrites the request URI of msg after determining the
 * new destination URI with the crc32 hash algorithm.
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _rewrite_user the localpart of the URI to be rewritten
 * @param _hsrc the SIP header used for hashing
 * @param _descavp the name of the AVP where the description is stored
 *
 * @return 1 on success, -1 on failure
 */
int cr_route(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc,
		gparam_t *_descavp)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching,
		_rewrite_user, _hsrc, alg_crc32, _descavp);
}

int cr_route5(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching,
		_rewrite_user, _hsrc, alg_crc32, NULL);
}


/**
 * rewrites the request URI of msg after determining the
 * new destination URI with the crc32 hash algorithm. The difference
 * to cr_route is that no fallback rule is chosen if there is something
 * wrong (like now obselete cr_prime_route)
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _rewrite_user the localpart of the URI to be rewritten
 * @param _hsrc the SIP header used for hashing
 * @param _dstavp the name of the destination AVP where the used host name is stored
 *
 * @return 1 on success, -1 on failure
 */
int cr_nofallback_route(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc,
		gparam_t *_dstavp)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching,
		_rewrite_user, _hsrc, alg_crc32_nofallback, _dstavp);
}

int cr_nofallback_route5(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching,
		_rewrite_user, _hsrc, alg_crc32_nofallback, NULL);
}


/**
 * Loads next domain from failure routing table and stores it in an AVP.
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _host the host name to be used for rule matching
 * @param _reply_code the reply code to be used for rule matching
 * @param _dstavp the name of the destination AVP
 *
 * @return 1 on success, -1 on failure
 */
int cr_load_next_domain(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_host, gparam_t *_reply_code, gparam_t *_dstavp) {

	int carrier_id, domain_id, ret = -1;
	str prefix_matching, host, reply_code;
	flag_t flags;
	struct route_data_t * rd;
	struct carrier_data_t * carrier_data;
	struct domain_data_t * domain_data;

	if (fixup_get_svalue(_msg, _prefix_matching, &prefix_matching)<0) {
		LM_ERR("cannot print the prefix_matching\n");
		return -1;
	}
	if (fixup_get_svalue(_msg, _host, &host)<0) {
		LM_ERR("cannot print the host\n");
		return -1;
	}
	if (fixup_get_svalue(_msg, _reply_code, &reply_code)<0) {
		LM_ERR("cannot print the reply_code\n");
		return -1;
	}

	flags = _msg->flags;

	do {
		rd = get_data();
	} while (rd == NULL);
	
	carrier_id = cr_gp2id(_msg, _carrier, rd->carrier_map, rd->carrier_num);
	if (carrier_id < 0) {
		LM_ERR("invalid carrier id %d\n", carrier_id);
		release_data(rd);
		return -1;
	}

	domain_id = cr_gp2id(_msg, _domain, rd->domain_map, rd->domain_num);
	if (domain_id < 0) {
		LM_ERR("invalid domain id %d\n", domain_id);
		release_data(rd);
		return -1;
	}

	carrier_data=NULL;
	if (carrier_id < 0) {
		if (cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
			LM_NOTICE("invalid tree id %i specified, using default tree\n", carrier_id);
			carrier_data = get_carrier_data(rd, rd->default_carrier_id);
		}
	} else if (carrier_id == 0) {
		carrier_data = get_carrier_data(rd, rd->default_carrier_id);
	} else {
		carrier_data = get_carrier_data(rd, carrier_id);
		if (carrier_data == NULL) {
			if (cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
				LM_NOTICE("invalid tree id %i specified, using default tree\n", carrier_id);
				carrier_data = get_carrier_data(rd, rd->default_carrier_id);
			}
		}
	}
	if (carrier_data == NULL) {
		LM_ERR("cannot get carrier data\n");
		goto unlock_and_out;
	}

	domain_data = get_domain_data(carrier_data, domain_id);
	if (domain_data == NULL) {
		LM_ERR("desired routing domain doesn't exist, prefix %.*s, carrier %d, domain %d\n",
			prefix_matching.len, prefix_matching.s, carrier_id, domain_id);
		goto unlock_and_out;
	}

	if (set_next_domain_recursor(domain_data->failure_tree, &prefix_matching, &host, &reply_code, flags, _dstavp) != 0) {
		LM_INFO("set_next_domain_recursor doesn't complete, prefix '%.*s', carrier %d, domain %d\n", prefix_matching.len,
			prefix_matching.s, carrier_id, domain_id);
		goto unlock_and_out;
	}
	
	ret = 1;
	
unlock_and_out:
	release_data(rd);
	return ret;
}
