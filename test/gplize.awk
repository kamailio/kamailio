# add copyright banner after the first comment
#
# text taken from file_copyright.txt

BEGIN {
	status=0;
}


status==1 {
	print 
	next
}

# end of comment not encountered yet

{ comment_begin=0 }

/\/\*/ { comment_begin=1; }


/\*\// {
	status=1
	# if it was a one-line comment print it and start a new comment
	if (comment_begin==1) {
		print
		print "/*"
	}

	print " *"
	print " * Copyright (C) 2001-2004 FhG Fokus"
	print " *"
	print " * This file is part of ser, a free SIP server."
	print " *"
	print " * ser is free software; you can redistribute it and/or modify"
	print " * it under the terms of the GNU General Public License as published by"
	print " * the Free Software Foundation; either version 2 of the License, or"
	print " * (at your option) any later version"
	print " *"
	print " * For a license to use the ser software under conditions"
	print " * other than those described here, or to purchase support for this"
	print " * software, please contact iptel.org by e-mail at the following addresses:"
	print " *    info@iptel.org"
	print " *"
	print " * ser is distributed in the hope that it will be useful,"
	print " * but WITHOUT ANY WARRANTY; without even the implied warranty of"
	print " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
	print " * GNU General Public License for more details."
	print " *"
	print " * You should have received a copy of the GNU General Public License "
	print " * along with this program; if not, write to the Free Software "
	print " * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA"
	print " */"
	print ""

	# don't print the original end of comment
	next
}

# end of comment not encountered yet -- print the line

{ print }
