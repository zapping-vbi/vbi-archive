# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2001-2004 Peter Thoeny, peter@thoeny.com
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
#
# Notes:
# - Latest version at http://twiki.org/
# - Installation instructions in $dataDir/Main/TWikiDocumentation.txt
# - Customize variables in TWiki.cfg when installing TWiki.
# - Optionally change TWiki.pm for custom extensions of rendering rules.
# - Upgrading TWiki is easy as long as you do not customize TWiki.pm.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log
#
# Jun 2001 - written by John Talintyre, jet@cheerful.com

=begin twiki

---+ TWiki::Render Module

This module provides most of the actual HTML rendering code in TWiki.

=cut

package TWiki::Render;

use strict;

use TWiki qw(:renderflags %regex $TranslationToken);

# Globals used in rendering
use vars qw(
	$isList @listTypes @listElements
        $newTopicFontColor $newTopicBgColor $linkToolTipInfo $noAutoLink
        $newLinkSymbol %ffCache
        
    );
    

$noAutoLink = 0;

=pod

---++ sub initialize ()

Initializes global render module state from preference values (NEWTOPICBGCOLOR, NEWTOPICFONTCOLOR NEWTOPICLINKSYMBOL LINKTOOLTIPINFO NOAUTOLINK)

Clears the FORMFIELD metadata cache preparatory to expanding %FORMFIELD
tags.

=cut

sub initialize
{
    # Add background color and font color (AlWilliams - 18 Sep 2000)
    # PTh: Moved from internalLink to initialize ('cause of performance)
    $newTopicBgColor   = TWiki::Prefs::getPreferencesValue("NEWTOPICBGCOLOR")   || "#FFFFCE";
    $newTopicFontColor = TWiki::Prefs::getPreferencesValue("NEWTOPICFONTCOLOR") || "#0000FF";
    $newLinkSymbol     = TWiki::Prefs::getPreferencesValue("NEWTOPICLINKSYMBOL") || "<sup>?</sup>";
    # tooltip init
    $linkToolTipInfo   = TWiki::Prefs::getPreferencesValue("LINKTOOLTIPINFO")   || "";
    $linkToolTipInfo = '$username - $date - r$rev: $summary' if( $linkToolTipInfo =~ /^on$/ );
    # Prevent autolink of WikiWords
    $noAutoLink        = TWiki::Prefs::getPreferencesValue("NOAUTOLINK") || 0;

  undef %ffCache;
}

=pod

---++ sub renderParent (  $web, $topic, $meta, $args  )

Not yet documented.

=cut

sub renderParent
{
    my( $web, $topic, $meta, $args ) = @_;
    
    my $text = "";

    my $dontRecurse = 0;
    my $noWebHome = 0;
    my $prefix = "";
    my $suffix = "";
    my $usesep = "";

    if( $args ) {
       $dontRecurse = TWiki::extractNameValuePair( $args, "dontrecurse" );
       $noWebHome =   TWiki::extractNameValuePair( $args, "nowebhome" );
       $prefix =      TWiki::extractNameValuePair( $args, "prefix" );
       $suffix =      TWiki::extractNameValuePair( $args, "suffix" );
       $usesep =      TWiki::extractNameValuePair( $args, "separator" );
    }

    if( ! $usesep ) {
       $usesep = " &gt; ";
    }

    my %visited = ();
    $visited{"$web.$topic"} = 1;

    my $sep = "";
    my $cWeb = $web;

    while( 1 ) {
        my %parent = $meta->findOne( "TOPICPARENT" );
        if( %parent ) {
            my $name = $parent{"name"};
            my $pWeb = $cWeb;
            my $pTopic = $name;
            if( $name =~ /^(.*)\.(.*)$/ ) {
               $pWeb = $1;
               $pTopic = $2;
            }
            if( $noWebHome && ( $pTopic eq $mainTopicname ) ) {
               last;  # exclude "WebHome"
            }
            $text = "[[$pWeb.$pTopic][$pTopic]]$sep$text";
            $sep = $usesep;
            if( $dontRecurse || ! $name ) {
               last;
            } else {
               my $dummy;
               if( $visited{"$pWeb.$pTopic"} ) {
                  last;
               } else {
                  $visited{"$pWeb.$pTopic"} = 1;
               }
               if( TWiki::Store::topicExists( $pWeb, $pTopic ) ) {
                   ( $meta, $dummy ) = TWiki::Store::readTopMeta( $pWeb, $pTopic );
               } else {
                   last;
               }
               $cWeb = $pWeb;
            }
        } else {
            last;
        }
    }

    if( $text && $prefix ) {
       $text = "$prefix$text";
    }

    if( $text && $suffix ) {
       $text .= $suffix;
    }

    return $text;
}

# ========================
=pod

---++ sub renderMoved (  $web, $topic, $meta  )

Not yet documented.

=cut

sub renderMoved
{
    my( $web, $topic, $meta ) = @_;
    
    my $text = "";
    
    my %moved = $meta->findOne( "TOPICMOVED" );
    
    if( %moved ) {
        my $from = $moved{"from"};
        $from =~ /(.*)\.(.*)/;
        my $fromWeb = $1;
        my $fromTopic = $2;
        my $to   = $moved{"to"};
        $to =~ /(.*)\.(.*)/;
        my $toWeb = $1;
        my $toTopic = $2;
        my $by   = $moved{"by"};
        $by = TWiki::userToWikiName( $by );
        my $date = $moved{"date"};
        $date = TWiki::formatTime( $date, "", "gmtime" );
        
        # Only allow put back if current web and topic match stored information
        my $putBack = "";
        if( $web eq $toWeb && $topic eq $toTopic ) {
            $putBack  = " - <a title=\"Click to move topic back to previous location, with option to change references.\"";
            $putBack .= " href=\"$dispScriptUrlPath/rename$scriptSuffix/$web/$topic?newweb=$fromWeb&newtopic=$fromTopic&";
            $putBack .= "confirm=on\">put it back</a>";
        }
        $text = "<i><nop>$to moved from <nop>$from on $date by $by </i>$putBack";
    }
    
    return $text;
}


# ========================
=pod

---++ sub renderFormField (  $meta, $args  )

Not yet documented.

=cut

sub renderFormField
{
    my( $meta, $args ) = @_;
    my $text = "";
    if( $args ) {
        my $name = TWiki::extractNameValuePair( $args, "name" );
        $text = TWiki::Search::getMetaFormField( $meta, $name ) if( $name );
    }
    return $text;
}

# =========================
=pod

---++ sub renderFormData (  $web, $topic, $meta  )

Not yet documented.

=cut

sub renderFormData
{
    my( $web, $topic, $meta ) = @_;

    my $metaText = "";
    
    my %form = $meta->findOne( "FORM" );
    if( %form ) {
        my $name = $form{"name"};
        $metaText = "<div class=\"twikiForm\">\n";
        $metaText .= "<p></p>\n"; # prefix empty line
        $metaText .= "|*[[$name]]*||\n"; # table header
        my @fields = $meta->find( "FIELD" );
        foreach my $field ( @fields ) {
            my $title = $field->{"title"};
            my $value = $field->{"value"};
            $value =~ s/\n/<br \/>/g;      # undo expansion
            $metaText .= "|  $title:|$value  |\n";
        }
        $metaText .= "\n</div>";
    }

    return $metaText;
}

# Before including topic text in a hidden field in web form, encode
# characters that would break the field
=pod

---++ sub encodeSpecialChars (  $text  )

Not yet documented.

=cut

sub encodeSpecialChars
{
    my( $text ) = @_;
    
    $text =~ s/&/%_A_%/g;
    $text =~ s/\"/%_Q_%/g;
    $text =~ s/>/%_G_%/g;
    $text =~ s/</%_L_%/g;
    # PTh, JoachimDurchholz 22 Nov 2001: Fix for Codev.OperaBrowserDoublesEndOfLines
    $text =~ s/(\r*\n|\r)/%_N_%/g;

    return $text;
}

=pod

---++ sub decodeSpecialChars (  $text  )

Not yet documented.

=cut

sub decodeSpecialChars
{
    my( $text ) = @_;
    
    $text =~ s/%_N_%/\r\n/g;
    $text =~ s/%_L_%/</g;
    $text =~ s/%_G_%/>/g;
    $text =~ s/%_Q_%/\"/g;
    $text =~ s/%_A_%/&/g;

    return $text;
}


# =========================
# Render bulleted and numbered lists, including nesting.
# Called from several places.  Accumulates @listTypes and @listElements
# to track nested lists.
=pod

---++ sub emitList (  $theType, $theElement, $theDepth, $theOlType  )

Not yet documented.

=cut

sub emitList {
    my( $theType, $theElement, $theDepth, $theOlType ) = @_;

    my $result = "";
    $isList = 1;

    # Ordered list type
    $theOlType = "" unless( $theOlType );
    $theOlType =~ s/^(.).*/$1/;
    $theOlType = "" if( $theOlType eq "1" );

    if( @listTypes < $theDepth ) {
        my $firstTime = 1;
        while( @listTypes < $theDepth ) {
            push( @listTypes, $theType );
            push( @listElements, $theElement );
            $result .= "<$theElement>\n" unless( $firstTime );
            if( $theOlType ) {
                $result .= "<$theType type=\"$theOlType\">\n";
            } else {
                $result .= "<$theType>\n";
            }
            $firstTime = 0;
        }

    } elsif( @listTypes > $theDepth ) {
        while( @listTypes > $theDepth ) {
            local($_) = pop @listElements;
            $result .= "</$_>\n";
            local($_) = pop @listTypes;
            $result .= "</$_>\n";
        }
        $result .= "</$listElements[$#listElements]>\n" if( @listElements );

    } elsif( @listElements ) {
        $result = "</$listElements[$#listElements]>\n";
    }

    if( ( @listTypes ) && ( $listTypes[$#listTypes] ne $theType ) ) {
        $result .= "</$listTypes[$#listTypes]>\n<$theType>\n";
        $listTypes[$#listTypes] = $theType;
        $listElements[$#listElements] = $theElement;
    }

    return $result;
}

# ========================
=pod

---++ sub emitTR (  $thePre, $theRow, $insideTABLE  )

Not yet documented.

=cut

sub emitTR {
    my ( $thePre, $theRow, $insideTABLE ) = @_;

    my $text = "";
    my $attr = "";
    my $l1 = 0;
    my $l2 = 0;
    if( $insideTABLE ) {
        $text = "$thePre<tr>";
    } else {
        $text = "$thePre<table border=\"1\" cellspacing=\"0\" cellpadding=\"1\"> <tr>";
    }
    $theRow =~ s/\t/   /g;  # change tabs to space
    $theRow =~ s/\s*$//;    # remove trailing spaces
    $theRow =~ s/(\|\|+)/$TranslationToken . length($1) . "\|"/ge;  # calc COLSPAN

    foreach( split( /\|/, $theRow ) ) {
        $attr = "";
        #AS 25-5-01 Fix to avoid matching also single columns
        if ( s/$TranslationToken([0-9]+)//o ) { 
            $attr = " colspan=\"$1\"" ;
        }
        s/^\s+$/ &nbsp; /;
        /^(\s*).*?(\s*)$/;
        $l1 = length( $1 || "" );
        $l2 = length( $2 || "" );
        if( $l1 >= 2 ) {
            if( $l2 <= 1 ) {
                $attr .= ' align="right"';
            } else {
                $attr .= ' align="center"';
            }
        }
        if( /^\s*(\*.*\*)\s*$/ ) {
            $text .= "<th$attr bgcolor=\"#99CCCC\"> $1 </th>";
        } else {
            $text .= "<td$attr> $_ </td>";
        }
    }
    $text .= "</tr>";
    return $text;
}

# =========================
=pod

---++ sub fixedFontText (  $theText, $theDoBold  )

Not yet documented.

=cut

sub fixedFontText
{
    my( $theText, $theDoBold ) = @_;
    # preserve white space, so replace it by "&nbsp; " patterns
    $theText =~ s/\t/   /g;
    $theText =~ s|((?:[\s]{2})+)([^\s])|'&nbsp; ' x (length($1) / 2) . "$2"|eg;
    if( $theDoBold ) {
        return "<code><b>$theText</b></code>";
    } else {
        return "<code>$theText</code>";
    }
}

# =========================
# Build an HTML &lt;Hn> element with suitable anchor for linking from %<nop>TOC%
=pod

---++ sub makeAnchorHeading (  $theText, $theLevel  )

Not yet documented.

=cut

sub makeAnchorHeading
{
    my( $theHeading, $theLevel ) = @_;

    # - Build '<nop><h1><a name="atext"></a> heading </h1>' markup
    # - Initial '<nop>' is needed to prevent subsequent matches.
    # - filter out $regex{headerPatternNoTOC} ( '!!' and '%NOTOC%' )
    # CODE_SMELL: Empty anchor tags seem not to be allowed, but validators and browsers tolerate them

    my $anchorName =       makeAnchorName( $theHeading, 0 );
    my $compatAnchorName = makeAnchorName( $theHeading, 1 );
    $theHeading =~ s/$regex{headerPatternNoTOC}//o; # filter '!!', '%NOTOC%'
    my $text = "<nop><h$theLevel>";
    $text .= "<a name=\"$anchorName\"> </a>";
    $text .= "<a name=\"$compatAnchorName\"> </a>" if( $compatAnchorName ne $anchorName );
    $text .= " $theHeading </h$theLevel>";

    return $text;
}

# =========================
# Build a valid HTML anchor name
=pod

---++ sub makeAnchorName (  $anchorName, $compatibilityMode  )

Not yet documented.

=cut

sub makeAnchorName
{
    my( $anchorName, $compatibilityMode ) = @_;

    if ( ! $compatibilityMode && $anchorName =~ /^$regex{anchorRegex}$/ ) {
	# accept, already valid -- just remove leading #
	return substr($anchorName, 1);
    }

    if ( $compatibilityMode ) {
	# remove leading/trailing underscores first, allowing them to be
	# reintroduced
	$anchorName =~ s/^[\s\#\_]*//;
        $anchorName =~ s/[\s\_]*$//;
    }
    $anchorName =~ s/<\w[^>]*>//gi;         # remove HTML tags
    $anchorName =~ s/\&\#?[a-zA-Z0-9]*;//g; # remove HTML entities
    $anchorName =~ s/\&//g;                 # remove &
    $anchorName =~ s/^(.+?)\s*$regex{headerPatternNoTOC}.*/$1/o; # filter TOC excludes if not at beginning
    $anchorName =~ s/$regex{headerPatternNoTOC}//o; # filter '!!', '%NOTOC%'
    # FIXME: More efficient to match with '+' on next line:
    $anchorName =~ s/$regex{singleMixedNonAlphaNumRegex}/_/g;      # only allowed chars
    $anchorName =~ s/__+/_/g;               # remove excessive '_'
    if ( !$compatibilityMode ) {
        $anchorName =~ s/^[\s\#\_]*//;      # no leading space nor '#', '_'
    }
    $anchorName =~ s/^(.{32})(.*)$/$1/;     # limit to 32 chars - FIXME: Use Unicode chars before truncate
    if ( !$compatibilityMode ) {
        $anchorName =~ s/[\s\_]*$//;        # no trailing space, nor '_'
    }

    # No need to encode 8-bit characters in anchor due to UTF-8 URL support

    return $anchorName;
}

# =========================
=pod

---++ sub linkToolTipInfo ( $theWeb, $theTopic )

Returns =title="..."= tooltip info in case LINKTOOLTIPINFO perferences variable is set. 
Warning: Slower performance if enabled.

=cut

sub linkToolTipInfo
{
    my( $theWeb, $theTopic ) = @_;
    return "" unless( $linkToolTipInfo );
    return "" if( $linkToolTipInfo =~ /^off$/i );

    # FIXME: This is slow, it can be improved by caching topic rev info and summary
    my( $date, $user, $rev ) = TWiki::Store::getRevisionInfo( $theWeb, $theTopic );
    my $text = $linkToolTipInfo;
    $text =~ s/\$web/<nop>$theWeb/g;
    $text =~ s/\$topic/<nop>$theTopic/g;
    $text =~ s/\$rev/1.$rev/g;
    $text =~ s/\$date/&TWiki::formatTime( $date )/ge;
    $text =~ s/\$username/<nop>$user/g;                                     # "jsmith"
    $text =~ s/\$wikiname/"<nop>" . &TWiki::userToWikiName( $user, 1 )/ge;  # "JohnSmith"
    $text =~ s/\$wikiusername/"<nop>" . &TWiki::userToWikiName( $user )/ge; # "Main.JohnSmith"
    if( $text =~ /\$summary/ ) {
        my $summary = &TWiki::Store::readFileHead( "$TWiki::dataDir/$theWeb/$theTopic.txt", 16 );
        $summary = &TWiki::makeTopicSummary( $summary, $theTopic, $theWeb );
        $summary =~ s/[\"\']/<nop>/g;       # remove quotes (not allowed in title attribute)
        $text =~ s/\$summary/$summary/g;
    }
    return " title=\"$text\"";
}

# =========================
=pod

---++ sub internalLink (  $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink, $doKeepWeb )

Not yet documented.

=cut

sub internalLink {
    my( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink, $doKeepWeb ) = @_;
    # $thePreamble is text used before the TWiki link syntax
    # $doLink is boolean: false means suppress link for non-existing pages
    # $doKeepWeb is boolean: true to keep web prefix (for non existing Web.TOPIC)

    # Get rid of leading/trailing spaces in topic name
    $theTopic =~ s/^\s*//;
    $theTopic =~ s/\s*$//;

    # Turn spaced-out names into WikiWords - upper case first letter of
    # whole link, and first of each word. TODO: Try to turn this off,
    # avoiding spaces being stripped elsewhere - e.g. $doPreserveSpacedOutWords 
    $theTopic =~ s/^(.)/\U$1/;
    $theTopic =~ s/\s($regex{singleMixedAlphaNumRegex})/\U$1/go;	

    # Add <nop> before WikiWord inside link text to prevent double links
    $theLinkText =~ s/([\s\(])($regex{singleUpperAlphaRegex})/$1<nop>$2/go;
 
     # Allow spacing out, etc
     if (TWiki::isWikiName($theLinkText)) {
        $theLinkText = TWiki::Plugins::renderWikiWordHandler( $theLinkText ) || $theLinkText;
     }

    my $exist = &TWiki::Store::topicExists( $theWeb, $theTopic );
    # I18N - Only apply plural processing if site language is English, or
    # if a built-in English-language web (Main, TWiki or Plugins).  Plurals
    # apply to names ending in 's', where topic doesn't exist with plural
    # name.
    if(  ( $doPluralToSingular ) and ( $siteLang eq 'en' 
					or $theWeb eq $mainWebname
					or $theWeb eq $twikiWebname
					or $theWeb eq 'Plugins' 
				     ) 
	    and ( $theTopic =~ /s$/ ) and not ( $exist ) ) {
        # Topic name is plural in form and doesn't exist as written
        my $tmp = $theTopic;
        $tmp =~ s/ies$/y/;       # plurals like policy / policies
        $tmp =~ s/sses$/ss/;     # plurals like address / addresses
        $tmp =~ s/([Xx])es$/$1/; # plurals like box / boxes
        $tmp =~ s/([A-Za-rt-z])s$/$1/; # others, excluding ending ss like address(es)
        if( &TWiki::Store::topicExists( $theWeb, $tmp ) ) {
            $theTopic = $tmp;
            $exist = 1;
        }
    }

    my $text = $thePreamble;
    if( $exist) {
        if( $theAnchor ) {
            my $anchor = makeAnchorName( $theAnchor );
            $text .= "<a class=\"twikiAnchorLink\" href=\"$dispScriptUrlPath$dispViewPath"
		  .  "$scriptSuffix/$theWeb/$theTopic\#$anchor\""
                  .  &linkToolTipInfo( $theWeb, $theTopic )
                  .  ">$theLinkText</a>";
            return $text;
        } else {
            $text .= "<a class=\"twikiLink\" href=\"$dispScriptUrlPath$dispViewPath"
		  .  "$scriptSuffix/$theWeb/$theTopic\""
                  .  &linkToolTipInfo( $theWeb, $theTopic )
                  .  ">$theLinkText</a>";
            return $text;
        }

    } elsif( $doLink ) {
        $text .= "<span class=\"twikiNewLink\" style='background : $newTopicBgColor;'>"
              .  "<font color=\"$newTopicFontColor\">$theLinkText</font>"
              .  "<a href=\"$dispScriptUrlPath/edit$scriptSuffix/$theWeb/$theTopic?topicparent=$TWiki::webName.$TWiki::topicName\">$newLinkSymbol</a></span>";
        return $text;

    } elsif( $doKeepWeb ) {
        $text .= "$theWeb.$theLinkText";
        return $text;

    } else {
        $text .= $theLinkText;
        return $text;
    }
}

# =========================
=pod

---++ sub internalCrosswebLink (  $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink  )

Not yet documented.

=cut

sub internalCrosswebLink
{
    my( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink ) = @_;
    if ( $theTopic eq $TWiki::mainTopicname && $theWeb ne $TWiki::webName ) {
        return internalLink( $thePreamble, $theWeb, $theTopic, $theWeb, $theAnchor, $doLink );
    } else {
        return internalLink( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink );
    }
}

# =========================
# Handle most internal and external links
=pod

---++ sub specificLink (  $thePreamble, $theWeb, $theTopic, $theText, $theLink  )

Not yet documented.

=cut

sub specificLink
{
    my( $thePreamble, $theWeb, $theTopic, $theText, $theLink ) = @_;

    # format: $thePreamble[[$theText]]
    # format: $thePreamble[[$theLink][$theText]]
    #
    # Current page's $theWeb and $theTopic are also used

    # Strip leading/trailing spaces
    $theLink =~ s/^\s*//;
    $theLink =~ s/\s*$//;

    if( $theLink =~ /^$regex{linkProtocolPattern}\:/ ) {

        # External link: add <nop> before WikiWord and ABBREV 
	# inside link text, to prevent double links
	$theText =~ s/([\s\(])($regex{singleUpperAlphaRegex})/$1<nop>$2/go;
        return "$thePreamble<a href=\"$theLink\" target=\"_top\">$theText</a>";

    } else {

	# Internal link: get any 'Web.' prefix, or use current web
	$theLink =~ s/^($regex{webNameRegex}|$regex{defaultWebNameRegex})\.//;
	my $web = $1 || $theWeb;
	(my $baz = "foo") =~ s/foo//;       # reset $1, defensive coding

	# Extract '#anchor'
	# FIXME and NOTE: Had '-' as valid anchor character, removed
	# $theLink =~ s/(\#[a-zA-Z_0-9\-]*$)//;
	$theLink =~ s/($regex{anchorRegex}$)//;
	my $anchor = $1 || "";

	# Get the topic name
	my $topic = $theLink || $theTopic;  # remaining is topic
	$topic =~ s/\&[a-z]+\;//gi;        # filter out &any; entities
	$topic =~ s/\&\#[0-9]+\;//g;       # filter out &#123; entities
	$topic =~ s/[\\\/\#\&\(\)\{\}\[\]\<\>\!\=\:\,\.]//g;
	$topic =~ s/$securityFilter//go;    # filter out suspicious chars
	if( ! $topic ) {
	    return "$thePreamble$theText"; # no link if no topic
	}

	return internalLink( $thePreamble, $web, $topic, $theText, $anchor, 1 );
    }

}

# =========================
=pod

---++ sub externalLink (  $pre, $url  )

Not yet documented.

=cut

sub externalLink
{
    my( $pre, $url ) = @_;
    if( $url =~ /\.(gif|jpg|jpeg|png)$/i ) {
        my $filename = $url;
        $filename =~ s@.*/([^/]*)@$1@go;
        return "$pre<img src=\"$url\" alt=\"$filename\" />";
    }

    return "$pre<a href=\"$url\" target=\"_top\">$url</a>";
}

# =========================
=pod

---++ sub mailtoLink (  $theAccount, $theSubDomain, $theTopDomain  )

Not yet documented.

=cut

sub mailtoLink
{
    my( $theAccount, $theSubDomain, $theTopDomain ) = @_;

    my $addr = "$theAccount\@$theSubDomain$TWiki::noSpamPadding\.$theTopDomain";
    return "<a href=\"mailto\:$addr\">$addr</a>";
}

# =========================
=pod

---++ sub mailtoLinkFull (  $theAccount, $theSubDomain, $theTopDomain, $theLinkText  )

Not yet documented.

=cut

sub mailtoLinkFull
{
    my( $theAccount, $theSubDomain, $theTopDomain, $theLinkText ) = @_;

    my $addr = "$theAccount\@$theSubDomain$TWiki::noSpamPadding\.$theTopDomain";
    return "<a href=\"mailto\:$addr\">$theLinkText</a>";
}

# =========================
=pod

---++ sub mailtoLinkSimple (  $theMailtoString, $theLinkText  )

Not yet documented.

=cut

sub mailtoLinkSimple
{
    # Does not do any anti-spam padding, because address will not include '@'
    my( $theMailtoString, $theLinkText ) = @_;	

    # Defensive coding
    if ($theMailtoString =~ s/@//g ) {
    	writeWarning("mailtoLinkSimple called with an '\@' in string - internal TWiki error");
    }
    return "<a href=\"mailto\:$theMailtoString\">$theLinkText</a>";
}

=pod

---++ sub getFormField ( $web, $topic, $args )

+Returns the expansion of a %FORMFIELD{}% tag.

=cut

sub getFormField
{
   my( $web, $topic, $args ) = @_;

  my $formField = TWiki::extractNameValuePair( $args );
  my $formTopic = TWiki::extractNameValuePair( $args, "topic" );
  my $altText   = TWiki::extractNameValuePair( $args, "alttext" );
  my $default   = TWiki::extractNameValuePair( $args, "default" ) || undef;
  my $format    = TWiki::extractNameValuePair( $args, "format" );

  unless ( $format ) {
       # if null format explicitly set, return empty
       return "" if ( $args =~ m/format\s*=/o);
       # Otherwise default to value
       $format = "\$value";
  }

  my $formWeb;
  if ( $formTopic ) {
       if ($topic =~ /^([^.]+)\.([^.]+)/o) {
         ( $formWeb, $topic ) = ( $1, $2 );
	   } else {
         # SMELL: Undocumented feature, "web" parameter
         $formWeb = TWiki::extractNameValuePair( $args, "web" );
	   }
       $formWeb = $web unless $formWeb;
  } else {
       $formWeb = $web;
       $formTopic = $topic;
  }

  my $meta = $ffCache{"$formWeb.$formTopic"};
  unless ( $meta ) {
       my $dummyText;
       ( $meta, $dummyText ) =
         TWiki::Store::readTopic( $formWeb, $formTopic );
       $ffCache{"$formWeb.$formTopic"} = $meta;
  }

  my $text = "";
  my $found = 0;
  if ( $meta ) {
       my @fields = $meta->find( "FIELD" );
       foreach my $field ( @fields ) {
         my $title = $field->{"title"};
         my $name = $field->{"name"};
         if( $title eq $formField || $name eq $formField ) {
               $found = 1;
               my $value = $field->{"value"};
               if (length $value) {
                 $text = $format;
                 $text =~ s/\$value/$value/go;
               } elsif ( defined $default ) {
                 $text = $default;
               }
               last; #one hit suffices
         }
       }
  }

  unless ( $found ) {
       $text = $altText;
  }

  return "" unless $text;

  return getRenderedVersion( $text, $web );
}

=pod

---++ sub getRenderedVersion (  $text, $theWeb, $meta  )

Not yet documented.

=cut

sub getRenderedVersion
{
    my( $text, $theWeb, $meta ) = @_;
    my( $head, $result, $extraLines, $insidePRE, $insideTABLE, $insideNoAutoLink );

    return "" unless $text;  # nothing to do

    # FIXME: Get $theTopic from parameter to handle [[#anchor]] correctly
    # (fails in %INCLUDE%, %SEARCH%)
    my $theTopic = $TWiki::topicName;

    # PTh 22 Jul 2000: added $theWeb for correct handling of %INCLUDE%, %SEARCH%
    if( !$theWeb ) {
        $theWeb = $TWiki::webName;
    }

    $head = "";
    $result = "";
    $insidePRE = 0;
    $insideTABLE = 0;
    $insideNoAutoLink = 0;      # PTh 02 Feb 2001: Added Codev.DisableWikiWordLinks
    $isList = 0;
    @listTypes = ();
    @listElements = ();

    # Initial cleanup
    $text =~ s/\r//g;
    $text =~ s/(\n?)$/\n<nop>\n/s; # clutch to enforce correct rendering at end of doc
    # Convert any occurrences of token (very unlikely - details in
    # Codev.NationalCharTokenClash)
    $text =~ s/$TranslationToken/!/go;	

    my @verbatim = ();
    $text = TWiki::takeOutVerbatim( $text, \@verbatim );
    $text =~ s/\\\n//g;  # Join lines ending in "\"

    # do not render HTML head, style sheets and scripts
    if( $text =~ m/<body[\s\>]/i ) {
        my $bodyTag = "";
        my $bodyText = "";
        ( $head, $bodyTag, $bodyText ) = split( /(<body)/i, $text, 3 );
        $text = $bodyTag . $bodyText;
    }
    
    # Wiki Plugin Hook
    &TWiki::Plugins::startRenderingHandler( $text, $theWeb, $meta );

    # $isList is tested and set by this loop and 'emitList' function
    $isList = 0;		# True when within a list

    foreach( split( /\n/, $text ) ) {

        # change state:
        m|<pre>|i  && ( $insidePRE = 1 );
        m|</pre>|i && ( $insidePRE = 0 );
        m|<noautolink>|i   && ( $insideNoAutoLink = 1 );
        m|</noautolink>|i  && ( $insideNoAutoLink = 0 );

        if( $insidePRE ) {
            # inside <PRE>

            # close list tags if any
            if( @listTypes ) {
                $result .= &emitList( "", "", 0 );
                $isList = 0;
            }

# Wiki Plugin Hook
            &TWiki::Plugins::insidePREHandler( $_ );

            s/(.*)/$1\n/;
            s/\t/   /g;		# Three spaces
            $result .= $_;

        } else {
          # normal state, do Wiki rendering

# Wiki Plugin Hook
          &TWiki::Plugins::outsidePREHandler( $_ );
          $extraLines = undef;   # Plugins might introduce extra lines
          do {                   # Loop over extra lines added by plugins
            $_ = $extraLines if( defined $extraLines );
            s/^(.*?)\n(.*)$/$1/s;
            $extraLines = $2;    # Save extra lines, need to parse each separately

# Escape rendering: Change " !AnyWord" to " <nop>AnyWord", for final " AnyWord" output
            s/(^|[\s\(])\!(?=[\w\*\=])/$1<nop>/g;

# Blockquoted email (indented with '> ')
            s/^>(.*?)$/> <cite> $1 <\/cite><br \/>/g;

# Embedded HTML
            s/\<(\!\-\-)/$TranslationToken$1/g;  # Allow standalone "<!--"
            s/(\-\-)\>/$1$TranslationToken/g;    # Allow standalone "-->"
	    # FIXME: next 2 lines are redundant since s///g's below do same
	    # thing
            s/(\<\<+)/"&lt\;" x length($1)/ge;
            s/(\>\>+)/"&gt\;" x length($1)/ge;
            s/\<nop\>/nopTOKEN/g;  # defuse <nop> inside HTML tags
            s/\<(\S.*?)\>/$TranslationToken$1$TranslationToken/g;
            s/</&lt\;/g;
            s/>/&gt\;/g;
            s/$TranslationToken(\S.*?)$TranslationToken/\<$1\>/go;
            s/nopTOKEN/\<nop\>/g;
            s/(\-\-)$TranslationToken/$1\>/go;
            s/$TranslationToken(\!\-\-)/\<$1/go;

# Handle embedded URLs
            s!(^|[\-\*\s\(])($regex{linkProtocolPattern}\:([^\s\<\>\"]+[^\s\.\,\!\?\;\:\)\<]))!&externalLink($1,$2)!geo;

# Entities
            s/&(\w+?)\;/$TranslationToken$1\;/g;      # "&abc;"
            s/&(\#[0-9]+)\;/$TranslationToken$1\;/g;  # "&#123;"
            s/&/&amp;/g;                              # escape standalone "&"
            s/$TranslationToken/&/go;

# Headings
            # '<h6>...</h6>' HTML rule
            s/$regex{headerPatternHt}/&makeAnchorHeading($2,$1)/geoi;
            # '\t+++++++' rule
            s/$regex{headerPatternSp}/&makeAnchorHeading($2,(length($1)))/geo;
            # '----+++++++' rule
            s/$regex{headerPatternDa}/&makeAnchorHeading($2,(length($1)))/geo;

# Horizontal rule
            s/^---+/<hr \/>/;
            s!^([a-zA-Z0-9]+)----*!<table width=\"100%\"><tr><td valign=\"bottom\"><h2>$1</h2></td><td width=\"98%\" valign=\"middle\"><hr /></td></tr></table>!o;

# Table of format: | cell | cell |
            # PTh 25 Jan 2001: Forgiving syntax, allow trailing white space
            if( $_ =~ /^(\s*)\|.*\|\s*$/ ) {
                s/^(\s*)\|(.*)/&emitTR($1,$2,$insideTABLE)/e;
                $insideTABLE = 1;
            } elsif( $insideTABLE ) {
                $result .= "</table>\n";
                $insideTABLE = 0;
            }

# Lists and paragraphs
            s/^\s*$/<p \/>/o                 && ( $isList = 0 );
            m/^(\S+?)/o                      && ( $isList = 0 );
	    # Definition list
            s/^(\t+)\$\s(([^:]+|:[^\s]+)+?):\s/<dt> $2 <\/dt><dd> /o && ( $result .= &emitList( "dl", "dd", length $1 ) );
            s/^(\t+)(\S+?):\s/<dt> $2<\/dt><dd> /o && ( $result .= &emitList( "dl", "dd", length $1 ) );
	    # Unnumbered list
            s/^(\t+)\* /<li> /o              && ( $result .= &emitList( "ul", "li", length $1 ) );
	    # Numbered list
            s/^(\t+)([1AaIi]\.|\d+\.?) ?/<li> /o && ( $result .= &emitList( "ol", "li", length $1, $2 ) );
	    # Finish the list
            if( ! $isList ) {
                $result .= &emitList( "", "", 0 );
                $isList = 0;
            }

# '#WikiName' anchors
            s/^(\#)($regex{wikiWordRegex})/ '<a name="' . &makeAnchorName( $2 ) . '"><\/a>'/ge;

# enclose in white space for the regex that follow
             s/(.*)/\n$1\n/;

# Emphasizing
            # PTh 25 Sep 2000: More relaxed rules, allow leading '(' and trailing ',.;:!?)'
            s/([\s\(])==([^\s]+?|[^\s].*?[^\s])==([\s\,\.\;\:\!\?\)])/$1 . &fixedFontText( $2, 1 ) . $3/ge;
            s/([\s\(])__([^\s]+?|[^\s].*?[^\s])__([\s\,\.\;\:\!\?\)])/$1<strong><em>$2<\/em><\/strong>$3/g;
            s/([\s\(])\*([^\s]+?|[^\s].*?[^\s])\*([\s\,\.\;\:\!\?\)])/$1<strong>$2<\/strong>$3/g;
            s/([\s\(])_([^\s]+?|[^\s].*?[^\s])_([\s\,\.\;\:\!\?\)])/$1<em>$2<\/em>$3/g;
            s/([\s\(])=([^\s]+?|[^\s].*?[^\s])=([\s\,\.\;\:\!\?\)])/$1 . &fixedFontText( $2, 0 ) . $3/ge;

# Mailto
	    # Email addresses must always be 7-bit, even within I18N sites

	    # RD 27 Mar 02: Mailto improvements - FIXME: check security...
	    # Explicit [[mailto:... ]] link without an '@' - hence no 
	    # anti-spam padding needed.
            # '[[mailto:string display text]]' link (no '@' in 'string'):
            s/\[\[mailto\:([^\s\@]+)\s+(.+?)\]\]/&mailtoLinkSimple( $1, $2 )/ge;

	    # Explicit [[mailto:... ]] link including '@', with anti-spam 
	    # padding, so match name@subdom.dom.
            # '[[mailto:string display text]]' link
            s/\[\[mailto\:([a-zA-Z0-9\-\_\.\+]+)\@([a-zA-Z0-9\-\_\.]+)\.(.+?)(\s+|\]\[)(.*?)\]\]/&mailtoLinkFull( $1, $2, $3, $5 )/ge;

	    # Normal mailto:foo@example.com ('mailto:' part optional)
	    # FIXME: Should be '?' after the 'mailto:'...
            s/([\s\(])(?:mailto\:)*([a-zA-Z0-9\-\_\.\+]+)\@([a-zA-Z0-9\-\_\.]+)\.([a-zA-Z0-9\-\_]+)(?=[\s\.\,\;\:\!\?\)])/$1 . &mailtoLink( $2, $3, $4 )/ge;

# Make internal links
            # Escape rendering: Change " ![[..." to " [<nop>[...", for final unrendered " [[..." output
            s/(\s)\!\[\[/$1\[<nop>\[/g;
	    # Spaced-out Wiki words with alternative link text
            # '[[Web.odd wiki word#anchor][display text]]' link:
            s/\[\[([^\]]+)\]\[([^\]]+)\]\]/&specificLink("",$theWeb,$theTopic,$2,$1)/ge;
            # RD 25 Mar 02: Codev.EasierExternalLinking
            # '[[URL#anchor display text]]' link:
            s/\[\[([a-z]+\:\S+)\s+(.*?)\]\]/&specificLink("",$theWeb,$theTopic,$2,$1)/ge;
	    # Spaced-out Wiki words
            # '[[Web.odd wiki word#anchor]]' link:
            s/\[\[([^\]]+)\]\]/&specificLink("",$theWeb,$theTopic,$1,$1)/ge;

            # do normal WikiWord link if not disabled by <noautolink> or NOAUTOLINK preferences variable
            unless( $noAutoLink || $insideNoAutoLink ) {

                # 'Web.TopicName#anchor' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})($regex{anchorRegex})/&internalLink($1,$2,$3,"$TranslationToken$3$4$TranslationToken",$4,1)/geo;
                # 'Web.TopicName' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})/&internalCrosswebLink($1,$2,$3,"$TranslationToken$3$TranslationToken","",1)/geo;

                # 'TopicName#anchor' link:
                s/([\s\(])($regex{wikiWordRegex})($regex{anchorRegex})/&internalLink($1,$theWeb,$2,"$TranslationToken$2$3$TranslationToken",$3,1)/geo;

                # 'TopicName' link:
		s/([\s\(])($regex{wikiWordRegex})/&internalLink($1,$theWeb,$2,$2,"",1)/geo;

		# Handle acronyms/abbreviations of three or more letters
                # 'Web.ABBREV' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{abbrevRegex})/&internalLink($1,$2,$3,$3,"",0,1)/geo;
                # 'ABBREV' link:
		s/([\s\(])($regex{abbrevRegex})/&internalLink($1,$theWeb,$2,$2,"",0)/geo;
                # (deprecated <link> moved to DefaultPlugin)

                s/$TranslationToken(\S.*?)$TranslationToken/$1/go;
            }

            s/^\n//;
            s/\t/   /g;
            $result .= $_;

          } while( defined( $extraLines ) );  # extra lines produced by plugins
        }
    }
    if( $insideTABLE ) {
        $result .= "</table>\n";
    }
    $result .= &emitList( "", "", 0 );
    if( $insidePRE ) {
        $result .= "</pre>\n";
    }

    # Wiki Plugin Hook
    &TWiki::Plugins::endRenderingHandler( $result );

    $result = TWiki::putBackVerbatim( $result, "pre", @verbatim );

    $result =~ s|\n?<nop>\n$||o; # clean up clutch
    return "$head$result";
}

=end twiki

=cut

1;
