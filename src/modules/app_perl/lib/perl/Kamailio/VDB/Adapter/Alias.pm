#
# $Id: Alias.pm 757 2007-01-05 10:56:28Z bastian $
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

=head1 Kamailio::VDB::Adapter::Alias

This package is intended for usage with the alias_db module. The query VTab
has to take two arguments and return an array of two arguments (user name/domain).

=cut

package Kamailio::VDB::Adapter::Alias;

use Kamailio::Constants;
use Kamailio qw ( log );

use Kamailio::VDB;
use Kamailio::VDB::Column;
use Kamailio::VDB::Result;

use Data::Dumper;

our @ISA = qw ( Kamailio::VDB );

=head2 query(conds,retkeys,order)

Queries the vtab with the given arguments for
request conditions, keys to return and sort order column name.

=cut

sub query {
	my $self = shift;

	my $conds = shift;
	my $retkeys = shift; # Unused value here.
	my $order = shift; # Unused value here.

	my @cols;
	
	my $alias_username = undef;
	my $alias_domain = undef;
	my $username;
	my $domain;

	for my $c (@$conds) {
		if (($c->key() eq "alias_username") && ($c->op() eq "=")) {
			$alias_username = $c->data();
		}
		if (($c->key() eq "alias_domain") && ($c->op() eq "=")) {
			$alias_domain = $c->data();
		}
	}

	my $vtab = $self->{vtabs}->{$self->{tablename}};
	$newaddr = $vtab->call("query", $alias_username, $alias_domain);
	
	my $result;

	push @cols, new Kamailio::VDB::Column(DB_STRING, "username");
	push @cols, new Kamailio::VDB::Column(DB_STRING, "domain");

	if ($newaddr) {
		my @row;
		my $resval1 = new Kamailio::VDB::Value(DB_STRING, $newaddr->{username} );
		push @row, $resval1;
		my $resval2 = new Kamailio::VDB::Value(DB_STRING, $newaddr->{domain} );
		push @row, $resval2;
		$result = new Kamailio::VDB::Result(\@cols, (bless \@row, "Kamailio::Utils::Debug"));
	} else {
		$result = new Kamailio::VDB::Result(\@cols);
	}

	return $result;
}

1;
