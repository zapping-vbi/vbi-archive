# Module of TWiki Collaboration Platform, http://TWiki.org/
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
#
# Notes:
# - Latest version at http://twiki.org/
# - Installation instructions in $dataDir/Main/TWikiDocumentation.txt
# - Customize variables in TWiki.cfg when installing TWiki.
# - Optionally change TWiki.pm for custom extensions of rendering rules.
# - Upgrading TWiki is easy as long as you do not customize TWiki.pm.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log
#

=begin twiki

---+ TWiki::User Package

This module hosts the user authentication implementation

=cut

package TWiki::User;

#use File::Copy;
#use Time::Local;

#if( $TWiki::OS eq "WINDOWS" ) {
#    require MIME::Base64;
#    import MIME::Base64 qw( encode_base64 );
#    require Digest::SHA1;
#    import Digest::SHA1 qw( sha1 );
#}


use strict;

# 'Use locale' for internationalisation of Perl sorting in getTopicNames
# and other routines - main locale settings are done in TWiki::setupLocale
BEGIN {
    # Do a dynamic 'use locale' for this module
    if( $TWiki::useLocale ) {
        require locale;
	import locale ();
    }
}

# FIXME: Move elsewhere?
# template variable hash: (built from %TMPL:DEF{"key"}% ... %TMPL:END%)
use vars qw( %templateVars $UserImpl ); # init in TWiki.pm so okay for modPerl

$UserImpl = "";

# ===========================
=pod

---+++ initialize ()
| Description: | loads the selected User Implementation |

=cut
sub initialize
{
    %templateVars = ();
	if ( # (-e $TWiki::htpasswdFilename ) && #<<< maybe
		( $TWiki::htpasswdFormatFamily eq "htpasswd" ) ) {
	    $UserImpl = "TWiki::User::HtPasswdUser";
#	} elseif ($TWiki::htpasswdFormatFamily eq "something?") {
#	    $UserImpl = "TWiki::User::SomethingUser";
	} else {
	    $UserImpl = "TWiki::User::NoPasswdUser";
	}
	eval "use ".$UserImpl;
}

# ===========================
=pod

---++ sub _getUserHandler (  $web, $topic, $attachment  )

Not yet documented.

=cut

sub _getUserHandler
{
   my( $web, $topic, $attachment ) = @_;

   $attachment = "" if( ! $attachment );

   my $handlerName = $UserImpl;

   my $handler = $handlerName->new( );
   return $handler;
}

#========================= 
=pod

---++ UserPasswordExists( $user ) ==> $passwordExists
| Description: | checks to see if there is a $user in the password system |
| Parameter: =$user= | the username we are looking for  |
| Return: =$passwordExists= | "1" if true, "" if not |
| TODO: | what if the login name is not the same as the twikiname?? (I think we don't have TWikiName to username mapping fully worked out|

=cut

sub UserPasswordExists
{
    my ( $user ) = @_;

    my $handler = _getUserHandler();

    return $handler->UserPasswordExists($user);
}
 
#========================= 
=pod

---++ UpdateUserPassword( $user, $oldUserPassword, $newUserPassword ) ==> $success
| Description: | used to change the user's password |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$oldUserPassword= | unencrypted password |
| Parameter: =$newUserPassword= | unencrypted password |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |
| Notes: | always return failure if the $user is AnonymousContributor |
| | this is to stop hyjacking of DeletedUser's content |

=cut

sub UpdateUserPassword
{
    my ( $user, $oldUserPassword, $newUserPassword ) = @_;

	if ( $user =~ /AnonymousContributor/ ) {
		return;
	}

    my $handler = _getUserHandler();
    return $handler->UpdateUserPassword($user, $oldUserPassword, $newUserPassword);
}

#========================= 
=pod

---++ AddUserPassword( $user, $newUserPassword ) ==> $success
| Description: | creates a new user & password entry |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$newUserPassword= | unencrypted password |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |
| Notes: | always return failure if the $user is AnonymousContributor |
| | this is to stop hyjacking of DeletedUser's content |

=cut

sub AddUserPassword
{
    my ( $user, $newUserPassword ) = @_;

	if ( $user =~ /AnonymousContributor/ ) {
		return;
	}

    my $handler = _getUserHandler();
    return $handler->AddUserPassword($user, $newUserPassword);
}

#========================= 
=pod

---++ RemoveUser( $user ) ==> $success
| Description: | used to remove the user from the password system |
| Parameter: =$user= | the username we are replacing  |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |

=cut

sub RemoveUser
{
    my ( $user ) = @_;

    my $handler = _getUserHandler();
    return $handler->RemoveUser($user);
}

# =========================
=pod

---++ CheckUserPasswd( $user, $password ) ==> $success
| Description: | used to check the user's password |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$password= | unencrypted password |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |

=cut

sub CheckUserPasswd
{
    my ( $user, $password ) = @_;

    my $handler = _getUserHandler();
    return $handler->CheckUserPasswd($user, $password);
}
 
# =========================
=pod

---++ addUserToTWikiUsersTopic( $wikiName, $remoteUser ) ==> $topicName
| Description: | create the User's TWikiTopic |
| Parameter: =$wikiName= | the user's TWikiName |
| Parameter: =$remoteUser= | the remote username (is this used in the password file at any time?) |
| Return: =$topicName= | the name of the TWikiTopic created |
| TODO: | does this really belong here? |

=cut

sub addUserToTWikiUsersTopic
{
    my ( $wikiName, $remoteUser ) = @_;
    my $today = &TWiki::formatTime(time(), "\$day \$mon \$year", "gmtime");
    my $topicName = $TWiki::wikiUsersTopicname;
    my( $meta, $text )  = &TWiki::Store::readTopic( $TWiki::mainWebname, $topicName );
    my $result = "";
    my $status = "0";
    my $line = "";
    my $name = "";
    my $isList = "";
    # add name alphabetically to list
    foreach( split( /\n/, $text) ) {
        $line = $_;
	# TODO: I18N fix here once basic auth problem with 8-bit user names is
	# solved
        $isList = ( $line =~ /^\t\*\s[A-Z][a-zA-Z0-9]*\s\-/go );
        if( ( $status == "0" ) && ( $isList ) ) {
            $status = "1";
        }
        if( $status == "1" ) {
            if( $isList ) {
                $name = $line;
                $name =~ s/(\t\*\s)([A-Z][a-zA-Z0-9]*)\s\-.*/$2/go;            
                if( $wikiName eq $name ) {
                    # name is already there, do nothing
                    return $topicName;
                } elsif( $wikiName lt $name ) {
                    # found alphabetical position
                    if( $remoteUser ) {
                        $result .= "\t* $wikiName - $remoteUser - $today\n";
                    } else {
                        $result .= "\t* $wikiName - $today\n";
                    }
                    $status = "2";
                }
            } else {
                # is last entry
                if( $remoteUser ) {
                    $result .= "\t* $wikiName - $remoteUser - $today\n";
                } else {
                    $result .= "\t* $wikiName - $today\n";
                }
                $status = "2";
            }
        }

        $result .= "$line\n";
    }
    &TWiki::Store::saveTopic( $TWiki::mainWebname, $topicName, $result, $meta, "", 1 );
    return $topicName;
}



1;

# EOF
