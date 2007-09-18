changequote({{,}})dnl
ifdef({{GS_NAT}},
{{
onreply_route[NAT_MANGLE]
{
	XNOTICE("INCOMING REPLY - %is(%fu): %rs(%tu)\nCall-Id:%ci\n");
ANNOTATE({{}},
{{	# Just return if callee is not NATed.}},
{{	if (!$nated_callee) return;
}})
ANNOTATE({{}},
{{	# Fix the Contact header}},
{{	fix_nated_contact();
}})
ANNOTATE({{}},
{{	# Only replies to INVITEs carry media}},
{{	if (@cseq.method != "INVITE") return;}})
ANNOTATE({{}},
{{	# Early media and 2xx codes with sdp content should be proxied}},
{{	if ((status =~ "(180)|(183)|2[0-9][0-9]") && search("^(Content-Type|c):.*application/sdp")) {
}})
	XINFO("    onreply_route[NAT_MANGLE] - forcing rtp proxy\nCall-Id: %ci\n");
ANNOTATE({{}},
{{	# Force media through our RTP proxy}},
{{
ifdef({{GS_NAT_RTPPROXY}},
{{		force_rtp_proxy();
}})dnl
ifdef({{GS_NAT_MEDIAPROXY}},
{{		use_media_proxy();
}})dnl
	}
}})
} # end onreply_route[NAT_MANGLE]
}})dnl ifdef GS_NAT
changequote(`,')dnl
