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

  * choose the compiler to be used by setting the variable `CC` to `gcc` or `clang`

```
$ CC=clang ./test/travis/run.sh
```

One can always [login](./README.md#login-inside-the-build-environment) inside
the container and build everything [manually](http://www.kamailio.org/wiki/install/devel/git#compile_kamailio)

Clean Sources
-------------

After building locally there will be some files owned by root, so you can easily remove them executing:

```
$ docker run --rm \
    -v $(pwd):/code:rw ${DOCKER_IMAGE}:${DOCKER_TAG} \
    /bin/bash -c "cd /code; make -f debian/rules clean; rm -rf debian"
```

Login Inside The Build Environment
----------------------------------

```
$ docker run -i -t --rm \
    -v $(pwd):/code:rw ${DOCKER_IMAGE}:${DOCKER_TAG} /bin/bash
```

Use other Debian Distributions
------------------------------

There are several container [images available](https://hub.docker.com/r/kamailio/pkg-kamailio-docker/tags/) already.
One can just use any of the them by selecting the proper value using `DIST`:

```
$ DIST=buster ./test/travis/run.sh
```


Use other docker image
----------------------

You can choose another docker image/tag setting the variables `DOCKER_IMAGE` and `DOCKER_TAG`, for instance:

 * default `DOCKER_IMAGE` value is `kamailio/pkg-kamailio-docker`
 * default `DOCKER_TAG` value is `dev-sid`

 Be aware that `DIST` needs to be set accordingly to use the proper pkg/kamailio/deb/$DIST/rules file

```
$ CC=gcc DIST=stretch DOCKER_IMAGE=linuxmaniac/pkg-kamailio-docker DOCKER_TAG=5.1-stretch ./test/travis/run.sh
```


Coverity scan intregration
--------------------------

branch `coverity_scan` is used to generate a custom build and push the results. Be aware of the [frequency](https://scan.coverity.com/faq#frequency) for build submissions to Coverity Scan.


TODO
----

  * tests inside the docker container
  * automate daily push `master` to `coverity_scan` branch
