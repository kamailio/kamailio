#!/bin/sh
#
# $Id$
#
# This script generates a self-signed TLS/SSL certificate that can be
# immediately used with the TLS module of SIP Router. The file was inspired
# by a script from Debian's uw-imapd package.
#

#############################################################################
# Configuration variables
#############################################################################
NAME=$MAIN_NAME
if [ -z "$NAME" ] ; then NAME="sip-router"; fi;
DEFAULT_DIR="/usr/local/etc/$NAME"
DEFAULT_DAYS=365
DEFAULT_INFO="Self-signed certificate for $NAME"
DEFAULT_CERT_FILENAME="$NAME-selfsigned.pem"
DEFAULT_KEY_FILENAME="$NAME-selfsigned.key"

DEFAULT_OPENSSL='openssl'

HOSTNAME=`hostname -s`
if hostname -f >/dev/null 2>/dev/null ; then
	FQDN=`hostname -f`
else
	FQDN=`hostname`
fi
MAILNAME=`cat /etc/mailname 2> /dev/null || echo $FQDN`

# test if we have the normal or enhanced getopt
getopt -T >/dev/null
if [ $? = 4 ]; then
	LONGOPTS_SUPPORTED=1
fi

longopts() {
	if [ -z "${LONGOPTS_SUPPORTED}" ]; then
		exit;
	fi
	case "$1" in
	-h)	echo ', --help';;
	-d)	echo ', --dir' ;;
	-c)	echo ', --certificate';;
	-k)	echo ', --key';;
	-e)	echo ', --expires';;
	-i)	echo ', --info';;
    	-o)	echo ', --overwrite' ;;
	esac
}

usage() {
cat <<EOF
NAME
  $COMMAND - Generate a self-signed TLS/SSL certificate for use with $NAME.

SYNOPSIS
  $COMMAND [options]

DESCRIPTION
  This is a simple shell script that generates a self signed TLS/SSL
  certificate (and private key) for use with the tls module of $NAME. The
  self-signed certificate is suitable for testing and/or private setups.
  You are encouraged to create a proper authorized one if needed.

  Both certificate and key files are by default stored in the directory
  containing the configuration file of $NAME (unless you change it using
  the options below).

OPTIONS
  -h`longopts -h` 
      Display this help text.

  -d`longopts -d`
      The path to the directory where cert and key files will be stored.
	  (Default value is '$DEFAULT_DIR')

  -c`longopts -c`
      The name of the file where the certificate will be stored.
	  (Default value is '$DEFAULT_CERT_FILENAME')

  -k`longopts -k`
      The name of the file where the private key will be stored.
	  (Default value is '$DEFAULT_KEY_FILENAME')

  -e`longopts -e`
      Number of days for which the certificate will be valid.
	  (Default value is '$DEFAULT_DAYS')

  -i`longopts -i`
      The description text to be embedded in the certificate.
	  (Default value is '$DEFAULT_INFO')

  -o`longopts -o`
      Overwrite certificate and key files if they exist already.
      (By default they will be not overwritten.)

ENVIRONMENT VARIABLES
  OPENSSL	Path to openssl command (Currently ${OPENSSL})

AUTHOR
  Written by Jan Janak <jan@iptel.org>

REPORTING BUGS
  Report bugs to <sr-dev@sip-router.org>
EOF
} #usage


COMMAND=`basename $0`
if [ -z "$DIR" ] ; then DIR=$DEFAULT_DIR; fi;
if [ -z "$DAYS" ] ; then DAYS=$DEFAULT_DAYS; fi;
if [ -z "$INFO" ] ; then INFO=$DEFAULT_INFO; fi;
if [ -z "$CERT_FILENAME" ] ; then CERT_FILENAME=$DEFAULT_CERT_FILENAME; fi;
if [ -z "$KEY_FILENAME" ] ; then KEY_FILENAME=$DEFAULT_KEY_FILENAME; fi;
if [ -z "$OPENSSL" ] ; then OPENSSL=$DEFAULT_OPENSSL; fi;

if [ -n "${LONGOPTS_SUPPORTED}" ]; then
	# enhanced version
	TEMP=`getopt -o hd:c:k:e:i:o --long help,dir:,certificate:,key:,expires:,info:,overwrite -n $COMMAND -- "$@"`
else
	# basic version	
	TEMP=`getopt hd:c:k:e:i:o "$@"`
fi
if [ $? != 0 ] ; then exit 1; fi
eval set -- "$TEMP"

while true ; do
	case "$1" in
	-h|--help)         usage;                 exit 0 ;;
	-d|--dir)          DIR=$2;                shift 2 ;;
	-c|--certificate)  CERT_FILENAME=$2;      shift 2 ;;
	-k|--key)          KEY_FILENAME=$2;       shift 2 ;;
	-e|--expires)      DAYS=$2;               shift 2 ;;
	-i|--info)         INFO=$2;               shift 2 ;;
    -o|--overwrite)    OVERWRITE=1;           shift ;;
	--)                shift;                 break ;;
	*)                 echo "Internal error"; exit 1 ;;
	esac
done

TEMP=`which $OPENSSL`
if [ $? != 0 ] ; then
	echo "Could not find openssl command"
	echo "Set OPENSSL environment variable properly (see -h for more info)"
	exit 1
fi

if [ ! -d "$DIR" ] ; then
	echo "Directory '$DIR' does not exist."
	exit 1
fi

if [ -z "$OVERWRITE" -a \( -f "$DIR/$CERT_FILENAME" \) ] ; then
	echo "File '$DIR/$CERT_FILENAME' already exists, doing nothing."
	echo "(Use -o to override)"
	exit 0;
fi


if [ -z "$OVERWRITE" -a \( -f "$DIR/$KEY_FILENAME" \) ] ; then
	echo "File '$DIR/$KEY_FILENAME' already exists, doing nothing."
	echo "(Use -o to override)."
	exit 0;
fi

touch "$DIR/$CERT_FILENAME" > /dev/null 2>&1
if [ $? != 0 ] ; then
	echo "Could not create file '$DIR/$CERT_FILENAME'"
	exit 1
fi

touch "$DIR/$KEY_FILENAME" > /dev/null 2>&1
if [ $? != 0 ] ; then
	echo "Could not create file '$DIR/$KEY_FILENAME'"
	rm -f "$DIR/$CERT_FILE"
	exit 1
fi

echo "Creating a new $NAME self-signed certificate for '$FQDN'" \
     "valid for $DAYS days."
openssl req -new -x509 -days "$DAYS" -nodes -out "$DIR/$CERT_FILENAME" \
        -keyout "$DIR/$KEY_FILENAME" > /dev/null 2>&1 <<+
.
.
.
$INFO
$HOSTNAME
$FQDN
root@$MAILNAME
+

if [ $? != 0 ] ; then
	echo "Error while executing openssl command."
	rm -f "$DIR/$CERT_FILE" "$DIR/$KEY_FILE"
	exit 1;
else
	echo "Private key stored in '$DIR/$KEY_FILENAME'."
	echo "Certificate stored in '$DIR/$CERT_FILENAME'."
	exit 0;
fi
