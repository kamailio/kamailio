#
# $Id: Auth.pm 757 2007-01-05 10:56:28Z bastian $
#
# Perl module for Kamailio
#
# Copyright (C) 2007 Collax GmbH
#                    (Bastian Friedrich <bastian.friedrich@collax.com>)
#
# This file is part of Kamailio, a free SIP server.
#
# Kamailio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# Kamailio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

=head1 Kamailio::VDB::Adapter::Auth

This adapter is intended for usage with the auth_db module.
The VTab should take a username as an argument and return a (plain text!)
password.

=cut

package Kamailio::VDB::Adapter::Auth;

use Kamailio::Constants;
use Kamailio qw ( log );

use Kamailio::VDB;
use Kamailio::VDB::Column;
use Kamailio::VDB::Result;
use Kamailio::VDB::Adapter::TableVersions;

use Data::Dumper;

our @ISA = qw ( Kamailio::VDB );

sub query {
	my $self = shift;

	my $conds = shift;
	my $retkeys = shift;
	my $order = shift; # Unused value here.

	my @cols;
	
	my $username = undef;
	my $password = undef;

	if ($self->{tablename} eq "version") {
		return Kamailio::VDB::Adapter::TableVersions::version(@$conds[0]->data());
	}
		
	if ((scalar @$conds != 1) || (scalar @$retkeys != 2)) {
		log(L_ERR, "perlvdb:Auth: Broken column count requested. Unknown behavior. Desperately exiting.\n");
		return undef;
	}

	for my $c (@$conds) {
		$username = $c->data();
	}

	for my $k (@$retkeys) {
		push @cols, new Kamailio::VDB::Column(DB_STRING, $k);
	}

	my $vtab = $self->{vtabs}->{$self->{tablename}};
	$password = $vtab->call("query", $username);
	
	my $result;

	if ($password) {
		my @row;
		push @row, new Kamailio::VDB::Value(DB_STRING, $password);
		push @row, undef;
		$result = new Kamailio::VDB::Result(\@cols, (bless \@row, "Kamailio::Utils::Debug"));
	} else {
		$result = new Kamailio::VDB::Result(\@cols);
	}

	return $result;
}

1;
