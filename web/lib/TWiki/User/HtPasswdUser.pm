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

---+ TWiki::User::HtPasswdUser Package

The HtPasswdUser module seperates out the User Authentication code that is htpasswd and htdigest
specific. 

TODO: User.pm and the impls propbably shouldn't use Store.pm - they are not TWikiTopics..

=cut

package TWiki::User::HtPasswdUser;

if( 'md5' eq $TWiki::htpasswdEncoding ) {
	require Digest::MD5;
} elsif( 'sha1' eq $TWiki::htpasswdEncoding ) {
    require MIME::Base64;
    import MIME::Base64 qw( encode_base64 );
    require Digest::SHA1;
    import Digest::SHA1 qw( sha1 );
}

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
use vars qw( %templateVars ); # init in TWiki.pm so okay for modPerl

# ======================
sub new
{
   my( $proto ) = @_;
   my $class = ref($proto) || $proto;
   my $self = {};
   bless( $self, $class );
#   $self->_init();
#   $self->{head} = 0;
   return $self;
}

# ===========================
sub writeDebug
{
#   TWiki::writeDebug( "User: $_[0]" );
}

# =========================
=pod

---+++ _htpasswdGeneratePasswd( $user, $passwd , $useOldSalt ) ==> $passwordExists
| Description: | (private) implementation method that generates an encrypted password |
| Parameter: =$user= | userName |
| Parameter: =$passwd= | unencypted password |
| Parameter: =$useOldSalt= | if $useOldSalt == 1 then we are attempting to match $passwd an existing one 
otherwise, we are just creating a new use encrypted passwd |
| Return: =$value= | returns "" on failure, an encrypted password otherwise |

=cut
sub _htpasswdGeneratePasswd
{
    my ( $user, $passwd , $useOldSalt ) = @_;

	my $encodedPassword = '';

    if( 'sha1' eq $TWiki::htpasswdEncoding ) {

        $encodedPassword = '{SHA}' . MIME::Base64::encode_base64( Digest::SHA1::sha1( $passwd ) ); 
        chomp $encodedPassword;

    } elsif ( 'crypt' eq $TWiki::htpasswdEncoding ) {
	    # by David Levy, Internet Channel, 1997
	    # found at http://world.inch.com/Scripts/htpasswd.pl.html

		my $salt;
		if ( $useOldSalt eq 1) {
		    my $currentEncryptedPasswordEntry = _htpasswdReadPasswd( $user );
	        $salt = substr( $currentEncryptedPasswordEntry, 0, 2 );
		} else {
		    srand( $$|time );
		    my @saltchars = ( 'a'..'z', 'A'..'Z', '0'..'9', '.', '/' );
		    $salt = $saltchars[ int( rand( $#saltchars+1 ) ) ];
		    $salt .= $saltchars[ int( rand( $#saltchars+1 ) ) ];
		}

		if ( ( $salt ) && (2 == length $salt) ) {
			$encodedPassword = crypt( $passwd, $salt );
		}

    } elsif ( 'md5' eq $TWiki::htpasswdEncoding ) {
#what does this do if we are using a htpasswd file?
		my $toEncode= "$user:$TWiki::authRealm:$passwd";
		$encodedPassword = Digest::MD5::md5_hex( $toEncode );

    } elsif ( 'plain' eq $TWiki::htpasswdEncoding ) {

		$encodedPassword = $passwd;

	}

    return $encodedPassword;
}

#========================= 
=pod

---+++ _htpasswdReadPasswd( $user ) ==> $encryptedPassword
| Description: | gets the encrypted password from the htpasswd / htdigest file |
| Parameter: =$user= | UserName |
| Return: =$encryptedPassword= | "" if there is none, the encrypted password otherwise |

=cut
sub _htpasswdReadPasswd
{
    my ( $user ) = @_;
 
    if( ! $user ) {
        return "";
    }
 
    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    if( $text =~ /$user\:(\S+)/ ) {
        return $1;
    }
    return "";
}
 
#========================= 
=pod

---+++ UserPasswordExists( $user ) ==> $passwordExists
| Description: | checks to see if there is a $user in the password system |
| Parameter: =$user= | the username we are looking for  |
| Return: =$passwordExists= | "1" if true, "" if not |

=cut
sub UserPasswordExists
{
    my ( $self, $user ) = @_;

    if( ! $user ) {
        return "";
    }

    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    if( $text =~ /^${user}:/gm ) {	# mod_perl: don't use /o
        return "1";
    }
    return "";
}
 
#========================= 
=pod

---+++ UpdateUserPassword( $user, $oldUserPassword, $newUserPassword ) ==> $success
| Description: | used to change the user's password |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$oldUserPassword= | unencrypted password |
| Parameter: =$newUserPassword= | unencrypted password |
| Return: =$success= | "1" if success |

=cut
# TODO: needs to fail if it doesw not succed due to file permissions
sub UpdateUserPassword
{
    my ( $self, $user, $oldUserPassword, $newUserPassword ) = @_;

    my $oldUserEntry = _htpasswdGeneratePasswd( $user, $oldUserPassword , 1);
    my $newUserEntry = _htpasswdGeneratePasswd( $user, $newUserPassword , 0);
 
    # can't use `htpasswd $wikiName` because htpasswd doesn't understand stdin
    # simply add name to file, but this is a security issue
    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    # escape + sign; SHA-passwords can have + signs
    $oldUserEntry =~ s/\+/\\\+/g;
    $text =~ s/$user:$oldUserEntry/$user:$newUserEntry/;
    &TWiki::Store::saveFile( $TWiki::htpasswdFilename, $text );

    return "1";
}

#===========================
=pod

---+++ htpasswdUpdateUser( $self, $oldEncryptedUserPassword, $newEncryptedUserPassword ) ==> $success
| Description: |  |
| Parameter: =$oldEncryptedUserPassword= | formated as in the htpasswd file user:encryptedPasswd |
| Parameter: =$newEncryptedUserPassword= | formated as in the htpasswd file user:encryptedPasswd |
| Return: =$success= |  |
| TODO: | __Needs to go away!__ |
| TODO: | we be better off generating a new password that we email to the user, and then let them change it? |
| Note: | used by the htpasswd specific installpasswd & script  |

=cut
sub htpasswdUpdateUser
{
    my ( $self, $oldEncryptedUserPassword, $newEncryptedUserPassword ) = @_;

    # can't use `htpasswd $wikiName` because htpasswd doesn't understand stdin
    # simply add name to file, but this is a security issue
    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    # escape + sign; SHA-passwords can have + signs
    $oldEncryptedUserPassword =~ s/\+/\\\+/g;
    $text =~ s/$oldEncryptedUserPassword/$newEncryptedUserPassword/;
    &TWiki::Store::saveFile( $TWiki::htpasswdFilename, $text );

    return "1";
}

#===========================
=pod

---+++ AddUserPassword( $user, $newUserPassword ) ==> $success
| Description: | creates a new user & password entry |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$newUserPassword= | unencrypted password |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |

=cut
sub AddUserPassword
{
    my ( $self, $user, $newUserPassword ) = @_;
    my $userEntry = $user.":". _htpasswdGeneratePasswd( $user, $newUserPassword , 0);

    # can't use `htpasswd $wikiName` because htpasswd doesn't understand stdin
    # simply add name to file, but this is a security issue
    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    ##TWiki::writeDebug "User entry is :$userEntry: before newline";
    $text .= "$userEntry\n";
    &TWiki::Store::saveFile( $TWiki::htpasswdFilename, $text );

	return "1";
}

#===========================
=pod

---+++ RemoveUser( $user ) ==> $success
| Description: | used to remove the user from the password system |
| Parameter: =$user= | the username we are replacing  |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |

=cut
#i'm a wimp - comment out the password entry
sub RemoveUser
{
    my ( $self, $user ) = @_;
    my $userEntry = $user.":"._htpasswdReadPasswd( $user );

    return $self->htpasswdUpdateUser( $userEntry, "#".$userEntry);
}

# =========================
=pod

---+++ CheckUserPasswd( $user, $password ) ==> $success
| Description: | used to check the user's password |
| Parameter: =$user= | the username we are replacing  |
| Parameter: =$password= | unencrypted password |
| Return: =$success= | "1" if success |
| TODO: | need to improve the error mechanism so TWikiAdmins know what failed |

=cut
sub CheckUserPasswd
{
    my ( $self, $user, $password ) = @_;
    my $currentEncryptedPasswordEntry = _htpasswdReadPasswd( $user );

    my $encryptedPassword = _htpasswdGeneratePasswd($user, $password , 1);

    # OK
    if( $encryptedPassword eq $currentEncryptedPasswordEntry ) {
        return "1";
    }
    # NO
    return "";
}
 
1;

# EOF
