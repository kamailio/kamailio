# Kamailio - The Open Source SIP Server [![Build Status](https://travis-ci.org/kamailio/kamailio.svg?branch=master)](https://travis-ci.org/kamailio/kamailio)

Project Website:

  * http://www.kamailio.org

## Overview

Kamailio is an open source implementation of a SIP Signaling Server. SIP is an open standard protocol specified by IETF, the core specification document is [RFC3261](https://tools.ietf.org/html/rfc3261).

It is designed for scalability, targeting large deployments (e.g. for IP telephony operators or carriers, which have a large subscriber base or route a big volume of calls), but can be also used in enterprises or for personal needs to provide VoIP, Instant Messaging and Presence. Kamailio is well known for its flexibility, robustness, strong security and the extensive number of features - for more, see:

  * http://www.kamailio.org/w/features/

Kamailio was started back in 2001 by [Fraunhofer Fokus](https://www.fokus.fraunhofer.de/), a research institute in Berlin, Germany, at that time having the name SIP Express Router (aka SER). In 2005, a fork named OpenSER was created, which was renamed to Kamailio in July 2008 due to trademark issues. Starting in the autumn of 2008, Kamailio and SER initiated the process to merge the two projects. After the merge was complete, Kamailio became the main name of the project, being better protected in terms of trademarks.

Fraunhofer Fokus is no longer actively involved in the evolution of the project, Kamailio being now developed and managed by its world wide community. Fokus still uses Kamailio in its research projects (such as OpenIMSCore) and it is hosting events related to the project, such as developer meetings or Kamailio World Conference.

For more about Kamailio, see the [website of the project](http://www.kamailio.org).

## Contributions

Github pull requests are the recommended way to contribute to Kamailio source code or documentation:

  * https://github.com/kamailio/kamailio/pulls

To keep a coherent and consistent history of the development, the commit messages format and content must follow the rules detailed at:

  * https://www.kamailio.org/wiki/devel/github-contributions

Contributions must conform with licensing rules of the Kamailio project.

## License

Main License: GPLv2.

Each source code file contains at the top the license and copyright details. Most of the code is licensed under GPLv2, some parts of the code are licensed under BSD.

### New Contributions Licensing

New contributions to the core and several main modules (auth, corex, sl, tls, tm) have to be done under the BSD license. New contributions under the GPL must grant the GPL-OpenSSL linking exception. Contributions to existing components released under BSD must be done under BSD as well.

## Documentation

The main index for documentation is available at:

  * http://www.kamailio.org/w/documentation/

The online documentation for modules in the latest stable branch:

  * http://kamailio.org/docs/modules/stable/

The wiki collects a consistent number of tutorials, the indexes for variables, functions and parameters:

  * http://www.kamailio.org/wiki/

Read also the README file from the source code.

### Installation

Step by step tutorials to install Kamailio from source code are available at:

  * http://www.kamailio.org/wiki/start#installation

Read also the INSTALL file from the source code.

Repositories for packages:

  * deb: https://www.kamailio.org/wiki/packages/debs
  * rpm: https://www.kamailio.org/wiki/packages/rpms

## Issues

To report a bug or make a request for new features, use the Issues Page from Github project:

  * https://github.com/kamailio/kamailio/issues

## Open Support and Community Discussions

### Mailing Lists

Mailing list for discussions regarding stable versions of Kamailio:

  * **sr-users (at) lists.sip-router.org** - [sr-users web page](http://lists.sip-router.org/cgi-bin/mailman/listinfo/sr-users)

Mailing list for discussions regarding the development of Kamailio and the state of master (devel) branch:

  * **sr-dev (at) lists.sip-router.org** - [sr-dev web page](http://lists.sip-router.org/cgi-bin/mailman/listinfo/sr-dev)

Mailing list for discussions with commercial purpose:

  * **business (at) lists.kamailio.org** - [business web page](http://lists.kamailio.org/cgi-bin/mailman/listinfo/business)

For more, see:

  * http://www.kamailio.org/w/mailing-lists/

### IRC Channel

An open IRC discussion channel is managed by the community:

  * irc server: irc.freenode.net
  * irc channel: #kamailio

## Useful Resources:

News:

  * http://www.kamailio.org/w/category/news/

## Travis-ci/Testing build environment

 * [travis-ci](https://travis-ci.org/kamailio/kamailio/builds/)
 * [docker build](/test/travis/README.md)

**Thank you for flying Kamailio!**
