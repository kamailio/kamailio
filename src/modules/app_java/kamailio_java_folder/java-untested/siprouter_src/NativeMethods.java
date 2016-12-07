
package org.siprouter;
import java.lang.*;

public abstract class NativeMethods
{
	    public static native void LM_GEN1(int logLevel, String s);
	    public static native void LM_GEN2(int logFacility, int logLevel, String s);
	    public static native void LM_ALERT(String s);
	    public static native void LM_CRIT(String s);
	    public static native void LM_WARN(String s);
	    public static native void LM_NOTICE(String s);
	    public static native void LM_ERR(String s);
	    public static native void LM_INFO(String s);
	    public static native void LM_DBG(String s);


	    protected final int mop = 0x0;				// 'message object pointer' (pointer to an original c pointer of 'struct sip_msg')

	    public static native int KamExec(String fname, String... params);
}

