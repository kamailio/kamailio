#! /usr/bin/env python
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
import sys,re

max_depth = 120
debug = 0

re_main_route = re.compile("^([a-z]+_)*route[\s\t]*(?![\(\)])[\s\t]*\{?", re.I)
re_def_route = re.compile("^([a-z]+_)*route(\[\"?([A-Za-z0-9-_:]+)\"?\])+[\s\t]*\{?", re.I)
re_call_route = re.compile("^(.*\([\s\t!]*)?route\(\"?([A-Za-z0-9-_]+)\"?\)", re.I)
routes = {}
f_routes = {}
b_routes = {}
r_routes = {}
s_routes = {}
e_routes = {}


def log(_s):
	if debug:
		print _s

def print_route_level(_l, _n):
	log("prl: %i, %s" % (_l, _n))
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
		print spacer
	if len(route) > 0:
		print route

def traverse_routes(_level, _name):
	log("tr: %i, %s" % (_level, _name))
	if _level > max_depth:
		print "warning: max_depth reached"
		return
	print_route_level(_level, _name)
	if routes.has_key(_name):
		for r in routes[_name]:
			traverse_routes(_level + 1, r)


if len(sys.argv) < 2:
	raise "usage: %s configuration-file [max_depth]" % sys.argv[0]
if len(sys.argv) == 3:
	max_depth = int(sys.argv[2])
cfg = file(sys.argv[1], "r")
if cfg == None:
	raise "Missing config file"
line = cfg.readline()
rt = routes
while line:
	line = line.strip()
	if not line.startswith("#"):
		log(line)
		main_match = re_main_route.search(line)
		def_match = re_def_route.search(line)
		call_match = re_call_route.search(line)
		if not call_match == None:
			log("CALL: " + line)
			name = call_match.group(2)
			log(rname +":"+name)
			rt[rname].append(name)
		elif not def_match == None:
			log("DEF: " + line)
			rtype = def_match.group(1)
			rname = def_match.group(3)
			if rtype == "failure_":
				rt = f_routes
				if rname == None:
					rname = "failure"
			elif rtype == "onreply_":
				rt = r_routes
				if rname == None:
					rname = "onreply"
			elif rtype == "onsend_":
				rt = s_routes
				if rname == None:
					rname = "onsend"
			elif rtype == "branch_":
				rt = b_routes
				if rname == None:
					rname = "branch"
			elif rtype == "event_":
				rt = e_routes
				if rname == None:
					rname = "event"
			else:
				rt = routes
			log(rname)
			rt[rname] = []
		elif not main_match == None:
			log("MAIN: " + line)
			rtype = main_match.group(1)
			if rtype == "failure_":
				rt = f_routes
				rname = "failure"
			elif rtype == "onreply_":
				rt = r_routes
				rname = "onreply"
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

log("routes: %s" % (routes))
log("branch_routes: %s" % (b_routes))
log("failure_routes: %s" % (f_routes))
log("onreply_routes: %s" % (r_routes))
log("onsend_routes: %s" % (s_routes))
log("event_routes: %s" % (e_routes))

for name in routes.keys():
	for val in routes[name]:
		if not routes.has_key(val):
			print "Missing Route %s!!!" % val
# checking for unreferenced routes does not work yet because functions
# can call routes as well?!
	#found = False
	#for n in routes.keys():
	#	for v in routes[n]:
	#		if v == name:
	#			found = True
	#if not found and (not (name == "Main" or name == "Failure" or name == "Onreply" or name == "Branch")):
	#	print "Unreferenced Route %s!!!" % name

print "\nMain"
traverse_routes(0, "Main")

if len(b_routes) > 0:
	print "\nBranch routes\n-------------"
	for br in b_routes.keys():
		print "\n%s" % (br)
		for r in b_routes[br]:
			traverse_routes(1, r)

if len(s_routes) > 0:
	print "\nSend routes\n-----------"
	for sr in s_routes.keys():
		print "\n%s" % (sr)
		for r in s_routes[sr]:
			traverse_routes(1, r)

if len(f_routes) > 0:
	print "\nFailure routes\n--------------"
	for fr in f_routes.keys():
		print "\n%s" % (fr)
		for r in f_routes[fr]:
			traverse_routes(1, r)

if len(r_routes) > 0:
	print "\nOnreply routes\n--------------"
	for onr in r_routes.keys():
		print "\n%s" % (onr)
		for r in r_routes[onr]:
			traverse_routes(1, r)

if len(e_routes) > 0:
	print "\nEvent routes\n--------------"
	for onr in e_routes.keys():
		print "\n%s" % (onr)
		for r in e_routes[onr]:
			traverse_routes(1, r)

print
