/*
 * Copyright (C) 2007-2008 1&1 Internet AG
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
#include "../../core/sr_module.h"
#include "../../core/action.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/ut.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/hf.h"
#include "../../core/mem/mem.h"
#include "../../core/qvalue.h"
#include "../../core/dset.h"
#include "../../core/rand/kam_rand.h"
#include "../../core/lvalue.h"
#include "cr_map.h"
#include "cr_rule.h"
#include "cr_domain.h"
#include "cr_carrier.h"
#include "prime_hash.h"
#include "carrierroute.h"
#include "config.h"

#define MAX_DESTINATIONS 64

enum hash_algorithm
{
	alg_crc32 = 1,		  /*!< hashing algorithm is CRC32 */
	alg_crc32_nofallback, /*!< same algorithm as alg_crc32, with only a backup rule, but no fallback tree is chosen
                           if there is something wrong. */
	alg_error
};


static const str SIP_URI = {.s = "sip:", .len = 4};
static const str SIPS_URI = {.s = "sips:", .len = 5};
static const str AT_SIGN = {.s = "@", .len = 1};
static char g_rewrite_uri[MAX_URI_SIZE + 1];


/**
 *
 */
static inline int cr_str2id(str *ss, struct name_map_t *map, int size)
{
	int id;

	if(str2sint(ss, &id) != 0) {
		id = map_name2id(map, size, ss);
		if(id < 0) {
			LM_ERR("could not find id '%.*s' from name\n", ss->len, ss->s);
			return -1;
		}
	}
	return id;
}


/**
 *
 */
static enum hash_source get_hash_source(str *_hsrc)
{
	enum hash_source my_hash_source = shs_error;

	if(strcasecmp("call_id", _hsrc->s) == 0) {
		my_hash_source = shs_call_id;
	} else if(strcasecmp("from_uri", _hsrc->s) == 0) {
		my_hash_source = shs_from_uri;
	} else if(strcasecmp("from_user", _hsrc->s) == 0) {
		my_hash_source = shs_from_user;
	} else if(strcasecmp("to_uri", _hsrc->s) == 0) {
		my_hash_source = shs_to_uri;
	} else if(strcasecmp("to_user", _hsrc->s) == 0) {
		my_hash_source = shs_to_user;
	} else if(strcasecmp("rand", _hsrc->s) == 0) {
		my_hash_source = shs_rand;
	} else {
		LM_ERR("invalid hash source\n");
	}

	return my_hash_source;
}


/**
 * Try to match the reply code rc to the reply code with wildcards.
 *
 * @param rcw reply code specifier with wildcards
 * @param rc the current reply code
 *
 * @return 0 on match, -1 otherwise
 */
static inline int reply_code_matcher(const str *rcw, const str *rc)
{
	int i;

	if(rcw->len == 0)
		return 0;

	if(rcw->len != rc->len)
		return -1;

	for(i = 0; i < rc->len; i++) {
		if(rcw->s[i] != '.' && rcw->s[i] != rc->s[i])
			return -1;
	}

	return 0;
}


/**
 * writes the next_domain avp using the rule list of failure_tree
 *
 * @param _msg SIP message
 * @param frr_head the head of the failure route rule list
 * @param host last tried host
 * @param reply_code the last reply code
 * @param flags flags for the failure route rule
 * @param dstavp the name of the AVP where to store the next domain
 *
 * @return 0 on success, -1 on failure
 */
static int set_next_domain_on_rule(sip_msg_t *_msg,
		struct failure_route_rule *frr_head, const str *host,
		const str *reply_code, const flag_t flags, pv_spec_t *dstavp)
{

	struct failure_route_rule *rr;
	pv_value_t val = {0};
	assert(frr_head != NULL);

	LM_DBG("searching for matching routing rules");
	for(rr = frr_head; rr != NULL; rr = rr->next) {
		/*
		LM_DBG("rr.flags=%d rr.mask=%d flags=%d\n", rr->flags, rr->mask, flags);
		LM_DBG("rr.host.len=%d host.len=%d\n", rr->host.len, host->len);
		LM_DBG("rr.host.s='%.*s' host.s='%.*s'\n", rr->host.len, rr->host.s, host->len, host->s);
		LM_DBG("rr.reply_code.len=%d reply_code.len=%d\n", rr->reply_code.len, reply_code->len);
		LM_DBG("rr.reply_code.s='%.*s' reply_code.s='%.*s'\n", rr->reply_code.len, rr->reply_code.s, reply_code->len, reply_code->s);
		*/
		if(((rr->mask & flags) == rr->flags)
				&& ((rr->host.len == 0) || (str_strcmp(host, &rr->host) == 0))
				&& (reply_code_matcher(&(rr->reply_code), reply_code) == 0)) {
			val.ri = rr->next_domain;

			/* set var */
			val.flags = PV_VAL_INT | PV_TYPE_INT;
			if(dstavp->setf(_msg, &dstavp->pvp, (int)EQ_T, &val) < 0) {
				LM_ERR("failed setting next domain id\n");
				return -1;
			}

			LM_INFO("next_domain is %d\n", rr->next_domain);
			return 0;
		}
	}

	LM_INFO("no matching rule for (flags=%d, host='%.*s', reply_code='%.*s') "
			"found\n",
			flags, host->len, host->s, reply_code->len, reply_code->s);
	return -1;
}


/**
 * traverses the failure routing tree until a matching rule is found.
 * The longest match is taken, so it is possible to define
 * failure route rules for a single number
 *
 * @param _msg SIP message
 * @param failure_node the current routing tree node
 * @param uri the uri to be rewritten at the current position
 * @param host last tried host
 * @param reply_code the last reply code
 * @param flags flags for the failure route rule
 * @param dstavp the name of the AVP where to store the next domain
 *
 * @return 0 on success, -1 on failure, 1 on no more matching child node and no rule list
 */
static int set_next_domain_recursor(sip_msg_t *_msg,
		struct dtrie_node_t *failure_node, const str *uri, const str *host,
		const str *reply_code, const flag_t flags, pv_spec_t *dstavp)
{
	str re_uri = *uri;
	void **ret;

	/* Skip over non-digits.  */
	while(re_uri.len > 0 && (!isdigit(*re_uri.s) && cr_match_mode == 10)) {
		++re_uri.s;
		--re_uri.len;
	}
	ret = dtrie_longest_match(
			failure_node, re_uri.s, re_uri.len, NULL, cr_match_mode);

	if(ret == NULL) {
		LM_INFO("URI or prefix tree nodes empty, empty rule list\n");
		return 1;
	}

	return set_next_domain_on_rule(_msg, *ret, host, reply_code, flags, dstavp);
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
static struct route_rule *get_rule_by_hash(
		const struct route_flags *rf, const int prob)
{
	struct route_rule *act_hash = NULL;

	if(prob > rf->rule_num) {
		LM_WARN("too large desired hash, taking highest\n");
		act_hash = rf->rules[rf->rule_num - 1];
	} else
		act_hash = rf->rules[prob - 1];

	if(!act_hash->status) {
		if(act_hash->backup && act_hash->backup->rr) {
			act_hash = act_hash->backup->rr;
		} else {
			act_hash = NULL;
		}
	}
	LM_INFO("desired hash was %i, return %i\n", prob,
			act_hash ? act_hash->hash_index : -1);
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

static void build_used_uris_list(avp_value_t *used_dests, int *no_dests)
{
	struct search_state st;
	int_str val;
	*no_dests = 0;

	if(!search_first_avp(AVP_VAL_STR | AVP_NAME_STR, cr_uris_avp, &val, &st)) {
		//LM_DBG("no AVPs - we are done!\n");
		return;
	}

	used_dests[(*no_dests)++] = val;
	//LM_DBG("	used_dests[%d]=%.*s \n", (*no_dests)-1, used_dests[(*no_dests)-1].s.len, used_dests[(*no_dests)-1].s.s);

	while(search_next_avp(&st, &val)) {
		if(MAX_DESTINATIONS == *no_dests) {
			LM_ERR("Too many  AVPs - we are done!\n");
			return;
		}
		used_dests[(*no_dests)++] = val;
		//LM_DBG("	used_dests[%d]=%.*s \n", (*no_dests)-1, used_dests[(*no_dests)-1].s.len, used_dests[(*no_dests)-1].s.s);
	}

	//LM_DBG("successfully built used_uris list!\n");
}

int cr_uri_already_used(str dest, avp_value_t *used_dests, int no_dests)
{
	int i;
	for(i = 0; i < no_dests; i++) {
		if((dest.len == used_dests[i].s.len)
				&& (memcmp(dest.s, used_dests[i].s.s, dest.len) == 0)) {
			LM_NOTICE("Candidate destination <%.*s> was previously used.\n",
					dest.len, dest.s);
			return 1;
		}
	}
	//LM_DBG("cr_uri_already_used: Candidate destination <%.*s> was NEVER USED.\n", dest.len, dest.s);
	return 0;
}


/**
 * does the work for rewrite_on_rule, writes the new URI into dest
 *
 * @param msg the sip message, not NULL
 * @param rs the route rule used for rewriting, not NULL
 * @param dest the returned new destination URI, not NULL
 * @param user the localpart of the uri to be rewritten, not NULL
 *
 * @return 0 on success, -1 on failure
 *
 * @see rewrite_on_rule()
 */
static int actually_rewrite(sip_msg_t *msg, const struct route_rule *rs,
		str *dest, const str *user, pv_spec_t *descavp)
{
	size_t len;
	char *p;

	pv_value_t val = {0};
	int strip = 0;
	str l_user;

	if(!rs || !dest || !msg || !user) {
		LM_ERR("NULL parameter\n");
		return -1;
	}

	l_user = *user;

	strip = (rs->strip > user->len ? user->len : rs->strip);
	strip = (strip < 0 ? 0 : strip);

	if(strcmp(user->s, "<null>") == 0 || user->len == 0) {
		l_user.s = NULL;
		l_user.len = 0;
		len = rs->host.len;
		strip = 0;
	} else {
		len = rs->local_prefix.len + l_user.len + rs->local_suffix.len
			  + AT_SIGN.len + rs->host.len - strip;
	}

	if(msg->parsed_uri.type == SIPS_URI_T) {
		len += SIPS_URI.len;
	} else {
		len += SIP_URI.len;
	}
	if(len > MAX_URI_SIZE) {
		LM_ERR("Calculated uri size too large: %lu\n", (unsigned long)len);
		return -1;
	}

	dest->s = g_rewrite_uri;
	dest->len = len;
	p = dest->s;
	if(msg->parsed_uri.type == SIPS_URI_T) {
		memcpy(p, SIPS_URI.s, SIPS_URI.len);
		p += SIPS_URI.len;
	} else {
		memcpy(p, SIP_URI.s, SIP_URI.len);
		p += SIP_URI.len;
	}
	if(l_user.len) {
		memcpy(p, rs->local_prefix.s, rs->local_prefix.len);
		p += rs->local_prefix.len;
		memcpy(p, l_user.s + strip, l_user.len - strip);
		p += l_user.len - strip;
		memcpy(p, rs->local_suffix.s, rs->local_suffix.len);
		p += rs->local_suffix.len;
		memcpy(p, AT_SIGN.s, AT_SIGN.len);
		p += AT_SIGN.len;
	}
	/* this could be an error, or a blocklisted destination */
	if(rs->host.len == 0) {
		*p = '\0';
		return -1;
	}
	memcpy(p, rs->host.s, rs->host.len);
	p += rs->host.len;
	*p = '\0';

	if(descavp) {
		val.rs = rs->comment;
		val.flags = PV_VAL_STR;
		if(descavp->setf(msg, &descavp->pvp, (int)EQ_T, &val) < 0) {
			LM_ERR("set AVP failed\n");
			return -1;
		}
	}

	return 0;
}


/**
 * writes the uri dest using the flags and rule list of rf_head
 *
 * @param msg the sip message
 * @param rf_head the head of the route flags list
 * @param flags user defined flags
 * @param dest the returned new destination URI
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 * @param descavp the name of the AVP where the description is stored
 *
 * @return 0 on success, -1 on failure, 1 on empty rule list
 */
static int rewrite_on_rule(sip_msg_t *msg, struct route_flags *rf_head,
		flag_t flags, str *dest, const str *user,
		const enum hash_source hash_source, const enum hash_algorithm alg,
		pv_spec_t *descavp)
{
	struct route_flags *rf;
	struct route_rule *rr;
	int prob;

	assert(rf_head != NULL);

	LM_DBG("searching for matching routing rules");
	for(rf = rf_head; rf != NULL; rf = rf->next) {
		/* LM_DBG("actual flags %i, searched flags %i, mask %i and match %i", rf->flags, flags, rf->mask, flags&rf->mask); */
		if((flags & rf->mask) == rf->flags)
			break;
	}

	if(rf == NULL) {
		LM_INFO("did not find a match for flags %d\n", flags);
		return -1;
	}

	if(rf->rule_list == NULL) {
		LM_INFO("empty rule list\n");
		return 1;
	}

	switch(alg) {
		case alg_crc32: {
			static avp_value_t used_dests[MAX_DESTINATIONS];
			static int no_dests = 0;
			avp_value_t cr_new_uri;

			if(rf->dice_max == 0) {
				LM_ERR("invalid dice_max value (route has probability 0)\n");
				return -1;
			}
			if((prob = hash_func(msg, hash_source, rf->dice_max)) < 0) {
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

			for(rr = rf->rule_list; rr->next != NULL && rr->dice_to <= prob;
					rr = rr->next) {
			}

			//LM_DBG("CR: candidate hashed destination is: <%.*s>\n", rr->host.len, rr->host.s);
			if(cr_avoid_failed_dests) {
				if(is_route_type(FAILURE_ROUTE)
						&& (mode == CARRIERROUTE_MODE_DB)) {
					build_used_uris_list(used_dests, &no_dests);

					if(cr_uri_already_used(rr->host, used_dests, no_dests)) {
						//LM_DBG("CR: selecting new destination !!! \n");
						for(rr = rf->rule_list; rr != NULL
												&& cr_uri_already_used(rr->host,
														used_dests, no_dests);
								rr = rr->next) {
						}
						/* are there any destinations that were not already used? */
						if(rr == NULL) {
							LM_NOTICE("All gateways from this group were "
									  "already used\n");
							return -1;
						}

						/* this is a hack: we do not take probabilities into consideration if first destination
						 * was previously tried */

						do {
							int rule_no = kam_rand() % rf->rule_num;
							//LM_DBG("CR: trying rule_no=%d \n", rule_no);
							for(rr = rf->rule_list;
									(rule_no > 0) && (rr->next != NULL);
									rule_no--, rr = rr->next) {
							}
						} while(cr_uri_already_used(
								rr->host, used_dests, no_dests));
						LM_DBG("CR: candidate selected destination is: "
							   "<%.*s>\n",
								rr->host.len, rr->host.s);
					}
				}
			}
			/*This should be regarded as an ELSE branch for the if above
			 * ( status exists for mode == CARRIERROUTE_MODE_FILE */
			if(!rr->status) {
				if(!rr->backup) {
					LM_ERR("all routes are off\n");
					return -1;
				} else {
					if(!rr->backup->rr) {
						LM_ERR("all routes are off\n");
						return -1;
					}
					rr = rr->backup->rr;
				}
			}

			if(cr_avoid_failed_dests) {
				//LM_DBG("CR: destination is: <%.*s>\n", rr->host.len, rr->host.s);
				cr_new_uri.s = rr->host;
				/* insert used destination into avp, in case corresponding request fails and
				 * another destination has to be used; this new destination must not be one
				 * that failed before
				 */

				if(mode == CARRIERROUTE_MODE_DB) {
					if(add_avp(AVP_VAL_STR | AVP_NAME_STR, cr_uris_avp,
							   cr_new_uri)
							< 0) {
						LM_ERR("set AVP failed\n");
						return -1;
					}
					//print_cr_uri_avp();
				}
			}
			break;
		}
		case alg_crc32_nofallback:
			if((prob = (hash_func(msg, hash_source, rf->max_targets))) < 0) {
				LM_ERR("could not hash message with CRC32");
				return -1;
			}
			/* Instead of search the whole rule_list if there is something broken
			 * this function just tries only a backup rule and otherwise
			 * returns -1. This way we get an error
			 */
			if((rr = get_rule_by_hash(rf, prob + 1)) == NULL) {
				LM_ERR("no route found\n");
				return -1;
			}
			break;
		default:
			LM_ERR("invalid hash algorithm\n");
			return -1;
	}
	return actually_rewrite(msg, rr, dest, user, descavp);
}


/**
 * traverses the routing tree until a matching rule is found
 * The longest match is taken, so it is possible to define
 * route rules for a single number
 *
 * @param msg the sip message
 * @param node the current routing tree node
 * @param pm the user to be used for prefix matching
 * @param flags user defined flags
 * @param dest the returned new destination URI
 * @param user the localpart of the uri to be rewritten
 * @param hash_source the SIP header used for hashing
 * @param alg the algorithm used for hashing
 * @param descavp the name of the AVP where the description is stored
 *
 * @return 0 on success, -1 on failure, 1 on no more matching child node and no rule list
 */
static int rewrite_uri_recursor(sip_msg_t *msg, struct dtrie_node_t *node,
		const str *pm, flag_t flags, str *dest, const str *user,
		const enum hash_source hash_source, const enum hash_algorithm alg,
		pv_spec_t *descavp)
{
	str re_pm = *pm;
	void **ret;

	/* Skip over non-digits.  */
	while(re_pm.len > 0 && (!isdigit(*re_pm.s) && cr_match_mode == 10)) {
		++re_pm.s;
		--re_pm.len;
	}
	ret = dtrie_longest_match(node, re_pm.s, re_pm.len, NULL, cr_match_mode);

	if(ret == NULL) {
		LM_INFO("URI or prefix tree nodes empty, empty rule list\n");
		return 1;
	}

	return rewrite_on_rule(
			msg, *ret, flags, dest, user, hash_source, alg, descavp);
}


/**
 *
 */
int ki_cr_do_route_helper(sip_msg_t *_msg, struct route_data_t *_rd,
		int _carrier, int _domain, str *_prefix_matching, str *_rewrite_user,
		enum hash_source _hsrc, enum hash_algorithm _halg, pv_spec_t *_dstavp)
{

	int ret;
	str dest;

	flag_t flags;
	struct domain_data_t *domain_data;
	struct carrier_data_t *carrier_data = NULL;

	struct action act;
	struct run_act_ctx ra_ctx;

	ret = -1;
	flags = _msg->flags;

	if(_carrier == 0) {
		carrier_data = get_carrier_data(_rd, _rd->default_carrier_id);
	} else {
		carrier_data = get_carrier_data(_rd, _carrier);
		if(carrier_data == NULL) {
			if(cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
				LM_NOTICE("invalid tree id %i specified, using default tree\n",
						_carrier);
				carrier_data = get_carrier_data(_rd, _rd->default_carrier_id);
			}
		}
	}
	if(carrier_data == NULL) {
		LM_ERR("cannot get carrier data\n");
		return -1;
	}

	domain_data = get_domain_data(carrier_data, _domain);
	if(domain_data == NULL) {
		LM_ERR("desired routing domain doesn't exist, prefix %.*s, carrier %d, "
			   "domain %d\n",
				_prefix_matching->len, _prefix_matching->s, _carrier, _domain);
		return -1;
	}

	ret = rewrite_uri_recursor(_msg, domain_data->tree, _prefix_matching, flags,
			&dest, _rewrite_user, _hsrc, _halg, _dstavp);

	if(ret != 0) {
		/* this is not necessarily an error, rewrite_recursor does already some error logging */
		LM_INFO("rewrite_uri_recursor doesn't complete, uri %.*s, carrier %d, "
				"domain %d\n",
				_prefix_matching->len, _prefix_matching->s, _carrier, _domain);
		return -1;
	}

	LM_INFO("uri %.*s was rewritten to %.*s, carrier %d, domain %d\n",
			_rewrite_user->len, _rewrite_user->s, dest.len, dest.s, _carrier,
			_domain);

	memset(&act, 0, sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = dest.s;
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, &act, _msg);
	if(ret < 0) {
		LM_ERR("Error in do_action()\n");
	}

	return ret;
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
int cr_do_route(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_rewrite_user, enum hash_source _hsrc,
		enum hash_algorithm _halg, char *_dstavp)
{
	int carrier_id, domain_id, ret = -1;
	str carrier, domain, rewrite_user, prefix_matching;

	struct route_data_t *rd;
	pv_spec_t *dstavp;

	if(fixup_get_svalue(_msg, (gparam_t *)_rewrite_user, &rewrite_user) < 0) {
		LM_ERR("cannot print the rewrite_user\n");
		return -1;
	}

	if(fixup_get_svalue(_msg, (gparam_t *)_prefix_matching, &prefix_matching)
			< 0) {
		LM_ERR("cannot print the prefix_matching\n");
		return -1;
	}

	dstavp = (pv_spec_t *)_dstavp;

	do {
		rd = get_data();
	} while(rd == NULL);

	if(get_str_fparam(&carrier, _msg, (fparam_t *)_carrier) < 0) {
		if(get_int_fparam(&carrier_id, _msg, (fparam_t *)_carrier) < 0) {
			LM_ERR("cannot print the carrier\n");
			goto unlock_and_out;
		}
	} else {
		carrier_id = cr_str2id(&carrier, rd->carrier_map, rd->carrier_num);
	}

	if(get_str_fparam(&domain, _msg, (fparam_t *)_domain) < 0) {
		if(get_int_fparam(&domain_id, _msg, (fparam_t *)_domain) < 0) {
			LM_ERR("cannot print the domain\n");
			goto unlock_and_out;
		}
	} else {
		domain_id = cr_str2id(&domain, rd->domain_map, rd->domain_num);
	}

	ret = ki_cr_do_route_helper(_msg, rd, carrier_id, domain_id,
			&prefix_matching, &rewrite_user, _hsrc, _halg, dstavp);

unlock_and_out:
	release_data(rd);
	return ret;
}


/**
 *
 */
int ki_cr_load_user_carrier_helper(
		sip_msg_t *_msg, str *user, str *domain, pv_spec_t *dvar)
{
	pv_value_t val = {0};

	/* get carrier id */
	if((val.ri = load_user_carrier(user, domain)) < 0) {
		LM_ERR("error in load user carrier");
		return -1;
	} else {
		/* set var */
		val.flags = PV_VAL_INT | PV_TYPE_INT;
		if(dvar->setf(_msg, &dvar->pvp, (int)EQ_T, &val) < 0) {
			LM_ERR("failed setting dst var\n");
			return -1;
		}
	}
	return 1;
}

/**
 *
 */
int ki_cr_load_user_carrier(
		sip_msg_t *_msg, str *user, str *domain, str *dstvar)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dstvar);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dstvar->len, dstvar->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dstvar->len, dstvar->s);
		return -1;
	}

	return ki_cr_load_user_carrier_helper(_msg, user, domain, dst);
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
int cr_load_user_carrier(
		sip_msg_t *_msg, char *_user, char *_domain, char *_dstvar)
{
	str user, domain;

	if(fixup_get_svalue(_msg, (gparam_t *)_user, &user) < 0) {
		LM_ERR("cannot print the user\n");
		return -1;
	}

	if(fixup_get_svalue(_msg, (gparam_t *)_domain, &domain) < 0) {
		LM_ERR("cannot print the domain\n");
		return -1;
	}

	return ki_cr_load_user_carrier_helper(
			_msg, &user, &domain, (pv_spec_t *)_dstvar);
}


/**
 *
 */
int ki_cr_route_helper(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_rewrite_user, str *_hsrc,
		enum hash_algorithm _halg, str *_dstvar)
{
	int carrier, domain, ret = -1;
	enum hash_source hsrc;

	struct route_data_t *rd;
	pv_spec_t *dstvar = NULL;

	if(_dstvar != NULL) {
		dstvar = pv_cache_get(_dstvar);
		if(dstvar == NULL) {
			LM_ERR("failed to get pv spec for: %.*s\n", _dstvar->len,
					_dstvar->s);
			return -1;
		}
		if(dstvar->setf == NULL) {
			LM_ERR("target pv is not writable: %.*s\n", _dstvar->len,
					_dstvar->s);
			return -1;
		}
	}

	hsrc = get_hash_source(_hsrc);

	do {
		rd = get_data();
	} while(rd == NULL);

	carrier = cr_str2id(_carrier, rd->carrier_map, rd->carrier_num);
	if(carrier < 0) {
		LM_ERR("invalid carrier %.*s\n", _carrier->len, _carrier->s);
		goto unlock_and_out;
	}

	domain = cr_str2id(_domain, rd->domain_map, rd->domain_num);
	if(domain < 0) {
		LM_ERR("invalid domain %.*s\n", _domain->len, _domain->s);
		goto unlock_and_out;
	}

	ret = ki_cr_do_route_helper(_msg, rd, carrier, domain, _prefix_matching,
			_rewrite_user, hsrc, _halg, dstvar);

unlock_and_out:
	release_data(rd);
	return ret;
}


/**
 *
 */
int ki_cr_route_info(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_rewrite_user, str *_hsrc, str *_dstvar)
{
	return ki_cr_route_helper(_msg, _carrier, _domain, _prefix_matching,
			_rewrite_user, _hsrc, alg_crc32, _dstvar);
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
int cr_route(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_rewrite_user, enum hash_source _hsrc,
		char *_dstvar)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching, _rewrite_user,
			_hsrc, alg_crc32, _dstvar);
}


/**
 *
 */
int ki_cr_route(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_rewrite_user, str *_hsrc)
{
	return ki_cr_route_info(_msg, _carrier, _domain, _prefix_matching,
			_rewrite_user, _hsrc, NULL);
}


/**
 *
 */
int cr_route5(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_rewrite_user, enum hash_source _hsrc)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching, _rewrite_user,
			_hsrc, alg_crc32, NULL);
}


/**
 *
 */
int ki_cr_nofallback_route_info(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_rewrite_user, str *_hsrc, str *_dstvar)
{
	return ki_cr_route_helper(_msg, _carrier, _domain, _prefix_matching,
			_rewrite_user, _hsrc, alg_crc32_nofallback, _dstvar);
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
int cr_nofallback_route(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_rewrite_user, enum hash_source _hsrc,
		char *_dstvar)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching, _rewrite_user,
			_hsrc, alg_crc32_nofallback, _dstvar);
}


/**
 *
 */
int ki_cr_nofallback_route(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_rewrite_user, str *_hsrc)
{
	return ki_cr_route_helper(_msg, _carrier, _domain, _prefix_matching,
			_rewrite_user, _hsrc, alg_crc32_nofallback, NULL);
}


/**
 *
 */
int cr_nofallback_route5(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_rewrite_user, enum hash_source _hsrc)
{
	return cr_do_route(_msg, _carrier, _domain, _prefix_matching, _rewrite_user,
			_hsrc, alg_crc32_nofallback, NULL);
}


/**
 *
 */
int ki_cr_load_next_domain_helper(sip_msg_t *_msg, struct route_data_t *_rd,
		int _carrier, int _domain, str *_prefix_matching, str *_host,
		str *_reply_code, pv_spec_t *_dstavp)
{
	int ret;
	flag_t flags;
	struct domain_data_t *domain_data;
	struct carrier_data_t *carrier_data = NULL;

	ret = -1;
	domain_data = NULL;
	flags = _msg->flags;

	if(_carrier == 0) {
		carrier_data = get_carrier_data(_rd, _rd->default_carrier_id);
	} else {
		carrier_data = get_carrier_data(_rd, _carrier);
		if(carrier_data == NULL) {
			if(cfg_get(carrierroute, carrierroute_cfg, fallback_default)) {
				LM_NOTICE("invalid tree id %i specified, using default tree\n",
						_carrier);
				carrier_data = get_carrier_data(_rd, _rd->default_carrier_id);
			}
		}
	}
	if(carrier_data == NULL) {
		LM_ERR("cannot get carrier data\n");
		return -1;
	}

	domain_data = get_domain_data(carrier_data, _domain);
	if(domain_data == NULL) {
		LM_ERR("desired routing domain doesn't exist, prefix %.*s, carrier %d, "
			   "domain %d\n",
				_prefix_matching->len, _prefix_matching->s, _carrier, _domain);
		return -1;
	}

	ret = set_next_domain_recursor(_msg, domain_data->failure_tree,
			_prefix_matching, _host, _reply_code, flags, _dstavp);

	if(ret != 0) {
		LM_INFO("set_next_domain_recursor doesn't complete, prefix '%.*s', "
				"carrier %d, domain %d\n",
				_prefix_matching->len, _prefix_matching->s, _carrier, _domain);
		return -1;
	}

	return 1;
}


/**
 *
 */
int ki_cr_load_next_domain(sip_msg_t *_msg, str *_carrier, str *_domain,
		str *_prefix_matching, str *_host, str *_reply_code, str *_dstavp)
{
	int carrier, domain, ret = -1;

	pv_spec_t *dstavp;
	struct route_data_t *rd;

	if(_prefix_matching == NULL) {
		LM_ERR("cannot get the prefix_matching\n");
		return -1;
	}
	if(_host == NULL) {
		LM_ERR("cannot get the host\n");
		return -1;
	}
	if(_reply_code == NULL) {
		LM_ERR("cannot get the reply_code\n");
		return -1;
	}

	dstavp = pv_cache_get(_dstavp);
	if(dstavp == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", _dstavp->len, _dstavp->s);
		return -1;
	}
	if(dstavp->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", _dstavp->len, _dstavp->s);
		return -1;
	}

	do {
		rd = get_data();
	} while(rd == NULL);

	carrier = cr_str2id(_carrier, rd->carrier_map, rd->carrier_num);
	if(carrier < 0) {
		LM_ERR("invalid carrier %.*s\n", _carrier->len, _carrier->s);
		goto unlock_and_out;
	}

	domain = cr_str2id(_domain, rd->domain_map, rd->domain_num);
	if(domain < 0) {
		LM_ERR("invalid domain %.*s\n", _domain->len, _domain->s);
		goto unlock_and_out;
	}

	ret = ki_cr_load_next_domain_helper(_msg, rd, carrier, domain,
			_prefix_matching, _host, _reply_code, dstavp);

unlock_and_out:
	release_data(rd);
	return ret;
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
int cr_load_next_domain(sip_msg_t *_msg, char *_carrier, char *_domain,
		char *_prefix_matching, char *_host, char *_reply_code, char *_dstavp)
{

	int carrier_id, domain_id, ret = -1;
	str carrier, domain, prefix_matching, host, reply_code;

	pv_spec_t *dstavp;
	struct route_data_t *rd;

	if(fixup_get_svalue(_msg, (gparam_t *)_prefix_matching, &prefix_matching)
			< 0) {
		LM_ERR("cannot print the prefix_matching\n");
		return -1;
	}
	if(fixup_get_svalue(_msg, (gparam_t *)_host, &host) < 0) {
		LM_ERR("cannot print the host\n");
		return -1;
	}
	if(fixup_get_svalue(_msg, (gparam_t *)_reply_code, &reply_code) < 0) {
		LM_ERR("cannot print the reply_code\n");
		return -1;
	}

	dstavp = (pv_spec_t *)_dstavp;

	do {
		rd = get_data();
	} while(rd == NULL);

	if(get_str_fparam(&carrier, _msg, (fparam_t *)_carrier) < 0) {
		if(get_int_fparam(&carrier_id, _msg, (fparam_t *)_carrier) < 0) {
			LM_ERR("cannot print the carrier\n");
			goto unlock_and_out;
		}
	} else {
		carrier_id = cr_str2id(&carrier, rd->carrier_map, rd->carrier_num);
	}

	if(get_str_fparam(&domain, _msg, (fparam_t *)_domain) < 0) {
		if(get_int_fparam(&domain_id, _msg, (fparam_t *)_domain) < 0) {
			LM_ERR("cannot print the domain\n");
			goto unlock_and_out;
		}
	} else {
		domain_id = cr_str2id(&domain, rd->domain_map, rd->domain_num);
	}

	ret = ki_cr_load_next_domain_helper(_msg, rd, carrier_id, domain_id,
			&prefix_matching, &host, &reply_code, dstavp);

unlock_and_out:
	release_data(rd);
	return ret;
}
