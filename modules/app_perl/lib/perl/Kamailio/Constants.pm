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

=head1 Kamailio::Constants

This package provides a number of constants taken from enums and defines of
Kamailio header files. Unfortunately, there is no mechanism for updating the
constants automatically, so check the values if you are in doubt.

=cut

package Kamailio::Constants;

use Exporter;

# Constants from route_struct.h

our @ISA = qw(Exporter);

our @EXPORT;

BEGIN {

	sub const {
		my $c = shift;
		my $v = shift;

		eval "use constant $c => $v";

		push(@EXPORT, $c);
	}

###########################################################
## Unfortunately, there are no "enum"s in Perl. The following blocks reflect
## some C headers from Kamailio.
## UPDATE THIS FILE WHEN THESE HEADER FILES CHANGE!

#####################
# From parse_fline.h
	const( SIP_REQUEST	=> 1);
	const( SIP_REPLY	=> 2);
	const( SIP_INVALID	=> 0);

#####################
# From route_struct.h

	const( EXP_T	=> 1);
	const( ELEM_T	=> 2);


	const( AND_OP	=> 1);
	const( OR_OP	=> 2);
	const( NOT_OP	=> 3);


	const( EQUAL_OP	=> 10);
	const( MATCH_OP	=> 11);
	const( GT_OP	=> 12);
	const( LT_OP	=> 13);
	const( GTE_OP	=> 14);
	const( LTE_OP	=> 15);
	const( DIFF_OP	=> 16);
	const( NO_OP	=> 17);


	const( METHOD_O		=> 1);
	const( URI_O		=> 2);
	const( FROM_URI_O	=> 3);
	const( TO_URI_O		=> 4);
	const( SRCIP_O		=> 5);
	const( SRCPORT_O	=> 6);
	const( DSTIP_O		=> 7);
	const( DSTPORT_O	=> 9);
	const( PROTO_O		=> 9);
	const( AF_O		=> 10);
	const( MSGLEN_O		=> 11);
	const( DEFAULT_O	=> 12);
	const( ACTION_O		=> 13);
	const( NUMBER_O		=> 14);
	const( RETCODE_O	=> 15);


	const( FORWARD_T		=> 1);
	const( SEND_T			=> 2);
	const( DROP_T			=> 3);
	const( LOG_T			=> 4);
	const( ERROR_T			=> 5);
	const( ROUTE_T			=> 6);
	const( EXEC_T			=> 7);
	const( SET_HOST_T		=> 8);
	const( SET_HOSTPORT_T		=> 9);
	const( SET_USER_T		=> 10);
	const( SET_USERPASS_T		=> 11);
	const( SET_PORT_T		=> 12);
	const( SET_URI_T		=> 13);
	const( IF_T			=> 14);
	const( MODULE_T			=> 15);
	const( SETFLAG_T		=> 16);
	const( RESETFLAG_T		=> 17);
	const( ISFLAGSET_T 		=> 18);
	const( LEN_GT_T			=> 19);
	const( PREFIX_T			=> 20);
	const( STRIP_T			=> 21);
	const( STRIP_TAIL_T		=> 22);
	const( APPEND_BRANCH_T		=> 23);
	const( REVERT_URI_T		=> 24);
	const( FORCE_RPORT_T		=> 25);
	const( FORCE_LOCAL_RPORT_T	=> 26);
	const( SET_ADV_ADDR_T		=> 27);
	const( SET_ADV_PORT_T		=> 28);
	const( FORCE_TCP_ALIAS_T	=> 29);
	const( FORCE_SEND_SOCKET_T	=> 30);
	const( SERIALIZE_BRANCHES_T	=> 31);
	const( NEXT_BRANCHES_T		=> 32);
	const( RETURN_T			=> 33);
	const( EXIT_T			=> 34);
	const( SWITCH_T			=> 35);
	const( CASE_T			=> 36);
	const( DEFAULT_T		=> 37);
	const( SBREAK_T			=> 38);
	const( SET_DSTURI_T		=> 39);
	const( RESET_DSTURI_T		=> 40);
	const( ISDSTURISET_T		=> 41);


	const( NOSUBTYPE	=> 0);
	const( STRING_ST	=> 1);
	const( NET_ST		=> 2);
	const( NUMBER_ST	=> 3);
	const( IP_ST		=> 4);
	const( RE_ST		=> 5);
	const( PROXY_ST		=> 6);
	const( EXPR_ST		=> 7);
	const( ACTIONS_ST	=> 8);
	const( CMD_ST		=> 9);
	const( MODFIXUP_ST	=> 10);
	const( MYSELF_ST	=> 11);
	const( STR_ST		=> 12);
	const( SOCKID_ST	=> 13);
	const( SOCKETINFO_ST	=> 14);


#####################
# non-enum constants from dprint.h:
# Logging levels
	const( L_NPRL	=> -6);
	const( L_MIN	=> -5);
	const( L_ALERT	=> -5);
	const( L_BUG	=> -4);
	const( L_CRIT2  => -3);
	const( L_CRIT	=> -2);
	const( L_ERR	=> -1);
	const( L_WARN	=> 0);
	const( L_NOTICE	=> 1);
	const( L_INFO	=> 2);
	const( L_DBG	=> 3);


#####################
# From flags.h:
# Flags for setflag et.al
	const( FL_WHITE		=> 1);
	const( FL_YELLOW	=> 2);
	const( FL_GREEN		=> 3);
	const( FL_RED		=> 4);
	const( FL_BLUE		=> 5);
	const( FL_MAGENTA	=> 6);
	const( FL_BROWN		=> 7);
	const( FL_BLACK		=> 8);
	const( FL_ACC		=> 9);
	const( FL_MAX		=> 10);


#####################
# From db/db_val.h:
# Value types for virtual database classes
	const( DB_INT		=> 0 );
	const( DB_DOUBLE	=> 1 );
	const( DB_STRING	=> 2 );
	const( DB_STR		=> 3 );
	const( DB_DATETIME	=> 4 );
	const( DB_BLOB		=> 5 );
	const( DB_BITMAP	=> 6 );


#####################
# For VDB call types
	const( VDB_CALLTYPE_FUNCTION_SINGLE	=> 0 );
	const( VDB_CALLTYPE_FUNCTION_MULTI	=> 0 );
	const( VDB_CALLTYPE_METHOD		=> 1 );

}

1;
