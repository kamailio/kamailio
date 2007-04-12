#
# $Id: VDB.pm 757 2007-01-05 10:56:28Z bastian $
#
# Perl module for OpenSER
#
# Copyright (C) 2006 Collax GmbH
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

=head1 OpenSER::VDB

This package is an (abstract) base class for all virtual databases. Derived
packages can be configured to be used by OpenSER as a database.

The base class itself should NOT be used in this context, as it does not
provide any functionality.

=cut

package OpenSER::VDB;

use OpenSER;
use OpenSER::Constants;

use OpenSER::VDB::Column;
use OpenSER::VDB::Pair;
use OpenSER::VDB::ReqCond;
use OpenSER::VDB::Result;
use OpenSER::VDB::Value;
use OpenSER::VDB::VTab;

use UNIVERSAL qw ( can );

our @ISA = qw ( OpenSER::Utils::Debug );

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

		OpenSER::log(L_DBG, "perlvdb:VDB: Setting VTab: v is $v (pkg is $pkg, func/method is $3)\n");

		if (can($pkg, $3)) {
			$self->{vtabs}->{$v} = new OpenSER::VDB::VTab( func => $pkg . "::" . $3);
		} elsif (can($v, "init")) {
			$v->init();
			$self->{vtabs}->{$v} = new OpenSER::VDB::VTab( obj => $v );
		} elsif (can($v, "new")) {
			my $obj = $v->new();
			$self->{vtabs}->{$v} = new OpenSER::VDB::VTab( obj => $obj );
		} else {
			OpenSER::log(L_ERR, "perlvdb:VDB: Invalid virtual table.\n");
			return -1;
		}
	} else {
		OpenSER::log(L_ERR, "perlvdb:VDB: Invalid virtual table.\n");
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
	OpenSER::log(L_INFO, "perlvdb:Insert not implemented in base class.\n");
	return -1;
}

sub replace {
	OpenSER::log(L_INFO, "perlvdb:Replace not implemented in base class.\n");
	return -1;
}

sub delete {
	OpenSER::log(L_INFO, "perlvdb:Delete not implemented in base class.\n");
	return -1;
}

sub update {
	OpenSER::log(L_INFO, "perlvdb:Update not implemented in base class.\n");
	return -1;
}

sub query {
	OpenSER::log(L_INFO, "perlvdb:Query not implemented in base class.\n");
	return -1;
}

1;
