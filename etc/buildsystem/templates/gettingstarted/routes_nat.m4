changequote({{,}})dnl
ifdef({{GS_NAT}},
{{route[NAT_DETECT]
{
ANNOTATE({{}},
{{	# Skip requests from ourselves}},
{{	if (src_ip == 127.0.0.1) return;
}})

ANNOTATE({{}},
{{	# Skip requests that already passed a proxy}},
{{	if (is_present_hf("^Record-Route:")) {
		return;
	}
}})

ANNOTATE({{}},
{{# This function checks various parts of the message for privat IP addresses.}},
{{	if (nat_uac_test("NATHELPER_UAC_TESTS_NEWDIALOG")) {
}})
ANNOTATE({{}},
{{# Explicitly set the rport parameter with the source port of the message}},
{{		force_rport();
}})
ANNOTATE({{}},
{{# Set the flag we set tp store that the caller (UAC) is behind a NAT}},
{{		setflag("FLAG_NAT_UAC");
}})
ANNOTATE({{}},
{{# Set the attribute we use in the config to identify that the caller is behind a NAT}},
{{		$nated_caller = true;
	}
}})
} # End NAT_DETECT

route[NAT_MANGLE]
{
ANNOTATE({{}},
{{	# If none of the parties are NATed, just return
}},
{{	if (!$nated_caller && !$nated_callee) {
		return;
	}
}})

ANNOTATE({{}},
{{	# We are not allowed to touch the Contact of a REGISTER, but this will cause save_contact() 
	# to save the correct location.}},
{{	if (method == "REGISTER") {
		XDEBUG("    NAT_MANGLE: Fixing REGISTER for NATed client\nCall-ID: %ci\n");
		fix_nated_register();
	}
}})
ANNOTATE({{}},
{{	# For all other message, we need to change the Contact header so that callee gets the correct public info.}},
{{	else {
		XDEBUG("    NAT_MANGLE: Fixing NATed %rm\nCall-ID: %ci\n");
		fix_nated_contact();
	}
}})

ANNOTATE({{}},
{{#	For BYE and CANCELs, we want to end the proxying.}},
{{	if (method == "BYE" || method == "CANCEL") {
		XDEBUG("    NAT_MANGLE: Ending proxy session %rm\nCall-ID: %ci\n");
ifdef({{GS_NAT_RTPROXY}},
{{		unforce_rtp_proxy();
}})dnl
ifdef({{GS_NAT_MEDIAPROXY}},
{{
		use_media_proxy();
}})dnl
	}
}})

ANNOTATE({{}},
{{	# If we are not routing this message based on Route headers, add a dialog cookie, so we store
	# NAT info into the dialog (for use in subsequent messages).}},
{{	if (!$loose_route) {
		setavpflag("$nated_caller", "dialog_cookie");
		setavpflag("$nated_callee", "dialog_cookie");
	} 
}})
ANNOTATE({{}},
{{	# For all INVITE messages that are NATed, we want to force RTP through our RTP proxy.}},
{{	if (method == "INVITE") {
		XDEBUG("    NAT_MANGLE: Starting proxy session %rm\nCall-ID: %ci\n");
ifdef({{GS_NAT_RTPROXY}},
{{		force_rtp_proxy();
}})dnl
ifdef({{GS_NAT_MEDIAPROXY}},
{{
		end_media_session();
}})dnl
	}
}})	  
}
}})dnl ifdef GS_NAT
changequote(`,')dnl
