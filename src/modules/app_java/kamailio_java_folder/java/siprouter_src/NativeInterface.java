
package org.siprouter;
import java.lang.*;

public interface NativeInterface
{
    public abstract class Ranks
    {
	public static final int PROC_MAIN 		= 0;			// Main ser process
	public static final int PROC_TIMER		= -1;			// Timer attendant process
	public static final int PROC_RPC		= -2;			// RPC type process
	public static final int PROC_FIFO		= PROC_RPC;		// FIFO attendant process
	public static final int PROC_TCP_MAIN		= -4;			// TCP main process
	public static final int PROC_UNIXSOCK		= -5;			// Unix socket server
	public static final int PROC_ATTENDANT		= -10;			// main "attendant process
	public static final int PROC_INIT		= -127;			/* special rank, the context is the main ser process, but this is 
										    guaranteed to be executed before any rocess is forked, so it 
										    can be used to setup shared variables that depend on some
										    after mod_init available information (e.g. total number of processes).
										    @warning child_init(PROC_MAIN) is again called in the same process (main)
										    (before tcp), so make sure you don't init things twice, bot in PROC_MAIN and PROC_INT
								*/
	public static final int PROC_NOCHLDINIT		= -128;			// no child init functions will be called if this rank is used in fork_process()
	public static final int PROC_SIPINIT		= 1;			// First SIP worker - some modules do special processing in this child, like loading db data
	public static final int PROC_SIPRPC		= 127;			/* Used to init RPC worker as SIP commands handler. 
										    Don't do any special processing in the child init with this rank - just bare child initialization
										*/
	public static final int PROC_MIN		= PROC_NOCHLDINIT;	// Minimum process rank
    }

    public abstract class LogParams
    {
	public static final int	L_ALERT			= -5;
	public static final int L_BUG			= -4;
	public static final int L_CRIT2			= -3;			// like L_CRIT, but adds prefix
	public static final int L_CRIT			= -2;			// no prefix added
	public static final int L_ERR			= -1;
	public static final int L_WARN			= 0;
	public static final int L_NOTICE		= 1;
	public static final int L_INFO			= 2;
	public static final int L_DBG			= 3;

	public static final int DEFAULT_FACILITY	= 0;
    }
}

