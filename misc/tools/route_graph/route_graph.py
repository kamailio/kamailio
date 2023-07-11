#! /usr/bin/env python3
"""
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
"""

import sys, re

max_depth = 120
debug = 0

re_main_route = re.compile("^([a-z]+_)*route[\s\t]*(?![\(\)])[\s\t]*\{?", re.I)  # Regular expression to match main routes
re_def_route = re.compile("^([a-z]+_)*route ?(\[\"?([A-Za-z0-9-_:]+)\"?\])+[\s\t]*\{?", re.I)  # Regular expression to match defined routes
re_call_route = re.compile("^(.*\([\s\t!]*)?route\(\"?([A-Za-z0-9-_]+)\"?\)", re.I)  # Regular expression to match called routes

routes = {}  # Dictionary to store main routes
f_routes = {}  # Dictionary to store failure routes
b_routes = {}  # Dictionary to store branch routes
or_routes = {}  # Dictionary to store onreply routes
r_routes = {}  # Dictionary to store reply routes
s_routes = {}  # Dictionary to store onsend routes
e_routes = {}  # Dictionary to store event routes

def log(_s):
    if debug:
        print(_s)

def print_route_level(_l, _n):
    log(f"prl: {_l}, {_n}")
    if _l > max_depth:
        return
    spacer = ""
    route = ""
    for i in range(_l):
        spacer += "|  "
        if i < _l - 1:
            route += "|  "
        else:
            route += "\- " + str(_n)
    if len(spacer) > 0:
        print(spacer)
    if len(route) > 0:
        print(route)

def traverse_routes(_level, _name):
    log(f"tr: {_level}, {_name}")
    if _level > max_depth:
        print("warning: max_depth reached")
        return
    print_route_level(_level, _name)
    if _name in routes:
        for r in routes[_name]:
            traverse_routes(_level + 1, r)

# Checking command-line arguments
if len(sys.argv) < 2:
    raise Exception('wrong number of arguments\nusage: ' + sys.argv[0] + ' configuration-file [max_depth]')
if len(sys.argv) == 3:
    max_depth = int(sys.argv[2])
cfg = open(sys.argv[1], "r")
if cfg is None:
    raise Exception('Missing config file')

# Reading the configuration file
line = cfg.readline()
rt = routes
while line:
    line = line.strip()
    if not line.startswith("#"):
        log(line)
        main_match = re_main_route.search(line)
        def_match = re_def_route.search(line)
        call_match = re_call_route.search(line)

        # Matching a called route
        if call_match is not None:
            log("CALL: " + line)
            name = call_match.group(2)
            log(f"{rname}:{name}")
            rt[rname].append(name)

        # Matching a defined route
        elif def_match is not None:
            log("DEF: " + line)
            rtype = def_match.group(1)
            rname = def_match.group(3)
            if rtype == "failure_":
                rt = f_routes
                if rname is None:
                    rname = "failure"
            elif rtype == "onreply_":
                rt = or_routes
                if rname is None:
                    rname = "onreply"
            elif rtype == "reply_":
                rt = r_routes
                if rname is None:
                    rname = "reply"
            elif rtype == "onsend_":
                rt = s_routes
                if rname is None:
                    rname = "onsend"
            elif rtype == "branch_":
                rt = b_routes
                if rname is None:
                    rname = "branch"
            elif rtype == "event_":
                rt = e_routes
                if rname is None:
                    rname = "event"
            else:
                rt = routes
            log(rname)
            rt[rname] = []

        # Matching a main route
        elif main_match is not None:
            log("MAIN: " + line)
            rtype = main_match.group(1)
            if rtype == "failure_":
                rt = f_routes
                rname = "failure"
            elif rtype == "onreply_":
                rt = or_routes
                rname = "onreply"
            elif rtype == "reply_":
                rt = r_routes
                rname = "reply"
            elif rtype == "onsend_":
                rt = s_routes
                rname = "onsend"
            elif rtype == "branch_":
                rt = b_routes
                rname = "branch"
            elif rtype == "event_":
                rt = e_routes
                rname = "event"
            else:
                rt = routes
                rname = "Main"
            log(rname)
            rt[rname] = []

    line = cfg.readline()

log(f"routes: {routes}")
log(f"branch_routes: {b_routes}")
log(f"failure_routes: {f_routes}")
log(f"onreply_routes: {or_routes}")
log(f"reply_routes: {r_routes}")
log(f"onsend_routes: {s_routes}")
log(f"event_routes: {e_routes}")

# Checking for missing routes
for name in routes.keys():
    for val in routes[name]:
        if val not in routes:
            print(f"Missing Route {val}!!!")

print("\nMain")
traverse_routes(0, "Main")

# Printing branch routes
if len(b_routes) > 0:
    print("\nBranch routes\n-------------")
    for br in b_routes.keys():
        print(f"\n{br}")
        for r in b_routes[br]:
            traverse_routes(1, r)

# Printing onsend routes
if len(s_routes) > 0:
    print("\nSend routes\n-----------")
    for sr in s_routes.keys():
        print(f"\n{sr}")
        for r in s_routes[sr]:
            traverse_routes(1, r)

# Printing failure routes
if len(f_routes) > 0:
    print("\nFailure routes\n--------------")
    for fr in f_routes.keys():
        print(f"\n{fr}")
        for r in f_routes[fr]:
            traverse_routes(1, r)

# Printing onreply routes
if len(or_routes) > 0:
    print("\nOnreply routes\n--------------")
    for onr in or_routes.keys():
        print(f"\n{onr}")
        for r in or_routes[onr]:
            traverse_routes(1, r)

# Printing reply routes
if len(r_routes) > 0:
    print("\nReply routes\n--------------")
    for onr in r_routes.keys():
        print(f"\n{onr}")
        for r in r_routes[onr]:
            traverse_routes(1, r)

# Printing event routes
if len(e_routes) > 0:
    print("\nEvent routes\n--------------")
    for onr in e_routes.keys():
        print(f"\n{onr}")
        for r in e_routes[onr]:
            traverse_routes(1, r)

print()
cfg.close()
