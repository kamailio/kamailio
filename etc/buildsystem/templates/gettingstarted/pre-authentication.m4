changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ifdef({{OPTIONS_REPLY_TO_LOCAL}},
{{ANNOTATE({{}},
{{	# OPTIONS processing}},
{{	route(OPTIONS_REPLY);   
}})dnl
}})dnl OPTIONS_REPLY_TO_LOCAL
}})dnl
changequote(`,')dnl
