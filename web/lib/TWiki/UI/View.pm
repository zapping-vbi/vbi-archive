# TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 1999-2004 Peter Thoeny, peter@thoeny.com
#
# Based on parts of Ward Cunninghams original Wiki and JosWiki.
# Copyright (C) 1998 Markus Peter - SPiN GmbH (warpi@spin.de)
# Some changes by Dave Harris (drh@bhresearch.co.uk) incorporated
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

---+ TWiki::UI::View

UI delegate for view function

=cut

package TWiki::UI::View;

use strict;
use TWiki;
use TWiki::UI;

=pod

---++ view( $web, $topic, $scruptUrl, $query )
Generate a complete HTML page that represents the viewed topics.
The view is controlled by CGI parameters as follows:
| =rev= | topic revision to view |
| =raw= | don't format body text if set |
| =unlock= | remove any topic locks if set |
| =skin= | name of skin to use |
| =contenttype= | |

=cut

sub view {
  my ( $webName, $topic, $userName, $query ) = @_;

  my $rev = $query->param( "rev" );
  my $viewRaw = $query->param( "raw" ) || "";
  my $unlock  = $query->param( "unlock" ) || "";
  my $skin    = $query->param( "skin" );
  my $contentType = $query->param( "contenttype" );

  TWiki::UI::writeDebugTimes( "view - initialized" );

  my $tmpl = "";
  my $text = "";
  my $meta = "";
  my $maxrev = 1;
  my $extra = "";
  my $wikiUserName = &TWiki::userToWikiName( $userName );
  my $revdate = "";
  my $revuser = "";

  $skin = &TWiki::Prefs::getPreferencesValue( "SKIN" ) unless ( $skin );

  # Set page generation mode to RSS if using an RSS skin
  if( $skin =~ /^rss/ ) {
    TWiki::setPageMode( 'rss' );
  }

  # get view template, standard view or a view with a different skin
  $tmpl = &TWiki::Store::readTemplate( "view", $skin );
  if( ! $tmpl ) {
    TWiki::writeHeader( $query );
    print "<html><body>\n"
      . "<h1>TWiki Installation Error</h1>\n"
        . "Template file view.tmpl not found or template directory \n"
          . "$TWiki::templateDir not found.<p />\n"
            . "Check the \$templateDir variable in TWiki.cfg.\n"
              . "</body></html>\n";
    return;
  }
  TWiki::UI::writeDebugTimes( "view - readTemplate" );

  return unless TWiki::UI::webExists( $webName, $topic );
  TWiki::UI::writeDebugTimes( "view - webExists" );

  if( $unlock eq "on" ) {
    # unlock topic, user cancelled out of edit
    &TWiki::Store::lockTopic( $topic, "on" );
  }

  # Most recent topic read in even if earlier topic requested - makes
  # code simpler and performance impact should be minimal
  my $topicExists = &TWiki::Store::topicExists( $webName, $topic );
  if( $topicExists ) {
    if( $viewRaw ) {
      $text = &TWiki::Store::readTopicRaw( $webName, $topic );
    } else {
      ( $meta, $text ) = &TWiki::Store::readTopic( $webName, $topic );
    }
    ( $revdate, $revuser, $maxrev ) = &TWiki::Store::getRevisionInfoFromMeta( $webName, $topic, $meta);
    $revdate = TWiki::formatTime( $revdate );

    if( $rev ) {
      $rev =~ s/r?1\.//go;  # cut 'r' and major
      if( $rev < 1 )       { $rev = 1; }
      if( $rev > $maxrev ) { $rev = $maxrev; }
    } else {
      $rev = $maxrev;
    }

    if( $rev < $maxrev ) {
      if( $viewRaw ) {
        $text = &TWiki::Store::readTopicRaw( $webName, $topic, "1.$rev" );
      } else {
        ( $meta, $text ) = &TWiki::Store::readTopicVersion( $webName, $topic, "1.$rev" );
      }
      ( $revdate, $revuser ) = &TWiki::Store::getRevisionInfo( $webName, $topic, "1.$rev");
      $revdate = TWiki::formatTime( $revdate );
      $extra .= "r1.$rev";
    }
  } else {
    $rev = 1;
    if( &TWiki::isWikiName( $topic ) || &TWiki::isAbbrev( $topic ) ) {
      ( $meta, $text ) = &TWiki::Store::readTemplateTopic( "WebTopicViewTemplate" );
    } else {
      ( $meta, $text ) = &TWiki::Store::readTemplateTopic( "WebTopicNonWikiTemplate" );
    }
    $extra .= " (not exist)";
  }

  if( $viewRaw ) {
    $extra .= " raw=$viewRaw";
    if( $viewRaw !~ /debug/i ) {
      $text = join( "\n", grep{ !/^%META:([^{]+){(.*)}%$/ } split( /\r?\n/, $text ) );
    }
    if( $skin !~ /^text/ ) {
      my $vtext = "<form><textarea readonly=\"readonly\" wrap=\"virtual\" rows=\"%EDITBOXHEIGHT%\" cols=\"%EDITBOXWIDTH%\">";
      $vtext = &TWiki::handleCommonTags( $vtext, $topic );
      $text =~ s/&/&amp\;/go;
      $text =~ s/%/&\#037\;/go;
      $text =~ s/</&lt\;/go;
      $text =~ s/>/&gt\;/go;
      $text =~ s/\t/   /go;
      $text = "$vtext$text</textarea></form>";
    }
  }

  TWiki::UI::writeDebugTimes( "view - get rev info" );

  if( ! $viewRaw ) {
    $text = &TWiki::handleCommonTags( $text, $topic );
    TWiki::UI::writeDebugTimes( "view - handleCommonTags done" );
    $text = &TWiki::Render::getRenderedVersion( $text );
    TWiki::UI::writeDebugTimes( "view - getRendereredVersion done" );
  }

  if( $TWiki::doLogTopicView ) {
    # write log entry
    &TWiki::Store::writeLog( "view", "$webName.$topic", $extra );
  }

  my( $mirrorSiteName, $mirrorViewURL, $mirrorLink, $mirrorNote ) = &TWiki::readOnlyMirrorWeb( $webName );
  if( $mirrorSiteName ) {
    # disable edit and attach
    # FIXME: won't work with non-default skins, see %EDITURL%
    $tmpl =~ s/%EDITTOPIC%/$mirrorLink | <strike>Edit<\/strike>/go;
    $tmpl =~ s/<a [^>]*?>Attach<\/a>/<strike>Attach<\/strike>/goi;
    if( $topicExists ) {
      # remove the NOINDEX meta tag
      $tmpl =~ s/<meta name="robots"[^>]*>//goi;
    } else {
      $text = "";
    }
    $tmpl =~ s/%REVTITLE%//go;
  } elsif( $rev < $maxrev ) {
    # disable edit of previous revisions - FIXME consider change to use two templates
    # FIXME: won't work with non-default skins, see %EDITURL%
    $tmpl =~ s/%EDITTOPIC%/<strike>Edit<\/strike>/go;
    $tmpl =~ s/<a [^>]*?>Attach<\/a>/<strike>Attach<\/strike>/goi;
    $tmpl =~ s|<a [^>]*?>Rename/move<\/a>|<strike>Rename/move<\/strike>|goi;
    $tmpl =~ s/%REVTITLE%/\(r1.$rev\)/go;
    $tmpl =~ s/%REVARG%/&rev=1.$rev/go;
  } else {
    # Remove the NOINDEX meta tag (for robots) from both Edit and 
    # Create pages
    $tmpl =~ s/<meta name="robots"[^>]*>//goi;
    my $editAction = $topicExists ? 'Edit' : 'Create';

    # Special case for 'view' to handle %EDITTOPIC% and Edit vs. Create.
    # New %EDITURL% variable is implemented by handleCommonTags, suffixes
    # '?t=NNNN' to ensure that every Edit link is unique, fixing
    # Codev.RefreshEditPage bug relating to caching of Edit page.
    $tmpl =~ s!%EDITTOPIC%!<a href=\"%EDITURL%\"><b>$editAction</b></a>!go;

    # FIXME: Implement ColasNahaboo's suggested %EDITLINK% along the 
    # same lines, within handleCommonTags
    $tmpl =~ s/%REVTITLE%//go;
    $tmpl =~ s/%REVARG%//go;
  }

#SMELL: HUH? - TODO: why would you not show the revisions around the version that you are displaying? and this logic is yucky@!
  my $i = $maxrev;
  my $j = $maxrev;
  my $revisions = "";
  my $breakRev = 0;
  if( ( $TWiki::numberOfRevisions > 0 ) && ( $TWiki::numberOfRevisions < $maxrev ) ) {
    $breakRev = $maxrev - $TWiki::numberOfRevisions + 1;
  }
  while( $i > 0 ) {
    if( $i == $rev) {
      $revisions = "$revisions | r1.$i";
    } else {
      $revisions = "$revisions | <a href=\"%SCRIPTURLPATH%/view%SCRIPTSUFFIX%/%WEB%/%TOPIC%?rev=1.$i\">r1.$i</a>";
    }
    if( $i != 1 ) {
      if( $i == $breakRev ) {
        $i = 1;
      } else {
        $j = $i - 1;
        $revisions = "$revisions | <a href=\"%SCRIPTURLPATH%/rdiff%SCRIPTSUFFIX%/%WEB%/%TOPIC%?rev1=1.$i&amp;rev2=1.$j\">&gt;</a>";
      }
    }
    $i = $i - 1;
  }
  $tmpl =~ s/%REVISIONS%/$revisions/go;

  $tmpl =~ s/%REVINFO%/%REVINFO%$mirrorNote/go;

  $tmpl = &TWiki::handleCommonTags( $tmpl, $topic );
  if( $viewRaw ) {
    $tmpl =~ s/%META{[^}]*}%//go;
  } else {
    $tmpl = &TWiki::handleMetaTags( $webName, $topic, $tmpl, $meta, ( $rev == $maxrev ) );
  }
  TWiki::UI::writeDebugTimes( "view - handleCommonTags for template done" );
  $tmpl = &TWiki::Render::getRenderedVersion( $tmpl, "", $meta ); ## better to use meta rendering?
  $tmpl =~ s/%TEXT%/$text/go;
  $tmpl =~ s/%MAXREV%/1.$maxrev/go;
  $tmpl =~ s/%CURRREV%/1.$rev/go;
  $tmpl =~ s/( ?) *<\/?(nop|noautolink)\/?>\n?/$1/gois;   # remove <nop> tags (PTh 06 Nov 2000)

  # check access permission
  my $viewAccessOK = &TWiki::Access::checkAccessPermission( "view", $wikiUserName, $text, $topic, $webName );

  if( (!$topicExists) || $TWiki::readTopicPermissionFailed ) {
    # Can't read requested topic and/or included (or other accessed topics
    # user could not be authenticated, may be not logged in yet?
    my $viewauthFile = $ENV{'SCRIPT_FILENAME'};
    $viewauthFile =~ s|/view|/viewauth|o;
    if( ( ! $query->remote_user() ) && (-e $viewauthFile ) ) {
      # try again with authenticated viewauth script
      # instead of non authenticated view script
      my $url = $ENV{"REQUEST_URI"};
      if( $url && $url =~ m|/view| ) {
        # $url i.e. is "twiki/bin/view.cgi/Web/Topic?cms1=val1&cmd2=val2"
        $url =~ s|/view|/viewauth|o;
        $url = "$TWiki::urlHost$url";
      } else {
       # If REQUEST_URI is rewritten and does not contain the name "view"
        # try looking at the CGI environment variable SCRIPT_NAME.
        #
        # Assemble the new URL using the host, the changed script name,
        # the path info, and the query string.  All three query variables
        # are in the list of the canonical request meta variables in CGI 1.1.
        my $script      = $ENV{'SCRIPT_NAME'};
        my $pathInfo    = $ENV{'PATH_INFO'};
        my $queryString = $ENV{'QUERY_STRING'};
        $pathInfo    = '/' . $pathInfo    if ($pathInfo);
        $queryString = '?' . $queryString if ($queryString);
        if ($script && $script =~ m|/view| ) {
          $script =~ s|/view|/viewauth|o;
          $url = "$TWiki::urlHost$script$pathInfo$queryString";
        } else {
          # If SCRIPT_NAME does not contain the name "view"
          # the last hope is to try the SCRIPT_FILENAME ...
          $viewauthFile =~ s|^.*/viewauth|/viewauth|o;  # strip off $Twiki::scriptUrlPath
         $url = "$TWiki::urlHost$TWiki::scriptUrlPath/$viewauthFile$pathInfo$queryString";
         }
      }
      TWiki::UI::redirect( $url );
    }
  }
  if( ! $viewAccessOK ) {
    TWiki::UI::oops( $webName, $topic, "accessview" );
  }

  TWiki::UI::writeDebugTimes( "view - checked access permissions" );

  # Write header based on "contenttype" parameter, used to produce
  # MIME types like text/plain or text/xml, e.g. for RSS feeds.
  if( $contentType ) {
    TWiki::writeHeaderFull( $query, 'basic', $contentType, 0);
    if( $skin =~ /^rss/ ) {
      $tmpl =~ s/<img [^>]*>//g;  # remove image tags
      $tmpl =~ s/<a [^>]*>//g;    # remove anchor tags
      $tmpl =~ s/<\/a>//g;        # remove anchor tags
    }
  } elsif( $skin =~ /^rss/ ) {
    TWiki::writeHeaderFull( $query, 'basic', 'text/xml', 0);
    $tmpl =~ s/<img [^>]*>//g;  # remove image tags
    $tmpl =~ s/<a [^>]*>//g;    # remove anchor tags
    $tmpl =~ s/<\/a>//g;        # remove anchor tags
  } else {
    TWiki::writeHeader( $query );
  }

  # print page content
  print $tmpl;

  TWiki::UI::writeDebugTimes( "view - done" );
}

1;
