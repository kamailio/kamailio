#
# $Id: ReqCond.pm 757 2007-01-05 10:56:28Z bastian $
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

=head1 Kamailio::VDB::ReqCond

This package represents a request condition for database access, consisting of a 
column name, an operator (=, <, >, ...), a data type and a value.

This package inherits from Kamailio::VDB::Pair and thus includes its methods.

=cut

package Kamailio::VDB::ReqCond;

use Kamailio::VDB::Value;
use Kamailio::VDB::Pair;

our @ISA = qw ( Kamailio::VDB::Pair Kamailio::Utils::Debug );

=head2 new(key,op,type,name)

Constructs a new Column object.

=cut

sub new {
	my $class = shift;
	my $key = shift;
	my $op = shift;
	my $type = shift;
	my $data = shift;

	my $self = new Kamailio::VDB::Pair($key, $type, $data);

	bless $self, $class;

	$self->{op} = $op;

	return $self;
}


=head2 op()

Returns or sets the current operator.

=cut

sub op {
	my $self = shift;
	if (@_) {
		$self->{op} = shift;
	}

	return $self->{op};
}

1;
