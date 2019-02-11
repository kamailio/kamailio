#!/usr/bin/perl

# perl script for sending an xmlrpc command to ser's xmlrpc module,
# extra verbose output
# Usage:  perl xmlrpc_test.pl command [params...]
#
# History:
# --------
#  2009-07-13  initial version (andrei)
#
#

use strict;
use warnings;

use XMLRPC::Lite;

	my $rpc=shift @ARGV;
	my @rpc_params=@ARGV;
	my $k;
	my %r;
	my $i;

	if (!defined $rpc) {
		die "Usage: $0 rpc_command [args..]";
	}

# actual rpc call

	my($rpc_call) = XMLRPC::Lite
		-> proxy("http://127.0.0.1:5060") -> call($rpc, @rpc_params);
	
	my $res= $rpc_call->result;

# extra verbose result printing (could be skipped)

	if (!defined $res){
		print "fault{\n";
		$res=$rpc_call->fault;
		%r=%{$res};
		foreach $k (sort keys %r) {
			print("\t$k: $r{$k}\n");
		}
		print "}\n";
		exit -1;
	}
	if (ref($res) eq "HASH"){
		print("{\n");
		%r=%{$res};
		foreach $k (keys %r) {
			print("\t$k: ",  $r{$k}, "\n");
		}
		print("}\n");
	} elsif (ref($res) eq "ARRAY"){
		print "[\n";
		for ($i=0; $i<@{$res}; $i++){
			print "\t${$res}[$i]\n";
		}
		print "]\n";
	}elsif (ref($res) eq "SCALAR"){
		print "${$res}\n";
	}elsif (!ref($res)){
		print "$res\n";
	}else{
		print("ERROR: reference to ", ref($res), " not handled\n");
	}

