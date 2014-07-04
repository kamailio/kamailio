#
# $Id: VDB.pm 757 2007-01-05 10:56:28Z bastian $
#
# Perl module for Kamailio
#
# Copyright (C) 2006 Collax GmbH
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

=head1 Kamailio::VDB

This package is an (abstract) base class for all virtual databases. Derived
packages can be configured to be used by Kamailio as a database.

The base class itself should NOT be used in this context, as it does not
provide any functionality.

=cut

package Kamailio::VDB;

use Kamailio;
use Kamailio::Constants;

use Kamailio::VDB::Column;
use Kamailio::VDB::Pair;
use Kamailio::VDB::ReqCond;
use Kamailio::VDB::Result;
use Kamailio::VDB::Value;
use Kamailio::VDB::VTab;

use UNIVERSAL qw ( can );

our @ISA = qw ( Kamailio::Utils::Debug );

sub new {
	my $class = shift;

	my $self = {
		tablename => undef,
		vtabs => {}
	};

	bless $self, $class;

	return $self;
}

sub use_table {
	my $self = shift;
	my $v = shift;
	
	$self->{tablename} = $v;

	if ($v eq "version") {
		# "version" table is handled individually. Don't initialize VTabs.
		return 0;
	}

	if ($self->{vtabs}->{$v}) {
		return 0;
	}

	if ($v =~ m/^((.*)::)?([\w_][\d\w_]*)$/) {
		my $pkg;
		if (!$2) {
			$pkg = "main";
		} else {
			$pkg = $2;
		}

		Kamailio::log(L_DBG, "perlvdb:VDB: Setting VTab: v is $v (pkg is $pkg, func/method is $3)\n");

		if (can($pkg, $3)) {
			$self->{vtabs}->{$v} = new Kamailio::VDB::VTab( func => $pkg . "::" . $3);
		} elsif (can($v, "init")) {
			$v->init();
			$self->{vtabs}->{$v} = new Kamailio::VDB::VTab( obj => $v );
		} elsif (can($v, "new")) {
			my $obj = $v->new();
			$self->{vtabs}->{$v} = new Kamailio::VDB::VTab( obj => $obj );
		} else {
			Kamailio::log(L_ERR, "perlvdb:VDB: Invalid virtual table.\n");
			return -1;
		}
	} else {
		Kamailio::log(L_ERR, "perlvdb:VDB: Invalid virtual table.\n");
		return -1;
	}
}

sub _insert {
	my $self = shift;
	return $self->insert(@_);
}

sub _replace {
	my $self = shift;
	return $self->replace(@_);
}

sub _delete {
	my $self = shift;
	return $self->delete(@_);
}

sub _update {
	my $self = shift;
	return $self->update(@_);
}

sub _query {
	my $self = shift;
	return $self->query(@_);
}



sub insert {
	Kamailio::log(L_INFO, "perlvdb:Insert not implemented in base class.\n");
	return -1;
}

sub replace {
	Kamailio::log(L_INFO, "perlvdb:Replace not implemented in base class.\n");
	return -1;
}

sub delete {
	Kamailio::log(L_INFO, "perlvdb:Delete not implemented in base class.\n");
	return -1;
}

sub update {
	Kamailio::log(L_INFO, "perlvdb:Update not implemented in base class.\n");
	return -1;
}

sub query {
	Kamailio::log(L_INFO, "perlvdb:Query not implemented in base class.\n");
	return -1;
}

1;
