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

# Service functions used by the UI packages

package TWiki::UI;

=pod twiki

---+++ oops( $web, $topic, $oopsTmplName, ...)
Generate a relevant URL to redirect to the given oops page

=cut

sub oops {
  my $webName = shift;
  my $topic = shift;
  my $script = shift;

  $webName = $TWiki::mainWebname unless ( $webName );
  $topic = $TWiki::mainTopicname unless ( $topic );

  my $url = TWiki::getOopsUrl( $webName, $topic, "oops$script", @_ );
  redirect( $url, @_ );
}

=pod twiki

---+++ redirect( $url, ... )
Generate a CGI redirect unless (1) TWiki::getCgiQuery() returns undef or
(2) $query->param('noredirect') is set to any value. Thus a redirect is
only generated when in a CGI context. The ... parameters are
concatenated to the message written when printing to STDOUT, and are
ignored for a redirect.

=cut

sub redirect {
  my $url = shift;

  my $query = TWiki::getCgiQuery();

  if ( $query && $query->param( 'noredirect' )) {
    TWiki::writeHeader( $query );
  } elsif ( $query ) {
    TWiki::redirect( $query, $url );
    return; # no print to STDOUT
  }

  print join(" ", @_) . " \n";
}

=pod twiki

---+++ webExists( $web, $topic ) => boolean
Check if the web exists, returning 1 if it does, or
calling TWiki::UI::oops and returning 0 if it doesn't.

=cut

sub webExists {
  my ( $webName, $topic ) = @_;

  return 1 if( TWiki::Store::webExists( $webName ) );

  oops( $webName, $topic, "noweb", "ERROR $webName.$topic Missing Web" );

  return 0;
}

=pod twiki

---+++ webExists( $web, $topic, $fn ) => boolean
Check if the given topic exists, returning 1 if it does, or
invoking TWiki::UI::oops and returning 0 if it doesn't. $fn is
the name of the command invoked, and will be used in composing
the oops template name thus: oops${fn}notopic

=cut

sub topicExists {
  my ( $webName, $topic, $fn ) = @_;

  return 1 if TWiki::Store::topicExists( $webName, $topic );

  oops( $webName, $topic, "${fn}notopic", "ERROR $webName.$topic Missing topic" );

  return 0;
}

=pod twiki

---+++ isMirror( $web, $topic ) => boolean
Checks if this web is a mirror web, returning 0 if is isn't, or
calling TWiki::UI::oops and returning 1 if it doesn't.

=cut

sub isMirror {
  my ( $webName, $topic ) = @_;

  my( $mirrorSiteName, $mirrorViewURL ) = TWiki::readOnlyMirrorWeb( $webName );

  return 0 unless ( $mirrorSiteName );

  if ( $print ) {
    print "ERROR: this is a mirror site\n";
  } else {
    oopsRedirect( $webName, $topic, "mirror",
                  $mirrorSiteName,
                  $mirrorViewURL );
  }
  return 1;
}

=cut

=pod twiki

---+++ isAccessPermitted( $web, $topic, $mode, $user ) => boolean
Check if the given mode of access by the given user to the given
web.topic is permissible. If it is, return 1. If not, invoke an
oops and return 0.

=cut

sub isAccessPermitted {
   my ( $web, $topic, $mode, $user ) = @_;

   return 1 if TWiki::Access::checkAccessPermission( $mode, $user, "",
                                                     $topic, $web );
   oops( $webName, $topic, "access$mode" );

   return 0;
}

=pod twiki

---+++ userIsAdmin( $web, $topic, $user ) => boolean
Check if the user is an admin. If they are, return 1. If not, invoke an
oops and return 0.

=cut

sub userIsAdmin {
  my ( $webName, $topic, $user ) = @_;

  use TWiki::Access;

  return 1 if TWiki::Access::userIsInGroup( $user, $TWiki::superAdminGroup );

  oops( $webName, $topic, "accessgroup",
        "$TWiki::mainWebname.$TWiki::superAdminGroup" );

  return 0;
}

=pod twiki

---+++ writeDebugTimes( $message )
Write a debugging message indicating the time at which this function
was called. Used for benchmarking.

=cut

sub writeDebugTimes {
  my $mess = shift;
  # TWiki::writeDebug();
}

1;
