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
package TWiki::UI::Preview;

use strict;
use TWiki;
use TWiki::UI;

sub preview {
  my ( $webName, $topic, $userName, $query ) = @_;

  my $skin = $query->param( "skin" );
  my $changeform = $query->param( 'submitChangeForm' ) || "";
  my $dontNotify = $query->param( "dontnotify" ) || "";
  my $saveCmd = $query->param( "cmd" ) || "";
  my $theParent = $query->param( 'topicparent' ) || "";
  my $formTemplate = $query->param( "formtemplate" );
  my $textparam = $query->param( "text" );

  return unless TWiki::UI::webExists( $webName, $topic );

  my $tmpl = ""; 
  my $text = "";
  my $ptext = "";
  my $meta = "";
  my $formFields = "";
  my $wikiUserName = &TWiki::userToWikiName( $userName );
  
  return if TWiki::UI::isMirror( $webName, $topic );

  # reset lock time, this is to prevent contention in case of a long edit session
  &TWiki::Store::lockTopic( $topic );

  $skin = &TWiki::Prefs::getPreferencesValue( "SKIN" ) unless( $skin );
  
  # Is user looking to change the form used?  Sits oddly in preview, but 
  # to avoid Javascript and pick up text on edit page it has to be in preview.
  if( $changeform ) {
    &TWiki::Form::changeForm( $webName, $topic, $query );
    return;
  }

  # get view template, standard view or a view with a different skin
  $tmpl = &TWiki::Store::readTemplate( "preview", $skin );
  $tmpl =~ s/%DONTNOTIFY%/$dontNotify/go;
  if( $saveCmd ) {
    return unless TWiki::UI::userIsAdmin( $webName, $topic, $wikiUserName );
    $tmpl =~ s/\(preview\)/\(preview cmd=$saveCmd\)/go;
  }
  $tmpl =~ s/%CMD%/$saveCmd/go;

  if( $saveCmd ne "repRev" ) {
    my $dummy = "";
    ( $meta, $dummy ) = &TWiki::Store::readTopic( $webName, $topic );

    # parent setting
    if( $theParent eq "none" ) {
      $meta->remove( "TOPICPARENT" );
    } elsif( $theParent ) {
      $meta->put( "TOPICPARENT", ( "name" => $theParent ) );
    }
    $tmpl =~ s/%TOPICPARENT%/$theParent/go;

    if( $formTemplate ) {
      $meta->remove( "FORM" );
      $meta->put( "FORM", ( name => $formTemplate ) ) if( $formTemplate ne "none" );
      $tmpl =~ s/%FORMTEMPLATE%/$formTemplate/go;
    } else {
      $tmpl =~ s/%FORMTEMPLATE%//go;
    }

    # get the edited text and combine text, form and attachments for preview
    &TWiki::Form::fieldVars2Meta( $webName, $query, $meta );
    $text = $textparam;
    if( ! $text ) {
      # empty topic not allowed
      TWiki::UI::oops( $webName, $topic, "empty" );
      return;
    }
    #AS added hook for plugins that want to do heavy stuff
    TWiki::Plugins::afterEditHandler( $text, $topic, $webName );
    $ptext = $text;

    if( $meta->count( "FORM" ) ) {
      $formFields = &TWiki::Form::getFieldParams( $meta );
    }
  } else {
    # undocumented "repRev" mode
    $text = $textparam; # text to save
    ( $meta, $ptext ) = &TWiki::Store::_extractMetaData( $webName, $topic, $text );
    $text =~ s/%_(.)_%/%__$1__%/go;
  }

  my @verbatim = ();
  $ptext = &TWiki::takeOutVerbatim( $ptext, \@verbatim );
  $ptext =~ s/ {3}/\t/go;
  $ptext = &TWiki::Prefs::updateSetFromForm( $meta, $ptext );
  $ptext = &TWiki::handleCommonTags( $ptext, $topic );
  $ptext = &TWiki::Render::getRenderedVersion( $ptext );

  # do not allow click on link before save: (mods by TedPavlic)
  my $oopsUrl = '%SCRIPTURLPATH%/oops%SCRIPTSUFFIX%/%WEB%/%TOPIC%';
  $oopsUrl = &TWiki::handleCommonTags( $oopsUrl, $topic );
  $ptext =~ s@(?<=<a\s)([^>]*)(href=(?:".*?"|[^"].*?(?=[\s>])))@$1href="$oopsUrl?template=oopspreview"@goi;
  $ptext =~ s@<form(?:|\s.*?)>@<form action="$oopsUrl">\n<input type="hidden" name="template" value="oopspreview">@goi;
  $ptext =~ s@(?<=<)([^\s]+?[^>]*)(onclick=(?:"location.href='.*?'"|location.href='[^']*?'(?=[\s>])))@$1onclick="location.href='$oopsUrl\?template=oopspreview'"@goi;

  $ptext = &TWiki::putBackVerbatim( $ptext, "pre", @verbatim );

  $tmpl = &TWiki::handleCommonTags( $tmpl, $topic );
  $tmpl = &TWiki::handleMetaTags( $webName, $topic, $tmpl, $meta );
  $tmpl = &TWiki::Render::getRenderedVersion( $tmpl );
  $tmpl =~ s/%TEXT%/$ptext/go;

  $text = &TWiki::Render::encodeSpecialChars( $text );

  $tmpl =~ s/%HIDDENTEXT%/$text/go;
  $tmpl =~ s/%FORMFIELDS%/$formFields/go;
  $tmpl =~ s/( ?) *<\/?(nop|noautolink)\/?>\n?/$1/gois;   # remove <nop> and <noautolink> tags

  &TWiki::writeHeader( $query );
  print $tmpl;
}

1;
