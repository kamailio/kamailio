
public class Kamailio
{
	static
	{
	    System.load("/opt/kamailio/lib/kamailio/modules/app_java.so");
	}

	private native void LM_GEN1(int logLevel, String s);
	private native void LM_GEN2(int logFacility, int logLevel, String s);
	private native void LM_ALERT(String s);
	private native void LM_CRIT(String s);
	private native void LM_WARN(String s);
	private native void LM_NOTICE(String s);
	private native void LM_ERR(String s);
	private native void LM_INFO(String s);
	private native void LM_DBG(String s);


	public Kamailio()
	{
	    System.out.println("*************** constructor initialized! **********************");
	}


//	public static int child_init(int rank)
	public int child_init(int rank)
	{
	    System.out.println("FROM JAVA: rank=" + rank);

	    this.LM_GEN1(1, "oh yeah!!!\n");
	    this.LM_GEN2(1, 2, "oh yeah!!!\n");
	    this.LM_ALERT("oh yeah!!!\n");
	    this.LM_CRIT("oh yeah!!!\n");
	    this.LM_WARN("oh yeah!!!\n");
	    this.LM_NOTICE("oh yeah!!!\n");
	    this.LM_ERR("oh yeah!!!\n");
	    this.LM_INFO("oh yeah!!!\n");
	    this.LM_DBG("oh yeah!!!\n");

	    return 1;
	}

}
