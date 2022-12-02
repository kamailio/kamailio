## Kamailio - equivalent of routing blocks in Python
##
## KSR - the new dynamic object exporting Kamailio functions
## Router - the old object exporting Kamailio functions
##

## Relevant remarks:
##  * return code -255 is used to propagate the 'exit' behaviour to the
##  parent route block function. The alternative is to use the native
##  Python function sys.exit() (or exit()) -- it throws an exception that
##  is caught by Kamailio and prevents the stop of the interpreter.


import sys
import KSR as KSR

# global variables corresponding to defined values (e.g., flags) in kamailio.cfg
FLT_ACC=1
FLT_ACCMISSED=2
FLT_ACCFAILED=3
FLT_NATS=5

FLB_NATB=6
FLB_NATSIPPING=7


# SIP request routing
# -- equivalent of request_route{}
def ksr_request_route():
    # KSR.info("===== request - from kamailio python script\n")
    # KSR.info("===== method [%s] r-uri [%s]\n" % (KSR.pv.get("$rm"),KSR.pv.get("$ru")))

    # per request initial checks
    if ksr_route_reqinit()==-255 :
        return 1

    # NAT detection
    if ksr_route_natdetect()==-255 :
        return 1

    # CANCEL processing
    if KSR.is_CANCEL() :
        if KSR.tm.t_check_trans()>0 :
            ksr_route_relay()
        return 1

    # handle requests within SIP dialogs
    if ksr_route_withindlg()==-255 :
        return 1

    # -- only initial requests (no To tag)

    # handle retransmissions
    if KSR.tmx.t_precheck_trans()>0 :
        KSR.tm.t_check_trans()
        return 1

    if KSR.tm.t_check_trans()==0 :
        return 1

    # authentication
    if ksr_route_auth()==-255 :
        return 1

    # record routing for dialog forming requests (in case they are routed)
    # - remove preloaded route headers
    KSR.hdr.remove("Route")
    if KSR.is_method_in("IS") :
        KSR.rr.record_route()


    # account only INVITEs
    if KSR.pv.get("$rm")=="INVITE" :
        KSR.setflag(FLT_ACC); # do accounting


    # dispatch requests to foreign domains
    if ksr_route_sipout()==-255 :
        return 1

    # # requests for my local domains

    # handle registrations
    if ksr_route_registrar()==-255 :
        return 1

    if KSR.corex.has_ruri_user() < 0 :
        # request with no Username in RURI
        KSR.sl.sl_send_reply(484,"Address Incomplete")
        return 1


    # user location service
    ksr_route_location()

    return 1


# wrapper around tm relay function
def ksr_route_relay():
    # enable additional event routes for forwarded requests
    # - serial forking, RTP relaying handling, a.s.o.
    if KSR.is_method_in("IBSU") :
        if KSR.tm.t_is_set("branch_route")<0 :
            KSR.tm.t_on_branch("ksr_branch_manage")

    if KSR.is_method_in("ISU") :
        if KSR.tm.t_is_set("onreply_route")<0 :
            KSR.tm.t_on_reply("ksr_onreply_manage")

    if KSR.is_INVITE() :
        if KSR.tm.t_is_set("failure_route")<0 :
            KSR.tm.t_on_failure("ksr_failure_manage")

    if KSR.tm.t_relay()<0 :
        KSR.sl.sl_reply_error()

    return -255


# Per SIP request initial checks
def ksr_route_reqinit():
    srcip = KSR.pv.get("$si")
    if not KSR.is_myself(srcip) :
        if not KSR.pv.is_null("$sht(ipban=>$si)") :
            # ip is already blocked
            KSR.dbg("request from blocked IP - " + KSR.pv.get("$rm")
                    + " from " + KSR.pv.get("$fu") + " (IP:"
                    + KSR.pv.get("$si") + ":" + str(KSR.pv.get("$sp")) + ")\n")
            return -255

        if KSR.pike.pike_check_req()<0 :
            KSR.err("ALERT: pike blocking " + KSR.pv.get("$rm")
                    + " from " + KSR.pv.get("$fu") + " (IP:"
                    + KSR.pv.get("$si") + ":" + str(KSR.pv.get("$sp")) + ")\n")
            KSR.pv.seti("$sht(ipban=>$si)", 1)
            return -255

    if KSR.corex.has_user_agent() > 0 :
        ua = KSR.pv.gete("$ua")
        if (ua.find("friendly")!=-1 or ua.find("scanner")!=-1
                or ua.find("sipcli")!=-1 or ua.find("sipvicious")!=-1) :
            KSR.sl.sl_send_reply(200, "Processed")
            return -255

    if KSR.maxfwd.process_maxfwd(10) < 0 :
        KSR.sl.sl_send_reply(483,"Too Many Hops")
        return -255

    if (KSR.is_OPTIONS()
            and KSR.is_myself_ruri()
            and KSR.corex.has_ruri_user() < 0) :
        KSR.sl.sl_send_reply(200,"Keepalive")
        return -255

    if KSR.sanity.sanity_check(17895, 7)<0 :
        KSR.err("Malformed SIP message from "
                + KSR.pv.get("$si") + ":" + str(KSR.pv.get("$sp")) +"\n")
        return -255


# Handle requests within SIP dialogs
def ksr_route_withindlg():
    if KSR.siputils.has_totag()<0 :
        return 1

    # sequential request withing a dialog should
    # take the path determined by record-routing
    if KSR.rr.loose_route()>0 :
        if ksr_route_dlguri()==-255 :
            return -255
        if KSR.is_BYE() :
            # do accounting ...
            KSR.setflag(FLT_ACC)
            # ... even if the transaction fails
            KSR.setflag(FLT_ACCFAILED)
        elif KSR.is_ACK() :
            # ACK is forwarded statelessly
            if ksr_route_natmanage()==-255 :
                return -255
        elif KSR.is_NOTIFY() :
            # Add Record-Route for in-dialog NOTIFY as per RFC 6665.
            KSR.rr.record_route()

        ksr_route_relay()
        return -255

    if KSR.is_ACK() :
        if KSR.tm.t_check_trans() >0 :
            # no loose-route, but stateful ACK
            # must be an ACK after a 487
            # or e.g. 404 from upstream server
            ksr_route_relay()
            return -255
        else:
            # ACK without matching transaction ... ignore and discard
            return -255

    KSR.sl.sl_send_reply(404, "Not here")
    return -255


# Handle SIP registrations
def ksr_route_registrar():
    if not KSR.is_REGISTER() :
        return 1
    if KSR.isflagset(FLT_NATS) :
        KSR.setbflag(FLB_NATB)
        # do SIP NAT pinging
        KSR.setbflag(FLB_NATSIPPING)

    if KSR.registrar.save("location", 0)<0 :
        KSR.sl.sl_reply_error()

    return -255


# User location service
def ksr_route_location():
    rc = KSR.registrar.lookup("location")
    if rc<0 :
        KSR.tm.t_newtran()
        if rc==-1 or rc==-3 :
            KSR.sl.send_reply(404, "Not Found")
            return -255
        elif rc==-2 :
            KSR.sl.send_reply(405, "Method Not Allowed")
            return -255

    # when routing via usrloc, log the missed calls also
    if KSR.is_INVITE() :
        KSR.setflag(FLT_ACCMISSED)

    ksr_route_relay()
    return -255



# IP authorization and user uthentication
def ksr_route_auth():

    if not KSR.is_REGISTER() :
        if KSR.permissions.allow_source_address(1)>0 :
            # source IP allowed
            return 1

    if KSR.is_REGISTER() or KSR.is_myself_furi() :
        # authenticate requests
        if KSR.auth_db.auth_check(KSR.pv.get("$fd"), "subscriber", 1)<0 :
            KSR.auth.auth_challenge(KSR.pv.get("$fd"), 0)
            return -255

        # user authenticated - remove auth header
        if not KSR.is_method_in("RP") :
            KSR.auth.consume_credentials()

    # if caller is not local subscriber, then check if it calls
    # a local destination, otherwise deny, not an open relay here
    if (not KSR.is_myself_furi()) and (not KSR.is_myself_ruri()) :
        KSR.sl.sl_send_reply(403,"Not relaying")
        return -255

    return 1


# Caller NAT detection
def ksr_route_natdetect():
    KSR.force_rport()
    if KSR.nathelper.nat_uac_test(19)>0 :
        if KSR.is_REGISTER() :
            KSR.nathelper.fix_nated_register()
        elif KSR.siputils.is_first_hop()>0 :
            KSR.nathelper.set_contact_alias()

        KSR.setflag(FLT_NATS)

    return 1


# RTPProxy control
def ksr_route_natmanage():
    if KSR.siputils.is_request()>0 :
        if KSR.siputils.has_totag()>0 :
            if KSR.rr.check_route_param("nat=yes")>0 :
                KSR.setbflag(FLB_NATB)

    if (not (KSR.isflagset(FLT_NATS) or KSR.isbflagset(FLB_NATB))) :
        return 1

    KSR.rtpproxy.rtpproxy_manage("co")

    if KSR.siputils.is_request()>0 :
        if not KSR.siputils.has_totag() :
            if KSR.tmx.t_is_branch_route()>0 :
                KSR.rr.add_rr_param(";nat=yes")

    if KSR.siputils.is_reply()>0 :
        if KSR.isbflagset(FLB_NATB) :
            KSR.nathelper.set_contact_alias()

    return 1


# URI update for dialog requests
def ksr_route_dlguri():
    if not KSR.isdsturiset() :
        KSR.nathelper.handle_ruri_alias()

    return 1


# Routing to foreign domains
def ksr_route_sipout():
    if KSR.is_myself_ruri() :
        return 1

    KSR.hdr.append("P-Hint: outbound\r\n")
    ksr_route_relay()
    return -255


# Manage outgoing branches
# -- equivalent of branch_route[...]{}
def ksr_branch_manage():
    KSR.dbg("new branch ["+ str(KSR.pv.get("$T_branch_idx"))
                + "] to "+ KSR.pv.get("$ru") + "\n")
    ksr_route_natmanage()
    return 1


# Manage incoming replies
# -- equivalent of onreply_route[...]{}
def ksr_onreply_manage():
    KSR.dbg("incoming reply\n")
    scode = KSR.pv.get("$rs")
    if scode>100 and scode<299 :
        ksr_route_natmanage()

    return 1


# Manage failure routing cases
# -- equivalent of failure_route[...]{}
def ksr_failure_manage():
    if ksr_route_natmanage()==-255 : return 1

    if KSR.tm.t_is_canceled()>0 :
        return 1

    return 1


# SIP response handling
# -- equivalent of reply_route{}
def ksr_reply_route():
    KSR.dbg("response handling - python script\n")

    if KSR.sanity.sanity_check(17604, 6)<0 :
        KSR.err("Malformed SIP response from "
                + KSR.pv.get("$si") + ":" + str(KSR.pv.get("$sp")) +"\n")
        KSR.set_drop()
        return -255

    return 1




# global helper function for debugging purposes
def dumpObj(obj):
    for attr in dir(obj):
        KSR.info("obj.%s = %s\n" % (attr, getattr(obj, attr)))

