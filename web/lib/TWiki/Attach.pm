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

=begin twiki

---+ TWiki::Attach Module

This package contains routines for dealing with attachments to topics.

=cut

package TWiki::Attach;

use vars qw(
        $viewableAttachmentCount $noviewableAttachmentCount $attachmentCount
    );

# ======================
=pod

---++ sub renderMetaData (  $web, $topic, $meta, $args, $isTopRev  )

Not yet documented.

=cut

sub renderMetaData
{
    my( $web, $topic, $meta, $args, $isTopRev ) = @_;
        
    my $metaText = "";
    
    my $showAttr = "";
    my $showAll = &TWiki::extractNameValuePair( $args, "all" );
    if( $showAll ) {
        $showAttr = "h";
    }
    
    $viewableAttachmentCount = 0;
    $noviewableAttachmentCount = 0;
    $attachmentCount = 0;
    
    my $header = "|  *[[$TWiki::twikiWebname.FileAttachment][Attachment]]*  |  *Action*  |  *Size*  |  *Date*  |  *Who*  |  *Comment*  |";
    if( $showAttr ) {
        $header .= "  *[[$TWiki::twikiWebname.FileAttribute][Attribute]]*  |";
    }
    $header .= "\n";
    
    my @attachments = $meta->find( "FILEATTACHMENT" );
    foreach my $attachment ( @attachments ) {
        $metaText .= formatAttachments( $web, $topic, $showAttr, $isTopRev, %$attachment );
    }
    
    my $text = "";
    if( $showAll || $viewableAttachmentCount ) {
       $text = "\n$header$metaText\n\n";
    }
    
    $text = &TWiki::handleCommonTags( $text, $topic, $web ); # FIXME needed?
    $text = &TWiki::getRenderedVersion( $text, $web );
    
    return $text;
}


# =========================
=pod

---++ sub filenameToIcon (  $fileName  )

Not yet documented.

=cut

sub filenameToIcon
{
    my( $fileName ) = @_;

    my @bits = ( split( /\./, $fileName ) );
    my $fileExt = lc $bits[$#bits];

    my $tmp = &TWiki::getPubDir();
    my $iconDir = "$tmp/icn";
    my $iconUrl = "$TWiki::pubUrlPath/icn";
    my $iconList = &TWiki::Store::readFile( "$iconDir/_filetypes.txt" );
    foreach( split( /\n/, $iconList ) ) {
        @bits = ( split( / / ) );
	if( $bits[0] eq $fileExt ) {
            return "<img src=\"$iconUrl/$bits[1].gif\" width=\"16\" height=\"16\" align=\"top\" alt=\"\" border=\"0\" />";
        }
    }
    return "<img src=\"$iconUrl/else.gif\" width=\"16\" height=\"16\" align=\"top\" alt=\"\" border=\"0\" />";
}

=pod

---++ sub formatAttachments (  $theWeb, $theTopic, $showAttr, $isTopRev, %attachment  )

This routine creates attachment links as part of attachment table etc; within
topic text, attachment links are created using %ATTACHURL% and %ATTACHURLPATH%.

=cut

sub formatAttachments
{
    my ( $theWeb, $theTopic, $showAttr, $isTopRev, %attachment ) = @_;

    my $row = "";

    my ( $file, $attrVersion, $attrPath, $attrSize, $attrDate, $attrUser, $attrComment, $attrAttr ) =
        TWiki::Attach::extractFileAttachmentArgs( %attachment );

    $attachmentCount++;
    if (  ! $attrAttr || ( $showAttr && $attrAttr =~ /^[$showAttr]*$/ ) ) {
        $viewableAttachmentCount++;     
        my $fileIcon = TWiki::Attach::filenameToIcon( $file );

	# I18N: To support attachments via UTF-8 URLs to attachment
	# directories/files that use non-UTF-8 character sets, go through viewfile. 
	# If using %PUBURL%, must URL-encode explicitly to site character set.
        my $fileUrl = "%SCRIPTURLPATH%/viewfile%SCRIPTSUFFIX%/$theWeb/$theTopic?rev=$attrVersion&filename=$file";
	# Go direct to file where possible, for efficiency
	if( $isTopRev || $attrVersion eq "1.1" ) {
	    $fileUrl = TWiki::handleNativeUrlEncode( "%PUBURLPATH%/$theWeb/$theTopic/$file" );
	}

        $attrSize = 100 if( $attrSize < 100 );
        $attrSize = sprintf( "%1.1f&nbsp;K", $attrSize / 1024 );
        $attrComment = $attrComment || "&nbsp;";
        $row .= "| $fileIcon <a href=\"$fileUrl\">$file</a> "
              . "|  <a href=\"%SCRIPTURL%/attach%SCRIPTSUFFIX%/$theWeb/$theTopic?filename=$file&revInfo=1\""
              . " title=\"change, update, previous revisions, move, delete...\">manage</a>  "
              . "|   $attrSize | $attrDate | $attrUser | $attrComment |";
        if ( $showAttr ) {
            $attrAttr = $attrAttr || " &nbsp; ";
            $row .= " $attrAttr |";
        }
        $row .= "\n";
    }  else {
        $noviewableAttachmentCount++;
    }

    return $row;
}


#=========================
=pod

---++ sub migrateFormatForTopic (  $theWeb, $theTopic, $doLogToStdOut  )

Not yet documented.

=cut

sub migrateFormatForTopic
{
   my ( $theWeb, $theTopic, $doLogToStdOut ) = @_;
   
   my $text = TWiki::Store::readWebTopic( $theWeb, $theTopic );
   my ( $before, $atext, $after ) = split( /<!--TWikiAttachment-->/, $text );
   if( ! $before ) { $before = ""; }
   if( ! $atext  ) { $atext  = ""; }

   if ( $atext =~ /<TwkNextItem>/ ) {
      my $newtext = migrateToFileAttachmentMacro( $atext );
      
      $text = "$before<!--TWikiAttachment-->$newtext<!--TWikiAttachment-->";

      my ( $dontLogSave, $doUnlock, $dontNotify ) = ( "", "1", "1" );
      my $error = TWiki::Store::save( $theWeb, $theTopic, $text, "", $dontLogSave, $doUnlock, $dontNotify, "upgraded attachment format" );
      if ( $error ) {
         print "Attach: error from save: $error\n";
      }
      if ( $doLogToStdOut ) {
         print "Changed attachment format for $theWeb.$theTopic\n";
      }
   }
}

# Get file attachment attributes for old html
# format.
# =========================
=pod

---++ sub getOldAttachAttr (  $atext  )

Not yet documented.

=cut

sub getOldAttachAttr
{
    my( $atext ) = @_;
    my $fileName="", $filePath="", $fileSize="", $fileDate="", $fileUser="", $fileComment="";
    my $before="", $item="", $after="";

    ( $before, $fileName, $after ) = split( /<(?:\/)*TwkFileName>/, $atext );
    if( ! $fileName ) { $fileName = ""; }
    if( $fileName ) {
        ( $before, $filePath,    $after ) = split( /<(?:\/)*TwkFilePath>/, $atext );
	if( ! $filePath ) { $filePath = ""; }
	$filePath =~ s/<TwkData value="(.*)">//go;
	if( $1 ) { $filePath = $1; } else { $filePath = ""; }
	$filePath =~ s/\%NOP\%//goi;   # delete placeholder that prevents WikiLinks
	( $before, $fileSize,    $after ) = split( /<(?:\/)*TwkFileSize>/, $atext );
	if( ! $fileSize ) { $fileSize = "0"; }
	( $before, $fileDate,    $after ) = split( /<(?:\/)*TwkFileDate>/, $atext );
	if( ! $fileDate ) { 
            $fileDate = "";
        } else {
            $fileDate =~ s/&nbsp;/ /go;
            $fileDate = &TWiki::revDate2EpSecs( $fileDate );
        }
	( $before, $fileUser,    $after ) = split( /<(?:\/)*TwkFileUser>/, $atext );
	if( ! $fileUser ) { 
            $fileUser = ""; 
        } else {
            $fileUser = &TWiki::wikiToUserName( $fileUser );
        }
	$fileUser =~ s/ //go;
	( $before, $fileComment, $after ) = split( /<(?:\/)*TwkFileComment>/, $atext );
	if( ! $fileComment ) { $fileComment = ""; }
    }

    return ( $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment );
}

# Migrate old HTML format, to %FILEATTACHMENT ... format
# for one piece of text
# =========================
=pod

---++ sub migrateToFileAttachmentMacro (  $meta, $text  )

Not yet documented.

=cut

sub migrateToFileAttachmentMacro
{
   my ( $meta, $text ) = @_;
   
   
   my ( $before, $atext, $after ) = split( /<!--TWikiAttachment-->/, $text );
   $text = $before || "";
   $text .= $after if( $after );
   $atext  = "" if( ! $atext  );

   if( $atext =~ /<TwkNextItem>/ ) {
      my $line = "";
      foreach $line ( split( /<TwkNextItem>/, $atext ) ) {
          my( $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment ) =
             getOldAttachAttr( $line );

          if( $fileName ) {
             my @args = formFileAttachmentArgs( $fileName, "", $filePath, $fileSize, 
                                              $fileDate, $fileUser, $fileComment, "" );
             $meta->put( "FILEATTACHMENT", @args );
          }
       }
   } else {
       # Format of macro that came before META:ATTACHMENT
       my $line = "";
       foreach $line ( split( /\n/, $atext ) ) {
           if( $line =~ /%FILEATTACHMENT{\s"([^"]*)"([^}]*)}%/ ) {
               my $name = $1;
               my $rest = $2;
               $rest =~ s/^\s*//;
               my @values = TWiki::Store::keyValue2list( $rest );
               unshift @values, $name;
               unshift @values, "name";
               $meta->put( "FILEATTACHMENT", @values );
           }
       }
   }
       
   return $text;
}


# =========================
=pod

---++ sub upgradeFrom1v0beta (  $meta  )

Not yet documented.

=cut

sub upgradeFrom1v0beta
{
   my( $meta ) = @_;
   
   my @attach = $meta->find( "FILEATTACHMENT" );
   foreach $att ( @attach ) {
       my $date = $att->{"date"};
       if( $date =~ /-/ ) {
           $date =~ s/&nbsp;/ /go;
           $date = TWiki::revDate2EpSecs( $date );
       }
       $att->{"date"} = $date;
       $att->{"user"} = &TWiki::wikiToUserName( $att->{"user"} );
   }
}



# =========================
=pod

---++ sub formFileAttachmentArgs ()

Not yet documented.

=cut

sub formFileAttachmentArgs
{
    my( $theFile, $theVersion, $thePath, $theSize, $theDate, $theUser, 
             $theComment, $theAttr ) = @_;

    my @args = (
       "name"    => $theFile,
       "version" => $theVersion,
       "path"    => $thePath,
       "size"    => $theSize,
       "date"    => $theDate,
       "user"    => $theUser,
       "comment" => $theComment,
       "attr"    => $theAttr );
    
    return @args;
}



# =========================
# Includes required formatting and conversion
=pod

---++ sub extractFileAttachmentArgs (  %attributes  )

Not yet documented.

=cut

sub extractFileAttachmentArgs
{
    my( %attributes ) = @_;

    my $file =        $attributes{"name"};
    my $attrVersion = $attributes{"version"};
    my $attrPath    = $attributes{"path"};
    my $attrSize    = $attributes{"size"};
    my $attrDate    = $attributes{"date"};
    my $attrUser    = $attributes{"user"};
    my $attrComment = $attributes{"comment"}; 
    my $attrAttr    = $attributes{"attr"};

    $attrDate = &TWiki::formatTime( $attrDate );
    
    $attrUser = &TWiki::userToWikiName( $attrUser );

    return ( $file, $attrVersion, $attrPath, $attrSize, $attrDate, $attrUser, 
             $attrComment, $attrAttr );
}

# FIXME - could be used more?
# ==========================
=pod

---++ sub extractArgsForFile (  $theText, $theFile  )

Not yet documented.

=cut

sub extractArgsForFile
{
   my ( $theText, $theFile ) = @_;
   
   if ( $theText =~ /%FILEATTACHMENT{[\s]*("$theFile" [^}]*)}%/o ) {
      return extractFileAttachmentArgs( $1 );
   } else {
      return "";
   }
}


# =========================
# Remove attachment macro for specified file from topic
# return "", or error string
=pod

---++ sub removeFile ()

Not yet documented.

=cut

sub removeFile
{
    my $theFile = $_[1];
    my $error = "";
    
    # %FILEATTACHMENT{[\s]*"$theFile"[^}]*}%
    if( ! ( $_[0] =~ s/%FILEATTACHMENT{[\s]*"$theFile"[^}]*}%//) ) {
       $error = "Failed to remove attachment $theFile";
    }
    return $error;
}

# =========================
=pod

---++ sub updateProperties (  $fileName, $hideFile, $fileComment, $meta  )

Not yet documented.

=cut

sub updateProperties
{
    my( $fileName, $hideFile, $fileComment, $meta ) = @_;

    my %fileAttachment = $meta->findOne( "FILEATTACHMENT", $fileName );
    $fileAttachment{"attr"} = ( $hideFile ) ? "h" : "";
    $fileAttachment{"comment"} = $fileComment;
    $meta->put( "FILEATTACHMENT", %fileAttachment );
    # FIXME warning if no entry?
}

# =========================
# Add/update attachment for a topic
# $text is full set of attachments, new attachments will be added to the end.
=pod

---++ sub updateAttachment (  $fileVersion, $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment, $hideFile, $meta  )

Not yet documented.

=cut

sub updateAttachment
{
    my ( $fileVersion, $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment, $hideFile, $meta ) = @_;

    my $tmpAttr = ( $hideFile ) ? "h" : "";

    my @args = formFileAttachmentArgs(
        $fileName, $fileVersion, $filePath, $fileSize, $fileDate, $fileUser, 
        $fileComment, $tmpAttr );
    $meta->put( "FILEATTACHMENT", @args );
}

1;
