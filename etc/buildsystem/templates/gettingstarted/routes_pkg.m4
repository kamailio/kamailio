dnl In this file you add routes that don't deserve a separate file per route.
changequote({{,}})dnl
ifdef({{GS_XMLRPC}},
{{# The RPC route is called directly for RPC calls because this route is configured in modparams
route[RPC]
{
ANNOTATE({{}},
{{# Protect from non-valid methods and sources	}},
{{	if ((method=="POST" || method=="GET") &&
		(XMLRPC_SRC_CHECK)) {

		if (msg:len >= 8192) {
			sl_reply("513", "Request to big");
			exit;
		}

		# lets see if a module wants to answer this
		dispatch_rpc();
		exit;
	}
}})
} # End route[RPC]
}})dnl
ifdef({{GS_MESSAGE}},
{{
route[MESSAGE]
{
ANNOTATE({{}},
{{		# Create a stateful transaction}},
{{		if (!t_newtran()) {
			sl_reply("500", "Server failure");
			exit;
		}
}})
ANNOTATE({{}},
{{		# Ignore iscomposing messages}},
{{		if (search("^(Content-Type|c):.*application/im-iscomposing\+xml.*")) {
			t_reply("202", "Ignored");
			exit;
		}
}})
ANNOTATE({{}},
{{		# Store MESSAGE in database}},
{{		if (m_store("0", "")) {
			XINFO("    MESSAGE - offline message stored\nCall-Id: %ci\n");
			if (!t_reply("202", "Accepted"))  {
				sl_reply("500", "Server failure");
				exit;
			}
		}
}})
ANNOTATE({{}},
{{		# The storing failed for some reason}},
{{		else {
			XINFO("    MESSAGE - offline message storage failed\nCall-Id: %ci\n");
			if (!t_reply("503", "Service Unavailable")) {
				sl_reply("500", "Server failure");
				exit;
			}
		}
}})

		exit;
} # End route[MESSAGE]
}})dnl ifdef GS_MESSAGE
changequote(`,')dnl
