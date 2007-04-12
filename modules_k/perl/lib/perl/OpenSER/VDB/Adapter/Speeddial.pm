#
# $Id: Speeddial.pm 757 2007-01-05 10:56:28Z bastian $
#
# Perl module for OpenSER
#
# Copyright (C) 2007 Collax GmbH
#                    (Bastian Friedrich <bastian.friedrich@collax.com>)
#
# This file is part of openser, a free SIP server.
#
# openser is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# openser is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

=head1 OpenSER::VDB::Adapter::Speeddial

This adapter can be used with the speeddial module.

=cut

package OpenSER::VDB::Adapter::Speeddial;

use OpenSER::Constants;
use OpenSER qw ( log );

use OpenSER::VDB;
use OpenSER::VDB::Column;
use OpenSER::VDB::Result;

our @ISA = qw ( OpenSER::VDB );

sub query {
	my $self = shift;

	my $conds = shift;
	my $retkeys = shift; # Unused value here.
	my $order = shift; # Unused value here.

	my @cols;
	
	my $requested_username;
	my $requested_sd_username;
	
	for my $c (@$conds) {
		if (($c->key() eq "username") && ($c->op() eq "=")) {
			$requested_username = $c->data();
		}
		if (($c->key() eq "sd_username") && ($c->op() eq "=")) {
			$requested_sd_username = $c->data();
		}
	}

	my $vtab = $self->{vtabs}->{$self->{tablename}};
	$newaddr = $vtab->call("query", $requested_username, $requested_sd_username);
	
	my $result;

	push @cols, new OpenSER::VDB::Column(DB_STRING, "uid_name");

	if ($newaddr) {
		my $resval = new OpenSER::VDB::Value(DB_STRING, $newaddr );
		push my @row, $resval;
		$result = new OpenSER::VDB::Result(\@cols, (bless \@row, "OpenSER::Utils::Debug"));
	} else {
		$result = new OpenSER::VDB::Result(\@cols);
	}

	return $result;
}

1;
