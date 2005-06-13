cat /tmp/ser_reply &
cat > /tmp/ser_fifo << EOF
:pdt_list:ser_reply
1
.

EOF
