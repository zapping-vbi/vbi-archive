# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2002 John Talintyre, john.talintyre@btinternet.com
# Copyright (C) 2002-2003 Peter Thoeny, peter@thoeny.com
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
# Functions used by both Rcs and RcsFile - they both inherit from this Class

=begin twiki

---+ TWiki::Store::RcsFile Module

This module is contains the shared Rcs code.

=cut

package TWiki::Store::RcsFile;

use strict;

use File::Copy;
use File::Spec;

# ======================
=pod

---++ sub new (  $proto, $web, $topic, $attachment, %settings  )

Not yet documented.

=cut to implementation

sub new
{
   my( $proto, $web, $topic, $attachment, %settings ) = @_;
   my $class = ref($proto) || $proto;
   my $self = {};
   bless( $self, $class );
   $self->{"web"} = $web;
   $self->{"topic"} = $topic;
   $self->{"attachment"} = $attachment || "";
   $self->_settings( %settings );
   $self->{"file"} = $self->_makeFileName();
   $self->{"rcsFile"} = $self->_makeFileName( ",v" );

   return $self;
}

# ======================
=pod

---++ sub _init (  $self  )

Not yet documented.

=cut to implementation

sub _init
{
   my( $self ) = @_;
   
   # If attachment - make sure file and history directories exist
   if( $self->{attachment} ) {
      # Make sure directory for rcs history file exists
      my $rcsDir = $self->_makeFileDir( 1, ",v" );
      my $tempPath = $self->{dataDir} . "/" . $self->{web};
      if( ! -e "$tempPath" ) {
         umask( 0 );
         mkdir( $tempPath, $self->{dirPermission} );
      }
      $tempPath = $rcsDir;
      if( ! -e "$tempPath" ) {
         umask( 0 );
         mkdir( $tempPath, $self->{dirPermission} );
      }
   }

   
   if( $self->{attachment} &&
       ! -e $self->{"rcsFile"} && 
       ! $self->isAsciiDefault() ) {
       $self->setBinary( 1 );
   }  
}


# ======================
=pod

---++ sub revisionFileExists (  $self  )

Not yet documented.

=cut to implementation

sub revisionFileExists
{
    my( $self ) = @_;
    return ( -e $self->{rcsFile} );
}

# ======================
# Psuedo revision information - useful when there is no revision file
=pod

---++ sub _getRevisionInfoDefault (  $self  )

Not yet documented.

=cut to implementation

sub _getRevisionInfoDefault
{
    my( $self ) = @_;
    my $fileDate = $self->getTimestamp();
    return ( "", 1, $fileDate, $TWiki::defaultUserName, "Default revision information - no revision file" );
}

# ======================
# Get the timestamp of the topic file
# Returns 0 if no file, otherwise epoch seconds
=pod

---++ sub getTimestamp (  $self  )

Not yet documented.

=cut to implementation

sub getTimestamp
{
    my( $self ) = @_;
    my $date = 0;
    if( -e $self->{file} ) {
        # Why big number if fail?
        $date = (stat $self->{file})[9] || 600000000;
    }
    return $date;
}


# ======================
=pod

---++ sub restoreLatestRevision (  $self  )

Not yet documented.

=cut to implementation

sub restoreLatestRevision
{
    my( $self ) = @_;
    
    my $rev = $self->numRevisions();
    my $text = $self->getRevision( $rev );
    $self->_saveFile( $self->{file}, $text );
}


# ======================
=pod

---++ sub moveMe (  $self, $newWeb, $newTopic, $attachment  )

Not yet documented.

=cut to implementation

sub moveMe
{
    my( $self, $newWeb, $newTopic, $attachment ) = @_;
    
    if( $self->{attachment} ) {
        $self->_moveAttachment( $newWeb, $newTopic, $self->{attachment} );
    } else {
        $self->_moveTopic( $newWeb, $newTopic );
    }
}


# =========================
# Move/rename a topic, allow for transfer between Webs
# It is the responsibility of the caller to check: exstance webs & topics, lock taken for topic
=pod

---++ sub _moveTopic (  $self, $newWeb, $newTopic  )

Not yet documented.

=cut to implementation

sub _moveTopic
{
   my( $self, $newWeb, $newTopic ) = @_;
   
   my $oldWeb = $self->{web};
   my $oldTopic = $self->{topic};
   
   my $error = "";

   # Change data file
   my $new = TWiki::Store::RcsFile->new( $newWeb, $newTopic, "",
        ( pubDir =>$self->{pubDir}, dataDir => $self->{dataDir} ) );
   my $from = $self->{file};
   my $to =  $new->{file};
   if( ! move( $from, $to ) ) {
       $error .= "data file move failed.  ";
   }

   # Change data file history
   my $oldHistory = $self->{rcsFile};
   if( ! $error && -e $oldHistory ) {
       if( ! move(
         $oldHistory,
         $new->{rcsFile}
       ) ) {
          $error .= "history file move failed.  ";
       }
   }   
   
   if( ! $error ) {
       # Make sure pub directory exists for newWeb
       my $newPubWebDir = $new->_makePubWebDir( $newWeb );
       if ( ! -e $newPubWebDir ) {
           umask( 0 );
           mkdir( $newPubWebDir, $self->{dirPermission} );
       }

       # Rename the attachment directory if there is one
       my $oldAttachDir = $self->_makeFileDir( 1, "" );
       my $newAttachDir = $new->_makeFileDir( 1, "");
       if( -e $oldAttachDir ) {
          if( ! move( $oldAttachDir, $newAttachDir ) ) {
              $error .= "attach move failed";
          }
       }
   }
      
   return $error;
}


# =========================
# Move an attachment from one topic to another.
# If there is a problem an error string is returned.
# The caller to this routine should check that all topics are valid and
# do lock on the topics.
=pod

---++ sub _moveAttachment (  $self, $newWeb, $newTopic  )

Not yet documented.

=cut to implementation

sub _moveAttachment
{
    my( $self, $newWeb, $newTopic ) = @_;
    
    my $oldWeb = $self->{web};
    my $oldTopic = $self->{topic};
    my $attachment = $self->{attachment};
        
    my $error = "";   
    my $what = "$oldWeb.$oldTopic.$attachment -> $newWeb.$newTopic";

    # FIXME might want to delete old directories if empty

    my $new = TWiki::Store::RcsFile->new( $newWeb, $newTopic, $attachment,
        ( pubDir => $self->{pubDir} ) );

    # before save, create directories if they don't exist
    my $tempPath = $new->_makePubWebDir();
    unless( -e $tempPath ) {
        umask( 0 );
        mkdir( $tempPath, 0775 );
    }
    $tempPath = $new->_makeFileDir( 1 );
    unless( -e $tempPath ) {
        umask( 0 );
        mkdir( $tempPath, 0775 ); # FIXME get from elsewhere
    }
    
    # Move attachment
    my $oldAttachment = $self->{file};
    my $newAttachment = $new->{file};
    if( ! move( $oldAttachment, $newAttachment ) ) {
        $error = "Failed to move attachment; $what ($!)";
        return $error;
    }
    
    # Make sure rcs directory exists
    my $newRcsDir = $new->_makeFileDir( 1, ",v" );
    if ( ! -e $newRcsDir ) {
        umask( 0 );
        mkdir( $newRcsDir, $self->{dirPermission} );
    }
    
    # Move attachment history
    my $oldAttachmentRcs = $self->{rcsFile};
    my $newAttachmentRcs = $new->{rcsFile};
    if( -e $oldAttachmentRcs ) {
        if( ! move( $oldAttachmentRcs, $newAttachmentRcs ) ) {
            $error .= "Failed to move attachment history; $what ($!)";
            # Don't return here as attachment file has already been moved
        }
    }

    return $error;
}

# ======================
=pod

---++ sub _epochToRcsDateTime (  $dateTime  )

Not yet documented.

=cut to implementation

sub _epochToRcsDateTime
{
   my( $dateTime ) = @_;
   # TODO: should this be gmtime or local time?
   my( $sec,$min,$hour,$mday,$mon,$year,$wday,$yday ) = gmtime( $dateTime );
   $year += 1900 if( $year > 99 );
   my $rcsDateTime = sprintf "%d.%02d.%02d.%02d.%02d.%02d", ( $year, $mon + 1, $mday, $hour, $min, $sec );
   return $rcsDateTime;
}

# ======================
# Suitable for rcs format stored in files (and that returned by rcs executables ???)
=pod

---++ sub _rcsDateTimeToEpoch (  $rcsDate  )

Not yet documented.

=cut to implementation

sub _rcsDateTimeToEpoch
{
    my( $rcsDate ) = @_;    
    return TWiki::revDate2EpSecs( $rcsDate );
}

# =========================
# Get rid a topic and its attachments completely
# Intended for TEST purposes.
# Use with GREAT CARE as file will be gone, including RCS history
=pod

---++ sub _delete (  $self  )

Not yet documented.

=cut to implementation

sub _delete
{
   my( $self ) = @_;
   
   my $web = $self->{web};
   my $topic = $self->{topic};
   if( $self->{attachment} ) {
      $self->delete();
      return;
   }

   my $file = $self->{file};
   my $rcsDirFile = $self->{dataDir} . "/$web/RCS/$topic,v";

   # Because test switches between using/not-using RCS dir, do both
   my @files = ( $file, "$file,v", $rcsDirFile );
   unlink( @files );
   
   # Delete all attachments and the attachment directory
   my $attDir = $self->_makeFileDir( 1, "" );
   if( -e $attDir ) {
       opendir( DIR, $attDir );
       my @attachments = readdir( DIR );
       closedir( DIR );
       my $attachment;
       foreach $attachment ( @attachments ) {
          if( ! -d "$attDir/$attachment" ) {
             unlink( "$attDir/$attachment" ) || warn "Couldn't remove $attDir/$attachment";
             if( $attachment !~ /,v$/ ) {
                #writeLog( "erase", "$web.$topic.$attachment" );
             }
          }
       }
       
       # Deal with RCS dir if it exists
       my $attRcsDir = "$attDir/RCS";
       if( -e $attRcsDir ) {
           opendir( DIR, $attRcsDir );
           my @attachments = readdir( DIR );
           closedir( DIR );
           my $attachment;
           foreach $attachment ( @attachments ) {
              if( ! -d "$attRcsDir/$attachment" ) {
                 unlink( "$attRcsDir/$attachment" ) || warn "Couldn't remove $attDir/$attachment";
              }
           }  
           rmdir( "$attRcsDir" ) || warn "Couldn't remove directory $attRcsDir";
       }
       
       rmdir( "$attDir" ) || warn "Couldn't remove directory $attDir";
   }
}

# ======================
# Delete topic or attachment and history
# Main use - unit testing
=pod

---++ sub delete (  $self  )

Not yet documented.

=cut to implementation

sub delete
{
    my( $self ) = @_;
    
    my $exist = 0;
    $exist++ if( -f $self->file() );
    $exist++ if( -f $self->rcsFile() );
        
    my( @files ) = ( $self->file(), $self->rcsFile() );
    my $numDeleted = unlink( @files );
    if( $numDeleted != $exist ) {
       print "numDeleted = $numDeleted\n"; # TODO: warning
    }
    
    $self->_init();
    
    $self->{"head"} = 0; # TODO move this to RcsLite
}

# ======================
=pod

---++ sub file (  $self  )

Not yet documented.

=cut to implementation

sub file
{
    my( $self ) = @_;
    return $self->{"file"};
}

# ======================
=pod

---++ sub rcsFile (  $self  )

Not yet documented.

=cut to implementation

sub rcsFile
{
    my( $self ) = @_;
    return $self->{"rcsFile"};
}

# ======================
=pod

---++ sub useRcsDir (  $self  )

Not yet documented.

=cut to implementation

sub useRcsDir
{
    my( $self ) = @_;
    return $self->{"useRcsDir"};
}

# ======================
=pod

---++ sub _settings (  $self, %settings  )

Not yet documented.

=cut to implementation

sub _settings
{
    my( $self, %settings ) = @_;
    $self->{"useRcsDir"} = $settings{"useRcsDir"};
    $self->{"dataDir"}   = $settings{"dataDir"};
    $self->{"pubDir"}    = $settings{"pubDir"};
    $self->{"binary"}    = "";
    $self->{attachAsciiPath} = $settings{attachAsciiPath};
    $self->{dirPermission} = $settings{dirPermission};
}

# ======================
=pod

---++ sub isAsciiDefault (  $self  )

Not yet documented.

=cut to implementation

sub isAsciiDefault
{
   my( $self ) = @_;
   
   my $attachAsciiPath = $self->{"attachAsciiPath"};
   my $filename = $self->{"attachment"};

   if( $filename =~ /$attachAsciiPath/ ) {
      return "ascii";
   } else {
      return "";
   }
}

# ======================
=pod

---++ sub setBinary (  $self, $binary  )

Not yet documented.

=cut to implementation

sub setBinary
{
    my( $self, $binary ) = @_;
    my $oldSetting = $self->{"binary"};
    $binary = "" if( ! $binary );
    $self->{"binary"} = $binary;
    $self->_binaryChange() if( (! $oldSetting && $binary) || ($oldSetting && ! $binary) );
}

# ======================
=pod

---++ sub getBinary (  $self  )

Not yet documented.

=cut to implementation

sub getBinary
{
    my( $self ) = @_;
    return $self->{"binary"};
}

# ======================
=pod

---++ sub _warn (  $self, $message  )

Not yet documented.

=cut to implementation

sub _warn
{
    my( $self, $message ) = @_;
    print "Warning: $message\n";
}

# ======================
=pod

---++ sub setLock (  $self, $lock, $userName  )

Not yet documented.

=cut to implementation

sub setLock
{
    my( $self, $lock, $userName ) = @_;
    
    $userName = $TWiki::userName if( ! $userName );

    my $lockFilename = $self->_makeFileName( ".lock" );
    if( $lock ) {
        my $lockTime = time();
        $self->_saveFile( $lockFilename, "$userName\n$lockTime" );    
    } else {
        unlink "$lockFilename";    
    }
}

# =========================
=pod

---++ sub _saveAttachment (  $self, $theTmpFilename  )

Not yet documented.

=cut to implementation

sub _saveAttachment
{
    my( $self, $theTmpFilename ) = @_;

    # before save, create directories if they don't exist
    my $tempPath = $self->_makePubWebDir();
    if( ! -e "$tempPath" ) {
        umask( 0 );
        mkdir( $tempPath, $self->{dirPermission} );
    }
    $tempPath = $self->_makeFileDir( 1 );
    if( ! -e "$tempPath" ) {
        umask( 0 );
        mkdir( $tempPath, 0775 );
    }
    
    # FIXME share with move - part of init?

    # save uploaded file
    my $newFile = $self->{file};
    copy($theTmpFilename, $newFile) or warn "copy($theTmpFilename, $newFile) failed: $!";
    # FIXME more consistant way of dealing with errors
    umask( 002 );
    chmod( 0644, $newFile ); # FIXME config permission for new attachment
}

# ======================
# This is really saveTopic
=pod

---++ sub _saveFile (  $self, $name, $text  )

Not yet documented.

=cut to implementation

sub _saveFile
{
    my( $self, $name, $text ) = @_;
    
    umask( 002 );
    unless ( open( FILE, ">$name" ) )  {
        warn "Can't create file $name - $!\n";
        return;
    }
    binmode( FILE );
    print FILE $text;
    close( FILE);
}

# ======================
# Deal differently with topics and attachments
# text is a reference for efficiency
=pod

---++ sub _save (  $self, $filename, $text  )

Not yet documented.

=cut to implementation

sub _save
{
    my( $self, $filename, $text ) = @_;
    
    if( $self->{attachment} ) {
        my $tmpFilename = $$text;
        $self->_saveAttachment( $tmpFilename );
    } else {
        $self->_saveFile( $filename, $$text );
    }
}

# ======================
=pod

---++ sub _readFile (  $self, $name  )

Not yet documented.

=cut to implementation

sub _readFile
{
    my( $self, $name ) = @_;
    my $data = "";
    undef $/; # set to read to EOF
    open( IN_FILE, "<$name" ) || return "";
    binmode IN_FILE;
    $data = <IN_FILE>;
    $/ = "\n";
    close( IN_FILE );
    $data = "" unless $data; # no undefined
    return $data;
}


# =========================
# Get full filename for attachment or topic, untaint
# Extension can be:
# If $attachment is blank
#    blank or .txt - topic data file
#    ,v            - topic history file
#    lock          - topic lock file
# If $attachment
#    blank         - attachment file
#    ,v            - attachment history file
=pod

---++ sub _makeFileName (  $self, $extension  )

Not yet documented.

=cut to implementation

sub _makeFileName
{
   my( $self, $extension ) = @_;

   if( ! $extension ) {
      $extension = "";
   }
 
   my $file = "";
   my $extra = "";
   my $web = $self->{"web"};
   my $topic = $self->{"topic"};
   my $attachment = $self->{"attachment"};
   my $dataDir = $self->{"dataDir"};
   my $pubDir  = $self->{"pubDir"};

   if( $extension eq ".lock" ) {
      $file = "$dataDir/$web/$topic$extension";

   } elsif( $attachment ) {
      if ( $extension eq ",v" && $self->{"useRcsDir"} && -d "$dataDir/$web/RCS" ) {
         $extra = "/RCS";
      }
      $file = "$pubDir/$web/$topic$extra/$attachment$extension";

   } else {
      if( ! $extension ) {
         $extension = ".txt";
      } else {
         if( $extension eq ",v" ) {
            $extension = ".txt$extension";
            if( $self->useRcsDir() && -d "$dataDir/$web/RCS" ) {
               $extra = "/RCS";
            }
         }
      }
      $file = "$dataDir/$web$extra/$topic$extension";
   }

   # FIXME: Dangerous, need to make sure that parameters are not tainted
   # Shouldn't really need to untaint here - done to be sure
   $file =~ /(.*)/;
   $file = $1; # untaint
   
   return $file;
}

# =========================
# Get directory that topic or attachment lives in
#    Leave topic blank if you want the web directory rather than the topic directory
#    should simply this with _makeFileName
=pod

---++ sub _makeFileDir (  $self, $attachment, $extension )

Not yet documented.

=cut to implementation

sub _makeFileDir
{
   my( $self, $attachment, $extension) = @_;
   
   $extension = "" if( ! $extension );
   
   my $dataDir = $self->{"dataDir"};
   my $pubDir  = $self->{"pubDir"};
   
   my $web = $self->{web};
   my $topic = $self->{topic};
   
   my $dir = "";
   if( ! $attachment ) {
      if( $extension eq ",v" && $self->{"useRcsDir"} && -d "$dataDir/$web/RCS" ) {
         $dir = "$dataDir/$web/RCS";
      } else {
         $dir = "$dataDir/$web";
      }
   } else {
      my $suffix = "";
      if ( $extension eq ",v" && $self->{"useRcsDir"} && -d "$dataDir/$web/RCS" ) {
         $suffix = "/RCS";
      }
      $dir = "$pubDir/$web/$topic$suffix";
   }

   # FIXME: Dangerous, need to make sure that parameters are not tainted
   # Shouldn't really need to untaint here - done to be sure
   $dir =~ /(.*)/;
   $dir = $1; # untaint
   
   return $dir;
}

# ======================
=pod

---++ sub _makePubWebDir (  $self  )

Not yet documented.

=cut to implementation

sub _makePubWebDir
{
    my( $self ) = @_;

    # FIXME: Dangerous, need to make sure that parameters are not tainted
    my $dir = $self->{pubDir} . "/" . $self->{web};
    $dir =~ /(.*)/;
    $dir = $1; # untaint

    return $dir;
}

=pod

---++ sub _mkTmpFilename ()

Not yet documented.

=cut to implementation

sub _mkTmpFilename
{
    my $tmpdir = File::Spec->tmpdir();
    my $file = _mktemp( "twikiAttachmentXXXXXX", $tmpdir );
    return File::Spec->catfile($tmpdir, $file);
}

# Adapted from CPAN - File::MkTemp
=pod

---++ sub _mktemp ( $template,$dir,$ext,$keepgen,$lookup )

Not yet documented.

=cut to implementation

sub _mktemp {
   my ($template,$dir,$ext,$keepgen,$lookup);
   my (@template,@letters);

   croak("Usage: mktemp('templateXXXXXX',['/dir'],['ext']) ") 
     unless(@_ == 1 || @_ == 2 || @_ == 3);

   ($template,$dir,$ext) = @_;
   @template = split //, $template;

   croak("The template must end with at least 6 uppercase letter X")
      if (substr($template, -6, 6) ne 'XXXXXX');

   if ($dir){
      croak("The directory in which you wish to test for duplicates, $dir, does not exist") unless (-e $dir);
   }

   @letters = split(//,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

   $keepgen = 1;

   while ($keepgen){
      for (my $i = $#template; $i >= 0 && ($template[$i] eq 'X'); $i--){
         $template[$i] = $letters[int(rand 52)];
      }

      undef $template;

      $template = pack "a" x @template, @template;

      $template = $template . $ext if ($ext);

         if ($dir){
            $lookup = File::Spec->catfile($dir, $template);
            $keepgen = 0 unless (-e $lookup);
         }else{
            $keepgen = 0;
         }

   next if $keepgen == 0;
   }

   return($template);
}

1;
