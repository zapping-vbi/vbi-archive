# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2003 Peter Thoeny, peter@thoeny.com
# Copyright (C) 2002 John Talintyre, john.talintyre@btinternet.com
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
#
# Wrapper around the RCS commands required by TWiki

=begin twiki

---+ TWiki::Store::RcsWrap Module

This module calls rcs

=cut

package TWiki::Store::RcsWrap;

use File::Copy;
use TWiki::Store::RcsFile;
@ISA = qw(TWiki::Store::RcsFile);

use strict;

## Details of settings
#
# attachAsciiPath         Defines which attachments will be treated as ASCII in RCS
# initBinaryCmd           RCS init command, needed when initialising a file as binary
# ciCmd                   RCS check in command
# coCmd                   RCS check out command
# histCmd                 RCS history command
# infoCmd                 RCS history on revision command
# diffCmd                 RCS revision diff command
# breakLockCmd            RCS for breaking a lock
# ciDateCmd               RCS check in command with date
# delRevCmd               RCS delete revision command
# unlockCmd               RCS unlock command
# lockCmd                 RCS lock command
# tagCmd                  RCS tag command
#
# (from RcsFile)
# dataDir
# pubDir
# attachAsciiPath         Defines which attachments will be automatically treated as ASCII in RCS
# dirPermission           File security for new directories

# ======================
=pod

---++ sub new (  $proto, $web, $topic, $attachment, %settings  )

Not yet documented.

=cut to implementation

sub new
{
   my( $proto, $web, $topic, $attachment, %settings ) = @_;
   my $class = ref($proto) || $proto;
   my $self = TWiki::Store::RcsFile->new( $web, $topic, $attachment, %settings );
   bless( $self, $class );
   $self->_settings( %settings );
   $self->_init();
   return $self;
}

# ======================
=pod

---++ sub _settings (  $self, %settings  )

Not yet documented.

=cut to implementation

sub _settings
{
    my( $self, %settings ) = @_;
    $self->{initBinaryCmd} = $settings{initBinaryCmd};
    $self->{tmpBinaryCmd}  = $settings{tmpBinaryCmd};
    $self->{ciCmd}        = $settings{ciCmd};
    $self->{coCmd}        = $settings{coCmd};
    $self->{histCmd}      = $settings{histCmd};
    $self->{infoCmd}      = $settings{infoCmd};
    $self->{diffCmd}      = $settings{diffCmd};
    $self->{breakLockCmd} = $settings{breakLockCmd};
    $self->{ciDateCmd}    = $settings{ciDateCmd};
    $self->{delRevCmd}    = $settings{delRevCmd};
    $self->{unlockCmd}    = $settings{unlockCmd};
    $self->{lockCmd}      = $settings{lockCmd};
    $self->{tagCmd}      = $settings{tagCmd};
}

#TODO set from TWiki.cfg
my $cmdQuote = "'";

# ======================
=pod

---++ sub _trace ()

Not yet documented.

=cut to implementation

sub _trace
{
   #my( $text ) = @_;
   #print $text;
}

# ======================
=pod

---++ sub _traceExec ()

Not yet documented.

=cut to implementation

sub _traceExec
{
   #my( $cmd, $string, $exit ) = @_;
   #if( $exit ) {
   #    $exit = " Error: $exit";
   #} else {
   #    $exit = "";
   #}
   #TWiki::writeDebug( "Rcs: $cmd($exit): $string\n" );
}


# ======================
# Returns false if okay, otherwise an error string
=pod

---++ sub _binaryChange (  $self  )

Not yet documented.

=cut to implementation

sub _binaryChange
{
    my( $self ) = @_;
    if( $self->getBinary() ) {
        # Can only do something when changing to binary
        my $cmd = $self->{"initBinaryCmd"};
        my $file = $self->file();
        $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote/go;
        $cmd =~ /(.*)/;
        $cmd = "$1";       # safe, so untaint variable
        my $rcsOutput = `$cmd`;
        my $exit = $? >> 8;
        _traceExec( $cmd, $rcsOutput, $exit );
        if( $exit && $rcsOutput ) {
           $rcsOutput = "$cmd\n$rcsOutput";
           return $rcsOutput;
        }

        # Sometimes (on Windows?) rcs file not formed, so check for it
        if( ! -e $file ) {
           return "$cmd\nFailed to create history file $file";
        }
    }
    return "";
}

# ======================
=pod

---++ sub addRevision (  $self, $text, $comment, $userName  )

Not yet documented.

=cut to implementation

sub addRevision
{
    my( $self, $text, $comment, $userName ) = @_;
    
    # Replace file (if exists) with text
    $self->_save( $self->file(), \$text );
    return $self->_ci( $self->file(), $comment, $userName );
}

# ======================
=pod

---++ sub replaceRevision (  $self, $text, $comment, $user, $date  )

Not yet documented.
# Replace the top revision
# Return non empty string with error message if there is a problem
| $date | is on epoch seconds |

=cut to implementation

sub replaceRevision
{
    my( $self, $text, $comment, $user, $date ) = @_;
    
    my $rev = $self->numRevisions();
    my $file    = $self->{file};
    my $rcsFile = $self->{rcsFile};
    my $cmd;
    my $rcsOut;
    
    # update repository with same userName and date
    if( $rev == 1 ) {
        # initial revision, so delete repository file and start again
        unlink $rcsFile;
    } else {
        $self->_deleteRevision( $rev );
    }
    $self->_saveFile( $self->file(), $text );
    $cmd = $self->{ciDateCmd};
	$date = TWiki::formatTime( $date , "\$rcs", "gmtime");
    $cmd =~ s/%DATE%/$date/;
    $cmd =~ s/%USERNAME%/$user/;
    $file =~ s/$TWiki::securityFilter//go;
    $rcsFile =~ s/$TWiki::securityFilter//go;
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote $cmdQuote$rcsFile$cmdQuote/;
    $cmd =~ /(.*)/;
    $cmd = $1;       # safe, so untaint variable
    $rcsOut = `$cmd`;
    my $exit = $? >> 8;
    _traceExec( $cmd, $rcsOut, $exit );
    #$rcsOut =~ s/^Warning\: missing newline.*//os; # forget warning
    if( $exit ) {
        $rcsOut = "$cmd\n$rcsOut";
        return $rcsOut;
    }
    return "";
}

# ======================
# Return with empty string if only one revision
=pod

---++ sub deleteRevision (  $self  )

Not yet documented.

=cut to implementation

sub deleteRevision
{
    my( $self ) = @_;
    my $rev = $self->numRevisions();
    return "" if( $rev == 1 );
    return $self->_deleteRevision( $rev );
}

# ======================
=pod

---++ sub _deleteRevision (  $self, $rev  )

Not yet documented.

=cut to implementation

sub _deleteRevision
{
    my( $self, $rev ) = @_;
    
    # delete latest revision (unlock, delete revision, lock)
    my $file    = $self->{file};
    my $rcsFile = $self->{rcsFile};
    my $cmd= $self->{unlockCmd};
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote $cmdQuote$rcsFile$cmdQuote/go;
    $cmd =~ /(.*)/;
    $cmd = $1;       # safe, so untaint
    my $rcsOut = `$cmd`; # capture stderr
    my $exit = $? >> 8;
    _traceExec( $cmd, $rcsOut, $exit );
    #$rcsOut =~ s/^Warning\: missing newline.*//os; # forget warning
    if( $exit ) {
        $rcsOut = "$cmd\n$rcsOut";
        return $rcsOut;
    }
    $cmd= $self->{delRevCmd};
    $cmd =~ s/%REVISION%/1.$rev/go;
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote $cmdQuote$rcsFile$cmdQuote/go;
    $cmd =~ /(.*)/;
    $cmd = $1;       # safe, so untaint variable
    $rcsOut = `$cmd`;
    $exit = $? >> 8;
    _traceExec( $cmd, $rcsOut, $exit );
    #$rcsOut =~ s/^Warning\: missing newline.*//os; # forget warning
    if( $exit ) {
        $rcsOut = "$cmd\n$rcsOut";
        return $rcsOut;
    }
    $cmd= $self->{lockCmd};
    $cmd =~ s/%REVISION%/$rev/go;
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote $cmdQuote$rcsFile$cmdQuote/go;
    $cmd =~ /(.*)/;
    $cmd = $1;       # safe, so untaint variable
    $rcsOut = `$cmd`;
    _traceExec( $cmd, $rcsOut, $exit );
    #$rcsOut =~ s/^Warning\: missing newline.*//os; # forget warning
    if( $exit ) {
        $rcsOut = "$cmd\n$rcsOut";
        return $rcsOut;
    }
}

# ======================
=pod

---++ sub getRevision (  $self, $version  )

Not yet documented.

=cut to implementation

sub getRevision
{
    my( $self, $version ) = @_;

    my $tmpfile = "";
    my $tmpRevFile = "";
    my $cmd = $self->{"coCmd"};
    my $file = $self->file();
    if( $TWiki::OS eq "WINDOWS" ) {
        # Need to take temporary copy of topic, check it out to file, then read that
        # Need to put RCS into binary mode to avoid extra \r appearing and
        # read from binmode file rather than stdout to avoid early file read termination
        $tmpfile = $self->_mkTmpFilename();
        $tmpRevFile = "$tmpfile,v";
        copy( $self->rcsFile(), $tmpRevFile );
        my $cmd1 = $self->{tmpBinaryCmd};
        $cmd1 =~ s/%FILENAME%/$cmdQuote$tmpRevFile$cmdQuote/;
        $cmd1 =~ /(.*)/;
        $cmd1 = "$1";
        my $tmp = `$cmd1`;
        _traceExec( $cmd1, $tmp );
        $file = $tmpfile;
        $cmd =~ s/-p%REVISION%/-r%REVISION%/;
    }    
    $cmd =~ s/%REVISION%/1.$version/;
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote/;
    $cmd =~ /(.*)/;
    $cmd = "$1"; # untaint
    my $text = `$cmd`;
    if( $tmpfile ) {
        $text = $self->_readFile( $tmpfile );
        $tmpfile =~ /(.*)/;
        $tmpfile = "$1"; # untaint		
        unlink $tmpfile;
        $tmpRevFile =~ /(.*)/;
        $tmpRevFile = "$1"; # untaint		
        unlink $tmpRevFile;
    }
    _traceExec( $cmd, $text );
    return $text;
}

# ======================
=pod

---++ sub numRevisions (  $self  )

Not yet documented.

=cut to implementation

sub numRevisions
{
    my( $self ) = @_;
    my $cmd= $self->{"histCmd"};
    my $rcsFile = $self->rcsFile();
    if( ! -e $rcsFile ) {
       return "";
    }

    $cmd =~ s/%FILENAME%/$cmdQuote$rcsFile$cmdQuote/;
    $cmd =~ /(.*)/;
    $cmd = $1;       # now safe, so untaint variable
    my $rcsOutput = `$cmd`;
    _traceExec( $cmd, $rcsOutput );
    if( $rcsOutput =~ /head:\s+\d+\.(\d+)\n/ ) {
        return $1;
    } else {
        return ""; # Note this hides possible errors
    }
}

# ======================
=pod

---++ sub getRevisionInfo (  $self, $version  )

| FIXME | there is an inconguity here. if you ask for a revisino that does not exist, getRevisionInfo gives you 1.1, but readTopic gives you the last version |
Not yet documented.
# Date return in epoch seconds
# If revision file is missing, information based on actual file is returned.

=cut to implementation

sub getRevisionInfo
{
    my( $self, $version ) = @_;
    
    if( ! $version ) {
        # PTh 03 Nov 2000: comment out for performance
        ### $theRev = getRevisionNumber( $theTopic, $theWebName );
        $version = "";  # do a "rlog -r filename" to get top revision info
    } else {
		if ( $version =~ /^\d/ ) 
		{
			#if we are asking for a minor nmber, re-constitue it to Major.minor
			$version = "1.$version";
		}
    }
    
    my $rcsFile = $self->{rcsFile};
    my $rcsError = "";
    my( $dummy, $rev, $date, $user, $comment );
    if ( -e $rcsFile ) {
       my $cmd= $self->{infoCmd};
       $cmd =~ s/%REVISION%/$version/;
       $cmd =~ s/%FILENAME%/$cmdQuote$rcsFile$cmdQuote/;
       $cmd =~ /(.*)/; $cmd = $1;       # Untaint
       my $rcsOut = `$cmd`;
       my $exit = $? >> 8;
       _traceExec( $cmd, $cmd, $exit );
       $rcsError = "Error with $cmd, output: $rcsOut" if( $exit );
       if( ! $rcsError ) {
            $rcsOut =~ /date: (.*?);  author: (.*?);.*\n(.*)\n/;
            $date = $1 || "";
            $user = $2 || "";
            $comment = $3 || "";
            $date = TWiki::Store::RcsFile::_rcsDateTimeToEpoch( $date );
            $rcsOut =~ /revision 1.([0-9]*)/;
            $rev = $1 || "";
            $rcsError = "Rev missing from revision file $rcsFile" if( ! $rev );
       }
    } else {
       $rcsError = "Revision file $rcsFile is missing";
    }
    
    ( $dummy, $rev, $date, $user, $comment ) = $self->_getRevisionInfoDefault() if( $rcsError );
    
    return( $rcsError, $rev, $date, $user, $comment );
}

# ======================
# rev2 newer than rev1
=pod

---++ sub revisionDiff (  $self, $rev1, $rev2, $contextLines  )

Not yet documented.
| Return: =\@diffArray= | reference to an array of [ diffType, $right, $left ] |

=cut to implementation

sub revisionDiff
{
    my( $self, $rev1, $rev2, $contextLines ) = @_;
    
    my $error = "";

    my $tmp= "";
    if ( $rev1 eq "1" && $rev2 eq "1" ) {
        my $text = $self->getRevision(1);
        $tmp = "1a1\n";
        foreach( split( /\n/, $text ) ) {
           $tmp = "$tmp> $_\n";
        }
    } else {
        $tmp= $self->{"diffCmd"};
        $tmp =~ s/%REVISION1%/1.$rev1/;
        $tmp =~ s/%REVISION2%/1.$rev2/;
        my $rcsFile = $self->rcsFile();
        $rcsFile =~ s/$TWiki::securityFilter//go;
        $tmp =~ s/%FILENAME%/$cmdQuote$rcsFile$cmdQuote/;
        $tmp =~ s/%CONTEXT%/$contextLines/;
        $tmp =~ /(.*)/;
        my $cmd = $1;       # now safe, so untaint variable
        $tmp = `$cmd`;
        my $exit = $? >> 8;
        $error = "Error $exit when runing $cmd";
        _traceExec( $cmd, $tmp, $exit );       
        _trace( "and now $tmp" );
        # Avoid showing change in revision number!
        # I'm not too happy with this implementation, I think it may be better to filter before sending to diff command,
        # possibly using Algorithm::Diff from CPAN.
#removed as it causes erronious changes - and it _has_ to be done in Store.pm so it works for rsclite too
#        $tmp =~ s/[0-9]+c[0-9]+\n[<>]\s*%META:TOPICINFO{[^}]*}%\s*\n---\n[+-<>]\s*%META:TOPICINFO{[^}]*}%\s*n//go;
#        $tmp =~ s/[+-<>]\s*%META:TOPICINFO{[^}]*}%\s*//go;
    }
	
    return ($error, parseRevisionDiff( $tmp ) );
}

# =========================
=pod

---+++ parseRevisionDiff( $text ) ==> \@diffArray

| Description: | parse the text into an array of diff cells |
| #Description: | unlike Algorithm::Diff I concatinate lines of the same diffType that are sqential (this might be something that should be left up to the renderer) |
| Parameter: =$text= | currently unified or rcsdiff format |
| Return: =\@diffArray= | reference to an array of [ diffType, $right, $left ] |
| TODO: | move into RcsFile and add indirection in Store |

=cut
# -------------------------
sub parseRevisionDiff
{
    my( $text ) = @_;

    my ( $diffFormat ) = "normal"; #or rcs, unified...
    my ( @diffArray );

    $diffFormat = "unified" if ( $text =~ /^---/ );

    $text =~ s/\r//go;  # cut CR

    my $lineNumber=1;
    if ( $diffFormat eq "unified" ) {
        foreach( split( /\n/, $text ) ) {
	    if ( $lineNumber > 3 ) {   #skip the first 2 lines (filenames)
 	   	    if ( /@@ [-+]([0-9]+)([,0-9]+)? [-+]([0-9]+)(,[0-9]+)? @@/ ) {
	    	        #line number
		        push @diffArray, ["l", $1, $3];
		    } elsif ( /^\-/ ) {
		        s/^\-//go;
		        push @diffArray, ["-", $_, ""];
		    } elsif ( /^\+/ ) {
		        s/^\+//go;
		        push @diffArray, ["+", "", $_];
		    } else {
	  		s/^ (.*)$/$1/go;
			push @diffArray, ["u", $_, $_];
		    }
	    }
	    $lineNumber = $lineNumber + 1;
       	 }
    } else {
        #"normal" rcsdiff output 
        foreach( split( /\n/, $text ) ) {
    	    if ( /^([0-9]+)[0-9\,]*([acd])([0-9]+)/ ) {
    	        #line number
	        push @diffArray, ["l", $1, $3];
	    } elsif ( /^</ ) {
	        s/^< //go;
	            push @diffArray, ["-", $_, ""];
	    } elsif ( /^>/ ) {
	        s/^> //go;
	            push @diffArray, ["+", "", $_];
	    } else {
	        #empty lines and the --- selerator in the diff
	        #push @diffArray, ["u", "$_", $_];
	    }
        }
    }
    return \@diffArray;
}

# ======================
=pod

---++ sub _ci (  $self, $file, $comment, $userName  )

Not yet documented.

=cut to implementation

sub _ci
{
    my( $self, $file, $comment, $userName ) = @_;

    # Check that we can write the file being checked in. This won't check that
    # $file,v is writable, but it _will_ trap 99% of all common errors (permissions
    # on directory tree)
    return "$file is not writable" unless ( -w $file );

    my $cmd = $self->{"ciCmd"};
    my $rcsOutput = "";
    $cmd =~ s/%USERNAME%/$userName/;
    $file =~ s/$TWiki::securityFilter//go;
    $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote/;
    $comment = "none" unless( $comment );
    $comment =~ s/[\"\'\`\;]//go;  # security, Codev.NoShellCharacterEscapingInFileAttachComment, MikeSmith
    $cmd =~ s/%COMMENT%/$comment/;
    $cmd =~ /(.*)/;
    $cmd = $1;       # safe, so untaint variable
    $rcsOutput = `$cmd`; # capture stderr  (S.Knutson)
    my $exit = $? >> 8;
    _traceExec( $cmd, $rcsOutput );
    if( $exit && $rcsOutput =~ /no lock set by/ ) {
          # Try and break lock, setting new one and doing ci again
          my $cmd = $self->{"breakLockCmd"};
          $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote/go;
          $cmd =~ /(.*)/;
          my $out = `$cmd`;
          _traceExec( $cmd, $out );
          # Assume it worked, as not sure how to trap failure
          $rcsOutput = `$cmd`; # capture stderr  (S.Knutson)
          $exit = $? >> 8;
          _traceExec( $cmd, $rcsOutput );
          if( ! $exit ) {
              $rcsOutput = "";
          }
    }
    if( $exit && $rcsOutput ) { # oops, stderr was not empty, return error
        $rcsOutput = "$cmd\n$rcsOutput";
    }
    return $rcsOutput;
}

=pod

---+++ setTopicRevisionTag( $web, $topic, $rev, $tag ) ==> $success

| Description: | sets a names tag on the specified revision |
| Parameter: =$web= | webname |
| Parameter: =$topic= | topic name |
| Parameter: =$rev= | the revision we are taging |
| Parameter: =$tag= | the string to tag with |
| Return: =$success= |  |
| TODO: | we _need_ an error mechanism! |
| TODO: | NEED to check if the version exists (rcs does not) |
| Since: | TWiki:: (20 April 2004) |

=cut

sub setTopicRevisionTag
{
	my ( $self,  $web, $topic, $rev, $tag ) = @_;

    my $file = $self->{file};
    if ( -e $file ) {
       my $cmd= $self->{tagCmd};
       $cmd =~ s/%REVISION%/$rev/;
       $cmd =~ s/%FILENAME%/$cmdQuote$file$cmdQuote/;
       $cmd =~ s/%TAG%/$tag/;
	   $cmd = $cmd."  2>> $TWiki::warningFilename";
       $cmd =~ /(.*)/; $cmd = $1;       # Untaint
       my $rcsOut = `$cmd`;
       my $exit = $? >> 8;
       _traceExec( $cmd, $cmd, $exit );
		if( $exit && $rcsOut ) { # oops, stderr was not empty, return error
			$rcsOut = "$cmd\n$$rcsOut";
			TWiki:writeDebug("RCSWrap::setTopicRevisionTag error - $rcsOut");
			return;
		}
   }
	   
	return 1;#success 
}

1;
