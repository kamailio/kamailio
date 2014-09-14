" -*- vim -*-
" FILE: kamailio.vim
" LAST MODIFICATION: 2009-05-28 18:30
" (C) Copyright 2008 Stanisław Pitucha <viraptor@gmail.com>
" (C) Copyright 2009-2010 Daniel-Constantin Mierla <miconda@gmail.com>
" Version: 1.03

" USAGE:
"
" Save this file to $VIMFILES/syntax/kamailio.vim. Either add a detection
" script to filetypes.vim, or set filetype manually to "kamailio" when
" editing Kamailio configuration file with 'setf kamailio'.
"
" List of keyword and core functions taken from latest dev version of
" Kamailio. Module functions not included.
"
" Tested only on vim 7.1
"
" Example: "setf kamailio"
"
" REQUIREMENTS:
" vim (>= 7)

if exists("b:current_syntax")
	finish
endif

syn match	kamailioConfigParamLine	'^[^=]\+=.*$' contains=kamailioCoreParameter,kamailioString,kamailioConfigConstant,kamailioSpecial,kamailioNumber,kamailioCppComment,kamailioHashComment,kamailioSlashSlashComment
syn region	kamailioConfigModparam	start='^\s*modparam\s*(' end=')' contains=kamailioString,kamailioNumber
syn match	kamailioConfigModule		'^\s*loadmodule\s*"[^"]\+"' contains=kamailioString

syn keyword	kamailioTodo	TODO FIXME XXX contained

syn match	kamailioOperator		'!\|&&\|||\|=[~=]\?\|>\|<\|+\|-\|/\|\*\||\|&\|^\|\~\|defined\|eq\|ieq\|ne\|ine\|mod' display contained

syn region	kamailioCppComment		start='/\*' end='\*/' contains=kamailioTodo
syn match	kamailioHashDefine	'#!define\s\|#!ifdef\s\|#!ifndef\s\|#!endif\|#!else\|#!substdef\|#!substdefs\|#!subst\|#!trydef\|#!trydefine\|#!redef\|#!redefine\|!!define\s\|!!ifdef\s\|!!ifndef\s\|!!endif\|!!else\|!!substdef\|!!substdefs\|!!subst\|!!trydef\|!!trydefine\|!!redef\|!!redefine\|#!KAMAILIO\|#!OPENSER\|#!SER\|#!MAXCOMPAT\|#!ALL\|#!include_file\|#!import_file\|!!include_file\|!!import_file'
" syn match	kamailioHashDefine	'^\s*#!.+$'
syn match	kamailioHashComment	'#[^!].*$\|#$' contains=kamailioTodo
syn match	kamailioSlashSlashComment	'//.*$\|//#$' contains=kamailioTodo

syn match	kamailioStringEscape	'\\.' contained
syn match	kamailioNumber			'[0-9]\+' contained
syn region	kamailioString			matchgroup=Normal start='"' skip='\\"' end='"' contained contains=kamailioVariable,kamailioStringEscape
syn match	kamailioVariable		"$[a-zA-Z_][a-zA-Z0-9_]*\(([^)]\+)\)\?" contained
syn match	kamailioIdentifier		'[a-zA-Z_][a-zA-Z0-9_]*' contained
syn keyword	kamailioStatement	route if else switch case default break exit return drop while include_file import_file contained
syn keyword	kamailioSpecial			yes no on off true false enabled disabled contained

syn keyword	kamailioCoreKeyword	af dst_ip dst_port from_uri method msg:len proto status snd_af snd_ip snd_port snd_proto src_ip src_port to_af to_ip to_port to_proto to_uri uri uri:host uri:port contained

syn keyword kamailioCoreValue		udp UDP tcp TCP tls TLS sctp SCTP ws WS wss WSS inet INET inet6 INET6 sslv23 SSLv23 SSLV23 sslv2 SSLv2 SSLV2 sslv3 SSLv3 SSLV3 tlsv1 TLSv1 TLSV1 max_len myself contained

syn keyword	kamailioCoreFunction	forward forward_tcp forward_udp forward_tls forward_sctp send send_tcp log error exec force_rport add_rport force_tcp_alias add_tcp_alias udp_mtu udp_mtu_try_proto setflag resetflag isflagset flags bool setavpflag resetavpflag isavpflagset avpflags rewritehost sethost seth rewritehostport sethostport sethp rewritehostporttrans sethostporttrans sethpt rewriteuser setuser setu rewriteuserpass setuserpass setup rewriteport setport setp rewriteuri seturi revert_uri prefix strip strip_tail userphone append_branch set_advertised_address set_advertised_port force_send_socket remove_branch clear_branches cfg_select cfg_reset contained

syn keyword	kamailioCoreParameter debug fork log_stderror log_facility log_name log_color log_prefix listen alias auto_aliases dns rev_dns dns_try_ipv6 dns_try_naptr dns_srv_lb dns_srv_loadbalancing dns_udp_pref dns_udp_preference dns_tcp_pref dns_tcp_preference dns_tls_pref dns_tls_preference dns_sctp_pref dns_sctp_preference dns_retr_time dns_retr_no dns_servers_no dns_use_search_list dns_search_full_match dns_cache_init use_dns_cache use_dns_failover dns_cache_flags dns_cache_negative_ttl dns_cache_min_ttl dns_cache_max_ttl dns_cache_mem dns_cache_gc_interval dns_cache_del_nonexp dns_cache_delete_nonexpired dst_blacklist_init use_dst_blacklist dst_blacklist_mem dst_blacklist_expire dst_blacklist_ttl dst_blacklist_gc_interval port statistics maxbuffer children check_via phone2tel syn_branch memlog mem_log memdbg mem_dbg sip_warning server_signature reply_to_via user uid group gid chroot workdir wdir mhomed disable_tcp tcp_children tcp_accept_aliases tcp_send_timeout tcp_connect_timeout tcp_connection_lifetime tcp_poll_method tcp_max_connections tcp_no_connect tcp_source_ipv4 tcp_source_ipv6 tcp_fd_cache tcp_buf_write tcp_async tcp_conn_wq_max tcp_wq_max tcp_rd_buf_size tcp_wq_blk_size tcp_defer_accept tcp_delayed_ack tcp_syncnt tcp_linger2 tcp_keepalive tcp_keepidle tcp_keepintvl tcp_keepcnt tcp_crlf_ping disable_tls tls_disable enable_tls tls_enable tlslog tls_log tls_port_no tls_method tls_verify tls_require_certificate tls_certificate tls_private_key tls_ca_list tls_handshake_timeout tls_send_timeout disable_sctp enable_sctp sctp_children sctp_socket_rcvbuf sctp_socket_receive_buffer sctp_socket_sndbuf sctp_socket_send_buffer sctp_autoclose sctp_send_ttl sctp_send_retries socket_workers advertised_address advertised_port disable_core_dump open_files_limit shm_force_alloc mlock_pages real_time rt_prio rt_policy rt_timer1_prio rt_fast_timer_prio rt_ftimer_prio rt_timer1_policy rt_ftimer_policy rt_timer2_prio rt_stimer_prio rt_timer2_policy rt_stimer_policy mcast_loopback mcast_ttl tos pmtu_discovery exit_timeout ser_kill_timeout max_while_loops stun_refresh_interval stun_allow_stun stun_allow_fp server_id description descr desc loadpath mpath fork_delay modinit_delay http_reply_hack latency_log latency_limit_action latency_limit_db mem_join mem_safety msg_time tcp_clone_rcvbuf tls_max_connections async_workers max_recursive_level dns_naptr_ignore_rfc http_reply_parse version_table tcp_accept_no_cl advertise auto_bind_ipv6 sql_buffer_size pv_buffer_size pv_buffer_slots corelog core_log udp4_raw udp4_raw_mtu udp4_raw_ttl contained

syn region	kamailioBlock	start='{' end='}' contained contains=kamailioBlock,@kamailioCodeElements

syn match	kamailioRouteBlock	'\(failure_\|onreply_\|branch_\|event_\|onsend_\|request_\|reply_\)\?route\(\s*\[[^\]]\+\]\)\?' contained contains=kamailioNumber,kamailioString,kamailioIdentifier
syn region	kamailioRrouteBlockFold	matchgroup=kamailioRouteBlock start="\(failure_\|onreply_\|branch_\|event_\|onsend_\|request_\|reply_\)\?route\(\s*\[[^\]]\+\]\)\?\s*\n\?{" matchgroup=NONE end="}" contains=kamailioBlock,@kamailioCodeElements

syn cluster	kamailioCodeElements contains=kamailioHashDefine,kamailioCppComment,kamailioHashComment,kamailioSlashSlashComment,kamailioNumber,kamailioString,kamailioVariable,kamailioOperator,kamailioStatement,kamailioKeyword,kamailioCoreKeyword,kamailioCoreValue,kamailioCoreFunction,kamailioIdentifier

hi def link kamailioCppComment Comment
hi def link kamailioHashComment Comment
hi def link kamailioSlashSlashComment Comment
hi def link kamailioHashDefine Special
hi def link kamailioTodo Todo

hi def link kamailioConfigModparam Function
hi def link kamailioConfigModule Keyword

hi def link kamailioKeyword Keyword
hi def link kamailioCoreKeyword Special
hi def link kamailioCoreValue Special
hi def link kamailioCoreFunction Function
hi def link kamailioRouteBlock Type
hi def link kamailioRrouteBlockFold Type
hi def link kamailioIdentifier Identifier
hi def link kamailioSpecial Special
hi def link kamailioCoreParameter Keyword

hi def link kamailioOperator Operator

hi def link kamailioStatement Conditional

hi def link kamailioNumber Number
hi def link kamailioVariable Identifier
hi def link kamailioString String
hi def link kamailioStringEscape Special

let b:current_syntax = "kamailio"
