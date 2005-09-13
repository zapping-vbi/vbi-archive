# Plugin for TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2000-2001 Andrea Sterbini, a.sterbini@flashnet.it
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
# http://www.gnu.ai.mit.edu/copyleft/gpl.html
#
# =========================
#
# This plugin replaces smilies with small smilies bitmaps
# see TWiki.SmiliesPlugin
#
# =========================
package TWiki::Plugins::SmiliesPlugin;
# =========================

use vars qw($web $topic $user $installWeb $VERSION $debug
            %smiliesUrls %smiliesEmotions
            $smiliesPattern $allPattern $smiliesPubUrl $smiliesFormat );

$VERSION = '1.003';

$smiliesPattern = '^\s*\|\s*<nop>(?:\&nbsp\;)?([^\s|]+)\s*\|\s*%ATTACHURL%\/([^\s]+)\s*\|\s*"([^"|]+)"\s*\|\s*$';
#                          smilie       url            emotion
$allPattern = "";
$smiliesPubUrl = "";

# =========================
sub initPlugin
{
    ( $topic, $web, $user, $installWeb ) = @_;

    # Get plugin debug flag
    $debug = &TWiki::Func::getPreferencesFlag( "SMILIESPLUGIN_DEBUG" );

    # Get plugin preferences
    $smiliesFormat = &TWiki::Func::getPreferencesValue( "SMILIESPLUGIN_FORMAT" ) 
					|| '<img src="$url" alt="$tooltip" title="$tooltip" border="0" />';

    my $topic = &TWiki::Func::getPreferencesValue( "SMILIESPLUGIN_TOPIC" ) 
					|| "$installWeb.SmiliesPlugin"; 

    my $web = $installWeb;
    if( $topic =~ /(.+)\.(.+)/ ) {
        $web = $1;
        $topic = $2;
    }

    $allPattern = "(";
    foreach( split( /\n/, TWiki::Func::readTopic( $web, $topic ) ) ) {
        if( m/$smiliesPattern/ ) {
            $allPattern .= "\Q$1\E|";
            $smiliesUrls{$1}     = $2;
            $smiliesEmotions{$1} = $3;
        }
    }
    $allPattern =~ s/\|$//o;
    $allPattern .= ")";
    $smiliesPubUrl = TWiki::Func::getUrlHost() . TWiki::Func::getPubUrlPath() .
                     "/$installWeb/SmiliesPlugin";

    # Initialization OK
    return 1;
}

# =========================
sub commonTagsHandler
{
#    my ( $text, $topic, $web ) = @_;
    $_[0] =~ s/%SMILIES%/&allSmiliesTable()/geo;
}

# =========================
sub outsidePREHandler
{
#    my ( $text, $web ) = @_;

    $_[0] =~ s/(\s|^)$allPattern(?=\s|$)/&renderSmilie( $1, $2 )/geo;
}

# =========================
sub renderSmilie
{
    my ( $thePre, $theSmilie ) = @_;

    return $thePre unless $theSmilie;  # return unless initialized

    my $text = "$thePre$smiliesFormat";
    $text =~ s/\$emoticon/$theSmilie/go;
    $text =~ s/\$tooltip/$smiliesEmotions{$theSmilie}/go;
    $text =~ s/\$url/$smiliesPubUrl\/$smiliesUrls{$theSmilie}/go;

    return $text;
}

# =========================
sub allSmiliesTable
{
    my $text = "| *What to Type* | *Graphic That Will Appear* | *Emotion* |\n";

#    my ($k, $a, $b);
    foreach $k ( sort { $smiliesEmotions{$b} cmp $smiliesEmotions{$a} }
                keys %smiliesEmotions )
    {
        $text .= "| <nop>$k | $k | ". $smiliesEmotions{$k} ." |\n";
    }
    return $text;
}

# =========================

1;
