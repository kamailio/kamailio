Kamailio Travis-CI Builds
=========================

Overview
--------

The Travis-CI build environment for Kamailio is based on docker containers,
so it can be easily reproducible by any developer of the project on local
systems.

Docker containers
-----------------

The default used container is built at [docker hub](https://hub.docker.com/r/kamailio/pkg-kamailio-docker/)
and it is a Debian Stretch based image done with the following [DockerFile](https://github.com/kamailio/pkg-kamailio-docker/blob/master/stretch/DockerfileDockerfile)

Build Locally
-------------

Same steps defined at [.travis.yml](https://github.com/kamailio/kamailio/blob/master/.travis.yml):

  * Choose the compiler to be used by setting the variable `CC` to `gcc` or `clang`
  * Choose version and distribution setting `VERSION` and `DIST`

```
$ export DIST=stretch VERSION=dev
$ docker pull kamailio/pkg-kamailio-docker:${VERSION}-${DIST}
$ docker run \
    -v $(pwd):/code:rw linuxmaniac/pkg-kamailio-docker:${VERSION}-${DIST} \
    /bin/bash -c "export CC=gcc; cd /code; ./test/travis/build_travis.sh"
```

One can always [login](./README.md#login-inside-the-build-environment) inside
the container and build everything [manually](https://www.kamailio.org/wikidocs/install/devel/git#compile_kamailio)

Clean Sources
-------------

```
$ docker run \
    -v $(pwd):/code:rw kamailio/pkg-kamailio-docker:${VERSION}-${DIST} \
    /bin/bash -c "cd /code; make -f debian/rules clean; rm -rf debian"
```

Login Inside The Build Environment
----------------------------------

```
$ docker run -i -t \
    -v $(pwd):/code:rw kamailio/pkg-kamailio-docker:${VERSION}-${DIST} /bin/bash
```

Use Other Debian Distributions
------------------------------

There are several container [images available](https://hub.docker.com/r/kamailio/pkg-kamailio-docker/tags/) already.
One can just use any of the them by selecting the proper tag:

```
$ export DIST=sid VERSION=dev
$ docker pull kamailio/pkg-kamailio-docker:${VERSION}-${DIST}
$ docker run \
    -v $(pwd):/code:rw kamailio/pkg-kamailio-docker:${VERSION}-${DIST} \
    /bin/bash -c "export CC=$CC; cd /code; DIST=$DIST ./test/travis/build_travis.sh"
```


TODO
----

  * tests inside the docker container
