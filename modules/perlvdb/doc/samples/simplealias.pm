package simplealias;

sub init {}

my @aliases = (
	{
		alias_username => "a1",
		alias_domain => "example.com",
		username => "user",
		domain => "example.com"
	},
	{
		alias_username => "a2",
		alias_domain => "example.com",
		username => "anotheruser",
		domain => "example.com"
	},
);

sub query {
	my $self = shift;
	my $alias_username = shift;
	my $alias_domain = shift;

	foreach my $entry (@aliases) {
		if (($entry->{alias_username} eq $alias_username) && ((!defined alias_domain) || ($entry->{alias_domain} == $alias_domain))) {
			my $ret;
			$ret->{username} = $entry->{username};
			$ret->{domain} = $entry->{domain};
			return $ret;
		}
	}
}


1;
