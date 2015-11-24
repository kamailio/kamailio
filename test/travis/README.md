Travis-ci build
===============

The build environment is based on docker containers, so it can be easily
reproducible by any developer of the project.

The container we use is build at [docker hub](https://hub.docker.com/r/linuxmaniac/pkg-kamailio-docker/)
It's Debian Stretch based image build [DockerFile](https://github.com/linuxmaniac/pkg-kamailio-docker/blob/master/stretch/Dockerfile)

Build locally
-------------

Same steps defined at [.travis.yml](https://github.com/kamailio/kamailio/blob/master/.travis.yml):

- Choose the compiler you want to use setting `CC` to `gcc` or `clang`

```
$ docker pull linuxmaniac/pkg-kamailio-docker:jessie
$ docker run \
    -v $(pwd):/code:rw linuxmaniac/pkg-kamailio-docker:jessie \
    /bin/bash -c "export CC=gcc; cd /code; ./test/travis/build_travis.sh"
```

You can always [login](./README.md#login-inside-the-build-environment) inside the container
and build it [manually](http://www.kamailio.org/wiki/install/devel/git#compile_kamailio)

Clean sources
-------------

```
$ docker run \
    -v $(pwd):/code:rw linuxmaniac/pkg-kamailio-docker:jessie \
    /bin/bash -c "cd /code; make -f debian/rules clean; rm -rf debian"
```

Login inside the build environment
----------------------------------

```
$ docker run -i -t \
    -v $(pwd):/code:rw linuxmaniac/pkg-kamailio-docker:jessie /bin/bash
```

Test other Debian distributions
-------------------------------

There are several container [images available](https://hub.docker.com/r/linuxmaniac/pkg-kamailio-docker/tags/) already.
You just need to use any of the them selecting the proper tag

```
$ export $DIST=wheezy
$ docker pull linuxmaniac/pkg-kamailio-docker:$DIST
$ docker run \
    -v $(pwd):/code:rw linuxmaniac/pkg-kamailio-docker:$DIST \
    /bin/bash -c "export CC=$CC; cd /code; DIST=$DIST ./test/travis/build_travis.sh"
```


TODO
----

- tests inside the docker container
