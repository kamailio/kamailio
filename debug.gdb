file ./ser
set args -f t_debug.cfg
break main
#break dump_all_statistic
#break lock_initialize
break udp_send
run
