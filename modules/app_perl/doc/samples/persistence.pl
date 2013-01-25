use Kamailio qw ( log );
use Kamailio::Constants;

use IPC::Shareable;

my $foo = 0;

my %lastcalltimes;
my %lastcallids;


# This function shows that normal persistent variables are _not_ valid between multiple instances of the Kamailio.
# With the default setup of 4 children, the value logged is only incremented every 4th time.
sub limited {

	use vars qw ($foo);

	log(L_INFO, "My persistent variable is $foo\n");

	$foo++;

	return 1;
}



# Only one call per hour to each recipient
# By using the IPC::Shareable perl module, real persistence between multiple instances can be achieved
# Consider using the locking mechanisms available in IPC::Shareable when using in real world environments.
sub onceperhour {
	my $m = shift;

	tie %lastcalltimes, IPC::Shareable, { key => "lstt", create => 1, destroy => 1 } or die "Error with IPC::Shareable";
	tie %lastcallids, IPC::Shareable, { key => "lsti", create => 1, destroy => 1 } or die "Error with IPC::Shareable";

	if ($m->getMethod() eq "INVITE") {

		my $dst = $m->getRURI();
		my $currentid = $m->getHeader("Call-ID");

		log(L_INFO, "invite to $dst");

		my $lasttime = %lastcalltimes->{$dst};
		log(L_INFO, "lasttime is $lasttime");

		if ($lasttime) {	# Value set.
			log(L_INFO, "I was already called at $lasttime");
			if ((time() - $lasttime) < 3600) {
				if ($currentid eq %lastcallids->{$dst}) {
					log(L_INFO, "I know this call already. Doing nothing.");
				} else {
					log(L_INFO, "replying, return 1");
					$m->sl_send_reply("488", "You already called this hour");
					return 1;
				}
			}
		}

		%lastcalltimes->{$dst} = time();
		%lastcallids->{$dst} = $currentid;
	}
	return -1;
}

