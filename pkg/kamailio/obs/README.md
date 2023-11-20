# RPM Specs

##  For OpenSuse Build Service (OBS)

Here are the RPM packaging specs for openSUSE Build Service:

 * https://build.opensuse.org/

It builds RPM packages for following distributions:

* CentOS 6 and 7
* RHEL 6 and 7
* Fedora 25 and 26
* openSUSE 42.2 and 42.3

Kamailio's RPMs bulding projects for various versions can be found at:

* https://build.opensuse.org/project/subprojects/home:kamailio
  
## For local build using mock

    # prepare source tarball from repo
	cd kamailio/
	mkdir -p ../rpmbuild/SOURCES ../rpmbuild/SPECS
	./pkg/kamailio/scripts/git-archive-all.sh kamailio-5.8.0 ../rpmbuild/SOURCES/kamailio-5.8.0_src
	# prepare src.rpm with version/release: replace MYVER/MYREL below
	cp pkg/kamailio/obs/kamailio.spec ../rpmbuild/SPECS/
	sed -i -e s'/^%define ver.*/%define ver MYVER/' -e s'/^%define rel.*/%define rel MYREL/' ../rpmbuild/SPECS/kamailio.spec
	rpmbuild -bs --define "_topdir $PWD/../rpmbuild" ../rpmbuild/SPECS/kamailio.spec
	
	# build for EL8/9
	mock -r pkg/kamailio/obs/kamailio-8-x86_64.cfg ../rpmbuild/SRPMS/*src.rpm
	mock -r pkg/kamailio/obs/kamailio-9-x86_64.cfg ../rpmbuild/SRPMS/*src.rpm


