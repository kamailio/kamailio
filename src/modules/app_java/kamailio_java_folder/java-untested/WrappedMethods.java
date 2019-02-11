import java.lang.*;
import java.io.*;

import org.siprouter.*;
import org.siprouter.NativeInterface.*;

public class WrappedMethods extends NativeMethods
{
	public static boolean is_method(String method)
	{
	    if (KamExec("is_method", method) == -1)
		return false;
	    else
		return true;
	}

	public static boolean t_check_trans()
	{
	    if (KamExec("t_check_trans") == -1)
		return false;
	    else
		return true;
	}

	public static boolean t_relay()
	{
	    if (KamExec("t_relay") == -1)
		return false;
	    else
		return true;
	}

	public static void record_route()
	{
	    KamExec("record_route");
	}

	public static void append_hf(String txt)
	{
	    KamExec("append_hf", txt);
	}

	public static void append_hf(String txt, String hdr)
	{
	    KamExec("append_hf", txt, hdr);
	}

	public static void remove_hf(String hname)
	{
	    KamExec("remove_hf", hname);
	}

	public static void sl_send_reply(String replycode, String replymsg)
	{
	    KamExec("sl_send_reply", replycode, replymsg);
	}

	public static void sl_reply_error()
	{
	    KamExec("sl_reply_error");
	}

	public static boolean has_totag()
	{
	    if (KamExec("has_totag") == -1)
                return false;
            else
                return true;
        }

	public static boolean loose_route()
	{
	    if (KamExec("loose_route") == -1)
                return false;
            else
                return true;
        }

	public static boolean save(String location)
	{
	    if (KamExec("save", location) == -1)
                return false;
            else
                return true;
        }

}
