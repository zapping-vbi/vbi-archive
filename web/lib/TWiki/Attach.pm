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

use strict;

=begin twiki

---+ TWiki::Attach Module

This package contains routines for dealing with attachments to topics.

=cut

package TWiki::Attach;

use vars qw( %templateVars );

# ======================
=pod

---++ sub renderMetaData (  $web, $topic, $meta, $args, $isTopRev  )

Generate a table of attachments suitable for the bottom of a topic
view, using templates for the header, footer and each row.
| =$web= | the web |
| =$topic= | the topic |
| =$meta= | meta-data hash for the topic |
| =$attrs= | hash of attachment arguments |
| $isTopTopicRev | 1 if this topic is being rendered at the most recent revision |

=cut

sub renderMetaData
{
    my( $web, $topic, $meta, $attrs, $isTopTopicRev ) = @_;

    my $showAll = TWiki::extractNameValuePair( $attrs, "all" );
    my $showAttr = $showAll ? "h" : "";
	my $a = ( $showAttr ) ? ":A" : "";

	my @attachments = $meta->find( "FILEATTACHMENT" );

	my $rows = "";
	my $row = _getTemplate("ATTACH:files:row$a");
    foreach my $attachment ( @attachments ) {
	  my $attrAttr = $attachment->{attr};

	  if( ! $attrAttr || ( $showAttr && $attrAttr =~ /^[$showAttr]*$/ )) {
		$rows .= _formatRow( $web,
							 $topic,
							 $attachment->{name},
							 $attachment->{version},
							 $isTopTopicRev,
							 $attachment->{date},
							 $attachment->{user},
							 $attachment->{comment},
							 $attachment,
							 $row );
	  }
    }

    my $text = "";

    if( $showAll || $rows ne "" ) {
	  my $header = _getTemplate("ATTACH:files:header$a");
	  my $footer = _getTemplate("ATTACH:files:footer$a");

	  $text = "$header$rows$footer";
    }
    return $text;
}

# PRIVATE get a template, reading the attachment tables template
# if not already defined.
sub _getTemplate {
  my $template = shift;

  if ( ! defined( $templateVars{$template} )) {
	TWiki::Store::readTemplate("attachtables");
  }

  return TWiki::Store::handleTmplP($template);
}

#=========================
=pod

---++ sub formatVersions (  $theWeb, $theTopic, $attachment, $attrs )

Generate a version history table for a single attachment
| =$web= | the web |
| =$topic= | the topic |
| =$attachment= | basename of attachment |
| =$attrs= | Hash of meta-data attributes |

=cut

sub formatVersions {
  my( $web, $topic, $attachment, $attrs ) = @_;

  my $latestRev = TWiki::Store::getRevisionNumber( $web, $topic, $attachment );
  $latestRev =~ m/\.(.*)/o;
  my $maxRevNum = $1;

  my $header = _getTemplate("ATTACH:versions:header");
  my $footer = _getTemplate("ATTACH:versions:footer");
  my $row    = _getTemplate("ATTACH:versions:row");

  my $rows ="";

  for( my $version = $maxRevNum; $version >= 1; $version-- ) {
    my $rev = "1.$version";

	my( $date, $userName, $minorRev, $comment ) = 
	  TWiki::Store::getRevisionInfo( $web, $topic, $rev, $attachment );
	$rows .= _formatRow( $web, $topic,
						 $attachment,
						 $rev,
						 ( $rev eq $latestRev),
						 $date,
						 $userName,
						 $comment,
						 $attrs,
						 $row );
  }

  return "$header$rows$footer";
}

#=========================
=pod

---++ sub _formatRow ( $web, $topic, $file, $rev, $topRev, $date, $userName, $comment, $attrs, $tmpl )

Format a single row in an attachment table by expanding a template.
| =$web= | the web |
| =$topic= | the topic |
| =$file= | the attachment file name |
| =$rev= | the required revision; required to be a full (major.minor) revision number |
| =$topRev= | boolean indicating if this revision is the most recent revision |
| =$date= | date of _this revision_ of the attachment |
| =$userName= | user (not wikiname) who uploaded this revision |
| =$comment= | comment against this revision |
| =$attrs= | reference to a hash of other meta-data attributes for the attachment |

=cut

sub _formatRow {
  my ( $web, $topic, $file, $rev, $topRev,
	   $date, $userName, $comment, $attrs, $tmpl ) = @_;

  my $row = $tmpl;

  $row =~ s/%A_REV%/$rev/go;

  if ( $row =~ /%A_ICON%/o ) {
	my $fileIcon = filenameToIcon( $file );
	$row =~ s/%A_ICON%/$fileIcon/go;
  }

  if ( $row =~ /%A_URL%/o ) {
	my $url;

	if ( $topRev || $rev eq "1.1" ) {
	  # I18N: To support attachments via UTF-8 URLs to attachment
	  # directories/files that use non-UTF-8 character sets, go through viewfile. 
	  # If using %PUBURL%, must URL-encode explicitly to site character set.
	  $url = TWiki::handleNativeUrlEncode
		( "%PUBURLPATH%/$web/$topic/$file" );
	} else {
	  $url = "%SCRIPTURLPATH%/viewfile%SCRIPTSUFFIX%/".
		"$web/$topic?rev=$rev&filename=$file";
	}
	$row =~ s/%A_URL%/$url/go;
  }

  if ( $row =~ /%A_SIZE%/o && $attrs ) {
    my $attrSize = $attrs->{size};
	$attrSize = 100 if( $attrSize < 100 );
	$attrSize = sprintf( "%1.1f&nbsp;K", $attrSize / 1024 );
	$row =~ s/%A_SIZE%/$attrSize/go;
  }

  $comment =~ s/\|/&#124;/g;
  $comment = "&nbsp;" unless ( $comment );
  $row =~ s/%A_COMMENT%/$comment/go;

  if ( $row =~ /%A_ATTRS%/o && $attrs ) {
	my $attrAttr = $attrs->{attr};
	$attrAttr = $attrAttr || "&nbsp;";
	$row =~ s/%A_ATTRS%/$attrAttr/go;
  }

  $row =~ s/%A_FILE%/$file/go;

  $date = TWiki::formatTime( $date );
  $row =~ s/%A_DATE%/$date/go;

  my $wikiUserName = TWiki::userToWikiName( $userName );
  $row =~ s/%A_USER%/$wikiUserName/go;

  return $row;
}

# =========================
=pod

---++ sub filenameToIcon (  $fileName  )

Produce an image tailored to the type of the file, guessed from
it's extension.

used in TWiki::handleIcon

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

# =========================
=pod

---++ sub removeFile ()

Remove attachment macro for specified file from topic
return "", or error string

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
=pod

---++ sub updateAttachment (  $fileVersion, $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment, $hideFile, $meta  )

Add/update attachment for a topic
$text is full set of attachments, new attachments will be added to the end.

=cut

sub updateAttachment
{
    my ( $fileVersion, $fileName, $filePath, $fileSize, $fileDate, $fileUser, $fileComment, $hideFile, $meta ) = @_;

    my $tmpAttr = ( $hideFile ) ? "h" : "";

    my( $theFile, $theVersion, $thePath, $theSize, $theDate, $theUser, 
             $theComment, $theAttr ) = @_;

    my @attrs = (
				 "name"    => $fileName,
				 "version" => $fileVersion,
				 "path"    => $filePath,
				 "size"    => $fileSize,
				 "date"    => $fileDate,
				 "user"    => $fileUser,
				 "comment" => $fileComment,
				 "attr"    => $tmpAttr
				);

    $meta->put( "FILEATTACHMENT", @attrs );
}

#=========================
=pod

---++ sub migrateFormatForTopic (  $theWeb, $theTopic, $doLogToStdOut  )

Not yet documented.
CODE_SMELL: Is this really necessary? migrateFormatForTopic?

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

# =========================
=pod

---++ sub getOldAttachAttr (  $atext  )

Get file attachment attributes for old html
format.
CODE_SMELL: Is this really necessary? getOldAttachAttr?

=cut

sub getOldAttachAttr
{
    my( $atext ) = @_;
    my $fileName="";
	my $filePath="";
	my $fileSize="";
	my $fileDate="";
	my $fileUser="";
	my $fileComment="";
    my $before="";
	my $item="";
	my $after="";

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

# =========================
=pod

---++ sub migrateToFileAttachmentMacro (  $meta, $text  )

Migrate old HTML format, to %FILEATTACHMENT ... format
for one piece of text
CODE_SMELL: Is this really necessary? migrateToFileAttachmentMacro?

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
			my @attrs = (
						"name"    => $fileName,
						"version" => "",
						"path"    => $filePath,
						"size"    => $fileSize,
						"date"    => $fileDate,
						"user"    => $fileUser,
						"comment" => $fileComment,
						"attr"    => ""
					   );
			$meta->put( "FILEATTACHMENT", @attrs );
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

CODE_SMELL: Is this really necessary? upgradeFrom1v0beta?

=cut

sub upgradeFrom1v0beta
{
   my( $meta ) = @_;
   
   my @attach = $meta->find( "FILEATTACHMENT" );
   foreach my $att ( @attach ) {
       my $date = $att->{"date"};
       if( $date =~ /-/ ) {
           $date =~ s/&nbsp;/ /go;
           $date = TWiki::revDate2EpSecs( $date );
       }
       $att->{"date"} = $date;
       $att->{"user"} = &TWiki::wikiToUserName( $att->{"user"} );
   }
}

1;
