# TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2004 Peter Thoeny, peter@thoeny.com
#
# Based on parts of Ward Cunninghams original Wiki and JosWiki.
# Copyright (C) 1998 Markus Peter - SPiN GmbH (warpi@spin.de)
# Some changes by Dave Harris (drh@bhresearch.co.uk) incorporated
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

---+ TWiki::UI::Edit
Edit command handler

=cut
package TWiki::UI::Edit;

use strict;
use TWiki;
use TWiki::Form;
use TWiki::Plugins;
use TWiki::Prefs;
use TWiki::Store;
use TWiki::UI;

=pod

---++ edit( $webName, $topic, $userName, $query )
Edit handler. Most parameters are in the CGI query:
| =cmd= | |
| =breaklock= | if defined, breaks any pre-existing lock before edit |
| =onlywikiname= | if defined, requires a wiki name for the topic name if this is a new topic |
| =onlynewtopic= | if defined, and the topic exists, then moans |
| =formtemplate= | name of the form for the topic; will replace existing form |
| =templatetopic= | name of the topic to copy if creating a new topic |
| =skin= | skin to use |
| =topicparent= | what to put in the topic prent meta data |
| =text= | text that will replace the old topic text if a formtemplate is defined (what the heck is this for?) |
| =contenttype= | optional parameter that defines the application type to write into the CGI header. Defaults to text/html. |

=cut
sub edit {
  my ( $webName, $topic, $userName, $query ) = @_;

  my $saveCmd = $query->param( 'cmd' ) || "";
  my $breakLock = $query->param( 'breaklock' ) || "";
  my $onlyWikiName = $query->param( 'onlywikiname' ) || "";
  my $onlyNewTopic = $query->param( 'onlynewtopic' ) || "";
  my $formTemplate  = $query->param( "formtemplate" ) || "";
  my $templateTopic = $query->param( "templatetopic" ) || "";
  # apptype is undocumented legacy
  my $cgiAppType = $query->param( 'contenttype' ) || $query->param( 'apptype' ) || "text/html";
  my $skin = $query->param( "skin" );
  my $theParent = $query->param( 'topicparent' ) || "";
  my $ptext = $query->param( 'text' );

  my $getValuesFromFormTopic = ( ( $formTemplate ) && ( ! $ptext ) );

  return unless TWiki::UI::webExists( $webName, $topic );

  return if TWiki::UI::isMirror( $webName, $topic );

  my $tmpl = "";
  my $text = "";
  my $meta = "";
  my $extra = "";
  my $topicExists  = &TWiki::Store::topicExists( $webName, $topic );

  # Prevent editing existing topic?
  if( $onlyNewTopic && $topicExists ) {
    # Topic exists and user requested oops if it exists
    TWiki::UI::oops( $webName, $topic, "createnewtopic" );
    return;
  }

  # prevent non-Wiki names?
  if( ( $onlyWikiName )
      && ( ! $topicExists )
      && ( ! ( &TWiki::isWikiName( $topic ) || &TWiki::isAbbrev( $topic ) ) ) ) {
    # do not allow non-wikinames, redirect to view topic
    TWiki::UI::redirect( TWiki::getViewUrl( $webName, $topic ) );
    return;
  }

  # Read topic 
  if( $topicExists ) {
    ( $meta, $text ) = &TWiki::Store::readTopic( $webName, $topic );
  }

  my $wikiUserName = &TWiki::userToWikiName( $userName );
  return unless TWiki::UI::isAccessPermitted( $webName, $topic,
                                            "change", $wikiUserName );

  # Special save command
  return if( $saveCmd && ! TWiki::UI::userIsAdmin( $webName, $topic, $wikiUserName ));

  # Check for locks
  my( $lockUser, $lockTime ) = &TWiki::Store::topicIsLockedBy( $webName, $topic );
  if( ( ! $breakLock ) && ( $lockUser ) ) {
    # warn user that other person is editing this topic
    $lockUser = &TWiki::userToWikiName( $lockUser );
    use integer;
    $lockTime = ( $lockTime / 60 ) + 1; # convert to minutes
    my $editLock = $TWiki::editLockTime / 60;
    TWiki::UI::oops( $webName, $topic, "locked",
                     $lockUser, $editLock, $lockTime );
    return;
  }
  &TWiki::Store::lockTopic( $topic );

  my $templateWeb = $webName;

  # Get edit template, standard or a different skin
  $skin = TWiki::Prefs::getPreferencesValue( "SKIN" ) unless ( $skin );
  $tmpl = &TWiki::Store::readTemplate( "edit", $skin );
  unless( $topicExists ) {
    if( $templateTopic ) {
      if( $templateTopic =~ /^(.+)\.(.+)$/ ) {
        # is "Webname.SomeTopic"
        $templateWeb   = $1;
        $templateTopic = $2;
      }

      ( $meta, $text ) = &TWiki::Store::readTopic( $templateWeb, $templateTopic );
    } elsif( ! $text ) {
      ( $meta, $text ) = &TWiki::Store::readTemplateTopic( "WebTopicEditTemplate" );
    }
    $extra = "(not exist)";

    # If present, instantiate form
    if( ! $formTemplate ) {
      my %args = $meta->findOne( "FORM" );
      $formTemplate = $args{"name"};
    }

    $text = TWiki::expandVariablesOnTopicCreation( $text, $userName );
  }

  # parent setting
  if( $theParent eq "none" ) {
    $meta->remove( "TOPICPARENT" );
  } elsif( $theParent ) {
    if( $theParent =~ /^([^.]+)\.([^.]+)$/ ) {
      my $parentWeb = $1;
      if( $1 eq $webName ) {
        $theParent = $2;
      }
    }
    $meta->put( "TOPICPARENT", ( "name" => $theParent ) );
  }
  $tmpl =~ s/%TOPICPARENT%/$theParent/;

  # Processing of formtemplate - comes directly from query parameter formtemplate , 
  # or indirectly from webtopictemplate parameter.
  my $oldargsr;
  if( $formTemplate ) {
    my @args = ( name => $formTemplate );
    $meta->remove( "FORM" );
    if( $formTemplate ne "none" ) {
      $meta->put( "FORM", @args );
    } else {
      $meta->remove( "FORM" );
    }
    $tmpl =~ s/%FORMTEMPLATE%/$formTemplate/go;
    if( defined $ptext ) {
      $text = $ptext;
      $text = &TWiki::Render::decodeSpecialChars( $text );
    }
  }

  if( $saveCmd eq "repRev" ) {
    $text = TWiki::Store::readTopicRaw( $webName, $topic );
  }

  $text =~ s/&/&amp\;/go;
  $text =~ s/</&lt\;/go;
  $text =~ s/>/&gt\;/go;
  $text =~ s/\t/   /go;

  #AS added hook for plugins that want to do heavy stuff
  TWiki::Plugins::beforeEditHandler( $text, $topic, $webName ) unless( $saveCmd eq "repRev" );
  #/AS

  if( $TWiki::doLogTopicEdit ) {
    # write log entry
    &TWiki::Store::writeLog( "edit", "$webName.$topic", $extra );
  }

  if( $saveCmd ) {
    $tmpl =~ s/\(edit\)/\(edit cmd=$saveCmd\)/go;
  }
  $tmpl =~ s/%CMD%/$saveCmd/go;
  $tmpl = &TWiki::handleCommonTags( $tmpl, $topic );
  if( $saveCmd ne "repRev" ) {
    $tmpl = &TWiki::handleMetaTags( $webName, $topic, $tmpl, $meta );
  } else {
    $tmpl =~ s/%META{[^}]*}%//go;
  }
  $tmpl = &TWiki::Render::getRenderedVersion( $tmpl );

  # Don't want to render form fields, so this after getRenderedVersion
  my %formMeta = $meta->findOne( "FORM" );
  my $form = "";
  $form = $formMeta{"name"} if( %formMeta );
  if( $form && $saveCmd ne "repRev" ) {
    my @fieldDefs = &TWiki::Form::getFormDef( $templateWeb, $form );

    if( ! @fieldDefs ) {
      TWiki::UI::oops( $webName, $topic, "noformdef" );
      return;
    }
    my $formText = &TWiki::Form::renderForEdit( $webName, $topic, $form, $meta, $query, $getValuesFromFormTopic, @fieldDefs );
    $tmpl =~ s/%FORMFIELDS%/$formText/go;
  } elsif( $saveCmd ne "repRev" && TWiki::Prefs::getPreferencesValue( "WEBFORMS", $webName )) {
	# follows a hybrid html monster to let the 'choose form button' align at
	# the right of the page in all browsers
    $form = '<div style="text-align:right;"><table width="100%" border="0" cellspacing="0" cellpadding="0" class="twikiChangeFormButtonHolder"><tr><td align="right">'
      . &TWiki::Form::chooseFormButton( "Add form" )
        . '</td></tr></table></div>';
    $tmpl =~ s/%FORMFIELDS%/$form/go;
  } else {
    $tmpl =~ s/%FORMFIELDS%//go;
  }

  $tmpl =~ s/%FORMTEMPLATE%//go; # Clear if not being used
  $tmpl =~ s/%TEXT%/$text/go;
  $tmpl =~ s/( ?) *<\/?(nop|noautolink)\/?>\n?/$1/gois;   # remove <nop> and <noautolink> tags

  TWiki::writeHeaderFull ( $query, 'edit', $cgiAppType, length($tmpl) );

  print $tmpl;
}

1;
