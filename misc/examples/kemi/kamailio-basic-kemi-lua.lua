-- Kamailio - equivalent of routing blocks in Lua
--
-- KSR - the object exporting Kamailio KEMI functions (app_lua module)
-- sr - the old object exporting Kamailio functions (app_lua_sr module)
--

-- Relevant remarks:
--  * do not execute Lua 'exit' - that will kill Lua interpreter which is
--  embedded in Kamailio, resulting in killing Kamailio
--  * use KSR.x.exit() to trigger the stop of executing the script
--  * KSR.drop() is only marking the SIP message for drop, but doesn't stop
--  the execution of the script. Use KSR.x.exit() after it or KSR.x.drop()
--
-- Hints:
--  * Lua syntax check: luac -p /path/to/script.lua
--

-- debug callback function to print details of execution trace
--[[
local ksr_exec_level=0

local function ksr_exec_hook(event)
	local s = "";
	local t = debug.getinfo(3)
	s = s .. ksr_exec_level .. ">>> " .. string.rep(" ", ksr_exec_level);
	if t~=nil and t.currentline>=0 then
		s = s .. t.short_src .. ":" .. t.currentline .. " ";
	end
	t=debug.getinfo(2)
	if event=="call" then
		ksr_exec_level = ksr_exec_level + 1;
	else
		ksr_exec_level = ksr_exec_level - 1;
		if ksr_exec_level < 0 then
			ksr_exec_level = 0;
		end
	end
	if t.what=="main" then
		if event=="call" then
			s = s .. "begin " .. t.short_src;
		else
			s = s .. "end " .. t.short_src;
		end
	elseif t.what=="Lua" then
		s = s .. event .. " " .. t.name or "(Lua)" .. " <" .. t.linedefined .. ":" .. t.short_src .. ">";
	else
		s = s .. event .. " " .. t.name or "(C)" .. " [" .. t.what .. "] ";
	end
	KSR.info(s .. "\n");
end

debug.sethook(ksr_exec_hook, "cr")
ksr_exec_level=0
]]--

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
	if KSR.is_CANCEL() then
		if KSR.tm.t_check_trans()>0 then
			ksr_route_relay();
		end
		return 1;
	end

	-- handle retransmissions
	if not KSR.is_ACK() then
		if KSR.tmx.t_precheck_trans()>0 then
			KSR.tm.t_check_trans();
			return 1;
		end
		if KSR.tm.t_check_trans()==0 then return 1 end
	end

	-- handle requests within SIP dialogs
	ksr_route_withindlg();

	-- -- only initial requests (no To tag)

	-- authentication
	ksr_route_auth();

	-- record routing for dialog forming requests (in case they are routed)
	-- - remove preloaded route headers
	KSR.hdr.remove("Route");
	-- if INVITE or SUBSCRIBE
	if KSR.is_method_in("IS") then
		KSR.rr.record_route();
	end

	-- account only INVITEs
	if KSR.is_INVITE() then
		KSR.setflag(FLT_ACC); -- do accounting
	end

	-- dispatch requests to foreign domains
	ksr_route_sipout();

	-- -- requests for my local domains

	-- handle registrations
	ksr_route_registrar();

	if KSR.corex.has_ruri_user() < 0 then
		-- request with no Username in RURI
		KSR.sl.sl_send_reply(484, "Address Incomplete");
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
	if KSR.is_method_in("IBSU") then
		if KSR.tm.t_is_set("branch_route")<0 then
			KSR.tm.t_on_branch("ksr_branch_manage");
		end
	end
	if KSR.is_method_in("ISU") then
		if KSR.tm.t_is_set("onreply_route")<0 then
			KSR.tm.t_on_reply("ksr_onreply_manage");
		end
	end

	if KSR.is_INVITE() then
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
	-- no connect for sending replies
	KSR.set_reply_no_connect();
	-- enforce symmetric signaling
	-- send back replies to the source address of request
	KSR.force_rport();
	if not KSR.is_myself_srcip() then
		local srcip = KSR.kx.get_srcip();
		if KSR.htable.sht_match_name("ipban", "eq", srcip) > 0 then
			-- ip is already blocked
			KSR.dbg("request from blocked IP - " .. KSR.kx.get_method()
					.. " from " .. KSR.kx.get_furi() .. " (IP:"
					.. srcip .. ":" .. KSR.kx.get_srcport() .. ")\n");
			KSR.x.exit();
		end
		if KSR.pike.pike_check_req() < 0 then
			KSR.err("ALERT: pike blocking " .. KSR.kx.get_method()
					.. " from " .. KSR.kx.get_furi() .. " (IP:"
					.. srcip .. ":" .. KSR.kx.get_srcport() .. ")\n");
			KSR.htable.sht_seti("ipban", srcip, 1);
			KSR.x.exit();
		end
	end
	local ua = KSR.kx.gete_ua();
	if string.find(ua, "friendly") or string.find(ua, "scanner")
			or string.find(ua, "sipcli") or string.find(ua, "sipvicious") then
		KSR.sl.sl_send_reply(200, "OK");
		KSR.x.exit();
	end

	if KSR.maxfwd.process_maxfwd(10) < 0 then
		KSR.sl.sl_send_reply(483, "Too Many Hops");
		KSR.x.exit();
	end

	if KSR.is_OPTIONS()
			and KSR.is_myself_ruri()
			and KSR.corex.has_ruri_user() < 0 then
		KSR.sl.sl_send_reply(200, "Keepalive");
		KSR.x.exit();
	end

	if KSR.sanity.sanity_check(17895, 7)<0 then
		KSR.err("malformed SIP message from "
				.. KSR.kx.get_srcip() .. ":" .. KSR.kx.get_srcport() .."\n");
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
		if KSR.is_BYE() then
			KSR.setflag(FLT_ACC); -- do accounting ...
			KSR.setflag(FLT_ACCFAILED); -- ... even if the transaction fails
		elseif KSR.is_ACK() then
			-- ACK is forwarded statelessly
			ksr_route_natmanage();
		elseif KSR.is_NOTIFY() then
			-- Add Record-Route for in-dialog NOTIFY as per RFC 6665.
			KSR.rr.record_route();
		end
		ksr_route_relay();
		KSR.x.exit();
	end
	if KSR.is_ACK() then
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
	if not KSR.is_REGISTER() then return 1; end
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
			KSR.sl.send_reply(404, "Not Found");
			KSR.x.exit();
		elseif rc==-2 then
			KSR.sl.send_reply(405, "Method Not Allowed");
			KSR.x.exit();
		end
	end

	-- when routing via usrloc, log the missed calls also
	if KSR.is_INVITE() then
		KSR.setflag(FLT_ACCMISSED);
	end

	ksr_route_relay();
	KSR.x.exit();
end


-- IP authorization and user authentication
function ksr_route_auth()
	if not KSR.auth then
		return 1;
	end

	if KSR.permissions and not KSR.is_REGISTER() then
		if KSR.permissions.allow_source_address(1)>0 then
			-- source IP allowed
			return 1;
		end
	end

	if KSR.is_REGISTER() or KSR.is_myself_furi() then
		-- authenticate requests
		if KSR.auth_db.auth_check(KSR.kx.gete_fhost(), "subscriber", 1)<0 then
			KSR.auth.auth_challenge(KSR.kx.gete_fhost(), 0);
			KSR.x.exit();
		end
		-- user authenticated - remove auth header
		if not KSR.is_method_in("RP") then
			KSR.auth.consume_credentials();
		end
	end

	-- if caller is not local subscriber, then check if it calls
	-- a local destination, otherwise deny, not an open relay here
	if (not KSR.is_myself_furi())
			and (not KSR.is_myself_ruri()) then
		KSR.sl.sl_send_reply(403, "Not relaying");
		KSR.x.exit();
	end

	return 1;
end

-- Caller NAT detection
function ksr_route_natdetect()
	if not KSR.nathelper then
		return 1;
	end
	if KSR.nathelper.nat_uac_test(19)>0 then
		if KSR.is_REGISTER() then
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
	if not KSR.rtpproxy then
		return 1;
	end
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
		if KSR.siputils.has_totag()<0 then
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
	if not KSR.nathelper then
		return 1;
	end
	if not KSR.isdsturiset() then
		KSR.nathelper.handle_ruri_alias();
	end
	return 1;
end

-- Routing to foreign domains
function ksr_route_sipout()
	if KSR.is_myself_ruri() then return 1; end

	KSR.hdr.append("P-Hint: outbound\r\n");
	ksr_route_relay();
	KSR.x.exit();
end

-- Manage outgoing branches
-- equivalent of branch_route[...]{}
function ksr_branch_manage()
	KSR.dbg("new branch [".. KSR.pv.get("$T_branch_idx")
				.. "] to " .. KSR.kx.get_ruri() .. "\n");
	ksr_route_natmanage();
	return 1;
end

-- Manage incoming replies
-- equivalent of onreply_route[...]{}
function ksr_onreply_manage()
	KSR.dbg("incoming reply\n");
	local scode = KSR.kx.get_status();
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
	KSR.dbg("response - from kamailio lua script\n");
	if KSR.sanity.sanity_check(17604, 6)<0 then
		KSR.err("malformed SIP response from "
				.. KSR.kx.get_srcip() .. ":" .. KSR.kx.get_srcport() .."\n");
		KSR.x.drop();
	end
	return 1;
end
