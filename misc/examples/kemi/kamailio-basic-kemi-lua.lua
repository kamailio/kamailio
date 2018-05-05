-- Kamailio - equivalent of routing blocks in Lua
--
-- KSR - the new dynamic object exporting Kamailio functions (kemi)
-- sr - the old static object exporting Kamailio functions
--

-- Relevant remarks:
--  * do not execute Lua 'exit' - that will kill Lua interpreter which is
--  embedded in Kamailio, resulting in killing Kamailio
--  * use KSR.x.exit() to trigger the stop of executing the script
--  * KSR.drop() is only marking the SIP message for drop, but doesn't stop
--  the execution of the script. Use KSR.x.exit() after it or KSR.x.drop()
--


-- global variables corresponding to defined values (e.g., flags) in kamailio.cfg
FLT_ACC=1
FLT_ACCMISSED=2
FLT_ACCFAILED=3
FLT_NATS=5

FLB_NATB=6
FLB_NATSIPPING=7

-- SIP request routing
-- equivalent of request_route{}
function ksr_request_route()

	-- per request initial checks
	ksr_route_reqinit();

	-- NAT detection
	ksr_route_natdetect();

	-- CANCEL processing
	if KSR.pv.get("$rm") == "CANCEL" then
		if KSR.tm.t_check_trans()>0 then
			ksr_route_relay();
		end
		return 1;
	end

	-- handle requests within SIP dialogs
	ksr_route_withindlg();

	-- -- only initial requests (no To tag)

	-- handle retransmissions
	if KSR.tmx.t_precheck_trans()>0 then
		KSR.tm.t_check_trans();
		return 1;
	end
	if KSR.tm.t_check_trans()==0 then return 1 end

	-- authentication
	ksr_route_auth();

	-- record routing for dialog forming requests (in case they are routed)
	-- - remove preloaded route headers
	KSR.hdr.remove("Route");
	if string.find("INVITE|SUBSCRIBE", KSR.pv.get("$rm")) then
		KSR.rr.record_route();
	end

	-- account only INVITEs
	if KSR.pv.get("$rm")=="INVITE" then
		KSR.setflag(FLT_ACC); -- do accounting
	end

	-- dispatch requests to foreign domains
	ksr_route_sipout();

	-- -- requests for my local domains

	-- handle registrations
	ksr_route_registrar();

	if KSR.pv.is_null("$rU") then
		-- request with no Username in RURI
		KSR.sl.sl_send_reply(484,"Address Incomplete");
		return 1;
	end

	-- user location service
	ksr_route_location();

	return 1;
end

-- wrapper around tm relay function
function ksr_route_relay()
	-- enable additional event routes for forwarded requests
	-- - serial forking, RTP relaying handling, a.s.o.
	if string.find("INVITE,BYE,SUBSCRIBE,UPDATE", KSR.pv.get("$rm")) then
		if KSR.tm.t_is_set("branch_route")<0 then
			KSR.tm.t_on_branch("ksr_branch_manage");
		end
	end
	if string.find("INVITE,SUBSCRIBE,UPDATE", KSR.pv.get("$rm")) then
		if KSR.tm.t_is_set("onreply_route")<0 then
			KSR.tm.t_on_reply("ksr_onreply_manage");
		end
	end

	if KSR.pv.get("$rm")=="INVITE" then
		if KSR.tm.t_is_set("failure_route")<0 then
			KSR.tm.t_on_failure("ksr_failure_manage");
		end
	end

	if KSR.tm.t_relay()<0 then
		KSR.sl.sl_reply_error();
	end
	KSR.x.exit();
end


-- Per SIP request initial checks
function ksr_route_reqinit()
	if not KSR.is_myself(KSR.pv.get("$si")) then
		if not KSR.pv.is_null("$sht(ipban=>$si)") then
			-- ip is already blocked
			KSR.dbg("request from blocked IP - " .. KSR.pv.get("$rm")
					.. " from " .. KSR.pv.get("$fu") .. " (IP:"
					.. KSR.pv.get("$si") .. ":" .. KSR.pv.get("$sp") .. ")\n");
			KSR.x.exit();
		end
		if KSR.pike.pike_check_req()<0 then
			KSR.err("ALERT: pike blocking " .. KSR.pv.get("$rm")
					.. " from " .. KSR.pv.get("$fu") .. " (IP:"
					.. KSR.pv.get("$si") .. ":" .. KSR.pv.get("$sp") .. ")\n");
			KSR.pv.seti("$sht(ipban=>$si)", 1);
			KSR.x.exit();
		end
	end
	if (not KSR.pv.is_null("$ua"))
			and (string.find(KSR.pv.get("$ua"), "friendly-scanner")
				or string.find(KSR.pv.get("$ua"), "sipcli")) then
		KSR.sl.sl_send_reply(200, "OK");
		KSR.x.exit();
	end

	if KSR.maxfwd.process_maxfwd(10) < 0 then
		KSR.sl.sl_send_reply(483,"Too Many Hops");
		KSR.x.exit();
	end

	if KSR.pv.get("$rm")=="OPTIONS"
			and KSR.is_myself(KSR.pv.get("$ru"))
			and KSR.pv.is_null("$rU") then
		KSR.sl.sl_send_reply(200,"Keepalive");
		KSR.x.exit();
	end

	if KSR.sanity.sanity_check(1511, 7)<0 then
		KSR.err("Malformed SIP message from "
				.. KSR.pv.get("$si") .. ":" .. KSR.pv.get("$sp") .."\n");
		KSR.x.exit();
	end

end


-- Handle requests within SIP dialogs
function ksr_route_withindlg()
	if KSR.siputils.has_totag()<0 then return 1; end

	-- sequential request withing a dialog should
	-- take the path determined by record-routing
	if KSR.rr.loose_route()>0 then
		ksr_route_dlguri();
		if KSR.pv.get("$rm")=="BYE" then
			KSR.setflag(FLT_ACC); -- do accounting ...
			KSR.setflag(FLT_ACCFAILED); -- ... even if the transaction fails
		elseif KSR.pv.get("$rm")=="ACK" then
			-- ACK is forwarded statelessly
			ksr_route_natmanage();
		elseif  KSR.pv.get("$rm")=="NOTIFY" then
			-- Add Record-Route for in-dialog NOTIFY as per RFC 6665.
			KSR.rr.record_route();
		end
		ksr_route_relay();
		KSR.x.exit();
	end
	if KSR.pv.get("$rm")=="ACK" then
		if KSR.tm.t_check_trans() >0 then
			-- no loose-route, but stateful ACK;
			-- must be an ACK after a 487
			-- or e.g. 404 from upstream server
			ksr_route_relay();
			KSR.x.exit();
		else
			-- ACK without matching transaction ... ignore and discard
			KSR.x.exit();
		end
	end
	KSR.sl.sl_send_reply(404, "Not here");
	KSR.x.exit();
end

-- Handle SIP registrations
function ksr_route_registrar()
	if KSR.pv.get("$rm")~="REGISTER" then return 1; end
	if KSR.isflagset(FLT_NATS) then
		KSR.setbflag(FLB_NATB);
		-- do SIP NAT pinging
		KSR.setbflag(FLB_NATSIPPING);
	end
	if KSR.registrar.save("location", 0)<0 then
		KSR.sl.sl_reply_error();
	end
	KSR.x.exit();
end

-- User location service
function ksr_route_location()
	local rc = KSR.registrar.lookup("location");
	if rc<0 then
		KSR.tm.t_newtran();
		if rc==-1 or rc==-3 then
			KSR.sl.send_reply("404", "Not Found");
			KSR.x.exit();
		elseif rc==-2 then
			KSR.sl.send_reply("405", "Method Not Allowed");
			KSR.x.exit();
		end
	end

	-- when routing via usrloc, log the missed calls also
	if KSR.pv.get("$rm")=="INVITE" then
		KSR.setflag(FLT_ACCMISSED);
	end

	ksr_route_relay();
	KSR.x.exit();
end


-- IP authorization and user uthentication
function ksr_route_auth()

	if KSR.pv.get("$rm")~="REGISTER" then
		if KSR.permissions.allow_source_address(1)>0 then
			-- source IP allowed
			return 1;
		end
	end

	if KSR.pv.get("$rm")=="REGISTER" or KSR.is_myself(KSR.pv.get("$fu")) then
		-- authenticate requests
		if KSR.auth_db.auth_check(KSR.pv.get("$fd"), "subscriber", 1)<0 then
			KSR.auth.auth_challenge(KSR.pv.get("$fd"), 0);
			KSR.x.exit();
		end
		-- user authenticated - remove auth header
		if not string.find("REGISTER,PUBLISH", KSR.pv.get("$rm")) then
			KSR.auth.consume_credentials();
		end
	end

	-- if caller is not local subscriber, then check if it calls
	-- a local destination, otherwise deny, not an open relay here
	if (not KSR.is_myself(KSR.pv.get("$fu"))
			and (not KSR.is_myself(KSR.pv.get("$ru")))) then
		KSR.sl.sl_send_reply(403,"Not relaying");
		KSR.x.exit();
	end

	return 1;
end

-- Caller NAT detection
function ksr_route_natdetect()
	KSR.force_rport();
	if KSR.nathelper.nat_uac_test(19)>0 then
		if KSR.pv.get("$rm")=="REGISTER" then
			KSR.nathelper.fix_nated_register();
		elseif KSR.siputils.is_first_hop()>0 then
			KSR.nathelper.set_contact_alias();
		end
		KSR.setflag(FLT_NATS);
	end
	return 1;
end

-- RTPProxy control
function ksr_route_natmanage()
	if KSR.siputils.is_request()>0 then
		if KSR.siputils.has_totag()>0 then
			if KSR.rr.check_route_param("nat=yes")>0 then
				KSR.setbflag(FLB_NATB);
			end
		end
	end
	if (not (KSR.isflagset(FLT_NATS) or KSR.isbflagset(FLB_NATB))) then
		return 1;
	end

	KSR.rtpproxy.rtpproxy_manage("co");

	if KSR.siputils.is_request()>0 then
		if not KSR.siputils.has_totag() then
			if KSR.tmx.t_is_branch_route()>0 then
				KSR.rr.add_rr_param(";nat=yes");
			end
		end
	end
	if KSR.siputils.is_reply()>0 then
		if KSR.isbflagset(FLB_NATB) then
			KSR.nathelper.set_contact_alias();
		end
	end
	return 1;
end

-- URI update for dialog requests
function ksr_route_dlguri()
	if not KSR.isdsturiset() then
		KSR.nathelper.handle_ruri_alias();
	end
	return 1;
end

-- Routing to foreign domains
function ksr_route_sipout()
	if KSR.is_myself(KSR.pv.get("$ru")) then return 1; end

	KSR.hdr.append("P-Hint: outbound\r\n");
	ksr_route_relay();
	KSR.x.exit();
end

-- Manage outgoing branches
-- equivalent of branch_route[...]{}
function ksr_branch_manage()
	KSR.dbg("new branch [".. KSR.pv.get("$T_branch_idx")
				.. "] to ".. KSR.pv.get("$ru") .. "\n");
	ksr_route_natmanage();
	return 1;
end

-- Manage incoming replies
-- equivalent of onreply_route[...]{}
function ksr_onreply_manage()
	KSR.dbg("incoming reply\n");
	local scode = KSR.pv.get("$rs");
	if scode>100 and scode<299 then
		ksr_route_natmanage();
	end
	return 1;
end

-- Manage failure routing cases
-- equivalent of failure_route[...]{}
function ksr_failure_manage()
	ksr_route_natmanage();

	if KSR.tm.t_is_canceled()>0 then
		return 1;
	end
	return 1;
end

-- SIP response handling
-- equivalent of reply_route{}
function ksr_reply_route()
	KSR.info("===== response - from kamailio lua script\n");
	return 1;
end
