#
# $Id: TableVersions.pm 757 2007-01-05 10:56:28Z bastian $
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

package OpenSER::VDB::Adapter::TableVersions;

use OpenSER;
use OpenSER::Constants;

sub version {
	my $table = shift;

	# FIXME We need to split table name again - possibly, this is a function only, so this one will not work
	my $v = $table->version();

	my @cols;
	my @row;

	push @cols, new OpenSER::VDB::Column(DB_INT, "table_version");
	push @row, new OpenSER::VDB::Value(DB_INT, $v);
	
	return new OpenSER::VDB::Result(\@cols, (bless \@row, OpenSER::Utils::Debug));
}

1;

