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

---+ TWiki::UI::Viewfile

UI functions for file viewing

=cut

package TWiki::UI::Viewfile;

use strict;
use TWiki;
use TWiki::Store;
use TWiki::UI;

=pod

---++ view( $web, $topic, $query )
Command handler for viewfile. View a file in the browser.
Some parameters are passed in CGI query:
| =filename= | |
| =rev= | |

=cut

sub view {
  my ( $webName, $topic, $query ) = @_;

  my $fileName = $query->param( 'filename' );
  my $rev = $query->param( 'rev' ) || "";
  $rev =~ s/r?1\.//o;  # cut 'r' and major
  # Fix for Codev.SecurityAlertExecuteCommandsWithRev
  $rev = "" unless( $rev =~ s/^.*?([0-9]+).*$/$1/so );

  return unless TWiki::UI::webExists( $webName, $topic );

  my $topRev = TWiki::Store::getRevisionNumber( $webName, $topic, $fileName );

  if( ( $rev ) && ( $rev ne $topRev ) ) {
    my $fileContent = TWiki::Store::readAttachmentVersion( $webName, $topic, $fileName, $rev ); 
    if( $fileContent ) {
      my $mimeType = _suffixToMimeType( $fileName );
      print $query->header( -type => $mimeType,
                            -Content_Disposition => "inline;filename=$fileName");
      print "$fileContent";
      return;
    } else {
      # If no file content we'll try and show pub content, should there be a warning FIXME
    }
  }

  # this should actually kick off a document conversion 
  # (.doc, .xls... to .html) and show the html file.
  # Convert only if html file does not yet exist
  # for now, show the original document:

  my $pubUrlPath = &TWiki::getPubUrlPath();
  my $host = $TWiki::urlHost;
  TWiki::UI::redirect( "$host$pubUrlPath/$webName/$topic/$fileName" );
}

sub _suffixToMimeType {
  my( $theFilename ) = @_;

  my $mimeType = 'text/plain';
  if( $theFilename =~ /\.(.+)$/ ) {
    my $suffix = $1;
    my @types = grep{ s/^\s*([^\s]+).*?\s$suffix\s.*$/$1/i }
      map{ "$_ " }
        split( /[\n\r]/, &TWiki::Store::readFile( $TWiki::mimeTypesFilename ) );
    $mimeType = $types[0] if( @types );
  }
  return $mimeType;
}

1;
