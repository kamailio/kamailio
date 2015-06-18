#!/usr/bin/perl
use utf8;
use LWP::UserAgent;
use HTTP::Cookies;

sub main
{
    # Create the fake browser (user agent).
    my $ua = LWP::UserAgent->new();

    # Pretend to be Internet Explorer.
    $ua->agent("Windows IE 7");
    # or maybe .... $ua->agent("Mozilla/8.0");

    # Get some HTML.
    my $response = $ua->get('http://www.bundesnetzagentur.de/cln_1421/DE/Sachgebiete/Telekommunikation/Unternehmen_Institutionen/Nummerierung/Technische%20Nummern/Portierungskennungen/VerzeichnisPortKenn_Basepage.html?nn=268376');

    unless($response->is_success) {
        print "Error: " . $response->status_line;
    }

    # Let's save the output.
    my $file = $response->decoded_content;
    utf8::encode($file);
    @pieces=split('\<tbody\>', $file);
    @pieces2=split('\</tbody\>', $pieces[1]);
    @linii=split('\</tr\>', $pieces2[0]);
    foreach(@linii)
    {
        my($first, $rest) = split(/>/, $_, 2);
        @tds=split('/td><td', $rest);
        @names=split('>', $tds[0], 2);
        my $name=$names[1];
        $name =~ s/<p>//;
        $name =~ s/<p>//;
        $name =~ s/<\/p>//;
        $name =~ s/<\/p>//;
        $name =~ s/<//;
        $name =~ s/\n//;
        $name =~ s/br\/>//;
        $name =~ s/br\/>//;
        $name =~ s/<//;
        $name =~ s/<//;
        $name =~ s/\n//;
        chomp($name);
        @tds2=split('>', $tds[1], 2);
        @tds3=split('</', $tds2[1]);
        @tds4=split('<br/>', $tds3[0]);
        foreach(@tds4)
        {
            $_ =~ s/^\n//;
            if ($_ =~ /^D/)
            {
                    $number=substr($_,0,4);
                    print "$number $name\n";
            }
        }
    }
}
main();
