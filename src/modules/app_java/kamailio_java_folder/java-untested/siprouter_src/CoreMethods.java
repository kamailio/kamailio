
package org.siprouter;
import java.lang.*;

public class CoreMethods
{
	    public static native int seturi(String ruri);
	    public static native int rewriteuri(String ruri);			// alias to seturi

	    public static native int add_local_rport();

	    public static native int append_branch();
	    public static native int append_branch(String branch);

	    public static native int drop();

	    public static native int force_rport();
	    public static native int add_rport();				// alias to force_rport

	    public static native int force_send_socket(String srchost, int srcport);

	    public static native int forward();
	    public static native int forward(String ruri, int port);

	    public static native boolean isflagset(int flag);
	    public static native void setflag(int flag);
	    public static native void resetflag(int flag);

	    public static native int revert_uri();

	    public static native int route(String target);

}

