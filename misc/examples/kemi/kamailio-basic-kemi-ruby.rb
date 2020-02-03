## Kamailio - equivalent of routing blocks in Ruby
##
## KSR - the new dynamic object exporting Kamailio functions

## Relevant remarks:
## KSR.x.exit() is not exported in ruby as per KEMI docs, so we use native exit
## Module names are capitalised and referenced with double colons - e.g. it's KSR::HDR.append instead of KSR.hdr.append in other KEMI languages

## Global variables corresponding to defined values (e.g., flags) in kamailio.cfg
$FLT_ACC =        1
$FLT_ACCMISSED =  2
$FLT_ACCFAILED =  3
$FLT_NATS =       5
$FLB_NATB =       6
$FLB_NATSIPPING = 7

def ksr_request_route()

  # Per request initial checks
  ksr_route_reqinit()

  ksr_route_natdetect()

  if KSR.is_CANCEL() then
    if KSR::TM.t_check_trans() > 0 then
      ksr_route_relay()
    end
    return
  end

  ksr_route_withindlg()

  # Retransmissions
  if !KSR.is_ACK() then
    if KSR::TMX.t_precheck_trans() > 0 then
      KSR::TM.t_check_trans()
      return
    end
    return if KSR::TM.t_check_trans() == 0
  end

  # Auth
  ksr_route_auth()
  
  # Record routing for dialog forming requests (in case they are routed)
  KSR::HDR.remove("Route")
  if KSR.is_method_in("IS") then
    KSR::RR.record_route()
  end

  KSR.setflag($FLT_ACC) if KSR.is_INVITE()

  # Dispatch to external
  ksr_route_sipout()

  # Registrations
  ksr_route_registrar()

  # USRLOC
  ksr_route_location()

  return
end

def ksr_route_reqinit()
  if KSR::COREX.has_user_agent() > 0 then
    ua = KSR::PV.gete("$ua");
    if ua.include? 'friendly-scanner' or ua.include? 'sipcli' then
      KSR::SL.sl_send_reply(200, "OK");
      exit
    end
  end

  if KSR::MAXFWD.process_maxfwd(10) < 0 then
    KSR::SL.sl_send_reply(483,"Too Many Hops");
    exit
  end

  if KSR.is_OPTIONS() and KSR.is_myself_ruri() and KSR::COREX.has_ruri_user() < 0 then
    KSR::SL.sl_send_reply(200, "Keepalive");
    exit
  end

  if KSR::SANITY.sanity_check(1511, 7) < 0 then
    KSR.err("Malformed SIP message from #{KSR::PV.get('$si')}:#{KSR::PV.get('$sp')}\n");
    exit
  end
end

def ksr_route_relay()
  if KSR.is_method_in("IBSU") then
    if KSR::TM.t_is_set("branch_route") < 0 then
      KSR::TM.t_on_branch("ksr_branch_manage")
    end
  end
  if KSR.is_method_in("ISU") then
    if KSR::TM.t_is_set("onreply_route") < 0 then
      KSR::TM.t_on_reply("ksr_onreply_manage")
    end
  end 

  if KSR.is_INVITE() then
    if KSR::TM.t_is_set("failure_route") < 0 then
      KSR::TM.t_on_failure("ksr_failure_manage")
    end
  end

  if KSR::TM.t_relay() < 0 then
    KSR::SL.sl_reply_error()
  end
  exit
end

def ksr_route_withindlg()
  return if KSR::SIPUTILS.has_totag() < 0

  if KSR::RR.loose_route() > 0 then
    ksr_route_dlguri()
    if KSR.is_BYE() then
      KSR.setflag($FLT_ACC)
      KSR.setflag($FLT_ACCFAILED)
    elsif KSR.is_ACK() then
      ksr_route_natmanage()
    elsif KSR.is_NOTIFY() then
      KSR::RR.record_route()
    end
    ksr_route_relay()
    exit
  end

  if KSR.is_ACK() then
    if KSR::TM.t_check_trans() > 0 then
      ksr_route_relay()
      exit
    else
      exit
    end
  end
  #KSR.info("404ing within dialog")
  KSR::SL.sl_send_reply(404, "Not here");
  exit
end

# IP authorization and user authenticaton
def ksr_route_auth()
	if !KSR.is_REGISTER() then
    # source IP allowed
    return if KSR::PERMISSIONS.allow_source_address(1).to_i > 0
	end

	if KSR.is_REGISTER() or KSR.is_myself_furi() then
		# auth requests
    if KSR::AUTH_DB.auth_check(KSR::PV.get("$fd"), "subscriber", 1).to_i < 0 then
			KSR::AUTH.auth_challenge(KSR::PV.get("$fd"), 0)
			exit
    end
		# user authenticated - remove auth header
		KSR::AUTH.consume_credentials() if !KSR.is_method_in("RP")
	end

	# if caller is not local subscriber, then check if it calls
	# a local destination, otherwise deny, not an open relay here
	if !KSR.is_myself_furi() && !KSR.is_myself_ruri() then
		KSR::SL.sl_send_reply(403,"Not relaying")
		exit
  end

	return
end

def ksr_route_natdetect()
  KSR.force_rport()
  if KSR::NATHELPER.nat_uac_test(19) > 0 then
    if KSR.is_REGISTER() then
      KSR::NATHELPER.fix_nated_register()
    elsif KSR::SIPUTILS.is_first_hop() > 0 then
      KSR::NATHELPER.set_contact_alias()
    end
    KSR.setflag($FLT_NATS)
  end
  return
end

def ksr_route_natmanage()
  if KSR::SIPUTILS.is_request() > 0 then
    if KSR::SIPUTILS.has_totag() > 0 then
      if KSR::RR.check_route_param("nat=yes") > 0 then
        KSR.info("Natmanage - nat=yes")
        KSR.setbflag($FLB_NATB)
      end
    end
  end
  #KSR.info("Natmange - returning if NAT flags set")
  return if !KSR.isflagset($FLT_NATS) and !KSR.isbflagset($FLB_NATB)
  #KSR.info("Natmanage - RTPPROXY from here on")
  KSR::RTPPROXY::RTPPROXY_manage("co");

  if KSR::SIPUTILS.is_request() > 0 then
    if !KSR::SIPUTILS.has_totag() then
      if KSR::TMX.t_is_branch_route() > 0 then
        KSR::RR.add_rr_param(";nat=yes")
      end
    end
  end
  if KSR::SIPUTILS.is_reply() > 0 then
    if KSR.isbflagset($FLB_NATB) then
      KSR::NATHELPER.set_contact_alias()
    end
  end
  return
end

def ksr_route_dlguri()
  KSR::NATHELPER.handle_ruri_alias() if !KSR.isdsturiset() 
	return
end

def ksr_branch_manage()
	KSR.dbg("new branch [#{KSR::PV.get("$T_branch_idx")}] to #{KSR::PV.get("$ru")}\n")
	ksr_route_natmanage()
	return
end

def ksr_onreply_manage()
  scode = KSR::PV.get("$rs");
  KSR.dbg("Reply status code is #{scode}")
	ksr_route_natmanage() if scode > 100 and scode <= 299
	return
end

def ksr_reply_route()
  #scode = KSR::PV.get("$rs");
  return
end

def ksr_onsend_route()
  return
end

def ksr_failure_manage()
	ksr_route_natmanage()
	return if KSR::TM.t_is_canceled() > 0
	return
end

def ksr_dialog_event(evname)
  KSR.info("===== dialog module triggered event: #{evname}");
  return
end

def ksr_tm_event(evname)
  KSR.info("===== TM module triggered event: #{evname}");
  return
end

def ksr_route_sipout()
  return if KSR.is_myself_ruri()
  KSR::HDR.append("P-Hint: outbound\r\n")
  ksr_route_relay()
  exit
end

def ksr_route_registrar()
	return if !KSR.is_REGISTER()
	if KSR.isflagset($FLT_NATS) then
		KSR.setbflag($FLB_NATB)
		# do SIP NAT pinging
    KSR.setbflag($FLB_NATSIPPING)
  end
  KSR.info("saving loc")
	if KSR::REGISTRAR.save("location", 0).to_i < 0 then
		KSR::SL.sl_reply_error()
  end
	exit
end

def ksr_route_location()
  rc = KSR::REGISTRAR.lookup("location").to_i
  if rc < 0 then
    KSR::TM.t_newtran()
    if rc == -1 or rc == -3 then
      KSR::SL.sl_send_reply(404, "Not Found")
			exit
    elsif rc == -2 then
			KSR::SL.sl_send_reply(405, "Method Not Allowed")
			exit
		end
  end

	# when routing via usrloc, log the missed calls also
	KSR.setflag($FLT_ACCMISSED) if KSR.is_INVITE()

	ksr_route_relay()
	exit
end
