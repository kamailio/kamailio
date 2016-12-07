#
# $Id: Result.pm 852 2007-03-01 20:32:37Z bastian $
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

use strict;

=head2 Kamailio::VDB::Result

This class represents a VDB result set. It contains a
column definition, plus an array of rows.
Rows themselves are simply references to arrays of scalars.

=cut

package Kamailio::VDB::Result;

use Kamailio;
use Kamailio::VDB::Column;

our @ISA = qw ( Kamailio::Utils::Debug );

=head2 new(coldefs,[row, row, ...])

The constructor creates a new Result object.
Its first parameter is a reference to an array of
 Kamailio::VDB::Column objects.
Additional parameters may be passed to provide initial rows, which
are references to arrays of scalars.

=cut

sub new {
	my $class = shift;
	my $coldefs = shift;

	my $self = {};

	bless $self, $class;

	$self->{coldefs} = $coldefs;

	while (@_) {
		push @{$self->{rows}}, shift;
	}

	return $self;
}

=head2 coldefs()

 Returns or sets the column definition of the object.

=cut

sub coldefs {
	my $self = shift;

	if (@_) {
		$self->{coldefs} = shift;
	}

	return $self->{coldefs};
}

=head2 rows()

 Returns or sets the rows of the object.

=cut

sub rows {
	my $self = shift;

	while (@_) {
		push @{$self->{rows}}, shift;
	}

	return $self->{rows};
}

1;
