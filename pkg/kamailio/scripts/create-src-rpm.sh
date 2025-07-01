#!/bin/bash
#

usage() {
    echo Usage: create-src-rpm.sh '<version>' '<release>'
}

# creates a src.rpm for mock
VER=$1
REL=$2
[[ -n "$VER" ]] || { echo "Version not specified"; usage; exit 1; }
[[ -n "$REL" ]] || { echo "Release not specified"; usage; exit 2; }

git submodule update --init

TMP=$(date +"%s")
RPMBUILD=rpmbuild.${TMP}

rm -rf $RPMBUILD/*
mkdir -p $RPMBUILD/{SRPMS,SPECS,SOURCES}
./pkg/kamailio/scripts/git-archive-all.sh kamailio-${VER} $RPMBUILD/SOURCES/kamailio-${VER}_src

# prepare src.rpm with version/release
cp pkg/kamailio/obs/kamailio.spec $RPMBUILD/SPECS/
sed -i -e s"/^%define ver.*/%define ver ${VER}/" -e s"/^%define rel.*/%define rel ${REL}/" ${RPMBUILD}/SPECS/kamailio.spec

rm -f ${RPMBUILD}/SRPMS/*.src.rpm
rpmbuild -bs --define "_topdir $PWD/$RPMBUILD" $RPMBUILD/SPECS/kamailio.spec

echo "src.rpm is created in $RPMBUILD/SRPMS"
echo "To build:"
echo Run: mock -r pkg/kamailio/obs/kamailio-8-x86_64.cfg $RPMBUILD/SRPMS/*src.rpm
echo Run: mock -r pkg/kamailio/obs/kamailio-9-x86_64.cfg $RPMBUILD/SRPMS/*src.rpm


exit 0

