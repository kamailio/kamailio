cat /tmp/openser_reply &
cat > /tmp/openser_fifo << EOF
:pdt_delete:openser_reply
localhost
127.com

EOF
