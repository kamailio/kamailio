
package org.siprouter;
import java.lang.*;

public abstract class SipMsg
{
	/* Constructor. Do not remove !!! */
	public SipMsg()
	{
	}

	public int id;					// message id, unique/process
	public int pid;					// process id
	public String eoh;				// pointer to the end of header (if found) or null
	public String unparsed;				// here we stopped parsing
	public String buf;				// scratch pad, holds a modified message, via, etc. point into it
	public int len;					// message len (orig)


	public String new_uri;				// changed first line uri, when you change this
	public String dst_uri;				// Destination URI, must be forwarded to this URI if dst_url lenght != 0
	public int parsed_uri_ok;			// 1 if parsed_orig_uri is valid, 0 if not, set if to 0 if you modify the uri (e.g change new_uri)
        public int parsed_orig_ruri_ok;			// 1 if parsed_orig_uri is valid, 0 if not, set if to 0 if you modify the uri (e.g change new_uri)

	public String add_to_branch_s;			// whatever whoever want to append to branch comes here
	public int add_to_branch_len;
	public int hash_index;				// index to TM hash table; stored in core to avoid unnecessary calculations
	public int msg_flags;				/* flags used by core. Allows to set various flags on the message; may be used for 
							    simple inter-module communication or remembering processing state reached */
	public String set_global_address;
	public String set_global_port;


	public static native SipMsg ParseSipMsg();

	public static native String getMsgType();
	public static native String getStatus();
	public static native String getRURI();
	public static native IPPair getSrcAddress();
	public static native IPPair getDstAddress();
	public static native String getBuffer();
}

