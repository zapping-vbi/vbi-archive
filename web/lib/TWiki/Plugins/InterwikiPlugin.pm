# Plugin for TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2000-2003 Andrea Sterbini, a.sterbini@flashnet.it
# Copyright (C) 2001-2004 Peter Thoeny, peter@thoeny.com
#
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
#
# =========================
# Interwiki Link Plugin:
#
# Here we handle inter-site links, i.e. links going outside TWiki
# The recognized syntax is:
#       InterSiteName:TopicName
# and inserts <a href="URL/TopicName">InterSiteName:TopicName</a>
# link, where URL is obtained by a topic that lists all
# InterSiteName/URL pairs.
# Inter-site name convention: Sites must start with upper case
# and must be preceeded by white space, '-', '*' or '('
#
# =========================
package TWiki::Plugins::InterwikiPlugin;
# =========================

use vars qw(
        $web $topic $user $installWeb  $VERSION $debug
        $interSiteLinkRulesTopicName $suppressTooltip
        $prefixPattern $upperAlpha $mixedAlphaNum $sitePattern $pagePattern $postfixPattern
        %interSiteTable $debug
    );

$VERSION = '1.005'; # 16 Feb 2004
$interSiteLinkRulesTopicName = "InterWikis";

# 'Use locale' for internationalisation of Perl sorting and searching - 
# main locale settings are done in TWiki::setupLocale
BEGIN {
    # Do a dynamic 'use locale' for this module
    if( $TWiki::useLocale ) {
        require locale;
	import locale ();
    }
}

# Regexes for the Site:page format InterWiki reference - updated to support
# 8-bit characters in both parts - see Codev.InternationalisationEnhancements
$prefixPattern  = '(^|[\s\-\*\(])';
if( $TWiki::Plugins::VERSION >= 1.020 ) {
    $upperAlpha    = TWiki::Func::getRegularExpression("upperAlpha");
    $mixedAlphaNum = TWiki::Func::getRegularExpression("mixedAlphaNum");
} else {
    $upperAlpha    = $TWiki::upperAlpha;
    $mixedAlphaNum = $TWiki::mixedAlphaNum;
}
$sitePattern    = "([${upperAlpha}][${mixedAlphaNum}]+)";
$pagePattern    = "([${mixedAlphaNum}_\/][${mixedAlphaNum}" . '\+\_\.\,\;\:\!\?\/\%\#-]+?)';
$postfixPattern = '(?=[\s\.\,\;\:\!\?\)]*(\s|$))';

# =========================
# Plugin startup - read preferences and get all InterWiki Site->URL mappings
sub initPlugin
{
    ( $topic, $web, $user, $installWeb ) = @_;

    # check for Plugins.pm versions
    if( $TWiki::Plugins::VERSION < 1 ) {
        &TWiki::Func::writeWarning( "Version mismatch between InterwikiPlugin and Plugins.pm" );
        return 0;
    }

    # Get plugin preferences from InterwikiPlugin topic
    my $interFileName = &TWiki::Func::getPreferencesValue( "INTERWIKIPLUGIN_RULESTOPIC" ) 
                        || "$installWeb.$interSiteLinkRulesTopicName";
    $suppressTooltip  = &TWiki::Func::getPreferencesFlag( "INTERWIKIPLUGIN_SUPPRESSTOOLTIP" );
    $debug            = &TWiki::Func::getPreferencesFlag( "INTERWIKIPLUGIN_DEBUG" );

    # get the Interwiki site->url map topic
    my $InterFile = '';
    my $dataDir = &TWiki::Func::getDataDir();
    if( $interFileName =~ m/^([^.]+)\.([^.]+)$/ ) {
        $InterFile = "$dataDir/$1/$2.txt"; 	
    } elsif( $interFileName =~ m/^([^.]+)$/ ) {
        $InterFile = "$dataDir/$installWeb/$1.txt"; 
    } else {
        return 0;
    }

    $InterFile =~ s/%MAINWEB%/&TWiki::Func::getMainWebname()/geo;
    $InterFile =~ s/%TWIKIWEB%/&TWiki::Func::getTwikiWebname/geo;
    &TWiki::Func::writeDebug( "- InterwikiPlugin, rules file: $InterFile" ) if $debug;

    # FIXME: use readWebTopic
    open(IN, "<$InterFile") or return 0;  # FIXME: Later warn?
    my @data = <IN>;
    close IN;

    # grep "| alias | URL | ..." table and extract into "alias", "URL" list
    # FIXME: Should be able to do this pipeline with just one regex match
    @data = map { split /\s+/, $_, 2 }
            map { s/^\|\s*$sitePattern\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|.*$/$1 $2 $3/os ; $_ }
            grep { m/^\|\s*$sitePattern\s*\|.*?\|.*?\|/o } @data;
    if( $debug ) {
        my $tmp = join(", " , @data );
        &TWiki::Func::writeDebug( "- InterwikiPlugin, table: $tmp" );
    }
    %interSiteTable  = @data;

    # Plugin correctly initialized
    &TWiki::Func::writeDebug( "- TWiki::Plugins::InterwikiPlugin::initPlugin( $web.$topic ) is OK" ) if $debug;
    return 1;
}

# =========================
sub startRenderingHandler
{
### my ( $text, $web ) = @_;   # do not uncomment, use $_[0], $_[1] instead
    &TWiki::Func::writeDebug( "- InterwikiPlugin::startRenderingHandler( $_[1] )" ) if $debug;

    $_[0] =~ s/(\[\[)$sitePattern:$pagePattern(\]\]|\]\[| )/&handleInterwiki($1,$2,$3,$4)/geo;
    $_[0] =~ s/$prefixPattern$sitePattern:$pagePattern$postfixPattern/&handleInterwiki($1,$2,$3,"")/geo;
}

# =========================
sub handleInterwiki
{
    my( $thePrefix, $theSite, $theTopic, $thePostfix ) = @_;

    &TWiki::Func::writeDebug( "- InterwikiPlugin::handleInterwikiSiteLink: (site: $theSite), (topic: $theTopic)" ) if $debug;

    my $text = "";
    if( defined( $interSiteTable{ $theSite } ) ) {
        my( $url, $help ) = split( /\s+/, $interSiteTable{ $theSite }, 2 );
        my $title = "";
        if( ! $suppressTooltip ) {
            $help =~ s/<nop>/&nbsp;/goi;
            $help =~ s/[\"\<\>]*//goi;
            $help =~ s/\$page/$theTopic/go;
            $title = " title=\"$help\"";
        }
        &TWiki::Func::writeDebug( "  (url: $url), (help: $help)" ) if $debug;
        if( $url =~ s/\$page/$theTopic/go ) {
            $text = $url;
        } else {
            $text = "$url$theTopic";
        }
        if( $thePostfix ) {
            $text = "$thePrefix$text][";
            $text .= "$theSite\:$theTopic]]" if( $thePostfix eq "]]" );
        } else {
            $text = "$thePrefix<a href=\"$text\"$title>$theSite\:$theTopic</a>";
        }
    } else {
        $text = "$thePrefix$theSite\:$theTopic$thePostfix";
    }
    return $text;
}

# =========================

1;
