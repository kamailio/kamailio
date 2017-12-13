### m4 macros to make the configuration easier

include(`rules.m4')

define(`SER_IP', `192.168.0.1')
define(`SER_HOSTNAME', `foo.bar')

define(`GW_IP_1', `192.168.0.2')
define(`GW_IP_2', `192.168.0.3')

declare(flags, ACC_FLAG, MISSED_FLAG, VM_FLAG, NAT_FLAG)
declare(route, PSTN_ROUTE, NAT_ROUTE, VOICEMAIL_ROUTE, PSTN2_ROUTE)
declare(onreply, NAT_REPLY)
declare(failure, PSTN_FAILURE, _1_FAILURE)

### End of m4 macro section

#
# $Id$
#
# sip-router.cfg m4 template
#

#
# Set the following in your CISCO PSTN gateway:
# sip-ua
#   nat symmetric role passive
#   nat symmetric check-media-src
#
fork=yes
port=5060
log_stderror=no
fifo="/tmp/sip-router_fifo"

# uncomment to enter testing mode
/*
fork=no
port=5064
log_stderror=yes
fifo="/tmp/sip-router_fifox"
 */

debug=3
memlog=4  # memlog set high (>debug) -- no final time-consuming memory reports on exit
mhomed=yes
listen=SER_IP
alias="SER_HOSTNAME"
check_via=yes
dns=yes
rev_dns=no
children=16

# if changing fifo mode to a more restrictive value, put
# decimal value in there, e.g. dec(rw|rw|rw)=dec(666)=438
fifo_mode=0666

loadmodule "/usr/local/lib/sip-router/modules/tm.so"
loadmodule "/usr/local/lib/sip-router/modules/sl.so"
loadmodule "/usr/local/lib/sip-router/modules/acc.so"
loadmodule "/usr/local/lib/sip-router/modules/rr.so"
loadmodule "/usr/local/lib/sip-router/modules/maxfwd.so"
loadmodule "/usr/local/lib/sip-router/modules/mysql.so"
loadmodule "/usr/local/lib/sip-router/modules/usrloc.so"
loadmodule "/usr/local/lib/sip-router/modules/registrar.so"
loadmodule "/usr/local/lib/sip-router/modules/auth.so"
loadmodule "/usr/local/lib/sip-router/modules/auth_db.so"
loadmodule "/usr/local/lib/sip-router/modules/textops.so"
loadmodule "/usr/local/lib/sip-router/modules/uri.so"
loadmodule "/usr/local/lib/sip-router/modules/group.so"
loadmodule "/usr/local/lib/sip-router/modules/msilo.so"
loadmodule "/usr/local/lib/sip-router/modules/nathelper.so"
loadmodule "/usr/local/lib/sip-router/modules/enum.so"
loadmodule "/usr/local/lib/sip-router/modules/domain.so"
#loadmodule "/usr/local/lib/sip-router/modules/permissions.so"

modparam("usrloc|acc|auth_db|group|msilo", "db_url", "sql://sip-router:heslo@localhost/sip-router")

# -- usrloc params --
/* 0 -- dont use mysql, 1 -- write_through, 2--write_back */
modparam("usrloc", "db_mode", 2)
modparam("usrloc", "timer_interval", 10)

# -- auth params --
modparam("auth_db", "calculate_ha1", yes)
modparam("auth_db", "plain_password_column", "password")
#modparam("auth_db", "use_rpid", 1)
modparam("auth", "nonce_expire", 300)
modparam("auth", "rpid_prefix", "<sip:")
modparam("auth", "rpid_suffix", "@GW_IP_3>;party=calling;id-type=subscriber;screen=yes;privacy=off")

# -- rr params --
# add value to ;lr param to make some broken UAs happy
modparam("rr", "enable_full_lr", 1)

# -- acc params --
# report ACKs too for sake of completeness -- as we account PSTN
# destinations which are RR, ACKs should show up
modparam("acc", "report_ack", 1)
modparam("acc", "log_level", 1)
# if BYE fails (telephone is dead, record-routing broken, etc.), generate
# a report nevertheless -- otherwise we would have no STOP event; => 1
modparam("acc", "failed_transactions", 1)

# that is the flag for which we will account -- don't forget to
# set the same one :-)
# Usage of flags is as follows:
#   1 == should account(all to gateway),
#   3 == should report on missed calls (transactions to iptel.org's users),
#   4 == destination user wishes to use voicemail
#   6 == nathelper
#
modparam("acc", "log_flag", ACC_FLAG)
modparam("acc", "db_flag", ACC_FLAG)
modparam("acc", "log_missed_flag", MISSED_FLAG)
modparam("acc", "db_missed_flag", MISSED_FLAG)

# report to syslog: From, i-uri, status, digest id, method
modparam("acc", "log_fmt", "fisum")

# -- tm params --
modparam("tm", "fr_timer", 20)
modparam("tm", "fr_inv_timer", 90)
modparam("tm", "wt_timer", 20)

# -- msilo params
modparam("msilo", "registrar", "sip:registrar@SER_HOSTNAME")

# -- enum params --
modparam("enum", "domain_suffix", "e164.arpa.")

# -- multi-domain
modparam("domain", "db_mode", 1)

# NAT features turned off -- smartnat available only in nat-capable release
# We will you flag 6 to mark NATed contacts
modparam("registrar", "nat_flag", NAT_FLAG)
# Enable NAT pinging
modparam("nathelper", "natping_interval", 15)
# Ping only contacts that are known to be behind NAT
modparam("nathelper", "ping_nated_only", 1)

# ---------------------  request routing logic -------------------
route {

        if (!mf_process_maxfwd_header("10")) {
                log("LOG: Too many hops\n");
                sl_send_reply("483", "Alas Too Many Hops");
                break;
        };

        if (msg:len >= max_len) {
                sl_send_reply("513", "Message too large");
                break;
        };

        # special handling for natted clients; first, nat test is
        # executed: it looks for via!=received and RFC1918 addresses
        # in Contact (may fail if line-folding used); also,
        # the received test should, if complete, should check all
        # vias for presence of received
        if (nat_uac_test("3")) {
                # allow RR-ed requests, as these may indicate that
                # a NAT-enabled proxy takes care of it; unless it is
                # a REGISTER

                if (method == "REGISTER" || !search("^Record-Route:")) {
                        log("LOG: Someone trying to register from private IP, rewriting\n");

                        # This will work only for user agents that support symmetric
                        # communication. We tested quite many of them and majority is
                        # smart smart enough to be symmetric. In some phones, like
                        # it takes a configuration option. With Cisco 7960, it is
                        # called NAT_Enable=Yes, with kphone it is called
                        # "symmetric media" and "symmetric signaling". (The latter
                        # not part of public released yet.)

                        fix_nated_contact(); # Rewrite contact with source IP of signalling
                        if (method == "INVITE") {
                                fix_nated_sdp("1");  # Add direction=active to SDP
                        };
                        force_rport();       # Add rport parameter to topmost Via
                        setflag(NAT_FLAG); # Mark as NATed

                        append_to_reply("P-NATed-Caller: Yes\r\n");
                };
        };


        # anti-spam -- if somene claims to belong to our domain in From,
        # challenge him (skip REGISTERs -- we will chalenge them later)
        if (search("(From|F):.*@SER_HOST_REGEX")) {
                # invites forwarded to other domains, like FWD may cause subsequent 
                # request to come from there but have iptel in From -> verify
                # only INVITEs (ignore FIFO/UAC's requests, i.e. src_ip==fox)
                if ((method == "INVITE" || method == "SUBSCRIBE") && !(FROM_MYSELF || FROM_GW)) {
                        if  (!(proxy_authorize("DIGEST_REALM", "subscriber"))) {
                                proxy_challenge("DIGEST_REALM", "0");
                                break;
                        };

                        # to maintain outside credibility of our proxy, we enforce
                        # username in From to equal digest username; user with
                        # "john.doe" id could advertise "bill.gates" in From otherwise;
                        if (!check_from()) {
                                log("LOG: From Cheating attempt in INVITE\n");
                                sl_send_reply("403", "That is ugly -- use From=id next time (OB)");
                                break;
                        };

                        # we better don't consume credentials -- some requests may be
                        # spiraled through our server (sfo@iptel->7141@iptel) and the
                        # subsequent iteration may challenge too, for example because of
                        # iptel claim in From; UACs then give up because they
                        # already submitted credentials for the given realm
                        #consume_credentials();
                }; # non-REGISTER from other domain
        } else if ((method == "INVITE" || method == "SUBSCRIBE" || method=="REGISTER" ) && 
                   !(uri == myself || uri =~ "TO_GW")) {
                # and we serve our gateway too (we RR requests to it, so that
                # its address may show up in subsequent requests after loose_route
                sl_send_reply("403", "No relaying");
                break;
        };

        # By default we record route everything except REGISTERs
        if (!(method=="REGISTER")) record_route();

        # if route forces us to forward to some explicit destination, do so
        #
        # loose_route returns true in case that a request included
        # route header fields instructing SER where to relay a request;
        # if that is the case, stop script processing and just forward there;
        # one could alternatively ignore the return value and treat the
        # request as if it was an outbound one; that would not work however
        # with broken UAs which strip RR parameters from Route. (What happens
        # is that with two RR /tcp2udp, spirals, etc./ and stripped parameters,
        # SER a) rewrites r-uri with RR1 b) matches uri==myself against RR1
        # c) applies mistakenly user-lookup to RR1 in r-uri

        if (loose_route()) {
                # check if someone has not introduced a pre-loaded INVITE -- if so,
                # verify caller's privileges before accepting rr-ing
                if ((method=="INVITE" || method=="ACK" || method=="CANCEL") && uri =~ "TO_GW") {
                        route(PSTN_ROUTE); # Forward to PSTN gateway
                } else {
                        append_hf("P-hint: rr-enforced\r\n");
                        # account all BYEs 
                        if (method=="BYE") setflag(ACC_FLAG);
                        route(NAT_ROUTE);  # Generic forward
                };
                break;
        };

        # -------  check for requests targeted out of our domain... -------
        if (!(uri == myself || uri =~ "TO_GW")) {
                # ... and we serve our gateway too (we RR requests to it, so that
                # its address may show up in subsequent requests after
                # rewriteFromRoute
                append_hf("P-hint: OUTBOUND\r\n");
                route(NAT_ROUTE);
                break;
        };


        # ------- now, the request is for sure for our domain -----------
        # registers always MUST be authenticated to
        # avoid stealing incoming calls
        if (method == "REGISTER") {
                /*
                if (!allow_register("register.allow", "register.deny")) {
                        log(1, "LOG: alert: Forbidden IP in Contact\n");
                        sl_send_reply("403", "Forbidden");
                        break;
                };
                */

                # prohibit attempts to grab someone else's To address 
                # using  valid credentials; 
                if (!www_authorize("DIGEST_REALM", "subscriber")) {
                        # challenge if none or invalid credentials
                        www_challenge("DIGEST_REALM", "0");
                        break;
                };

                if (!check_to()) {
                        log("LOG: To Cheating attempt\n");
                        sl_send_reply("403", "That is ugly -- use To=id in REGISTERs");
                        break;
                };

                # it is an authenticated request, update Contact database now
                if (!save("location")) {
                        sl_reply_error();
                };

                m_dump();
                break;
        };

        # some UACs might be fooled by Contacts our UACs generate to make MSN
        # happy (web-im, e.g.) -- tell its urneachable
        if (uri =~ "sip:daemon@") {
                sl_send_reply("410", "Daemon is gone");
                break;
        };

        # aliases
        # note: through a temporary error in provisioning interface, there
        # are now aliases 905xx ... they take precedence overy any PSTN numbers
        # as they are resolved first
        lookup("aliases");

        # check again, if it is still for our domain after aliases
        if (!(uri == myself || uri =~ "TO_GW")) {
                append_hf("P-hint: ALIASED-OUTBOUND\r\n");
                route(NAT_ROUTE);
                break;
        };

	# Remove leading + if it is a number begining with +
	if (uri =~ "^[a-zA-Z]+:\+[0-9]+@") {
		strip(1);
		prefix("00");
	};		

	if (!does_uri_exist()) {
		# Try numeric destinations through the gateway
		if (uri =~ "^[a-zA-Z]+:[0-9]+@") {
			route(PSTN_ROUTE);
		} else {
			sl_send_reply("604", "Does Not Exist Anywhere");
		};
		break;
	};

        # does the user wish redirection on no availability? (i.e., is he
        # in the voicemail group?) -- determine it now and store it in
        # flag 4, before we rewrite the flag using UsrLoc
        if (is_user_in("Request-URI", "voicemail")) {
                setflag(VM_FLAG);
        };

        # native SIP destinations are handled using our USRLOC DB
        if (!lookup("location")) {
                # handle user which was not found
                route(VOICEMAIL_ROUTE);
                break;
        };

        # check whether some inventive user has uploaded  gateway
        # contacts to UsrLoc to bypass our authorization logic
        if (uri =~ "TO_GW") {
                log(1, "LOG: Weird! Gateway address in UsrLoc!\n");
                route(PSTN_ROUTE);
                break;
        };

        # if user is on-line and is in voicemail group, enable redirection
        /* no voicemail currently activated
        if (method == "INVITE" && isflagset(VM_FLAG)) {
                t_on_failure(_1_FAILURE);    # failure_route() not defined
        };
        */

        # ... and also report on missed calls ... note that reporting
        # on missed calls is mutually exclusive with silent C timer
        setflag(MISSED_FLAG);

        # we now know we may, we know where, let it go out now!
        append_hf("P-hint: USRLOC\r\n");
        route(NAT_ROUTE);
}

#
# Forcing media relay if necesarry
#
route[NAT_ROUTE] {
    if (uri=~"[@:](192\.168\.|10\.|172\.(1[6-9]|2[0-9]|3[0-1])\.)" && !search("^Route:")) {
            sl_send_reply("479", "We don't forward to private IP addresses");
            break;
    };
    if (isflagset(NAT_FLAG)) {
	    if (!is_present_hf("P-RTP-Proxy")) {
            	force_rtp_proxy();
		append_hf("P-RTP-Proxy: YES\r\n");
	    };
            append_hf("P-NATed-Calee: Yes\r\n");
    };

    # nat processing of replies; apply to all transactions (for example,
    # re-INVITEs from public to private UA are hard to identify as
    # natted at the moment of request processing); look at replies

    t_on_reply(NAT_REPLY);

    if (!t_relay()) {
            sl_reply_error();
            break;
    };
}


onreply_route[NAT_REPLY] {
        # natted transaction ?
        if (isflagset(NAT_FLAG) && status =~ "(183)|2[0-9][0-9]") {
                fix_nated_contact();
                force_rtp_proxy();
        # otherwise, is it a transaction behind a NAT and we did not
        # know at time of request processing? (RFC1918 contacts)
        } else if (nat_uac_test("1")) {
                fix_nated_contact();
        };

        # keep Cisco gateway sending keep-alives
        if (isflagset(7) && status=~"2[0-9][0-9]") {   # flag(7) is mentioned NAT_FLAG ??
                remove_hf("Session-Expires");
                append_hf("Session-Expires: 60;refresher=UAC\r\n");
                fix_nated_sdp("1");
        };
}


#
# logic for calls to the PSTN
#
route[PSTN_ROUTE] {

        # discard non-PSTN methods
        if (!(method == "INVITE" || method == "ACK" || method == "CANCEL" || method == "OPTIONS" || method == "BYE")) {
                sl_send_reply("500", "only VoIP methods accepted for GW");
                break;
        };

        # turn accounting on
        setflag(ACC_FLAG);

        # continue with requests to PSTN gateway ...

        # no authentication needed if the destination is on our free-pstn
        # list or if the caller is the digest-less gateway
        #
        # apply ACLs only to INVITEs -- we don't need to protect other
        # requests, as they don't imply charges; also it could cause troubles
        # when a call comes in via PSTN and goes to a party that can't
        # authenticate (voicemail, other domain) -- BYEs would fail then
        if (method == "INVITE") {
		if (!is_user_in("Request-URI", "free-pstn")) {
                	if (!proxy_authorize("DIGEST_REALM", "subscriber"))  {
                        	proxy_challenge("DIGEST_REALM", "0");
                        	break;
                	};

                	# let's check from=id ... avoids accounting confusion
                	if (!check_from()) {
                        	log("LOG: From Cheating attempt\n");
                        	sl_send_reply("403", "That is ugly -- use From=id next time (gw)");
                        	break;
                	};
		} else {
			# Allow free-pstn destinations without any checks
			route(PSTN2_ROUTE);
			break;
		};

		if (uri =~ "^sip:00[1-9][0-9]+@") {
			if (!is_user_in("credentials", "int")) {
			    sl_send_reply("403", "International numbers not allowed");
			    break;
			};
			route(PSTN2_ROUTE);
		} else {
			sl_send_reply("403", "Invalid Number");
			break;
		};
        }; # authorized PSTN
	break;
}

route[PSTN2_ROUTE] {
	rewritehostport("GW_IP_1:5060");
	consume_credentials();
	append_hf("P-Hint: GATEWAY\r\n");

	# Try alternative gateway on failure
	t_on_failure(PSTN_FAILURE);
        # Our PSTN gateway is symmetric and can handle direction=active flag
        # properly, therefore we don't have to use RTP proxy
	t_relay();
}



failure_route[PSTN_FAILURE] {
	rewritehostport("GW_IP_2:5060");
	append_branch();
	t_relay();	
}


# ------------- handling of unavailable user ------------------
route[VOICEMAIL_ROUTE] {
        # message store
        if (method == "MESSAGE") {
                if (!t_newtran()) {
                        sl_reply_error();
                        break;
                };

                if (m_store("0")) {
                        t_reply("202", "Accepted for Later Delivery");
                        break;
                };

                t_reply("503", "Service Unavailable");
                break;
        };

        # non-Voip -- just send "off-line"
        if (!(method == "INVITE" || method == "ACK" || method == "CANCEL")) {
                sl_send_reply("404", "Not Found");
                break;
        };

        if (t_newtran()) {
                if (method == "ACK") {
                        log(1, "CAUTION: strange thing: ACK passed t_newtran\n");
                        break;
                };

                t_reply("404", "Not Found");
        };

        # we account missed incoming calls; previous statteful processing
        # guarantees that retransmissions are not accounted
        if (method == "INVITE") {
                acc_log_request("404 missed call\n");
                acc_db_request("404 missed call", "missed_calls");
        };
}
