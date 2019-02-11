#
# $Id$
#
# Perl module for Kamailio
#
# Copyright (C) 2006 Collax GmbH
#		     (Bastian Friedrich <bastian.friedrich@collax.com>)
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

package Kamailio::Message;
require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
our @EXPORT = qw ( t );
bootstrap Kamailio;

sub AUTOLOAD{
	use vars qw($AUTOLOAD);
	my $a = $AUTOLOAD;
	
	$a =~ s/^Kamailio::Message:://;

	my $l = scalar @_;
	if ($l == 0) {
		croak("Usage: $a(self, param1 = undef, param2 = undef)");
	} elsif ($l == 1) {
		return Kamailio::Message::moduleFunction(@_[0], $a);
	} elsif ($l == 2) {
		return Kamailio::Message::moduleFunction(@_[0], $a, @_[1]);
	} elsif ($l == 3) {
		return Kamailio::Message::moduleFunction(@_[0],
							$a, @_[1], @_[2]);
	} else {
		croak("Usage: $a(self, param1 = undef, param2 = undef)");
	}

}

sub DESTROY {}

1;

