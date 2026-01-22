FROM mcr.microsoft.com/devcontainers/base:trixie
# utils packages
RUN apt-get update && \
  export DEBIAN_FRONTEND=noninteractive && \
  apt-get -y install --no-install-recommends \
    apt-utils dialog clang-format clang pbuilder cmake

# trixie packaging
RUN mkdir -p /usr/local/src/pkg
COPY trixie /usr/local/src/pkg/debian

# get build dependences
RUN cd /usr/local/src/pkg/ && \
  /usr/lib/pbuilder/pbuilder-satisfydepends-experimental

# clean
RUN apt-get clean && rm -rf /var/lib/apt/lists/*
