#!/bin/sh -e

# This script is wrote by Sergey Safarov <s.safarov@gmail.com>

BUILD_ROOT=/tmp/kamailio
FILELIST=/tmp/filelist
FILELIST_BINARY=/tmp/filelist_binary
TMP_TAR=/tmp/kamailio_min.tar.gz
IMG_TAR=kamailio_img.tar.gz

prepare_build() {
apk add --no-cache abuild git gcc build-base bison db-dev flex expat-dev perl-dev postgresql-dev python2-dev pcre-dev mariadb-dev \
    libxml2-dev curl-dev unixodbc-dev confuse-dev ncurses-dev sqlite-dev lua-dev openldap-dev \
    libressl-dev net-snmp-dev libuuid libev-dev jansson-dev json-c-dev libevent-dev linux-headers \
    libmemcached-dev rabbitmq-c-dev hiredis-dev libmaxminddb-dev libunistring-dev freeradius-client-dev lksctp-tools-dev

    adduser -D build && addgroup build abuild
    echo "%abuild ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/abuild
    su - build -c "git config --global user.name 'Your Full Name'"
    su - build -c "git config --global user.email 'your@email.address'"
    su - build -c "abuild-keygen -a -i"
}

build_and_install(){
    if [ ! -z "$GIT_TAG" ]; then
        sed -i -e "s/^_gitcommit=.*/_gitcommit=$GIT_TAG/" /usr/src/kamailio/pkg/kamailio/alpine/APKBUILD
    fi
    chown -R build /usr/src/kamailio
    su - build -c "cd /usr/src/kamailio/pkg/kamailio/alpine; abuild snapshot"
    su - build -c "cd /usr/src/kamailio/pkg/kamailio/alpine; abuild -r"
    cd /home/build/packages/kamailio/x86_64
    ls -1 kamailio-*.apk |  xargs apk --no-cache --allow-untrusted add
}

list_installed_kamailio_packages() {
	apk info | grep kamailio
}

kamailio_files() {
    local PACKAGES
    PACKAGES=$(apk info | grep kamailio)
    PACKAGES="musl $PACKAGES"
    for pkg in $PACKAGES
    do
        # list package files and filter package name
        apk info --contents $pkg 2> /dev/null | sed -e '/\S\+ contains:/d'  -e '/^$/d' -e 's/^/\//'
    done
}

extra_files() {
    cat << EOF
/etc
/bin
/bin/busybox
/usr/bin
/usr/bin/dumpcap
/usr/lib
/usr/sbin
/usr/sbin/tcpdump
/var
/var/run
/run
EOF
}

sort_filelist() {
    sort $FILELIST | uniq > $FILELIST.new
    mv -f $FILELIST.new $FILELIST
}

filter_unnecessary_files() {
# excluded following files and directories recursive
# /usr/lib/debug/usr/lib/kamailio/
# /usr/share/doc/kamailio
# /usr/share/man
# /usr/share/snmp

    sed -i \
        -e '\|^/usr/lib/debug/|d' \
        -e '\|^/usr/share/doc/kamailio/|d' \
        -e '\|^/usr/share/man/|d' \
        -e '\|^/usr/share/snmp/|d' \
        $FILELIST
}

ldd_helper() {
    TESTFILE=$1
    LD_PRELOAD=/usr/sbin/kamailio ldd $TESTFILE 2> /dev/null > /dev/null || return

    LD_PRELOAD=/usr/sbin/kamailio ldd $TESTFILE | sed -e 's/^.* => //' -e 's/ (.*)//' -e 's/\s\+//' -e '/^ldd$/d'
}

find_binaries() {
    rm -f $FILELIST_BINARY
    set +e
    for f in $(cat $FILELIST)
    do
        ldd_helper /$f >> $FILELIST_BINARY
    done
    set -e
    sort $FILELIST_BINARY | sort | uniq > $FILELIST_BINARY.new
    mv -f $FILELIST_BINARY.new $FILELIST_BINARY

    # Resolving simbolic links
    cat $FILELIST_BINARY | xargs realpath > $FILELIST_BINARY.new
    mv -f $FILELIST_BINARY.new $FILELIST_BINARY
}

tar_files() {
    local TARLIST=/tmp/tarlist
    cat $FILELIST > $TARLIST
    cat $FILELIST_BINARY >> $TARLIST
    tar -czf $TMP_TAR --no-recursion -T $TARLIST
    rm -f $TARLIST
}

make_image_tar() {
    mkdir -p $BUILD_ROOT
    cd $BUILD_ROOT
    tar xzf $TMP_TAR
    /bin/busybox --install -s bin
    sed -i -e '/mi_fifo/d' etc/kamailio/kamailio.cfg
    tar czf /usr/src/kamailio/pkg/kamailio/alpine_docker/$IMG_TAR *
}

prepare_build
build_and_install
#install PCAP tools
apk add --no-cache wireshark-common tcpdump

kamailio_files > $FILELIST
extra_files >> $FILELIST
sort_filelist
filter_unnecessary_files
find_binaries
tar_files
make_image_tar
