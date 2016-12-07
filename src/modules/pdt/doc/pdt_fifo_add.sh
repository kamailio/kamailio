cat /tmp/openser_reply &
cat > /tmp/openser_fifo << EOF
:pdt_add:openser_reply
localhost
*57
127.com

EOF
