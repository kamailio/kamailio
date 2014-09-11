#! /bin/sh

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/bin/pdb_server
NAME=pdb_server
DESC=pdb_server
PIDFILE=/var/run/pdb_server.pid

test -x $DAEMON || exit 0

# Include pdb_server defaults if available
if [ -f /etc/default/pdb_server ] ; then
        . /etc/default/pdb_server
fi

# Include pdb_server config if available
if [ -f /etc/pdb_server.conf ] ; then
        . /etc/pdb_server.conf
fi

set -e

case "$1" in
  start)
        echo -n "Starting $DESC: "
        start-stop-daemon --start --background --make-pidfile --pidfile $PIDFILE -c $PDBUSER -g $PDBGROUP --exec $DAEMON -- -d -m "$MMAPFILE" -i "$BINDADDR" -p "$BINDPORT"
        echo "."
        ;;
  stop)
        echo -n "Stopping $DESC: "
        # start-stop-daemon --stop --pidfile $PIDFILE --exec $DAEMON
        if pgrep -x $NAME &> /dev/null; then killall $NAME; fi
        rm -f $PIDFILE
        echo "$NAME."
        ;;
  restart|force-reload)
        if pgrep -x $NAME &> /dev/null; then killall $NAME; fi
        rm -f $PIDFILE
        start-stop-daemon --start --background --make-pidfile --pidfile $PIDFILE -c $PDBUSER -g $PDBGROUP --exec $DAEMON -- -d -m "$MMAPFILE" -i "$BINDADDR" -p "$BINDPORT"
        ;;
  *)
        N=/etc/init.d/$NAME
        echo "Usage: $N {start|stop|restart|force-reload}" >&2
        exit 1
        ;;
esac

exit 0
