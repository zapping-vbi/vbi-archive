# TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2004 Peter Thoeny, peter@thoeny.com
#
# For licensing info read license.txt file in the TWiki root.
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details, published at 
# http://www.gnu.org/copyleft/gpl.html
=begin twiki

---+ TWiki::UI::Search

UI functions for searchng.

=cut

package TWiki::UI::Search;

use strict;
use TWiki;
use TWiki::UI;

=pod

---+++ search( $web, $topic, $query )
Perform a search as dictated by CGI parameters:
| query_string | is the actual search string |
| =search= | |
| =web= | |
| =topic= | |
| =excludetopic= | |
| =scope= | |
| =order= | |
| =type= | |
| =regex= | |
| =limit= | |
| =reverse= | |
| =casesensitive= | |
| =nosummary= | |
| =nosearch= | |
| =noheader= | |
| =nototal= | |
| =bookview= | |
| =renameview= | |
| =showlock= | |
| =expandvariables= | |
| =noempty= | |
| =template= | |
| =header= | |
| =format= | |
| =multiple= | |
| =separator= | |
See the documentation on %SEARCH for a full description of parameters.
=cut

sub search {
    my ($webName, $topic, $query ) = @_;

    return unless TWiki::UI::webExists( $webName, $topic );

    # The CGI.pm docs claim that it returns all of the values in a
    # multiple select if called in a list context, but that may not
    # work (didn't on the dev box -- perl 5.004_4 and CGI.pm 2.36 on
    # Linux (Slackware 2.0.33) with Apache 1.2.  That being the case,
    # we need to parse them out here.

#    my @webs          = $query->param( "web" ) || ( $webName ); #doesn't work

    # Note for those unused to Perlishness:
    # -------------------------------------
    # The pipeline at the end of this assignment splits the full query
    # string on '&' or ';' and selects out the params that begin with 'web=',
    # replacing them with whatever is after that.  In the case of a
    # single list of webs passed as a string (say, from a text entry
    # field) it does more processing than it needs to to get the
    # correct string, but so what?  The pipline is the second
    # parameter to the join, and consists of the last two lines.  The
    # join takes the results of the pipeline and strings them back
    # together, space delimited, which is exactly what &searchWikiWeb
    # needs.
    # Note that mod_perl/cgi appears to use ';' as separator, whereas plain cgi uses '&'

    my $attrWeb       = join ' ',
                        grep { s/^web=(.*)$/$1/ }
                        split(/[&;]/, $query->query_string);
    # need to unescape URL-encoded data since we use the raw query_string
    # suggested by JeromeBouvattier
    $attrWeb =~ tr/+/ /;       # pluses become spaces
    $attrWeb =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;  # %20 becomes space

    &TWiki::writeHeader( $query );
    &TWiki::Search::searchWeb(
        "inline"        => "0",
        "search"        => scalar $query->param( "search" ),
        "web"           => $attrWeb,
        "topic"         => scalar $query->param( "topic" ),
        "excludetopic"  => scalar $query->param( "excludetopic" ),
        "scope"         => scalar $query->param( "scope" ),
        "order"         => scalar $query->param( "order" ),
        "type"          => scalar $query->param( "type" )
                        || TWiki::Prefs::getPreferencesValue( "SEARCHDEFAULTTTYPE" ),
        "regex"         => scalar $query->param( "regex" ),
        "limit"         => scalar $query->param( "limit" ),
        "reverse"       => scalar $query->param( "reverse" ),
        "casesensitive" => scalar $query->param( "casesensitive" ),
        "nosummary"     => scalar $query->param( "nosummary" ),
        "nosearch"      => scalar $query->param( "nosearch" ),
        "noheader"      => scalar $query->param( "noheader" ),
        "nototal"       => scalar $query->param( "nototal" ),
        "bookview"      => scalar $query->param( "bookview" ),
        "renameview"    => scalar $query->param( "renameview" ),
        "showlock"      => scalar $query->param( "showlock" ),
        "expandvariables" => scalar $query->param( "expandvariables" ),
        "noempty"       => scalar $query->param( "noempty" ),
        "template"      => scalar $query->param( "template" ),
        "header"        => scalar $query->param( "header" ),
        "format"        => scalar $query->param( "format" ),
        "multiple"      => scalar $query->param( "multiple" ),
        "separator"     => scalar $query->param( "separator" ),
    );
}

1;

