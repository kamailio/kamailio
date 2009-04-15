cat /tmp/ser_reply &
cat > /tmp/ser_fifo << EOF
:pdt_delete:ser_reply
123.com

EOF
