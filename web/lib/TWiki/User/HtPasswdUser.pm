# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2003 Peter Thoeny, peter@thoeny.com
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


#TODO: User.pm and the impls propbably shouldn't use Store.pm - they are not TWikiTopics..
package TWiki::User::HtPasswdUser;

#use File::Copy;
#use Time::Local;

if( $TWiki::OS eq "WINDOWS" ) {
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
#   my $self = TWiki::User::RcsFile->new( $web, $topic, $attachment, %settings );
   my $self = {};
   bless( $self, $class );
#   $self->_init();
#   $self->{head} = 0;
   return $self;
}


# ===========================
# Normally writes no output, uncomment writeDebug line to get output of all RCS etc command to debug file
sub _traceExec
{
   #my( $cmd, $result ) = @_;
   #TWiki::writeDebug( "User exec: $cmd -> $result" );
}

# ===========================
sub writeDebug
{
#   TWiki::writeDebug( "User: $_[0]" );
}

# =========================
sub _htpasswdGeneratePasswd
{
    my ( $user, $passwd ) = @_;
    # by David Levy, Internet Channel, 1997
    # found at http://world.inch.com/Scripts/htpasswd.pl.html

    # check for Windows and use SHA1 digest instead of crypt()
    if( $TWiki::OS eq "WINDOWS" ) {
        my $pwd = $user . ':{SHA}' . MIME::Base64::encode_base64( Digest::SHA1::sha1( $passwd ) ); 
        chomp $pwd;
        return $pwd
    }
    srand( $$|time );
    my @saltchars = ( 'a'..'z', 'A'..'Z', '0'..'9', '.', '/' );
    my $salt = $saltchars[ int( rand( $#saltchars+1 ) ) ];
    $salt .= $saltchars[ int( rand( $#saltchars+1 ) ) ];
    my $passwdcrypt = crypt( $passwd, $salt );
    return "$passwdcrypt";
}

#==========================
#use the same salt as the password in the htpasswd file
sub _htpasswdEncryptSameSalt
{
    my ( $user, $password ) = @_;

    my $currentEncryptedPasswordEntry = _htpasswdReadPasswd( $user );
    my $encryptedPassword;

    # check for Windows
    if ( $TWiki::OS eq "WINDOWS" ) {
        $encryptedPassword = '{SHA}' . MIME::Base64::encode_base64( Digest::SHA1::sha1( $password ) );
        # strip whitespace at end of line
        $encryptedPassword =~ /(.*)$/ ;
        $encryptedPassword = $1;

    } else {
        my $salt = substr( $currentEncryptedPasswordEntry, 0, 2 );
        $encryptedPassword = crypt( $password, $salt );
    }
    return $encryptedPassword;
}
 
#========================= 
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
# TODO: needs to fail if it doesw not succed due to file permissions
sub UpdateUserPassword
{
    my ( $self, $user, $oldUserPassword, $newUserPassword ) = @_;

    my $oldUserEntry = _htpasswdEncryptSameSalt( $user, $oldUserPassword );
    my $newUserEntry = _htpasswdGeneratePasswd( $user, $newUserPassword );
 
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
# only used by the htpasswd specific installpasswd script 
# (the 2 parameters have the format as in the htpasswd file user:password)
#(do we use it that much?)
#would we be better off generating a new password that we email to the user, and then let them change it?
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


sub AddUserPassword
{
    my ( $self, $user, $newUserPassword ) = @_;
    my $userEntry = $user.":". _htpasswdGeneratePasswd( $user, $newUserPassword );

    # can't use `htpasswd $wikiName` because htpasswd doesn't understand stdin
    # simply add name to file, but this is a security issue
    my $text = &TWiki::Store::readFile( $TWiki::htpasswdFilename );
    ##TWiki::writeDebug "User entry is :$userEntry: before newline";
    $text .= "$userEntry\n";
    &TWiki::Store::saveFile( $TWiki::htpasswdFilename, $text );
}

# =========================
sub CheckUserPasswd
{
    my ( $self, $user, $password ) = @_;
    my $currentEncryptedPasswordEntry = _htpasswdReadPasswd( $user );

    my $encryptedPassword = _htpasswdEncryptSameSalt($user, $password );

    # OK
    if( $encryptedPassword eq $currentEncryptedPasswordEntry ) {
        return "1";
    }
    # NO
    return "";
}
 
 # =========================
sub addUserToTWikiUsersTopic
{
    my ($self,  $wikiName, $remoteUser ) = @_;
    my $today = &TWiki::getGmDate();
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
