# Kamailio Packaging #

The `pkg/kamailio/` directory contains packaging specs for several operation systems.

Currently the DEB and RPM specs are actively maintained, the rest are still kept
in case someone wants to pick up and update.

## DEBS ##

DEB packages can be generated for several flavours of Debian and Ubuntu
operating systems.

To generate deb packages, run:

```
make deb
```

This is using a generic Debian spec. To select a specific Debian or Ubuntu, go
to root folder of Kamailio source tree, create a `debian` symlink to the
desired distro from `pkg/kamailio/deb/` and run `make deb`. For example,
on a Debian Stretch (9.x), do:

```
ln -s pkg/kamailio/deb/stretch debian
make deb
```

The DEB files are generated in the parent folder.

## RPMS ##

There are couple of variants of RPM specs. The most actual one is stored in `obs/`
subfolder and has conditional options to build for many operating systems that
use RPM for packages (e.g., CenOS, RedHat, Fedora, OpenSuse). The folders with
the name reflecting an operating system might be older, some not really
maintained.

To build RPM packages for CentOS, RHEL, Fedora, OpenSUSE and Oracle linux execute

```
make rpm
```

When utility is finished, you can see the directory where compiled RPM files
are located.

Example:

```
Finish: rpmbuild kamailio-5.2.0-dev1.0.fc25.src.rpm
Finish: build phase for kamailio-5.2.0-dev1.0.fc25.src.rpm
INFO: Done(../../kamailio-5.2.0-dev1.0.fc25.src.rpm) Config(default) 8 minutes 30 seconds
INFO: Results and/or logs in: /var/lib/mock/fedora-25-x86_64/result
Finish: run
```

The `obs` folder aims at using it also in OpenSuse Build Service.

  * https://build.opensuse.org

Kamailio build project on OBS is at:

  * https://build.opensuse.org/project/show/home:kamailio

## Gentoo ##

Not actively mentained, still fairly recent updated.

## BSD ##

There are specs for FreeBSD, NetBST and OpenBSD. They are not actively
maintained, being quite old.

## Solaris ##

Not actively maintained, being quite old.

## Alpine Linux ##

To build apk packages please execute

```
make cfg
make apk
cd alpine && abuild -r
```

NOTICE: Now `abuild -r` command is failed when called from `Makefile`. This reason why need to execute command from shell.