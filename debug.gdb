file ./ser
set args -f td.cfg
break main
#break dump_all_statistic
#break lock_initialize
break udp_send
run
