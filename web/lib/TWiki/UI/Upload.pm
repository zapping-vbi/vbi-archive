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
=begin twiki

---+ TWiki::UI::Upload

UI delegate for attachment management functions

=cut
package TWiki::UI::Upload;

use strict;
use TWiki;
use TWiki::UI;

=pod

---++ attach( $web, $topic, $query )
Perform the functions of an 'attach' URL. CGI parameters are:
| =filename= | Name of attachment |
| =skin= | Skin to use in presenting pages |

=cut
sub attach {
  my ( $webName, $topic, $userName, $query ) = @_;

  my $fileName = $query->param( 'filename' ) || "";
  my $skin = $query->param( "skin" );

  return unless TWiki::UI::webExists( $webName, $topic );

  my $tmpl = "";
  my $text = "";
  my $meta = "";
  my $atext = "";
  my $fileUser = "";

  my $isHideChecked = "";

  return if TWiki::UI::isMirror( $webName, $topic );

  my $wikiUserName = &TWiki::userToWikiName( $userName );
  return unless TWiki::UI::isAccessPermitted( $webName, $topic,
                                            "change", $wikiUserName );

  return unless TWiki::UI::topicExists( $webName, $topic, "attach" );

  ( $meta, $text ) = &TWiki::Store::readTopic( $webName, $topic );
  my %args = $meta->findOne( "FILEATTACHMENT", $fileName );
  %args = ( "attr" => "", "path" => "", "comment" => "" ) if( ! % args );

  if ( $args{"attr"} =~ /h/o ) {
    $isHideChecked = "checked";
  }

  # why log attach before post is called?
  # FIXME: Move down, log only if successful (or with error msg?)
  # Attach is a read function, only has potential for a change
  if( $TWiki::doLogTopicAttach ) {
    # write log entry
    &TWiki::Store::writeLog( "attach", "$webName.$topic", $fileName );
  }
  
  my $fileWikiUser = "";
  $skin = TWiki::Prefs::getPreferencesValue( "SKIN" ) unless ( $skin );
  if( $fileName && %args ) {
    $tmpl = TWiki::Store::readTemplate( "attachagain", $skin );
    $fileWikiUser = &TWiki::userToWikiName( $args{"user"} );
  } else {
    $tmpl = TWiki::Store::readTemplate( "attachnew", $skin );
  }
  if ( $fileName ) {
	# must come after templates have been read
    $atext .= TWiki::Attach::formatVersions( $webName, $topic, $fileName, %args );
  }
  $tmpl =~ s/%ATTACHTABLE%/$atext/go;
  $tmpl =~ s/%FILEUSER%/$fileWikiUser/go;
  $tmpl = &TWiki::handleCommonTags( $tmpl, $topic );
  $tmpl = &TWiki::Render::getRenderedVersion( $tmpl );
  $tmpl = &TWiki::handleMetaTags( $webName, $topic, $tmpl, $meta );
  $tmpl =~ s/%HIDEFILE%/$isHideChecked/go;
  $tmpl =~ s/%FILENAME%/$fileName/go;
  $tmpl =~ s/%FILEPATH%/$args{"path"}/go;
  $tmpl =~ s/%FILECOMMENT%/$args{"comment"}/go;
  $tmpl =~ s/( ?) *<\/?(nop|noautolink)\/?>\n?/$1/gois;   # remove <nop> and <noautolink> tags
  TWiki::writeHeader( TWiki::getCgiQuery() );
  print $tmpl;
}

# =========================
# code fragment to extract pixel size from images
# taken from http://www.tardis.ed.ac.uk/~ark/wwwis/
# subroutines: _imgsize, _gifsize, _OLDgifsize, _gif_blockskip,
#              _NEWgifsize, _jpegsize
#
# looking at the filename really sucks I should be using the first 4 bytes
# of the image. If I ever do it these are the numbers.... (from chris@w3.org)
#  PNG 89 50 4e 47
#  GIF 47 49 46 38
#  JPG ff d8 ff e0
#  XBM 23 64 65 66


# =========================
sub _imgsize {
  my( $file ) = shift @_;
  my( $x, $y) = ( 0, 0 );

  if( defined( $file ) && open( STRM, "<$file" ) ) {
    binmode( STRM ); # for crappy MS OSes - Win/Dos/NT use is NOT SUPPORTED
    if( $file =~ /\.jpg$/i || $file =~ /\.jpeg$/i ) {
      ( $x, $y ) = &_jpegsize( \*STRM );
    } elsif( $file =~ /\.gif$/i ) {
      ( $x, $y ) = &_gifsize(\*STRM);
    } elsif( $file =~ /\.png$/i ) {
      ( $x, $y ) = &_pngsize(\*STRM);
    }
    close( STRM );
  }
  return( $x, $y );
}


# =========================
sub _gifsize
{
  my( $GIF ) = @_;
  if( 0 ) {
    return &_NEWgifsize( $GIF );
  } else {
    return &_OLDgifsize( $GIF );
  }
}


# =========================
sub _OLDgifsize {
  my( $GIF ) = @_;
  my( $type, $a, $b, $c, $d, $s ) = ( 0, 0, 0, 0, 0, 0 );

  if( defined( $GIF )              &&
      read( $GIF, $type, 6 )       &&
      $type =~ /GIF8[7,9]a/        &&
      read( $GIF, $s, 4 ) == 4     ) {
    ( $a, $b, $c, $d ) = unpack( "C"x4, $s );
    return( $b<<8|$a, $d<<8|$c );
  }
  return( 0, 0 );
}


# =========================
# part of _NEWgifsize
sub _gif_blockskip {
  my ( $GIF, $skip, $type ) = @_;
  my ( $s ) = 0;
  my ( $dummy ) = '';

  read( $GIF, $dummy, $skip );       # Skip header (if any)
  while( 1 ) {
    if( eof( $GIF ) ) {
      #warn "Invalid/Corrupted GIF (at EOF in GIF $type)\n";
      return "";
    }
    read( $GIF, $s, 1 );             # Block size
    last if ord( $s ) == 0;          # Block terminator
    read( $GIF, $dummy, ord( $s ) ); # Skip data
  }
}


# =========================
# this code by "Daniel V. Klein" <dvk@lonewolf.com>
sub _NEWgifsize {
  my( $GIF ) = @_;
  my( $cmapsize, $a, $b, $c, $d, $e ) = 0;
  my( $type, $s ) = ( 0, 0 );
  my( $x, $y ) = ( 0, 0 );
  my( $dummy ) = '';

  return( $x,$y ) if( !defined $GIF );

  read( $GIF, $type, 6 );
  if( $type !~ /GIF8[7,9]a/ || read( $GIF, $s, 7 ) != 7 ) {
    #warn "Invalid/Corrupted GIF (bad header)\n";
    return( $x, $y );
  }
  ( $e ) = unpack( "x4 C", $s );
  if( $e & 0x80 ) {
    $cmapsize = 3 * 2**(($e & 0x07) + 1);
    if( !read( $GIF, $dummy, $cmapsize ) ) {
      #warn "Invalid/Corrupted GIF (global color map too small?)\n";
      return( $x, $y );
    }
  }
 FINDIMAGE:
  while( 1 ) {
    if( eof( $GIF ) ) {
      #warn "Invalid/Corrupted GIF (at EOF w/o Image Descriptors)\n";
      return( $x, $y );
    }
    read( $GIF, $s, 1 );
    ( $e ) = unpack( "C", $s );
    if( $e == 0x2c ) {           # Image Descriptor (GIF87a, GIF89a 20.c.i)
      if( read( $GIF, $s, 8 ) != 8 ) {
        #warn "Invalid/Corrupted GIF (missing image header?)\n";
        return( $x, $y );
      }
      ( $a, $b, $c, $d ) = unpack( "x4 C4", $s );
      $x = $b<<8|$a;
      $y = $d<<8|$c;
      return( $x, $y );
    }
    if( $type eq "GIF89a" ) {
      if( $e == 0x21 ) {         # Extension Introducer (GIF89a 23.c.i)
        read( $GIF, $s, 1 );
        ( $e ) = unpack( "C", $s );
        if( $e == 0xF9 ) {       # Graphic Control Extension (GIF89a 23.c.ii)
          read( $GIF, $dummy, 6 );        # Skip it
          next FINDIMAGE;       # Look again for Image Descriptor
        } elsif( $e == 0xFE ) {  # Comment Extension (GIF89a 24.c.ii)
          &_gif_blockskip( $GIF, 0, "Comment" );
          next FINDIMAGE;       # Look again for Image Descriptor
        } elsif( $e == 0x01 ) {  # Plain Text Label (GIF89a 25.c.ii)
          &_gif_blockskip( $GIF, 12, "text data" );
          next FINDIMAGE;       # Look again for Image Descriptor
        } elsif( $e == 0xFF ) {  # Application Extension Label (GIF89a 26.c.ii)
          &_gif_blockskip( $GIF, 11, "application data" );
          next FINDIMAGE;       # Look again for Image Descriptor
        } else {
          #printf STDERR "Invalid/Corrupted GIF (Unknown extension %#x)\n", $e;
          return( $x, $y );
        }
      } else {
        #printf STDERR "Invalid/Corrupted GIF (Unknown code %#x)\n", $e;
        return( $x, $y );
      }
    } else {
      #warn "Invalid/Corrupted GIF (missing GIF87a Image Descriptor)\n";
      return( $x, $y );
    }
  }
}

# =========================
# _jpegsize : gets the width and height (in pixels) of a jpeg file
# Andrew Tong, werdna@ugcs.caltech.edu           February 14, 1995
# modified slightly by alex@ed.ac.uk
sub _jpegsize {
  my( $JPEG ) = @_;
  my( $done ) = 0;
  my( $c1, $c2, $ch, $s, $length, $dummy ) = ( 0, 0, 0, 0, 0, 0 );
  my( $a, $b, $c, $d );

  if( defined( $JPEG )             &&
      read( $JPEG, $c1, 1 )        &&
      read( $JPEG, $c2, 1 )        &&
      ord( $c1 ) == 0xFF           &&
      ord( $c2 ) == 0xD8           ) {
    while ( ord( $ch ) != 0xDA && !$done ) {
      # Find next marker (JPEG markers begin with 0xFF)
      # This can hang the program!!
      while( ord( $ch ) != 0xFF ) {
        return( 0, 0 ) unless read( $JPEG, $ch, 1 );
      }
      # JPEG markers can be padded with unlimited 0xFF's
      while( ord( $ch ) == 0xFF ) {
        return( 0, 0 ) unless read( $JPEG, $ch, 1 );
      }
      # Now, $ch contains the value of the marker.
      if( ( ord( $ch ) >= 0xC0 ) && ( ord( $ch ) <= 0xC3 ) ) {
        return( 0, 0 ) unless read( $JPEG, $dummy, 3 );
        return( 0, 0 ) unless read( $JPEG, $s, 4 );
        ( $a, $b, $c, $d ) = unpack( "C"x4, $s );
        return( $c<<8|$d, $a<<8|$b );
      } else {
        # We **MUST** skip variables, since FF's within variable names are
        # NOT valid JPEG markers
        return( 0, 0 ) unless read( $JPEG, $s, 2 );
        ( $c1, $c2 ) = unpack( "C"x2, $s );
        $length = $c1<<8|$c2;
        last if( !defined( $length ) || $length < 2 );
        read( $JPEG, $dummy, $length-2 );
      }
    }
  }
  return( 0, 0 );
}

# =========================
#  _pngsize : gets the width & height (in pixels) of a png file
#  cor this program is on the cutting edge of technology! (pity it's blunt!)
#  GRR 970619:  fixed bytesex assumption
#  source: http://www.la-grange.net/2000/05/04-png.html
sub _pngsize {
  my ($PNG) = @_;
  my ($head) = "";
  my($a, $b, $c, $d, $e, $f, $g, $h)=0;
  if(defined($PNG)                              &&
     read( $PNG, $head, 8 ) == 8                &&
     $head eq "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a" &&
     read($PNG, $head, 4) == 4                  &&
     read($PNG, $head, 4) == 4                  &&
     $head eq "IHDR"                            &&
     read($PNG, $head, 8) == 8                  ){
    ($a,$b,$c,$d,$e,$f,$g,$h)=unpack("C"x8,$head);
    return ($a<<24|$b<<16|$c<<8|$d, $e<<24|$f<<16|$g<<8|$h);
  }
  return (0,0);
} 

# =========================
sub _addLinkToEndOfTopic
{
    my ( $text, $pathFilename, $fileName, $fileComment ) = @_;
    my $fileLink = "";
    my $imgSize = "";

    if( $fileName =~ /\.(gif|jpg|jpeg|png)$/i ) {
        # inline image
        $fileComment = $fileName if( ! $fileComment );
        my( $nx, $ny ) = &_imgsize( $pathFilename );
        if( ( $nx > 0 ) && ( $ny > 0 ) ) {
            $imgSize = " width=\"$nx\" height=\"$ny\" ";
        }
        $fileLink = &TWiki::Prefs::getPreferencesValue( "ATTACHEDIMAGEFORMAT" )
                  || '   * $comment: <br />'
                   . ' <img src="%ATTACHURLPATH%/$name" alt="$name"$size />';
    } else {
        # normal attached file
        $fileLink = &TWiki::Prefs::getPreferencesValue( "ATTACHEDFILELINKFORMAT" )
                 || '   * [[%ATTACHURL%/$name][$name]]: $comment';
    }

    $fileLink =~ s/^      /\t\t/go;
    $fileLink =~ s/^   /\t/go;
    $fileLink =~ s/\$name/$fileName/g;
    $fileLink =~ s/\$comment/$fileComment/g;
    $fileLink =~ s/\$size/$imgSize/g;
    $fileLink =~ s/\\t/\t/go;
    $fileLink =~ s/\\n/\n/go;
    $fileLink =~ s/([^\n])$/$1\n/;

    return "$text$fileLink";
}

=pod

---++ upload( $web, $topic, $userName, $query)
Perform the functions of an 'upload' url.
CGI parameters, passed in $query:
| =hidefile= | if defined, will not show file in attachment table |
| =filepath= | |
| =filename= | |
| =filecomment= | Comment to associate with file in attachment table |
| =createlink= | if defined, will create a link to file at end of topic |
| =changeproperties= | |

=cut
sub upload {
  my ( $webName, $topic, $userName, $query ) = @_;

  my $hideFile = $query->param( 'hidefile' ) || "";
  my $fileComment = $query->param( 'filecomment' ) || "";
  my $createLink = $query->param( 'createlink' ) || "";
  my $doChangeProperties = $query->param( 'changeproperties' );
  my $filePath = $query->param( 'filepath' ) || "";
  my $fileName = $query->param( 'filename' ) || "";
  if ( $filePath && ! $fileName ) {
    $filePath =~ m|([^/\\]*$)|;
    $fileName = $1;
  }
  my $tmpFilename = $query->tmpFileName( $filePath ) || "";
  # CODE_SMELL: should really be using the file handle, not
  # an undocumented CGI function. The previous line of code causes
  # an Apache warning.
  #my $tmpFile = $query->upload( "filepath" ) || "";

  $fileComment =~ s/\s+/ /go;
  $fileComment =~ s/^\s*//o;
  $fileComment =~ s/\s*$//o;

  close $filePath if( $TWiki::OS eq "WINDOWS");

  # Change Windows path to Unix path
  $tmpFilename =~ s!\\!/!go;
  $tmpFilename =~ /(.*)/;
    $tmpFilename = $1;
    ##TWiki::writeDebug( "upload: tmpFilename $tmpFilename" );
  
  my @error =
    updateAttachment( $webName, $topic, $userName,
                      $createLink,
                      $doChangeProperties,
                      $filePath, $tmpFilename,
                      $fileName, $hideFile, $fileComment );

  if ( ( @error ) && scalar( @error ) && defined( $error[0] )) {
    # error[0] will be "" if redirect already printed
    TWiki::UI::oops( $webName, $topic, @error ) if ( $error[0] )
  } else {
    # and finally display topic
    TWiki::UI::redirect( &TWiki::getViewUrl( $webName, $topic ) );
    my $message = ( $doChangeProperties ) ? "properties changed" : "$fileName uploaded";
    print( "OK $message\n" );
  }
}
  
=pod
  
---++ updateAttachment( $webName, $topic, $userName, $createLink, $propsOnly, $filePath, $localFile, $attName, $hideFile, $comment ) => undef or error
  
CODE_SMELL: this should really be in Store
  
Update an attachment, file or properties or both. This may also be used to
create an attachment.
| =$webName= | Web containing topic |
| =$topic= | Topic |
| =$userName= | Username of user doing upload/change - username, *not* wikiName |
| =$createLink= | 1 if a link is to be created in the topic text |
| =$propsOnly= | 1 if only change properties, not atachment |
| =$filePath= | if !propsOnly, gives the remote path name of the file to upload. This is used to derive the attName. |
| =$localFile= | Name of local file to replace attachment |
| =$attName= | If propsOnly, the name of the attachment. Ignored if !propsOnly. |
| =$hideFile= | (property) on if files is to be hidden in normal view |
| =$comment= | (property) comment associated with file |
| return | on error, a list of parameters to the TWiki::UI::oops function, not including the webName and topic. |
|               |  If the first element in the list is the empty string, an error has already been printed to the browser, and no oops call is necessary. |

=cut
sub updateAttachment {
  my ( $webName, $topic, $userName,
       $createLink,
       $propsOnly,
       $filePath, $localFile,
       $attName, $hideFile, $comment ) = @_;

  my $wikiUserName = TWiki::userToWikiName( $userName );
  return ( 0 ) unless TWiki::UI::webExists( $webName, $topic );
  return ( 0 ) if TWiki::UI::isMirror( $webName, $topic );
  return ( 0 ) unless TWiki::UI::isAccessPermitted( $webName, $topic,
                                              "change", $wikiUserName );
  return ( 0 ) unless TWiki::UI::topicExists( $webName, $topic, "upload" );

  my( $fileSize, $fileUser, $fileDate, $fileVersion ) = "";

  unless( $propsOnly ) {
      # cut path from filepath name (Windows "\" and Unix "/" format)
      my @pathz = ( split( /\\/, $filePath ) );
      my $filetemp = $pathz[$#pathz];
      my @pathza = ( split( '/', $filetemp ) );
    $attName = $pathza[$#pathza];
  
      # Delete unwanted characters from filename, with I18N
      my $nonAlphaNum = "[^$TWiki::regex{mixedAlphaNum}" . '\._-]+';
    $attName =~ s/${nonAlphaNum}//go;
    $attName =~ s/$TWiki::uploadFilter/$1\.txt/goi;  # apply security filter
    $attName =~ /(.*)/;  # untaint
    $attName = $1;

    ##TWiki::writeDebug ("Upload filename after cleanup is '$attName'");

    # check if file exists and has non zero size
    my $size = -s $localFile;
  
    if( ! -e $localFile || ! $size ) {
      return ( "upload",
               "ERROR $webName.$topic File missing or zero size", $attName );
    }

    my $maxSize = TWiki::Prefs::getPreferencesValue( "ATTACHFILESIZELIMIT" );
    $maxSize = 0 unless ( $maxSize =~ /([0-9]+)/o );

    if( $maxSize && $size > $maxSize * 1024 ) {
      return ( "uploadlimit", $attName, $maxSize );
    }
  
      # Update
      my $text1 = "";
    my $saveCmd = "";
    my $doNotLogChanges = 1;
    my $doUnlock = 0;
    my $dontNotify = "";
    my $error =
      TWiki::Store::saveAttachment( $webName, $topic, $text1, $saveCmd,
                                    $attName, $doNotLogChanges, $doUnlock,
                                    $dontNotify, $comment, $localFile );

    if ( $error ) {
      return ( "saveerr", "Save attachment error $error" );
    }

    # get user name
    $fileUser = $userName;

    # get time stamp and file size of uploaded file:
    my @stats = stat $localFile;
    $fileSize = $stats[7];
    $fileDate = $stats[9];

    $fileVersion = TWiki::Store::getRevisionNumber( $webName, $topic,
                                                    $attName );

    if( $TWiki::doLogTopicUpload ) {
      # write log entry
      TWiki::Store::writeLog( "upload", "$webName.$topic", $attName );
      #FIXE also do log for change property?
    }
  }

  # update topic
  my( $meta, $text ) = TWiki::Store::readTopic( $webName, $topic );

  # update meta-data
  if( $propsOnly ) {
    TWiki::Attach::updateProperties( $attName, $hideFile, $comment, $meta );
  } else {
    TWiki::Attach::updateAttachment( $fileVersion, $attName, $filePath,
                                     $fileSize,
                                     $fileDate, $fileUser, $comment,
                                     $hideFile, $meta );
  }

  if( $createLink ) {
    $filePath = TWiki::Store::getFileName( $webName, $topic, $attName );
    $text = _addLinkToEndOfTopic( $text, $filePath, $attName, $comment );
  }

  # update topic
  my $error = TWiki::Store::saveTopic( $webName, $topic, $text, $meta, "", 1 );
  if( $error ) {
    return ( "saveerr", "Save topic error $error" );
  }

  return undef;
}

1;
