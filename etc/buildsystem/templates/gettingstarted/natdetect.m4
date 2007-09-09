changequote({{,}})dnl
ifdef({{GS_NAT}},
{{ANNOTATE({{}},
{{	# Do NAT detection}},
{{	route(NAT_DETECT);
}})
}})dnl
changequote(`,')dnl
