#!/usr/bin/env bash
# packaging script to include submodules
# modified from: https://gist.github.com/arteymix/03702e3eb05c2c161a86b49d4626d21f

usage() {
    echo Usage:  pkg/kamailio/scripts/git-archive-all.sh kamailio-5.8.0 ../output/kamailio-5.8.0_src
}

if [ -z "$1" ]; then
    echo "You must specify a prefix name."
    usage
    exit 1
fi

if [ -z "$2" ]; then
    echo "You must specify a super-archive name."
    usage
    exit 1
fi

git archive --prefix "$1/" -o "$2.tar" HEAD
git submodule foreach --recursive "git archive --prefix=$1/\$sm_path/ --output=\$sha1.tar HEAD && tar --concatenate --file=$(pwd)/$2.tar \$sha1.tar && rm \$sha1.tar"

gzip "$2.tar"
