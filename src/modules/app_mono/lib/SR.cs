using System;
using System.Runtime.CompilerServices;

namespace SR {
	public class Core {
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static string APIVersion();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static void Log(int level, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static void Err(string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static void Dbg(string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int ModF(string text);
	}
	public class PV {
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static string GetS(string pvn);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int GetI(string pvn);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int SetS(string pvn, string val);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int SetI(string pvn, int val);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int Unset(string pvn);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int IsNull(string pvn);
	}
	public class HDR {
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int Append(string hv);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int Remove(string hv);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int Insert(string hv);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
			public extern static int AppendToReply(string hv);
	}
}
