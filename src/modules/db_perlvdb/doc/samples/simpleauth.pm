package simpleauth;

sub init {}

sub version { 6 };

my @accounts = (
	{
		username => "foo",
		password => "abc",
	},
	{
		username => "bar",
		password => "def",
	},
);

sub query {
	my $self = shift;
	my $username = shift;

	foreach my $entry (@accounts) {
		if ($entry->{username} eq $username) {
			return $entry->{password};
		}
	}
}


1;
