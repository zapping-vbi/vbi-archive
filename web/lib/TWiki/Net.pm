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
# - Installation instructions in $dataDir/TWiki/TWikiDocumentation.txt
# - Customize variables in TWiki.cfg when installing TWiki.
#
#  14-02-2001 - Nicholas Lee

=begin twiki

---+ TWiki::Net Module

This module handles network related functions like http access and
send mail.

=cut

package TWiki::Net;

use strict;

use vars qw(
        $useNetSmtp
        $mailInitialized $mailHost $helloHost
    );

BEGIN {
    $useNetSmtp = 0;
    $mailInitialized = 0;
}

# =========================
=pod

---++ sub getUrl (  $theHost, $thePort, $theUrl, $theUser, $thePass, $theHeader  )

Not yet documented.

=cut

sub getUrl
{
    my ( $theHost, $thePort, $theUrl, $theUser, $thePass, $theHeader ) = @_;

    # Run-time use of Socket module when needed
    require Socket;
    import Socket qw(:all);

    if( $thePort < 1 ) {
        $thePort = 80;
    }
    my $base64;
    my $result = '';
    $theUrl = "/" unless( $theUrl );
    my $req = "GET $theUrl HTTP/1.0\r\n";

    # RNF 22 Jan 2002 Support for vhosts and user authentication.
    $req .= "Host: $theHost\r\n";
    if( $theUser && $thePass ) {
	# Use MIME::Base64 at run-time if using outbound proxy with 
	# authentication
	require MIME::Base64;
	import MIME::Base64 ();
        $base64 = encode_base64( "$theUser:$thePass", "\r\n" );
        $req .= "Authorization: Basic $base64";
    }

    # RNF 19 Apr 2002 Support for outbound proxies.
    my $proxyHost = &TWiki::Prefs::getPreferencesValue("PROXYHOST");
    my $proxyPort = &TWiki::Prefs::getPreferencesValue("PROXYPORT");
    if($proxyHost && $proxyPort) {
        $req = "GET http://$theHost$theUrl HTTP/1.0\r\n";
        $theHost = $proxyHost;
        $thePort = $proxyPort;
    }

    $req .= $theHeader if( $theHeader );
    $req .= "\r\n\r\n";

    my ( $iaddr, $paddr, $proto );
    $iaddr   = inet_aton( $theHost );
    $paddr   = sockaddr_in( $thePort, $iaddr );
    $proto   = getprotobyname( 'tcp' );
    unless( socket( *SOCK, &PF_INET, &SOCK_STREAM, $proto ) ) {
        &TWiki::writeWarning( "TWiki::Net::getUrl socket: $!" );
        return "content-type: text/plain\n\nERROR: TWiki::Net::getUrl socket: $!.";
    }
    unless( connect( *SOCK, $paddr ) ) {
        &TWiki::writeWarning( "TWiki::Net::getUrl connect: $!" );
        return "content-type: text/plain\n\nERROR: TWiki::Net::getUrl connect: $!. \n$req";
    }
    select SOCK; $| = 1;
    print SOCK $req;
    while( <SOCK> ) { $result .= $_; }
    unless( close( SOCK ) ) {
        &TWiki::writeWarning( "TWiki::Net::getUrl close: $!" );
    }
    select STDOUT;
    return $result;
}

# =========================
=pod

---++ sub sendEmail (  $theText  )

Not yet documented.

=cut

sub sendEmail
{
    # $theText Format: "Date: ...\nFrom: ...\nTo: ...\nCC: ...\nSubject: ...\n\nMailBody..."

    my( $theText ) = @_;

    # Put in a Date header, mainly for Qmail
    my $dateStr = &TWiki::formatTime(time, 'email');
    $theText = "Date: " . $dateStr . "\n" . $theText;

    # Check if Net::SMTP is available
    if( ! $mailInitialized ) {
        $mailInitialized = 1;
        $mailHost  = &TWiki::Prefs::getPreferencesValue( "SMTPMAILHOST" );
        $helloHost = &TWiki::Prefs::getPreferencesValue( "SMTPSENDERHOST" );
        if( $mailHost ) {
	   eval {	# May fail if Net::SMTP not installed
	       $useNetSmtp = require Net::SMTP;
	   }
        }
    }

    my $error = "";
    # Send the email.  Use Net::SMTP if it's installed, otherwise use a
    # sendmail type program.
    if( $useNetSmtp ) {
        my ( $header, $body ) = split( "\n\n", $theText, 2 );
        my @headerlines = split( /\n/, $header );
        $header =~ s/\nBCC\:[^\n]*//os;  #remove BCC line from header
        $header =~ s/([\n\r])(From|To|CC|BCC)(\:\s*)([^\n\r]*)/$1 . $2 . $3 . _fixLineLength( $4 )/geois;
        $theText = "$header\n\n$body";   # rebuild message

        # extract 'From:'
        my $from = "";
        my @arr = grep( /^From: /i, @headerlines );
        if( scalar( @arr ) ) {
            $from = $arr[0];
            $from =~ s/^From:\s*//io;
        }
        if( ! ( $from ) ) {
            return "ERROR: Can't send mail, missing 'From:'";
        }

        # extract @to from 'To:', 'CC:', 'BCC:'
        my @to = ();
        @arr = grep( /^To: /i, @headerlines );
        my $tmp = "";
        if( scalar( @arr ) ) {
            $tmp = $arr[0];
            $tmp =~ s/^To:\s*//io;
            @arr = split( /[,\s]+/, $tmp );
            push( @to, @arr );
        }
        @arr = grep( /^CC: /i, @headerlines );
        if( scalar( @arr ) ) {
            $tmp = $arr[0];
            $tmp =~ s/^CC:\s*//io;
            @arr = split( /[,\s]+/, $tmp );
            push( @to, @arr );
        }
        @arr = grep( /^BCC: /i, @headerlines );
        if( scalar( @arr ) ) {
            $tmp = $arr[0];
            $tmp =~ s/^BCC:\s*//io;
            @arr = split( /[,\s]+/, $tmp );
            push( @to, @arr );
        }

        if( ! ( scalar( @to ) ) ) {
            return "ERROR: Can't send mail, missing receipient";
        }

        $error = _sendEmailByNetSMTP( $from, \@to, $theText );

    } else {
        # send with sendmail
        my ( $header, $body ) = split( "\n\n", $theText, 2 );
        $header =~ s/([\n\r])(From|To|CC|BCC)(\:\s*)([^\n\r]*)/$1 . $2 . $3 . _fixLineLength( $4 )/geois;
        $theText = "$header\n\n$body";   # rebuild message
        $error = _sendEmailBySendmail( $theText );
    }
    return $error;
}

# =========================
=pod

---++ sub _fixLineLength (  $theAddrs  )

Not yet documented.

=cut

sub _fixLineLength
{
    my( $theAddrs ) = @_;
    # split up header lines that are too long
    $theAddrs =~ s/(.{60}[^,]*,\s*)/$1\n        /go;
    $theAddrs =~ s/\n\s*$//gos;
    return $theAddrs;
}

# =========================
=pod

---++ sub _sendEmailBySendmail (  $theText  )

Not yet documented.

=cut

sub _sendEmailBySendmail
{
    my( $theText ) = @_;

    if( open( MAIL, "|-" ) || exec "$TWiki::mailProgram" ) {
        print MAIL $theText;
        close( MAIL );
        return "";
    }
    return "ERROR: Can't send mail using TWiki::mailProgram";
}

# =========================
=pod

---++ sub _sendEmailByNetSMTP (  $from, $toref, $data  )

Not yet documented.

=cut

sub _sendEmailByNetSMTP
{
    my( $from, $toref, $data ) = @_;

    my @to;
    # $to is not a reference then it must be a single email address
    @to = ($toref) unless ref( $toref ); 
    if ( ref( $toref ) =~ /ARRAY/ ) {
	@to = @{$toref};
    }
    return undef unless( scalar @to );
    
    my $smtp = 0;
    if( $helloHost ) {
        $smtp = Net::SMTP->new( $mailHost, Hello => $helloHost );
    } else {
        $smtp = Net::SMTP->new( $mailHost );
    }
    my $status = "";
    if ($smtp) {
        {
            $smtp->mail( $from ) or last;
            $smtp->to( @to, { SkipBad => 1 } ) or last;
            $smtp->data( $data ) or last;
            $smtp->dataend() or last;
	}
	$status = ($smtp->ok() ? "" : "ERROR: Can't send mail using Net::SMTP. " . $smtp->message );
	$smtp->quit();

    } else {
	$status = "ERROR: Can't send mail using Net::SMTP (can't connect to '$mailHost')";
    }
    return $status;    
}

# =========================

1;

# EOF
