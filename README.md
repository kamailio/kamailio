# Kamailio - The Open Source SIP Server

[![Build Status](https://github.com/kamailio/kamailio/actions/workflows/main.yml/badge.svg)](https://github.com/kamailio/kamailio/actions)
[![Code Triage Badge](https://www.codetriage.com/kamailio/kamailio/badges/users.svg)](https://www.codetriage.com/kamailio/kamailio)

Project Website:

  * https://www.kamailio.org

[20 Years Of Kamailio Development: Sep 3, 2001 - Sep 3, 2021](https://www.kamailio.org/w/2021/09/kamailio-20-years-of-development/)

## Overview

Kamailio is an open source implementation of a SIP Signaling Server. SIP is an open standard protocol specified by the IETF. The core specification document is [RFC3261](https://tools.ietf.org/html/rfc3261).

The Kamailio SIP server is designed for scalability, targeting large deployments (e.g. for IP telephony operators or carriers, which have a large subscriber base or route a big volume of calls), but can be also used in enterprises or for personal needs to provide VoIP, Instant Messaging and Presence. Kamailio is well known for its flexibility, robustness, strong security and the extensive number of features - for more information, please see:

  * https://www.kamailio.org/w/features/

Kamailio development was started back in 2001 by [Fraunhofer Fokus](https://www.fokus.fraunhofer.de/), a research institute in Berlin, Germany. At that time the project name was SIP Express Router (aka SER). In 2005, a fork named OpenSER was created, which was renamed to Kamailio in July 2008 due to trademark issues. Starting in the autumn of 2008, Kamailio and SER initiated the process to merge the two projects. After the merge was complete, Kamailio became the main name of the project, being better protected in terms of trademarks.

Fraunhofer Fokus is no longer actively involved in the evolution of the project. Kamailio is  now developed and managed by its world wide community. Fokus still uses Kamailio in its research projects (such as OpenIMSCore) and it is hosting events related to the project, such as developer meetings or the Kamailio World Conference.

For more information about Kamailio, see the [website of the project](https://www.kamailio.org), where you can find pointers to documentation, the project wiki and much more.

## Contributions

Github pull requests are the recommended way to contribute to Kamailio source code or documentation:

  * https://github.com/kamailio/kamailio/pulls

To keep a coherent and consistent history of the development, the commit messages format and content must follow the rules detailed at:

  * https://www.kamailio.org/wikidocs/devel/github-contributions

Contributions must conform with licensing rules of the Kamailio project.

## License

Main License: GPLv2.

Each source code file refers to the license and copyright details in the top of the file. Most of the code is licensed under GPLv2, some parts of the code are licensed under BSD.

### License Of New Code Contributions

New contributions to the core and several main modules (auth, corex, sl, tls, tm) have to be done under the BSD license. New contributions under the GPL must grant the GPL-OpenSSL linking exception. Contributions to existing components released under BSD must be done under BSD as well.

## Documentation

The main index for documentation is available at:

  * https://www.kamailio.org/w/documentation/

The online documentation for modules in the latest stable branch:

  * https://kamailio.org/docs/modules/stable/

The wiki collects a consistent number of tutorials, the indexes for variables, functions and parameters:

  * https://www.kamailio.org/wikidocs/

Please read the README file in the source code, one per module.

### Installation

Step by step tutorials to install Kamailio from source code are available at:

  * https://www.kamailio.org/wikidocs/#installation

Please read the INSTALL file from the source code for more information.

Repositories for Linux packages:

  * deb: https://www.kamailio.org/wikidocs/packages/debs
  * rpm: https://www.kamailio.org/wikidocs/packages/rpms

## Issues And Bug Reports

To report a bug or make a request for new features, use the Issues Page in the Kamailio Github project:

  * https://github.com/kamailio/kamailio/issues

## Open Support And Community Discussions

### Mailing Lists

Mailing list for discussions regarding stable versions of Kamailio:

  * **sr-users (at) lists.kamailio.org** - [sr-users web page](https://lists.kamailio.org/mailman3/postorius/lists/sr-users.lists.kamailio.org/)

Mailing list for discussions regarding the development of Kamailio and the state of master (devel) branch:

  * **sr-dev (at) lists.kamailio.org** - [sr-dev web page](https://lists.kamailio.org/mailman3/postorius/lists/sr-dev.lists.kamailio.org/)

Mailing list for discussions with commercial purpose:

  * **business (at) lists.kamailio.org** - [business web page](https://lists.kamailio.org/mailman3/postorius/lists/business.lists.kamailio.org/)

For more information about the mailing lists, please see:

  * https://www.kamailio.org/w/mailing-lists/

### IRC Channel

An open IRC discussion channel is managed by the community:

  * irc server: irc.freenode.net
  * irc channel: #kamailio

### Matrix Channel

An open Matrix discussion channel is managed by the community:

  * server: https://riot.kamailio.dev/
  * Room: https://riot.kamailio.dev/#/room/#kamailio:matrix.kamailio.dev

## Useful Resources

News:

  * https://www.kamailio.org/w/category/news/
  * Twitter @kamailio

## Travis-CI - Testing Build Environment

 * [travis-ci](https://travis-ci.org/kamailio/kamailio/builds/)
 * [docker build](/test/travis/README.md)

**Thank you for flying Kamailio!**
