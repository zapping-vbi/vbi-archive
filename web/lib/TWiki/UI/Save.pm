#
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

---+ TWiki::UI::Save

UI delegate for save function

=cut

package TWiki::UI::Save;

use strict;
use TWiki;
use TWiki::UI;
use TWiki::UI::Preview;

=pod

---++ save( )
Command handler for save command. Some parameters are passed in CGI:
| =cmd= | |
| =text= | target text for the topic |
| =unlock= | if defined, unlock the written topic |
| =dontnotify= | if defined, suppress change notification |
| =submitChangeForm= | |
| =topicparent= | |
| =formtemplate= | if define, use the named template for the form |
Note that this function is rundundant once savemulti takes over all saving
duties.

=cut

sub save {
  my( $webName, $topic, $userName, $query ) = @_;
  if ( _save( @_ )) {
    TWiki::redirect( $query, TWiki::getViewUrl( TWiki::Store::normalizeWebTopicName($webName, $topic)) );
  }
}

# Private - do not call outside this module!
sub _save {
  my( $webName, $topic, $userName, $query ) = @_;

  my $saveCmd = $query->param( "cmd" ) || "";
  my $text = $query->param( "text" );
  my $meta = "";

  # A template was requested; read it, and expand URLPARAMs within the
  # template using our CGI record
  my $templatetopic = $query->param( "templatetopic");
  if ($templatetopic) {
    ($meta, $text) = &TWiki::Store::readTopic( $webName, $templatetopic );
    $text = TWiki::expandVariablesOnTopicCreation( $text );
  }
	
  my $unlock = $query->param( "unlock" ) || "";
  my $dontNotify = $query->param( "dontnotify" ) || "";
  my $changeform = $query->param( 'submitChangeForm' ) || "";
  my $theParent = $query->param( 'topicparent' ) || "";
  my $formTemplate = $query->param( "formtemplate" );

  return 0 unless TWiki::UI::webExists( $webName, $topic );

  return 0 if TWiki::UI::isMirror( $webName, $topic );

  my $wikiUserName = TWiki::userToWikiName( $userName );
  return 0 unless TWiki::UI::isAccessPermitted( $webName, $topic,
                                            "change", $wikiUserName );

  # check permission for undocumented cmd=... parameter
  return 0 if ( $saveCmd &&
              ! TWiki::UI::userIsAdmin( $webName, $topic, $wikiUserName ));

  # PTh 06 Nov 2000: check if proper use of save script
  if( ! ( defined $text ) ) {
    TWiki::UI::oops( $webName, $topic, "save" );
    return 0;
  } elsif( ! $text ) {
    # empty topic not allowed
    TWiki::UI::oops( $webName, $topic, "empty" );
    return 0;
  }

  if( $changeform ) {
    use TWiki::Form;
    TWiki::Form::changeForm( $webName, $topic, $query );
    return 0;
  }

  $text = TWiki::Render::decodeSpecialChars( $text );
  $text =~ s/ {3}/\t/go;

  if( $saveCmd eq "repRev" ) {
    $text =~ s/%__(.)__%/%_$1_%/go;
    ( $meta, $text ) = TWiki::Store::_extractMetaData( $webName, $topic, $text );
  } else {
    # normal case: Get latest attachment from file for preview
    my $tmp;
	# read meta (if not already read when reading template)
    ( $meta, $tmp ) = TWiki::Store::readTopic( $webName, $topic ) unless $meta;

    # parent setting
    if( $theParent eq "none" ) {
      $meta->remove( "TOPICPARENT" );
    } elsif( $theParent ) {
      $meta->put( "TOPICPARENT", ( "name" => $theParent ) );
    }

    if( $formTemplate ) {
      $meta->remove( "FORM" );
      $meta->put( "FORM", ( name => $formTemplate ) ) if( $formTemplate ne "none" );
    }

    use TWiki::Form;
    # CODE_SMELL: this fieldVars2Meta thing should be in UI, not Meta
	# Expand field variables, unless this new page is templated
    TWiki::Form::fieldVars2Meta( $webName, $query, $meta ) unless $templatetopic;
    use TWiki::Prefs;
    $text = TWiki::Prefs::updateSetFromForm( $meta, $text );
  }

  my $error = TWiki::Store::saveTopic( $webName, $topic, $text, $meta, $saveCmd, $unlock, $dontNotify );
  if( $error ) {
    TWiki::UI::oops( $webName, $topic, "saveerr", $error );
    return 0;
  }

  return 1;
}

=pod

---++ savemulti( )
Command handler for savemulti command. Some parameters are passed in CGI:
| =action= | savemulti overrides, everything else is passed on the normal =save= |
action values are:
| =save= | save, unlock topic, return to view, dontnotify is OFF |
| =quietsave= | save, unlock topic,  return to view, dontnotify is ON |
| =checkpoint= | save and continue editing, dontnotify is ON |
| =cancel= | exit without save, unlock topic, return to view (does _not_ undo Checkpoint saves) |
| =preview= | preview edit text; same as before |
This function can replace "save" eventually.

=cut

sub savemulti {
  my( $webName, $topic, $userName, $query ) = @_;

  my $redirecturl = TWiki::getViewUrl( TWiki::Store::normalizeWebTopicName($webName, $topic));

  my $saveaction = lc($query->param( 'action' ));
  if ( $saveaction eq "checkpoint" ) {
    $query->param( -name=>"dontnotify", -value=>"checked" );
    $query->param( -name=>"unlock", -value=>'0' );
    my $editURL = TWiki::getScriptUrl( $webName, $topic, "edit" );
    my $randompart = randomURL();
    $redirecturl = "$editURL|$randompart";
  } elsif ( $saveaction eq "quietsave" ) {
    $query->param( -name=>"dontnotify", -value=>"checked" );
  } elsif ( $saveaction eq "cancel" ) {
    my $viewURL = TWiki::getScriptUrl( $webName, $topic, "view" );
    TWiki::redirect( $query, "$viewURL?unlock=on" );
    return;
  } elsif( $saveaction eq "preview" ) {
    TWiki::UI::Preview::preview( $webName, $topic, $userName, $query );
    return;
  }

  # save called by preview
  if ( _save( $webName, $topic, $userName, $query )) {
    TWiki::redirect( $query, $redirecturl );
  }
}

## Random URL:
# returns 4 random bytes in 0x01-0x1f range in %xx form
# =========================
sub randomURL
{
  my (@hc) = (qw (01 02 03 04 05 06 07 08 09 0b 0c 0d 0e 0f 10
		  11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f));
  #  srand; # needed only for perl < 5.004
  return "%$hc[rand(30)]%$hc[rand(30)]%$hc[rand(30)]%$hc[rand(30)]";
}

1;

