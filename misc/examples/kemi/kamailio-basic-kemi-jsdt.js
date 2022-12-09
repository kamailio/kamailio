// Kamailio - equivalent of routing blocks in JavaScript
//
// KSR - the new dynamic object exporting Kamailio functions
// sr - the old static object exporting Kamailio functions
//

// global variables corresponding to defined values (e.g., flags) in kamailio.cfg
var FLT_ACC=1
var FLT_ACCMISSED=2
var FLT_ACCFAILED=3
var FLT_NATS=5

var FLB_NATB=6
var FLB_NATSIPPING=7

// SIP request routing
// equivalent of request_route{}
function ksr_request_route()
{
	// KSR.sl.sl_send_reply(100,"Intelligent trying");
	// KSR.info("===== request - from kamailio javascript script\n");

	// per request initial checks
	ksr_route_reqinit();

	// NAT detection
	ksr_route_natdetect();

	// CANCEL processing
	if (KSR.is_CANCEL()) {
		if(KSR.tm.t_check_trans()>0) {
			ksr_route_relay();
		}
		return;
	}

	// handle retransmissions
	if (!KSR.is_ACK()) {
		if (KSR.tmx.t_precheck_trans()>0) {
			KSR.tm.t_check_trans();
			return;
		}
		if (KSR.tm.t_check_trans()==0) { return; }
	}

	// handle requests within SIP dialogs
	ksr_route_withindlg();

	// -- only initial requests (no To tag)

	// authentication
	ksr_route_auth();

	// record routing for dialog forming requests (in case they are routed)
	// - remove preloaded route headers
	KSR.hdr.remove("Route");
	if (KSR.is_method_in("IS")) {
		KSR.rr.record_route();
	}

	// account only INVITEs
	if (KSR.is_INVITE()) {
		KSR.setflag(FLT_ACC); // do accounting
	}

	// dispatch requests to foreign domains
	ksr_route_sipout();

	// -- requests for my local domains

	// handle registrations
	ksr_route_registrar();

	if (KSR.corex.has_ruri_user() < 0) {
		// request with no Username in RURI
		KSR.sl.sl_send_reply(484, "Address Incomplete");
		return;
	}

	// user location service
	ksr_route_location();

	return;
}

// wrapper around tm relay function
function ksr_route_relay()
{
	// enable additional event routes for forwarded requests
	// - serial forking, RTP relaying handling, a.s.o.
	if (KSR.is_method_in("IBSU")) {
		if (KSR.tm.t_is_set("branch_route")<0) {
			KSR.tm.t_on_branch("ksr_branch_manage");
		}
	}
	if (KSR.is_method_in("ISU")) {
		if (KSR.tm.t_is_set("onreply_route")<0) {
			KSR.tm.t_on_reply("ksr_onreply_manage");
		}
	}

	if (KSR.is_INVITE()) {
		if (KSR.tm.t_is_set("failure_route")<0) {
			KSR.tm.t_on_failure("ksr_failure_manage");
		}
	}

	if (KSR.tm.t_relay()<0) {
		KSR.sl.sl_reply_error();
	}
	KSR.x.exit();
}


// Per SIP request initial checks
function ksr_route_reqinit()
{
	if (!KSR.is_myself_srcip()) {
		var srcip = KSR.kx.get_srcip();
		if (KSR.htable.sht_match_name("ipban", "eq", srcip) > 0) {
			// ip is already blocked
			KSR.dbg("request from blocked IP - " + KSR.kx.get_method()
					+ " from " + KSR.kx.get_furi() + " (IP:"
					+ srcip + ":" + KSR.kx.get_srcport() + ")\n");
			KSR.x.exit();
		}
		if (KSR.pike.pike_check_req()<0) {
			KSR.err("ALERT: pike blocking " + KSR.kx.get_method()
					+ " from " + KSR.kx.get_furi() + " (IP:"
					+ srcip + ":" + KSR.kx.get_srcport() + ")\n");
			KSR.htable.sht_seti("ipban", srcip, 1);
			KSR.x.exit();
		}
	}
	if (KSR.corex.has_user_agent()>0) {
		var UA = KSR.kx.gete_ua();
		if (UA.indexOf("friendly")>=0 || UA.indexOf("scanner")>=0
				|| UA.indexOf("sipcli")>=0 || UA.indexOf("sipvicious")>=0) {
			KSR.sl.sl_send_reply(200, "OK");
			KSR.x.exit();
		}
	}

	if (KSR.maxfwd.process_maxfwd(10) < 0) {
		KSR.sl.sl_send_reply(483,"Too Many Hops");
		KSR.x.exit();
	}

	if (KSR.is_OPTIONS()
			&& KSR.is_myself_ruri()
			&& KSR.corex.has_ruri_user() < 0) {
		KSR.sl.sl_send_reply(200,"Keepalive");
		KSR.x.exit();
	}

	if (KSR.sanity.sanity_check(17895, 7)<0) {
		KSR.err("Malformed SIP message from "
				+ KSR.kx.get_srcip() + ":" + KSR.kx.get_srcport() + "\n");
		KSR.x.exit();
	}
}


// Handle requests within SIP dialogs
function ksr_route_withindlg()
{
	if (KSR.siputils.has_totag()<0) { return; }

	// sequential request within a dialog should
	// take the path determined by record-routing
	if (KSR.rr.loose_route()>0) {
		ksr_route_dlguri();
		if (KSR.is_BYE()) {
			KSR.setflag(FLT_ACC); // do accounting ...
			KSR.setflag(FLT_ACCFAILED); // ... even if the transaction fails
		} else if (KSR.is_ACK()) {
			// ACK is forwarded statelessly
			ksr_route_natmanage();
		} else if (KSR.is_NOTIFY()) {
			// Add Record-Route for in-dialog NOTIFY as per RFC 6665.
			KSR.rr.record_route();
		}
		ksr_route_relay();
		KSR.x.exit();
	}
	if (KSR.is_ACK()) {
		if (KSR.tm.t_check_trans() >0) {
			// no loose-route, but stateful ACK;
			// must be an ACK after a 487
			// or e.g. 404 from upstream server
			ksr_route_relay();
			KSR.x.exit();
		} else {
			// ACK without matching transaction ... ignore and discard
			KSR.x.exit();
		}
	}
	KSR.sl.sl_send_reply(404, "Not here");
	KSR.x.exit();
}

// Handle SIP registrations
function ksr_route_registrar()
{
	if (!KSR.is_REGISTER()) { return; }
	if (KSR.isflagset(FLT_NATS)) {
		KSR.setbflag(FLB_NATB);
		// do SIP NAT pinging
		KSR.setbflag(FLB_NATSIPPING);
	}
	if (KSR.registrar.save("location", 0)<0) {
		KSR.sl.sl_reply_error();
	}
	KSR.x.exit();
}

// User location service
function ksr_route_location()
{
	var rc = KSR.registrar.lookup("location");
	if (rc<0) {
		KSR.tm.t_newtran();
		if (rc==-1 || rc==-3) {
			KSR.sl.send_reply(404, "Not Found");
			KSR.x.exit();
		} else if (rc==-2) {
			KSR.sl.send_reply(405, "Method Not Allowed");
			KSR.x.exit();
		}
	}

	// when routing via usrloc, log the missed calls also
	if (KSR.is_INVITE()) {
		KSR.setflag(FLT_ACCMISSED);
	}

	ksr_route_relay();
	KSR.x.exit();
}


// IP authorization and user authentication
function ksr_route_auth()
{
	if (!KSR.is_REGISTER()) {
		if (KSR.permissions.allow_source_address(1)>0) {
			// source IP allowed
			return;
		}
	}

	if (KSR.is_REGISTER() || KSR.is_myself_furi()) {
		// authenticate requests
		if (KSR.auth_db.auth_check(KSR.kx.gete_fhost(), "subscriber", 1)<0) {
			KSR.auth.auth_challenge(KSR.kx.gete_fhost(), 0);
			KSR.x.exit();
		}
		// user authenticated - remove auth header
		if (!KSR.is_method_in("RP")) {
			KSR.auth.consume_credentials();
		}
	}

	// if caller is not local subscriber, then check if it calls
	// a local destination, otherwise deny, not an open relay here
	if ((!KSR.is_myself_furi())
			&& (!KSR.is_myself_ruri())) {
		KSR.sl.sl_send_reply(403,"Not relaying");
		KSR.x.exit();
	}

	return;
}

// Caller NAT detection
function ksr_route_natdetect()
{
	KSR.force_rport();
	if (KSR.nathelper.nat_uac_test(19)>0) {
		if (KSR.is_REGISTER()) {
			KSR.nathelper.fix_nated_register();
		} else if (KSR.siputils.is_first_hop()>0) {
			KSR.nathelper.set_contact_alias();
		}
		KSR.setflag(FLT_NATS);
	}
	return;
}

// RTPProxy control
function ksr_route_natmanage()
{
	if (KSR.siputils.is_request()>0) {
		if (KSR.siputils.has_totag()>0) {
			if (KSR.rr.check_route_param("nat=yes")>0) {
				KSR.setbflag(FLB_NATB);
			}
		}
	}
	if (! (KSR.isflagset(FLT_NATS) || KSR.isbflagset(FLB_NATB))) {
		return;
	}

	KSR.rtpproxy.rtpproxy_manage("co");

	if (KSR.siputils.is_request()>0) {
		if (! KSR.siputils.has_totag()) {
			if (KSR.tmx.t_is_branch_route()>0) {
				KSR.rr.add_rr_param(";nat=yes");
			}
		}
	}
	if (KSR.siputils.is_reply()>0) {
		if (KSR.isbflagset(FLB_NATB)) {
			KSR.nathelper.set_contact_alias();
		}
	}
	return;
}

// URI update for dialog requests
function ksr_route_dlguri()
{
	if (! KSR.isdsturiset()) {
		KSR.nathelper.handle_ruri_alias();
	}
	return;
}

// Routing to foreign domains
function ksr_route_sipout()
{
	if (KSR.is_myself_ruri()) { return; }

	KSR.hdr.append_hf("P-Hint: outbound\r\n");
	ksr_route_relay();
	KSR.x.exit();
}

// Manage outgoing branches
// equivalent of branch_route[...]{}
function ksr_branch_manage()
{
	KSR.dbg("new branch [" + KSR.tm.t_get_branch_index()
				+ "] to " + KSR.kx.get_ruri() + "\n");
	ksr_route_natmanage();
	return;
}

// Manage incoming replies
// equivalent of onreply_route[...]{}
function ksr_onreply_manage()
{
	KSR.dbg("incoming reply\n");
	var scode = KSR.kx.gets_status();
	if (scode>100 && scode<=299) {
		ksr_route_natmanage();
	}
	return;
}

// Manage failure routing cases
// equivalent of failure_route[...]{}
function ksr_failure_manage()
{
	ksr_route_natmanage();

	if (KSR.tm.t_is_canceled()>0) {
		return;
	}
	return;
}

// SIP response handling
// equivalent of reply_route{}
function ksr_reply_route()
{
	KSR.info("===== response - from kamailio JS script\n");
	return;
}
