
import java.lang.*;
import java.io.*; 

import org.siprouter.*;
import org.siprouter.NativeInterface.*;

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
	    switch (rank)
	    {
		case Ranks.PROC_MAIN:
		    LM_INFO("We're at PROC_MAIN\n");
		    break;
		case Ranks.PROC_TIMER:
		    LM_INFO("We're at PROC_TIMER\n");
		    break;
		case Ranks.PROC_RPC:
		    LM_INFO("We're at PROC_RPC/PROC_FIFO\n");
		    break;
		case Ranks.PROC_TCP_MAIN:
		    LM_INFO("We're at PROC_TCP_MAIN\n");
		    break;
		case Ranks.PROC_UNIXSOCK:
		    LM_INFO("We're at PROC_UNIXSOCK\n");
		    break;
		case Ranks.PROC_ATTENDANT:
		    LM_INFO("We're at PROC_ATTENDANT\n");
		    break;
		case Ranks.PROC_INIT:
		    LM_INFO("We're at PROC_INIT\n");
		    break;
		case Ranks.PROC_NOCHLDINIT:
		    LM_INFO("We're at PROC_NOCHLDINIT/PROC_MIN\n");
		    break;
		case Ranks.PROC_SIPINIT:
		    LM_INFO("We're at PROC_SIPINIT\n");
		    break;
		case Ranks.PROC_SIPRPC:
		    LM_INFO("We're at PROC_SIPRPC\n");
		    break;
	    }

	    return 1;
	}

	public int TestMethod()
	{

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

	    return 1;
	}
}















