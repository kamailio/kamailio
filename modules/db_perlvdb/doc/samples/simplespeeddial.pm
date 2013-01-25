package simplespeeddial;

sub init {}

my @dials = (
	{
		un => "user1",
		sd => 44,
		na => "sip:someone\@somewhere.com"
	},
	{
		un => "user2",
		sd => 45,
		na => "sip:someoneelse\@somewheredifferent.com"
	},
);

sub query {
	my $self = shift;
	my $un = shift;
	my $sd = shift;

	foreach my $entry (@dials) {
		if (($entry->{un} eq $un) && ($entry->{sd} == $sd)) {
			return $entry->{na};
		}
	}
}


1;
