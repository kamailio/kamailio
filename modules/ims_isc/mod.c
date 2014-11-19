/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "mod.h"

#include "checker.h"
#include "mark.h"
#include "third_party_reg.h"
#include "isc.h"

MODULE_VERSION

struct tm_binds isc_tmb;
usrloc_api_t isc_ulb; /*!< Structure containing pointers to usrloc functions*/

/* fixed parameter storage */
str isc_my_uri = str_init("scscf.ims.smilecoms.com:6060"); /**< Uri of myself to loop the message in str	*/
str isc_my_uri_sip = {0, 0}; /**< Uri of myself to loop the message in str with leading "sip:" */
int isc_expires_grace = 120; /**< expires value to add to the expires in the 3rd party register*/
int isc_fr_timeout = 5000; /**< default ISC response timeout in ms */
int isc_fr_inv_timeout = 20000; /**< default ISC invite response timeout in ms */
int add_p_served_user = 0; /**< should the P-Served-User header be inserted? */

/** module functions */
static int mod_init(void);
int isc_match_filter(struct sip_msg *msg, char *str1, udomain_t* d);
int isc_match_filter_reg(struct sip_msg *msg, char *str1, udomain_t* d);
int isc_from_as(struct sip_msg *msg, char *str1, char *str2);

/*! \brief Fixup functions */
static int domain_fixup(void** param, int param_no);
static int w_isc_match_filter_reg(struct sip_msg* _m, char* str1, char* str2);
static int w_isc_match_filter(struct sip_msg* _m, char* str1, char* str2);

static cmd_export_t cmds[] = {
    { "isc_match_filter_reg", (cmd_function) w_isc_match_filter_reg, 2, domain_fixup, 0, REQUEST_ROUTE},
    { "isc_from_as", (cmd_function) isc_from_as, 1, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    { "isc_match_filter", (cmd_function) w_isc_match_filter, 2, domain_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    { 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    { "my_uri", PARAM_STR, &isc_my_uri}, /**< SIP Uri of myself for getting the messages back */
    { "expires_grace", INT_PARAM, &isc_expires_grace}, /**< expires value to add to the expires in the 3rd party register to prevent expiration in AS */
    { "isc_fr_timeout", INT_PARAM, &isc_fr_timeout}, /**< Time in ms that we are waiting for a AS response until we
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 consider it dead. Has to be lower than SIP transaction timeout
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 to prevent downstream timeouts. Not too small though because
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 AS are usually slow as hell... */
    { "isc_fr_inv_timeout", INT_PARAM, &isc_fr_inv_timeout}, /**< Time in ms that we are waiting for a AS INVITE response until we
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 consider it dead. Has to be lower than SIP transaction timeout
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 to prevent downstream timeouts. Not too small though because
 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 AS are usually slow as hell... */
    { "add_p_served_user", INT_PARAM, &add_p_served_user}, /**< boolean indicating if the P-Served-User (RFC5502) should be added on the ISC interface or not */
    { 0, 0, 0}
};

/** module exports */
struct module_exports exports = {"ims_isc", DEFAULT_DLFLAGS, /* dlopen flags */
    cmds, /* Exported functions */
    params, 0, /* exported statistics */
    0, /* exported MI functions */
    0, /* exported pseudo-variables */
    0, /* extra processes */
    mod_init, /* module initialization function */
    0, 0, 0 /* per-child init function */};

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no) {
    udomain_t* d;

    if (param_no == 2) {
        if (isc_ulb.register_udomain((char*) *param, &d) < 0) {
            LM_ERR("failed to register domain\n");
            return E_UNSPEC;
        }
        *param = (void*) d;
    }
    return 0;
}

/*! \brief
 * Wrapper to do isc match filter refg
 */
static int w_isc_match_filter_reg(struct sip_msg* _m, char* str1, char* str2) {
    return isc_match_filter_reg(_m, str1, (udomain_t*) str2);
}

/*! \brief
 * Wrapper to do isc match filter
 */
static int w_isc_match_filter(struct sip_msg* _m, char* str1, char* str2) {
    return isc_match_filter(_m, str1, (udomain_t*) str2);
}

static int fix_parameters() {
    return 1;
}

/**
 * init module function
 */
static int mod_init(void) {

    bind_usrloc_t bind_usrloc;
    /* fix the parameters */
    if (!fix_parameters())
        goto error;

    /* load the TM API */
    if (load_tm_api(&isc_tmb) != 0) {
        LM_ERR("can't load TM API\n");
        goto error;
    }

    bind_usrloc = (bind_usrloc_t) find_export("ul_bind_usrloc", 1, 0);
    if (!bind_usrloc) {
        LM_ERR("can't bind usrloc\n");
        return -1;
    }

    if (bind_usrloc(&isc_ulb) < 0) {
        return -1;
    }

    /* Init the isc_my_uri parameter */
    if (!isc_my_uri.s || isc_my_uri.len<=0) {
        LM_CRIT("mandatory parameter \"isc_my_uri\" found empty\n");
        goto error;
    }

    isc_my_uri_sip.len = 4 + isc_my_uri.len;
    isc_my_uri_sip.s = shm_malloc(isc_my_uri_sip.len + 1);
    memcpy(isc_my_uri_sip.s, "sip:", 4);
    memcpy(isc_my_uri_sip.s + 4, isc_my_uri.s, isc_my_uri.len);
    isc_my_uri_sip.s[isc_my_uri_sip.len] = 0;

    LM_DBG("ISC module successfully initialised\n");

    return 0;
error:
    LM_ERR("Failed to initialise ISC module\n");
    return -1;
}

/**
 * Returns the direction of the dialog as int dialog_direction from a string.
 * @param direction - "orig" or "term"
 * @returns DLG_MOBILE_ORIGINATING, DLG_MOBILE_TERMINATING if successful, or 
 * DLG_MOBILE_UNKNOWN on error
 */
static inline enum dialog_direction get_dialog_direction(char *direction) {
    switch (direction[0]) {
        case 'o':
        case 'O':
        case '0':
            return DLG_MOBILE_ORIGINATING;
        case 't':
        case 'T':
        case '1':
            return DLG_MOBILE_TERMINATING;
        default:
            LM_ERR("Unknown direction %s", direction);
            return DLG_MOBILE_UNKNOWN;
    }
}

/**
 * Checks if there is a match.
 * Inserts route headers and set the dst_uri
 * @param msg - the message to check
 * @param str1 - the direction of the request orig/term
 * @param str2 - not used
 * @returns #ISC_RETURN_TRUE if found, #ISC_RETURN_FALSE if not, #ISC_RETURN_BREAK on error
 */
int isc_match_filter(struct sip_msg *msg, char *str1, udomain_t* d) {
    int k = 0;
    isc_match *m = NULL;
    str s = {0, 0};

    //sometimes s is populated by an ims_getter method cscf_get_terminating_user that alloc memory that must be free-ed at the end
    int free_s = 0;


    int ret = ISC_RETURN_FALSE;
    isc_mark new_mark, old_mark;

    enum dialog_direction dir = get_dialog_direction(str1);

    LM_INFO("Checking triggers\n");

    if (dir == DLG_MOBILE_UNKNOWN)
        return ISC_RETURN_BREAK;

    if (!cscf_is_initial_request(msg))
        return ISC_RETURN_FALSE;

    /* starting or resuming? */
    memset(&old_mark, 0, sizeof (isc_mark));
    memset(&new_mark, 0, sizeof (isc_mark));
    if (isc_mark_get_from_msg(msg, &old_mark)) {
        LM_DBG("Message returned s=%d;h=%d;d=%d;a=%.*s\n", old_mark.skip, old_mark.handling, old_mark.direction, old_mark.aor.len, old_mark.aor.s);
    } else {
        LM_DBG("Starting triggering\n");
    }

    if (is_route_type(FAILURE_ROUTE)) {
        /* need to find the handling for the failed trigger */
        if (dir == DLG_MOBILE_ORIGINATING) {
            k = cscf_get_originating_user(msg, &s);
            if (k) {
                k = isc_is_registered(&s, d);
                if (k == IMPU_NOT_REGISTERED) {
                    ret = ISC_RETURN_FALSE;
                    goto done;
                }
                new_mark.direction = IFC_ORIGINATING_SESSION;
                LM_DBG("Orig User <%.*s> [%d]\n", s.len, s.s, k);
            } else
                goto done;
        }
        if (dir == DLG_MOBILE_TERMINATING) {
            k = cscf_get_terminating_user(msg, &s);
            //sometimes s is populated by an ims_getter method cscf_get_terminating_user that alloc memory that must be free-ed at the end
            free_s = 1;

            if (k) {
                k = isc_is_registered(&s, d);
                //LOG(L_DBG,"after isc_is_registered in ISC_match_filter\n");
                if (k == IMPU_REGISTERED) {
                    new_mark.direction = IFC_TERMINATING_SESSION;
                } else {
                    new_mark.direction = IFC_TERMINATING_UNREGISTERED;
                }
                LM_DBG("Term User <%.*s> [%d]\n", s.len, s.s, k);
            } else {
                goto done;
            }
        }
        struct cell * t = isc_tmb.t_gett();
        LM_CRIT("SKIP: %d\n", old_mark.skip);
        int index = old_mark.skip;
        for (k = 0; k < t->nr_of_outgoings; k++) {
            m = isc_checker_find(s, new_mark.direction, index, msg,
                    isc_is_registered(&s, d), d);
            if (m) {
                index = m->index;
                if (k < t->nr_of_outgoings - 1)
                    isc_free_match(m);
            } else {
                LM_ERR("On failure, previously matched trigger no longer matches?!\n");
                ret = ISC_RETURN_BREAK;
                goto done;
            }
        }
        if (m->default_handling == IFC_SESSION_TERMINATED) {
            /* Terminate the session */
            LM_DBG("Terminating session.\n");
            isc_tmb.t_reply(msg, IFC_AS_UNAVAILABLE_STATUS_CODE, "AS Contacting Failed - iFC terminated dialog");
            LM_DBG("Responding with %d to URI: %.*s\n",
                    IFC_AS_UNAVAILABLE_STATUS_CODE, msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s);
            isc_free_match(m);
            ret = ISC_RETURN_BREAK;
            goto done;
        }
        /* skip the failed triggers (IFC_SESSION_CONTINUED) */
        old_mark.skip = index + 1;

        isc_free_match(m);
        isc_mark_drop_route(msg);
    }

    LM_DBG("Checking if ISC is for originating user\n");
    /* originating leg */
    if (dir == DLG_MOBILE_ORIGINATING) {
        k = cscf_get_originating_user(msg, &s);
        LM_DBG("ISC is for Orig user\n");
        if (k) {
            LM_DBG("Orig user is [%.*s]\n", s.len, s.s);
            k = isc_is_registered(&s, d);
            if (k == IMPU_NOT_REGISTERED) {
                LM_DBG("User is not registered\n");
                return ISC_RETURN_FALSE;
            }

            LM_DBG("Orig User <%.*s> [%d]\n", s.len, s.s, k);
            //CHECK if this is a new call (According to spec if the new uri and old mark URI are different then this is a new call and should
            //be triggered accordingly
            LM_DBG("Checking if RURI has changed...comparing: <%.*s> and <%.*s>\n",
                    old_mark.aor.len, old_mark.aor.s,
                    s.len, s.s);
            if ((old_mark.aor.len == s.len) && memcmp(old_mark.aor.s, s.s, s.len) != 0) {
                LM_DBG("This is a new call....... trigger accordingly\n");
                m = isc_checker_find(s, old_mark.direction, 0, msg,
                        isc_is_registered(&s, d), d);
            } else {
                m = isc_checker_find(s, old_mark.direction, old_mark.skip, msg,
                        isc_is_registered(&s, d), d);
            }
            if (m) {
                new_mark.direction = IFC_ORIGINATING_SESSION;
                new_mark.skip = m->index + 1;
                new_mark.handling = m->default_handling;
                new_mark.aor = s;
                ret = isc_forward(msg, m, &new_mark);
                isc_free_match(m);
                goto done;
            }
        }
        goto done;
    }
    LM_DBG("Checking if ISC is for terminating user\n");
    /* terminating leg */
    if (dir == DLG_MOBILE_TERMINATING) {
        k = cscf_get_terminating_user(msg, &s);
        //sometimes s is populated by an ims_getter method cscf_get_terminating_user that alloc memory that must be free-ed at the end
        free_s = 1;
        LM_DBG("ISC is for Term user\n");
        if (k) {
            k = isc_is_registered(&s, d);
            if (k == IMPU_REGISTERED) {
                new_mark.direction = IFC_TERMINATING_SESSION;
            } else {
                new_mark.direction = IFC_TERMINATING_UNREGISTERED;
            }
            LM_DBG("Term User <%.*s> [%d]\n", s.len, s.s, k);
            //CHECK if this is a new call (According to spec if the new uri and old mark URI are different then this is a new call and should
            //be triggered accordingly
            LM_DBG("Checking if RURI has changed...comparing: <%.*s> and <%.*s>\n",
                    old_mark.aor.len, old_mark.aor.s,
                    s.len, s.s);
            if ((old_mark.aor.len == s.len) && memcmp(old_mark.aor.s, s.s, s.len) != 0) {
                LM_DBG("This is a new call....... trigger accordingly\n");
                m = isc_checker_find(s, new_mark.direction, 0, msg, isc_is_registered(&s, d), d);
            } else {
                LM_DBG("Resuming triggering\n");
                m = isc_checker_find(s, new_mark.direction, old_mark.skip, msg, isc_is_registered(&s, d), d);
            }
            if (m) {
                new_mark.skip = m->index + 1;
                new_mark.handling = m->default_handling;
                new_mark.aor = s;
                ret = isc_forward(msg, m, &new_mark);
                isc_free_match(m);
                goto done;
            }
        }
        goto done;
    }

done:

    if (s.s && free_s == 1)
        shm_free(s.s); // shm_malloc in cscf_get_terminating_user  

    if (old_mark.aor.s)
        pkg_free(old_mark.aor.s);
    return ret;
}

void clean_impu_str(str* impu_s) {
    char *p;
    
    if ((p = memchr(impu_s->s, ';', impu_s->len))) {
	impu_s->len = p - impu_s->s;
    }
}
/**
 * Checks if there is a match on REGISTER.
 * Inserts route headers and set the dst_uri
 * @param msg - the message to check
 * @param str1 - if the user was previously registered 0 - for initial registration, 1 for re/de-registration
 * @param str2 - not used
 * @returns #ISC_RETURN_TRUE if found, #ISC_RETURN_FALSE if not
 */
int isc_match_filter_reg(struct sip_msg *msg, char *str1, udomain_t* d) {
    int k;
    isc_match *m;
    str s = {0, 0};
    int ret = ISC_RETURN_FALSE;
    isc_mark old_mark;

    enum dialog_direction dir = DLG_MOBILE_ORIGINATING;

    LM_DBG("Checking triggers\n");

    /* starting or resuming? */
    memset(&old_mark, 0, sizeof (isc_mark));
    LM_DBG("Starting triggering\n");

    /* originating leg */
    if (dir == DLG_MOBILE_ORIGINATING) {
        k = cscf_get_originating_user(msg, &s);
        if (k) {
            if (str1 == 0 || strlen(str1) != 1) {
                LM_ERR("wrong parameter - must be \"0\" (initial registration) or \"1\"(previously registered) \n");
                return ret;
            } else if (str1[0] == '0')
                k = 0;
            else
                k = 1;

	    LM_DBG("Orig User before clean: <%.*s> [%d]\n", s.len, s.s, k);
	    clean_impu_str(&s);
            LM_DBG("Orig User after clean: <%.*s> [%d]\n", s.len, s.s, k);
	    
            m = isc_checker_find(s, old_mark.direction, old_mark.skip, msg, k, d);
            while (m) {
                LM_DBG("REGISTER match found in filter criteria\n");
                ret = isc_third_party_reg(msg, m, &old_mark);
                old_mark.skip = m->index + 1;
                isc_free_match(m);
                m = isc_checker_find(s, old_mark.direction, old_mark.skip, msg, k, d);
            }

            if (ret == ISC_RETURN_FALSE)
                LM_DBG("No REGISTER match found in filter criteria\n");
        }
    }
    return ret;
}

/**
 * Check if the message is from the AS.
 * Inserts route headers and set the dst_uri
 * @param msg - the message to check
 * @param str1 - the direction of the request orig/term
 * @param str2 - not used
 * @returns #ISC_RETURN_TRUE if from AS, #ISC_RETURN_FALSE if not, #ISC_RETURN_BREAK on error
 */
int isc_from_as(struct sip_msg *msg, char *str1, char *str2) {
    int ret = ISC_RETURN_FALSE;
    isc_mark old_mark;
    str s = {0, 0};
    //sometimes s is populated by an ims_getter method cscf_get_terminating_user that alloc memory that must be free-ed at the end
    int free_s = 0;
    
    
    enum dialog_direction dir = get_dialog_direction(str1);

    if (dir == DLG_MOBILE_UNKNOWN)
        return ISC_RETURN_BREAK;

    if (!cscf_is_initial_request(msg))
        return ISC_RETURN_FALSE;

    /* starting or resuming? */
    if (isc_mark_get_from_msg(msg, &old_mark)) {
        LM_DBG("Message returned s=%d;h=%d;d=%d\n", old_mark.skip, old_mark.handling, old_mark.direction);

        /*according to spec 24.229, 5.4.3.3 if the URI is different then the RURI is retargeted and we can do one of 2 things,
         * a) mark as originating session and forward according to normal RURI procedures,
         * b) run IFC criteria on new retrageturi  and route accordingly.
         *
         * We will therefore leave it in the hands of the config writer to decide. We will make them aware here that retargeting has happened
         * by retirning a special error code ISC_RETURN_RETARGET(-2).
         */
        if (dir == DLG_MOBILE_TERMINATING) {
            cscf_get_terminating_user(msg, &s);
            //sometimes s is populated by an ims_getter method cscf_get_terminating_user that alloc memory that must be free-ed at the end
            free_s = 1;
            if (memcmp(old_mark.aor.s, s.s, s.len) != 0) {
                LM_DBG("This is a new call....... RURI has been retargeted\n");
                return ISC_RETURN_RETARGET;
            }
        }
        if (old_mark.direction == IFC_ORIGINATING_SESSION
                && dir != DLG_MOBILE_ORIGINATING)
            ret = ISC_RETURN_FALSE;
        else if ((old_mark.direction == IFC_TERMINATING_SESSION
                || old_mark.direction == IFC_TERMINATING_UNREGISTERED)
                && dir != DLG_MOBILE_TERMINATING)
            ret = ISC_RETURN_FALSE;
        else
            ret = ISC_RETURN_TRUE;
    } else {
        ret = ISC_RETURN_FALSE;
    }
    if (old_mark.aor.s)
        pkg_free(old_mark.aor.s);
    
    if (s.s && free_s == 1)
        shm_free(s.s); // shm_malloc in cscf_get_terminating_user  
    
    return ret;
}

