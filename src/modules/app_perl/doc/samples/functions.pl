use Kamailio qw ( log );
use Kamailio::Constants;

#This function demonstrates how to call functions that are exported by other modules.
sub exportedfuncs {
	my $m = shift;
	my $res = -1;

	if (($m->getMethod() eq "INVITE") || ($m->getMethod eq "CANCEL")) {
		
		$m->moduleFunction("xlog", "L_INFO", "x foobar");
		
		if ($m->getRURI() =~ m/sip:555[0-9]+@/) {
			$m->moduleFunction("sl_send_reply", "500", "Error: 555 not available");
		} else {
			$res = $m->moduleFunction("alias_db_lookup", "dbaliases");
		}
	}

	return $res;
}

# This demonstrates that a parameter may be passed to a function
sub paramfunc {
	my $m = shift;
	my $param = shift;

	log(L_INFO, "This function was called with a parameter: $param\n");

	$param =~ s/l/L/g;

	log(L_INFO, "We can fiddle with it: $param\n");

	return 1;
}


# The following function shows that you can use exported functions just as if they were "real".
# This is achieved through Perl's autoloading mechanisms.

sub autotest {
	my $m = shift;

	$m->xlog("L_ERR", "This logging is done via perl's autoload mechanism and the xlog function");

	return 1;
}

# The following two functions demonstrate that the Kamailio perl module handles
# dieing interpreters correctly. Kamailio itself will not crash.

sub diefunc1 {
	my $m = shift;

	warn("I'll die in a moment...");

	log(L_INFO, "About to die...");

	die("Here I die!");

	return 1;
}

sub diefunc2 {
	my $m = shift;
	NoSuchClass::NoSuchMethod(noSuchParameter);

	return 1;
}

