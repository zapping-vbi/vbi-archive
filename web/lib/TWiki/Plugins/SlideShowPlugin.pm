# Plugin for TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2002-2004 Peter Thoeny, peter@thoeny.com
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
#
# Each plugin is a package that contains the subs:
#
#   initPlugin           ( $topic, $web, $user, $installWeb )
#   commonTagsHandler    ( $text, $topic, $web )
#   startRenderingHandler( $text, $web )
#   outsidePREHandler    ( $text )
#   insidePREHandler     ( $text )
#   endRenderingHandler  ( $text )
#   beforeSaveHandler    ( $text, $topic, $web )
#
# initPlugin is required, all other are optional.
# For increased performance, all handlers except initPlugin are
# disabled. To enable a handler remove the leading DISABLE_ from
# the function name.
#
# NOTE: To interact with TWiki use the official TWiki functions
# in the &TWiki::Func module. Do not reference any functions or
# variables elsewhere in TWiki!!


# =========================
package TWiki::Plugins::SlideShowPlugin;

# =========================
use vars qw(
        $web $topic $user $installWeb $VERSION $debug
    );

$VERSION = '1.003';

# =========================
sub initPlugin
{
    ( $topic, $web, $user, $installWeb ) = @_;

    # check for Plugins.pm versions
    if( $TWiki::Plugins::VERSION < 1 ) {
        &TWiki::Func::writeWarning( "Version mismatch between SlideShowPlugin and Plugins.pm" );
        return 0;
    }

    # Get plugin debug flag
    $debug = &TWiki::Func::getPreferencesFlag( "SLIDESHOWPLUGIN_DEBUG" );

    # Plugin correctly initialized
    TWiki::Func::writeDebug( "- TWiki::Plugins::SlideShowPlugin::initPlugin( $web.$topic ) is OK" ) if $debug;
    return 1;
}

# =========================
sub commonTagsHandler
{
### my ( $text, $topic, $web ) = @_;   # do not uncomment, use $_[0], $_[1]... instead

    TWiki::Func::writeDebug( "- SlideShowPlugin::commonTagsHandler( $_[2].$_[1] )" ) if $debug;

    # This is the place to define customized tags and variables
    # Called by sub handleCommonTags, after %INCLUDE:"..."%

    if( $_[0] =~ /%SLIDESHOWSTART/ ) {
        $_[0] = slideShowHandler( $_[0], $_[2], $_[1] );
    }
}

# =========================
sub slideShowHandler
{
    my( $text, $theWeb, $theTopic ) = @_;

    my $textPre = "";
    my $textPost = "";
    my $args = "";
    if( $text =~ /^(.*)%SLIDESHOWSTART%(.*)$/s ) {
        $textPre = $1;
        $text = $2;
    } elsif( $text =~ /^(.*)%SLIDESHOWSTART{(.*?)}%(.*)$/s ) {
        $textPre = $1;
        $args = $2;
        $text = $3;
    }
    if( $text =~ /^(.*)%SLIDESHOWEND%(.*)$/s ) {
        $text = $1;
        $textPost = $2;
    }

    $query = TWiki::Func::getCgiQuery();
    if( $query && $query->param( 'slideshow' ) ) {
        # in presentation mode

        $textPre .= "\n#StartPresentation\n";
        $textPre .= renderSlideNav( $theWeb, $theTopic, 1, 1, "e" );

        my $slideMax = 0;

        if( $text =~ /(.*?[\n\r])\-\-\-+(\++)\!* (.*)/s ) {
            $textPre .= $1;
            $text = $3;
            my $level = $2;
            $level =~ s/\+/\\\+/go;
            my @slides = split( /[\n\r]\-\-\-+$level\!* /, $text );
            $text = "";

            my $hideComments = TWiki::Prefs::getPreferencesValue( "SLIDESHOWPLUGIN_HIDECOMMENTS" ) || "";

            my $tmplText = readTmplText( $theWeb, $args );
            my $slideText = "";
            my $slideTitle = "";
            my $slideBody = "";
            my $slideComment = "";
            my $slideNum = 1;
            $slideMax = @slides;
            my @titles = ();
            foreach( @slides ) {
                /^([^\n\r]*)(.*)$/s;
                $slideTitle = $1 || "";
                $slideBody  = $2 || "";
                $slideComment = "";
                if( $hideComments && $slideBody =~ s/(\-\-\-+\+$level+\!*\s*Comments.*)//is ) {
                    $slideComment = $1;
                }
                push( @titles, $slideTitle );
                $slideText = $tmplText;
                $slideText =~ s/%SLIDETITLE%/$slideTitle/go;
                $slideText =~ s/%SLIDETEXT%/$slideBody/go;
                $slideText =~ s/%SLIDENUM%/$slideNum/go;
                $slideText =~ s/%SLIDEMAX%/$slideMax/go;
                $slideText =~ s/%SLIDENAV%/renderSlideNav(      $theWeb, $theTopic, $slideNum, $slideMax, "f p n" )/geo;
                $slideText =~ s/%SLIDENAVALL%/renderSlideNav(   $theWeb, $theTopic, $slideNum, $slideMax, "f p n l" )/geo;
                $slideText =~ s/%SLIDENAVFIRST%/renderSlideNav( $theWeb, $theTopic, $slideNum, $slideMax, "f" )/geo;
                $slideText =~ s/%SLIDENAVPREV%/renderSlideNav(  $theWeb, $theTopic, $slideNum, $slideMax, "p" )/geo;
                $slideText =~ s/%SLIDENAVNEXT%/renderSlideNav(  $theWeb, $theTopic, $slideNum, $slideMax, "n" )/geo;
                $slideText =~ s/%SLIDENAVLAST%/renderSlideNav(  $theWeb, $theTopic, $slideNum, $slideMax, "l" )/geo;
                $text .= "\n\n-----\n#GoSlide$slideNum\n$slideText";
                $text .= "\n$slideComment\n\n" if( $slideComment );
                $text .= "%BR%\n\n" x 20;
                $slideNum++;
            }
            $text =~ s/%TOC(?:\{.*?\})*%/renderSlideToc( $theWeb, $theTopic, @titles )/geo;
            $text .= "\n#GoSlide$slideNum\n%BR%\n";
        }

        $text = "$textPre\n$text\n";
        $text .= renderSlideNav( $theWeb, $theTopic, $slideMax + 1, $slideMax, "f p e" );
        $text .= "\n";
        $text .= "%BR%\n\n" x 30;
        $text =~ s/%BR%/<br \/>/go;
        $text .= $textPost;

    } else {
        # in normal topic view mode

        if( $text =~ /[\n\r]\-\-\-+(\++)/s ) {
            my $level = $1;
            $level =~ s/\+/\\\+/go;
            # add slide number to heading
            my $slideNum = 1;
            $text =~ s/([\n\r]\-\-\-+$level\!*) ([^\n\r]+)/"$1 Slide " . $slideNum++ . ": $2"/ges;
        }
        $text = "$textPre \n#StartPresentation\n"
              . renderSlideNav( $theWeb, $theTopic, 1, 1, "s" )
              . "\n$text $textPost";
    }

    return $text;
}

# =========================
sub renderSlideNav
{
    my( $theWeb, $theTopic, $theNum, $theMax, $theButtons ) = @_;
    my $prev = $theNum - 1 || 1;
    my $next = $theNum + 1;
    my $text = "";
    my $viewUrl = "%SCRIPTURLPATH%/view%SCRIPTSUFFIX%/$theWeb/$theTopic";
    if( $theButtons =~ /f/ ) {
        # first slide button
        if( $theButtons =~ / f/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide1\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/first.gif\" border=\"0\""
               . " alt=\"First slide\" /></a>";
    }
    if( $theButtons =~ /p/ ) {
        # previous slide button
        if( $theButtons =~ / p/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide$prev\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/prev.gif\" border=\"0\""
               . " alt=\"Previous\" /></a>";
    }
    if( $theButtons =~ /n/ ) {
        # next slide button
        if( $theButtons =~ / n/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide$next\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/next.gif\" border=\"0\""
               . " alt=\"Next\" /></a>";
    }
    if( $theButtons =~ /l/ ) {
        # last slide button
        if( $theButtons =~ / l/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide$theMax\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/last.gif\" border=\"0\""
               . " alt=\"Last slide\" /></a>";
    }
    if( $theButtons =~ /e/ ) {
        # end slideshow button
        if( $theButtons =~ / e/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl#StartPresentation\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/endpres.gif\" border=\"0\""
               . " alt=\"End Presentation\" /></a>";
    }
    if( $theButtons =~ /s/ ) {
        # start slideshow button
        if( $theButtons =~ / s/ ) {
            $text .= "&nbsp;";
        }
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide1\">"
               . "<img src=\"%PUBURLPATH%/$installWeb/SlideShowPlugin/startpres.gif\" border=\"0\""
               . " alt=\"Start Presentation\" /></a>";
    }

    return $text;
}

# =========================
sub renderSlideToc
{
    my( $theWeb, $theTopic, @theTitles ) = @_;

    my $slideNum = 1;
    my $text = "";
    my $viewUrl = "%SCRIPTURLPATH%/view%SCRIPTSUFFIX%/$theWeb/$theTopic";
    foreach( @theTitles ) {
        $text .= "\t\* ";
        $text .= "<a href=\"$viewUrl?slideshow=on&amp;skin=print#GoSlide$slideNum\">";
        $text .= " $_ </a>\n";
        $slideNum++;
    }
    return $text;
}

# =========================
sub readTmplText
{
    my( $theWeb, $theArgs ) = @_;

    my $tmplTopic =  TWiki::Func::extractNameValuePair( $theArgs, "template" );
    unless( $tmplTopic ) {
        $theWeb = $installWeb;
        $tmplTopic =  TWiki::Func::getPreferencesValue( "SLIDESHOWPLUGIN_TEMPLATE" )
                   || "SlideShowPlugin";
    }
    if( $tmplTopic =~ /^([^\.]+)\.(.*)$/o ) {
        $theWeb = $1;
        $tmplTopic = $2;
    }
    my( $meta, $text ) = TWiki::Func::readTopic( $theWeb, $tmplTopic );
    # remove everything before %STARTINCLUDE% and after %STOPINCLUDE%
    $text =~ s/.*?%STARTINCLUDE%//os;
    $text =~ s/%STOPINCLUDE%.*//os;

    unless( $text ) {
        $text = "<font color=\"red\"> $installWeb.SlideShowPlugin Error: </font>"
              . "Slide template topic <nop>$theWeb.$tmplTopic not found or empty!\n\n"
              . "%SLIDETITLE%\n\n%SLIDETEXT%\n\n";
    } elsif( $text =~ /%SLIDETITLE%/ && $text =~ /%SLIDETEXT%/ ) {
        # assume that format is OK
    } else {
        $text = "<font color=\"red\"> $installWeb.SlideShowPlugin Error: </font>"
              . "Missing =%<nop>SLIDETITLE%= or =%<nop>SLIDETEXT%= in "
              . "slide template topic $theWeb.$tmplTopic.\n\n"
              . "%SLIDETITLE%\n\n%SLIDETEXT%\n\n";
    }
    $text =~ s/%WEB%/$theWeb/go;
    $text =~ s/%TOPIC%/$tmplTopic/go;
    $text =~ s/%ATTACHURL%/%PUBURL%\/$theWeb\/$tmplTopic/go;
    return $text;
}

# =========================

1;
