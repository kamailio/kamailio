/*
 * ims_rdn module -  rdn (railway dedicated network) related functions
 * for an IMS environment (S-CSCF and/or I-CSCF).
 *
 * -----------------------------------------------------------------
 * Copyright Peter Friedrich 2016 
 * Copyright (C) 2016 - 2026 Kontron Transportation GmbH,
 *               theodor.scherney@kontron.com
 *               christoph.eckl@kontron.com
 *               luca.nardin@kontron.com
 *               christoph.valentin@kontron.com
 *
 * The RDN Session controller project (vic) 
 * This module copies (and modifies) some parts from following modules: 
 *   - ims_userloc_scscf
 *   - ims_registrar_scscf
 *   - dispatcher
 *  
 * The module implements the handling of "RDN Services" (Railway
 * Dedicated Network Services).
 * 
 * RDN Services are terminating unregistered services, which are
 * implemented on ASs.
 *
 * This module is intended to run on a kamailio S-CSCF and/or
 * I-CSCF. It will select the correct ASs for RDN Services.
 *
 * Functional addresses are SIP URIs with the parameter user=gsmr
 *
 * This module depends on 
 *   - kamailio core
 *   - tm  
 *   - ims_userloc_scscf  
 *   - ims_registrar_scscf  
 * -----------------------------------------------------------------
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

#ifndef _IMS_RDN_H_
#define _IMS_RDN_H_

// Hardcoded names for AVPs
#define RDN_AS_NAME "rdn_as_name"
#define RDN_SCSCF_NAME "rdn_scscf_name"
#define RDN_SESSIONTYPE "rdn_sessiontype"
#define RDN_MCX_DOMAIN "rdn_mcx_domain"
#define RDN_IWF_DOMAIN "rdn_iwf_domain"
#define RDN_HARDCODED_RURI "rdn_hardcoded_ruri"
#define RDN_DEL "rdn_del"
#define RDN_INSRT "rdn_insrt"

// Macros for the handling of (hex) digits
#define RDN_NUM_DIGITS (16)


// URI parameter ";user=ip|phone|gsmr|dialstring"
// We can implement 4 digit trees, one for each type of user part.
// Currently, only two digit trees are implemented: "user=phone" and "user=gsmr".
#define RDN_USER_PARAM_INET "ip"
#define RDN_USER_PARAM_E164 "phone"
#define RDN_USER_PARAM_EIRENE "gsmr"
#define RDN_USER_PARAM_DIAL "dialstring"

// A special handling is a configurable R-URI, which replaces the EIRENE or E.164
// number in the request line,
// a count of leading characters that shall be cut from the EIRENE/E.164 number and a
// string of digits that shall be prepended to the remaining digits,
// when creating an "intermediate" number (a so-called "RDN Address") for further
// processing by the IWF.
// The "special handling" is also called "MCx AS Delivery of RDN Services" in documents.
typedef struct _rdn_special_handling
{
	str hardcoded_ruri;
	int del;
	str insrt;
	str mcx_domain;
	str iwf_domain;
} rdn_special_handling_t;

// An RDN Service is the URI of an AS plus a session-type and an optional
// special handling-
// On the I-CSCF, the as_name actually contains an S-CSCF address, scscf_flag
// is true, therefore.
typedef struct _rdn_service
{
	int scscf_flag;
	str as_name;
	str session_type;
	rdn_special_handling_t *special_handling;
} rdn_service_t;

// An array of RDN_NUM_DIGITS digit nodes handles all possible values of one digit
// each value of the digit can lead either
//  a) to an RDN Service
//  b) or to a next level of digit nodes (next digit has to be evaluated)
typedef struct _rdn_digit_node
{
	struct _rdn_digit_node *
			next_level; //either NULL or points to an array of RDN_NUM_DIGITS digit nodes
	rdn_service_t *service;
} rdn_digit_node_t;

// A distinct PSI is identified by the user part of a SIP URI.
// It points to an RDN Service.
typedef struct _rdn_distinct_psi
{
	str key;
	rdn_service_t *service;
} rdn_distinct_psi_t;

#define RDN_HASH_SIZE 64

// Result of the RDN analysis for a given SIP URI.
// Used by analyse_*_pvar() to pass data back into config-level pseudo-variables.
typedef struct rdn_analysis_result
{
	str as_name;
	str scscf_name;
	str session_type;
	str iwf_domain;
	str hardcoded_ruri;
	str insrt;
	str mcx_domain;
	int del;
} rdn_analysis_result_t;

// Hash table entry
struct rdn_hash_entry
{
	str key; /* user part for dpsi, method@domain for domain based service */
	rdn_service_t *service;		 /* Pointer to service */
	struct rdn_hash_entry *next; /* Next element in hash table collision slot */
};

// type of configuration file (rdn_services.list, dpsi.list)
typedef enum rdn_config_type
{
	rdn_config,
	rdn_dpsi_config
} rdn_config_type;
// Type of template file (CxUserData_tmpl.xml)
typedef enum rdn_template_type
{
	rdn_cx_userdata_template
} rdn_template_type;

// Multi MCx Domain Support

// Module parameter indicating the RDN Node Type and the support of Multiple MCx Domains
#define RDN_NODE_TYPE_DEFAULT 0 // Default to S-CSCF
#define RDN_NODE_TYPE_SCSCF 0
#define RDN_NODE_TYPE_ICSCF 1
#define RDN_NODE_TYPE_MULTI_MC_SC 16
#define RDN_NODE_TYPE_MULTI_MC_IC 17
#define RDN_NODE_TYPE_MASK 0xf			 // Lower 4 bits hold node type
#define RDN_NODE_TYPE_MULTI_MC_MASK 0x10 // Bit 4 indicates Multi MCx support

#endif