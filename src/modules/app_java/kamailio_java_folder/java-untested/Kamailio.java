
import java.lang.*;
import java.io.*; 

import org.siprouter.*;
import org.siprouter.NativeInterface.*;

//import org.siprouter.CoreMethods.*;

public class Kamailio extends NativeMethods
{
	static
	{
	    System.load("/opt/kamailio/lib/kamailio/modules/app_java.so");
	}

	/* Constructor. Do not remove !!! */
	public Kamailio()
	{
	}


	public int child_init(int rank)
	{
	    return 1;
	}

	public int TestMethod()
	{

	    int retval = 0;
	    boolean boolstate = false;

/*
	    LM_INFO(String.format("Msg Type: %s\n", SipMsg.getMsgType()));

	    IPPair src = SipMsg.getSrcAddress();
	    if (src != null)
	    {
		LM_INFO(String.format("src address=%s, src port=%d\n", src.ip, src.port));
	    }
	    else
	    {
		LM_ERR("IPPair src is null!");
	    }

	    IPPair dst = SipMsg.getDstAddress();
	    if (dst != null)
	    {
		LM_INFO(String.format("dst address=%s, dst port=%d\n", dst.ip, dst.port));
	    }
	    else
	    {
		LM_ERR("IPPair dst is null!");
	    }

	    LM_INFO(String.format("buffer:\n%s\n", SipMsg.getBuffer().trim()));

	    SipMsg msg = SipMsg.ParseSipMsg();
	    if (msg != null)
	    {
		LM_INFO("msg:\n");
		LM_INFO(String.format("\tid=%d\n", msg.id));
		LM_INFO(String.format("\tpid=%d\n", msg.pid));
		LM_INFO(String.format("\teoh='%s'\n", msg.eoh));
		LM_INFO(String.format("\tunparsed='%s'\n", msg.unparsed));
		LM_INFO(String.format("\tbuf='%s'\n", msg.buf));
		LM_INFO(String.format("\tlen=%d\n", msg.len));
		LM_INFO(String.format("\tnew_uri='%s'\n", msg.new_uri));
		LM_INFO(String.format("\tdst_uri='%s'\n", msg.dst_uri));
		LM_INFO(String.format("\tparsed_uri_ok=%d\n", msg.parsed_uri_ok));
		LM_INFO(String.format("\tparsed_orig_ruri_ok=%d\n", msg.parsed_orig_ruri_ok));
		LM_INFO(String.format("\tadd_to_branch_s='%s'\n", msg.add_to_branch_s));
		LM_INFO(String.format("\tadd_to_branch_len=%d\n", msg.add_to_branch_len));
		LM_INFO(String.format("\thash_index=%d\n", msg.hash_index));
		LM_INFO(String.format("\tmsg_flags=%d\n", msg.msg_flags));
		LM_INFO(String.format("\tset_global_address='%s'\n", msg.set_global_address));
		LM_INFO(String.format("\tset_global_port='%s'\n", msg.set_global_port));
	    }
	    else
	    {
		LM_ERR("SipMsg msg is null!\n");
	    }

*/
//	    retval = KamExec("append_hf", "P-hint: VOICEMAIL\r\n");

//	    KamExec("sl_send_reply", "404", "Not relaying");

//	    retval = KamExec("is_method", "INVITE|SUBSCRIBE");

//	    LM_INFO(String.format("return value: %d\n", retval));




//	    retval = CoreMethods.rewriteuri("sip:0002@192.168.254.99:5060");


//	    retval = KamExec("rewriteuri", "sip:0002@192.168.254.99:5060");
//	    retval = KamExec("sl_send_reply", "404", "relaying failed");

//	    LM_INFO(String.format("return value: %d\n", retval));


//	    retval = CoreMethods.add_local_rport();
//	    retval = CoreMethods.drop();
//	    retval = CoreMethods.force_rport();
//	    retval = CoreMethods.force_send_socket("192.168.254.9", 50349);
//	    retval = CoreMethods.forward("198.61.206.9", 5060);
//	    retval = CoreMethods.forward();


//	    CoreMethods.setflag(3);

//	    boolstate = CoreMethods.isflagset(3);
//	    LM_INFO("return state: " + boolstate + "\n");

//	    retval = CoreMethods.revert_uri();
    
//	    retval = CoreMethods.route("NATDETECT");
//	    retval = CoreMethods.route("5");

	    retval = KamExec("is_method", "INVITE");

	    LM_INFO(String.format("return value: %d\n", retval));



	    return 1;
	}


	public static final int FLT_ACC		= 1;
	public static final int FLT_ACCMISSED	= 2;
	public static final int FLT_ACCFAILED	= 3;
	public static final int FLT_NATS	= 5;
	public static final int FLB_NATB	= 6;
	public static final int FLB_NATSIPPING	= 7;


	/// route ///
	public int route()
	{
	    return 1;
	}



	/// request_route ///
	public int request_route()
	{

	    // per request initial checks
	    CoreMethods.route("REQINIT");

	    // NAT detection
    	    CoreMethods.route("NATDETECT");

	    // CANCEL processing
    	    if (WrappedMethods.is_method("CANCEL"))
    	    {
                if (WrappedMethods.t_check_trans())
		{
                    WrappedMethods.t_relay();
		}

                return 1;
    	    }

/*


    	    // handle requests within SIP dialogs
    	    CoreMethods.route("WITHINDLG");

	    //### only initial requests (no To tag)
	    WrappedMethods.t_check_trans();

	    // authentication
	    CoreMethods.route("AUTH");

    	    // record routing for dialog forming requests (in case they are routed)
	    // - remove preloaded route headers
	    WrappedMethods.remove_hf("Route");
	    if (WrappedMethods.is_method("INVITE|SUBSCRIBE"))
	    {
                WrappedMethods.record_route();
	    }

	    // account only INVITEs
	    if (WrappedMethods.is_method("INVITE"))
    	    {
        	CoreMethods.setflag(FLT_ACC); // do accounting
	    }

	    // dispatch requests to foreign domains
	    CoreMethods.route("SIPOUT");

	    // ### requests for my local domains

	    // handle presence related requests
	    CoreMethods.route("PRESENCE");

	    String ruri = SipMsg.getRURI();
    	    if (ruri == null || ruri.length() <= 0)
    	    {
                // request with no Username in RURI
                WrappedMethods.sl_send_reply("484", "Address Incomplete");
                return 1;
    	    }

	    // dispatch destinations to PSTN
    	    CoreMethods.route("PSTN");

	    // user location service
	    CoreMethods.route("LOCATION");

	    CoreMethods.route("RELAY");

*/

	    return 1;
	}








	public int Route_REQINIT()
	{
	    return 1;
	}

	public int Route_NATDETECT()
	{
	    return 1;
	}

	public int Route_WITHINDLG()	
	{
	    if (WrappedMethods.has_totag())
	    {
		// sequential request withing a dialog should
                // take the path determined by record-routing

		if (WrappedMethods.loose_route())
		{
		    CoreMethods.route("DLGURI");

		    if (WrappedMethods.is_method("BYE"))
		    {
			CoreMethods.setflag(FLT_ACC);		// do accounting ...
			CoreMethods.setflag(FLT_ACCFAILED);	// ... even if the transaction fails
		    }
		    else if (WrappedMethods.is_method("ACK"))
		    {
			// ACK is forwarded statelessy
			CoreMethods.route("NATMANAGE");
		    }
		    else if (WrappedMethods.is_method("NOTIFY"))
		    {
			// Add Record-Route for in-dialog NOTIFY as per RFC 6665.
			WrappedMethods.record_route();
		    }

		    CoreMethods.route("RELAY");
		}
		else
		{
/* // this block would not work -- 'uri' and 'myself' aren't implemented yet //
                        if (is_method("SUBSCRIBE") && uri == myself) {
                                # in-dialog subscribe requests
                                route(PRESENCE);
                                exit;
                        }
*/
			if (WrappedMethods.is_method("ACK"))
			{
			    if (WrappedMethods.t_check_trans())
			    {
                                // no loose-route, but stateful ACK;
                                // must be an ACK after a 487
                                // or e.g. 404 from upstream server
                                WrappedMethods.t_relay();
			    }
			    else
			    {
				// ACK without matching transaction ... ignore and discard
				return 1;
			    }	
			}
			WrappedMethods.sl_send_reply("404", "Not here");
		}

	    }


	    return 1;
	}

	public int Route_AUTH()
	{
	    return 1;
	}

	public int Route_SIPOUT()
	{
	    return 1;
	}

	public int Route_PRESENCE()
	{
	    return 1;
	}

	public int Route_PSTN()
	{
	    return 1;
	}

	public int Route_LOCATION()
	{
	    return 1;
	}

	public int Route_RELAY()
	{
	    // enable additional event routes for forwarded requests
	    //  - serial forking, RTP relaying handling, a.s.o.
    	    if (WrappedMethods.is_method("INVITE|SUBSCRIBE"))
	    {
////                WrappedMethods.t_on_branch("MANAGE_BRANCH");
////                WrappedMethods.t_on_reply("MANAGE_REPLY");
    	    }
	    
	    if (WrappedMethods.is_method("INVITE"))
	    {
////                WrappedMethods.t_on_failure("MANAGE_FAILURE");
    	    }

    	    if (!WrappedMethods.t_relay())
	    {
                WrappedMethods.sl_reply_error();
	    }

	    return 1;
	}

	public int Route_REGISTRAR()
	{
	    if (WrappedMethods.is_method("REGISTER"))
	    {
		if (CoreMethods.isflagset(FLT_NATS))
		{
		    CoreMethods.setflag(FLB_NATB);
		    // uncomment next line to do SIP NAT pinging
		    // CoreMethods.setflag(FLB_NATSIPPING);
		}

		if (!WrappedMethods.save("location"))
		{
		    WrappedMethods.sl_reply_error();
		}
	    }

	    return 1;
	}

}















