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
	var METHOD = KSR.pv.get("$rm");
	// KSR.sl.sl_send_reply(100,"Intelligent trying");
	// KSR.info("===== request - from kamailio lua script\n");

	// per request initial checks
	ksr_route_reqinit();

	// NAT detection
	ksr_route_natdetect();

	// CANCEL processing
	if (METHOD == "CANCEL") {
		if(KSR.tm.t_check_trans()>0) {
			ksr_route_relay();
		}
		return;
	}

	// handle requests within SIP dialogs
	ksr_route_withindlg();

	// -- only initial requests (no To tag)

	// handle retransmissions
	if (KSR.tmx.t_precheck_trans()>0) {
		KSR.tm.t_check_trans();
		return;
	}
	if (KSR.tm.t_check_trans()==0) { return; }

	// authentication
	ksr_route_auth();

	// record routing for dialog forming requests (in case they are routed)
	// - remove preloaded route headers
	KSR.hdr.remove("Route");
	if (METHOD=="INVITE" || METHOD=="SUBSCRIBE") {
		KSR.rr.record_route();
	}

	// account only INVITEs
	if (METHOD=="INVITE") {
		KSR.setflag(FLT_ACC); // do accounting
	}

	// dispatch requests to foreign domains
	ksr_route_sipout();

	// -- requests for my local domains

	// handle registrations
	ksr_route_registrar();

	if (KSR.pv.is_null("$rU")) {
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
	var METHOD = KSR.pv.get("$rm");
	if (METHOD=="INVITE" || METHOD=="BYE" || METHOD=="SUBSCRIBE"
			|| METHOD=="UPDATE") {
		if (KSR.tm.t_is_set("branch_route")<0) {
			KSR.tm.t_on_branch("ksr_branch_manage");
		}
	}
	if (METHOD=="INVITE" || METHOD=="SUBSCRIBE" || METHOD=="UPDATE") {
		if (KSR.tm.t_is_set("onreply_route")<0) {
			KSR.tm.t_on_reply("ksr_onreply_manage");
		}
	}

	if (METHOD=="INVITE") {
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
	if (!KSR.is_myself(KSR.pv.get("$si"))) {
		if (!KSR.pv.is_null("$sht(ipban=>$si)")) {
			// ip is already blocked
			KSR.dbg("request from blocked IP - " + KSR.pv.get("$rm")
					+ " from " + KSR.pv.get("$fu") + " (IP:"
					+ KSR.pv.get("$si") + ":" + KSR.pv.get("$sp") + ")\n");
			KSR.x.exit();
		}
		if (KSR.pike.pike_check_req()<0) {
			KSR.err("ALERT: pike blocking " + KSR.pv.get("$rm")
					+ " from " + KSR.pv.get("$fu") + " (IP:"
					+ KSR.pv.get("$si") + ":" + KSR.pv.get("$sp") + ")\n");
			KSR.pv.seti("$sht(ipban=>$si)", 1);
			KSR.x.exit();
		}
	}
	if (!KSR.pv.is_null("$ua")) {
		var UA = KSR.pv.get("$ua");
		if (UA.indexOf("friendly-scanner")>=0 || UA.indexOf("sipcli")>=0) {
			KSR.sl.sl_send_reply(200, "OK");
			KSR.x.exit();
		}
	}

	if (KSR.maxfwd.process_maxfwd(10) < 0) {
		KSR.sl.sl_send_reply(483,"Too Many Hops");
		KSR.x.exit();
	}

	if (KSR.pv.get("$rm")=="OPTIONS"
			&& KSR.is_myself(KSR.pv.get("$ru"))
			&& KSR.pv.is_null("$rU")) {
		KSR.sl.sl_send_reply(200,"Keepalive");
		KSR.x.exit();
	}

	if (KSR.sanity.sanity_check(1511, 7)<0) {
		KSR.err("Malformed SIP message from "
				+ KSR.pv.get("$si") + ":" + KSR.pv.get("$sp") + "\n");
		KSR.x.exit();
	}
}


// Handle requests within SIP dialogs
function ksr_route_withindlg()
{
	if (KSR.siputils.has_totag()<0) { return; }

	var METHOD = KSR.pv.get("$rm");
	// sequential request withing a dialog should
	// take the path determined by record-routing
	if (KSR.rr.loose_route()>0) {
		ksr_route_dlguri();
		if (METHOD=="BYE") {
			KSR.setflag(FLT_ACC); // do accounting ...
			KSR.setflag(FLT_ACCFAILED); // ... even if the transaction fails
		} else if (METHOD=="ACK") {
			// ACK is forwarded statelessly
			ksr_route_natmanage();
		} else if (METHOD=="NOTIFY") {
			// Add Record-Route for in-dialog NOTIFY as per RFC 6665.
			KSR.rr.record_route();
		}
		ksr_route_relay();
		KSR.x.exit();
	}
	if (METHOD=="ACK") {
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
	if (KSR.pv.get("$rm")!="REGISTER") { return; }
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
			KSR.sl.send_reply("404", "Not Found");
			KSR.x.exit();
		} else if (rc==-2) {
			KSR.sl.send_reply("405", "Method Not Allowed");
			KSR.x.exit();
		}
	}

	// when routing via usrloc, log the missed calls also
	if (KSR.pv.get("$rm")=="INVITE") {
		KSR.setflag(FLT_ACCMISSED);
	}

	ksr_route_relay();
	KSR.x.exit();
}


// IP authorization and user uthentication
function ksr_route_auth()
{
	var METHOD = KSR.pv.get("$rm");
	if (METHOD!="REGISTER") {
		if (KSR.permissions.allow_source_address(1)>0) {
			// source IP allowed
			return;
		}
	}

	if (METHOD=="REGISTER" || KSR.is_myself(KSR.pv.get("$fu"))) {
		// authenticate requests
		if (KSR.auth_db.auth_check(KSR.pv.get("$fd"), "subscriber", 1)<0) {
			KSR.auth.auth_challenge(KSR.pv.get("$fd"), 0);
			KSR.x.exit();
		}
		// user authenticated - remove auth header
		if (METHOD!="REGISTER" && METHOD!="PUBLISH") {
			KSR.auth.consume_credentials();
		}
	}

	// if caller is not local subscriber, then check if it calls
	// a local destination, otherwise deny, not an open relay here
	if ((! KSR.is_myself(KSR.pv.get("$fu"))
			&& (! KSR.is_myself(KSR.pv.get("$ru"))))) {
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
		if (KSR.pv.get("$rm")=="REGISTER") {
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
	if (KSR.is_myself(KSR.pv.get("$ru"))) { return; }

	KSR.hdr.append_hf("P-Hint: outbound\r\n");
	ksr_route_relay();
	KSR.x.exit();
}

// Manage outgoing branches
// equivalent of branch_route[...]{}
function ksr_branch_manage()
{
	KSR.dbg("new branch [" + KSR.pv.get("$T_branch_idx")
				+ "] to " + KSR.pv.get("$ru") + "\n");
	ksr_route_natmanage();
	return;
}

// Manage incoming replies
// equivalent of onreply_route[...]{}
function ksr_onreply_manage()
{
	KSR.dbg("incoming reply\n");
	var scode = KSR.pv.get("$rs");
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
