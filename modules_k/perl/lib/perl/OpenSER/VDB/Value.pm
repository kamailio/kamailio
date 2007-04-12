#
# $Id: Value.pm 852 2007-03-01 20:32:37Z bastian $
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

=head1 OpenSER::VDB::Value

This package represents a database value. Additional to the data itself,
information about its type is stored.

=head2 stringification

When accessing a OpenSER::VDB::Value object as a string, it simply returns its data
regardless of its type.
=cut

use strict;

package OpenSER::VDB::Value;

use overload '""' => \&stringify;

sub stringify { shift->{data} }

use OpenSER;
use OpenSER::Constants;

our @ISA = qw ( OpenSER::Utils::Debug );

=head2 new(type,data)

Constructs a new Value object. Its data type and the data are passed as
parameters.

=cut

sub new {
	my $class = shift;
	my $type = shift;
	my $data = shift;

	my $self = {
		type => $type,
		data => $data,
	};

	bless $self, $class;

	return $self;
}


=head2 type()

Returns or sets the current data type. Please consider using the constants
from OpenSER::Constants

=cut

sub type {
	my $self = shift;
	if (@_) {
		$self->{type} = shift;
	}

	return $self->{type};
}


=head2 data()

Returns or sets the current data.

=cut

sub data {
	my $self = shift;
	if (@_) {
		$self->{data} = shift;
	}

	return $self->{data};
}

1;
