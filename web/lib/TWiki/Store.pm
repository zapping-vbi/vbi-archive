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
# 20000917 - NicholasLee : Split file/storage related functions from wiki.pm
# 200105   - JohnTalintyre : AttachmentsUnderRevisionControl & meta data in topics
# 200106   - JohnTalintyre : Added Form capability (replaces Category tables)
# 200401   - RafaelAlvarez : Added a new Plugin callback (afterSaveHandler)
=begin twiki

---+ TWiki::Store Module

This module hosts the generic storage backend.

=cut

package TWiki::Store;

use File::Copy;
use Time::Local;

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

# ===========================
=pod

---++ sub initialize ()

Not yet documented.

=cut

sub initialize
{
    %templateVars = ();
    eval "use TWiki::Store::$TWiki::storeTopicImpl;";
}

=pod

---++ sub _traceExec ()

Normally writes no output, uncomment writeDebug line to get output of all RCS etc command to debug file

=cut

sub _traceExec
{
   #my( $cmd, $result ) = @_;
   #TWiki::writeDebug( "Store exec: $cmd -> $result" );
}

=pod

---++ sub writeDebug ()

Not yet documented.

=cut

sub writeDebug
{
   #TWiki::writeDebug( "Store: $_[0]" );
}

=pod

---++ sub _getTopicHandler (  $web, $topic, $attachment  )

Not yet documented.

=cut

sub _getTopicHandler
{
   my( $web, $topic, $attachment ) = @_;

   $attachment = "" if( ! $attachment );

   my $handlerName = "TWiki::Store::$TWiki::storeTopicImpl";

   my $handler = $handlerName->new( $web, $topic, $attachment, @TWiki::storeSettings );
   return $handler;
}


=pod

---++ sub normalizeWebTopicName (  $theWeb, $theTopic  )

Normalize a Web.TopicName
<pre>
Input:                      Return:
  ( "Web",  "Topic" )         ( "Web",  "Topic" )
  ( "",     "Topic" )         ( "Main", "Topic" )
  ( "",     "" )              ( "Main", "WebHome" )
  ( "",     "Web/Topic" )     ( "Web",  "Topic" )
  ( "",     "Web.Topic" )     ( "Web",  "Topic" )
  ( "Web1", "Web2.Topic" )    ( "Web2", "Topic" )
</pre>
Note: Function renamed from getWebTopic

=cut

sub normalizeWebTopicName
{
   my( $theWeb, $theTopic ) = @_;

   if( $theTopic =~ m|^([^.]+)[\.\/](.*)$| ) {
       $theWeb = $1;
       $theTopic = $2;
   }
   $theWeb = $TWiki::webName unless( $theWeb );
   $theTopic = $TWiki::topicName unless( $theTopic );

   return( $theWeb, $theTopic );
}


=pod

---++ sub erase (  $web, $topic  )

Get rid of a topic and its attachments completely
Intended for TEST purposes.
Use with GREAT CARE as file will be gone, including RCS history

=cut

sub erase
{
    my( $web, $topic ) = @_;

    my $topicHandler = _getTopicHandler( $web, $topic );
    $topicHandler->_delete();

    writeLog( "erase", "$web.$topic", "" );
}

=pod

---++ sub moveAttachment (  $oldWeb, $oldTopic, $newWeb, $newTopic, $theAttachment  )

Move an attachment from one topic to another.
If there is a problem an error string is returned.
The caller to this routine should check that all topics are valid and
do lock on the topics.

=cut

sub moveAttachment
{
    my( $oldWeb, $oldTopic, $newWeb, $newTopic, $theAttachment ) = @_;
    
    my $topicHandler = _getTopicHandler( $oldWeb, $oldTopic, $theAttachment );
    my $error = $topicHandler->moveMe( $newWeb, $newTopic );
    return $error if( $error );

    # Remove file attachment from old topic
    my( $meta, $text ) = readTopic( $oldWeb, $oldTopic );
    my %fileAttachment = $meta->findOne( "FILEATTACHMENT", $theAttachment );
    $meta->remove( "FILEATTACHMENT", $theAttachment );
    $error .= saveNew( $oldWeb, $oldTopic, $text, $meta, "", "", "", "doUnlock", "dont notify", "" ); 
    
    # Remove lock file
    $topicHandler->setLock( "" );
    
    # Add file attachment to new topic
    ( $meta, $text ) = readTopic( $newWeb, $newTopic );

    $fileAttachment{"movefrom"} = "$oldWeb.$oldTopic";
    $fileAttachment{"moveby"}   = $TWiki::userName;
    $fileAttachment{"movedto"}  = "$newWeb.$newTopic";
    $fileAttachment{"movedwhen"} = time();
    $meta->put( "FILEATTACHMENT", %fileAttachment );    
    
    $error .= saveNew( $newWeb, $newTopic, $text, $meta, "", "", "", "doUnlock", "dont notify", "" ); 
    # Remove lock file.
    my $newTopicHandler = _getTopicHandler( $newWeb, $newTopic, $theAttachment );
    $newTopicHandler->setLock( "" );
    
    writeLog( "move", "$oldWeb.$oldTopic", "Attachment $theAttachment moved to $newWeb.$newTopic" );

    return $error;
}

=pod

---++ sub changeRefTo (  $text, $oldWeb, $oldTopic  )

When moving a topic to another web, change within-web refs from this topic so that they'll work
when the topic is in the new web. I have a feeling this shouldn't be in Store.pm.

=cut

sub changeRefTo
{
   my( $text, $oldWeb, $oldTopic ) = @_;

   my $preTopic = '^|[\*\s\[][-\(\s]*';
   # I18N: match non-alpha before/after topic names
   my $alphaNum = $TWiki::regex{mixedAlphaNum};
   my $postTopic = '$|' . "[^${alphaNum}_.]" . '|\.\s';
   my $metaPreTopic = '"|[\s[,\(-]';
   my $metaPostTopic = "[^${alphaNum}_.]" . '|\.\s';
   
   my $out = "";
   
   # Get list of topics in $oldWeb, replace local refs to these topics with full web.topic
   # references
   my @topics = getTopicNames( $oldWeb );
   
   my $insidePRE = 0;
   my $insideVERBATIM = 0;
   my $noAutoLink = 0;
   
   foreach( split( /\n/, $text ) ) {
       if( /^%META:TOPIC(INFO|MOVED)/ ) {
           $out .= "$_\n";
           next;
       }

       # change state:
       m|<pre>|i  && ( $insidePRE = 1 );
       m|</pre>|i && ( $insidePRE = 0 );
       if( m|<verbatim>|i ) {
           $insideVERBATIM = 1;
       }
       if( m|</verbatim>|i ) {
           $insideVERBATIM = 0;
       }
       m|<noautolink>|i   && ( $noAutoLink = 1 );
       m|</noautolink>|i  && ( $noAutoLink = 0 );
   
       if( ! ( $insidePRE || $insideVERBATIM || $noAutoLink ) ) {
           # Fairly inefficient, time will tell if this should be changed.
           foreach my $topic ( @topics ) {
              if( $topic ne $oldTopic ) {
                  if( /^%META:/ ) {
                      s/($metaPreTopic)\Q$topic\E(?=$metaPostTopic)/$1$oldWeb.$topic/g;
                  } else {
                      s/($preTopic)\Q$topic\E(?=$postTopic)/$1$oldWeb.$topic/g;
                  }
              }
           }
       }
       $out .= "$_\n";
   }

   return $out;
}


=pod

---++ sub renameTopic (  $oldWeb, $oldTopic, $newWeb, $newTopic, $doChangeRefTo  )

Rename a topic, allowing for transfer between Webs
It is the responsibility of the caller to check for existence of webs,
topics & lock taken for topic

=cut

sub renameTopic
{
   my( $oldWeb, $oldTopic, $newWeb, $newTopic, $doChangeRefTo ) = @_;
   
   my $topicHandler = _getTopicHandler( $oldWeb, $oldTopic, "" );
   my $error = $topicHandler->moveMe( $newWeb, $newTopic );

   if( ! $error ) {
      my $time = time();
      my $user = $TWiki::userName;
      my @args = (
         "from" => "$oldWeb.$oldTopic",
         "to"   => "$newWeb.$newTopic",
         "date" => "$time",
         "by"   => "$user" );
      my $fullText = readTopicRaw( $newWeb, $newTopic );
      if( ( $oldWeb ne $newWeb ) && $doChangeRefTo ) {
         $fullText = changeRefTo( $fullText, $oldWeb, $oldTopic );
      }
      my ( $meta, $text ) = _extractMetaData( $newWeb, $newTopic, $fullText );
      $meta->put( "TOPICMOVED", @args );
      saveNew( $newWeb, $newTopic, $text, $meta, "", "", "", "unlock" );
   }
   
   # Log rename
   if( $TWiki::doLogRename ) {
      writeLog( "rename", "$oldWeb.$oldTopic", "moved to $newWeb.$newTopic $error" );
   }
   
   # Remove old lock file
   $topicHandler->setLock( "" );
   
   return $error;
}


=pod

---++ sub updateReferingPages (  $oldWeb, $oldTopic, $wikiUserName, $newWeb, $newTopic, @refs  )

Update pages that refer to the one being renamed/moved.

=cut

sub updateReferingPages
{
    my ( $oldWeb, $oldTopic, $wikiUserName, $newWeb, $newTopic, @refs ) = @_;

    my $lockFailure = 0;

    my $result = "";
    my $preTopic = '^|\W';		# Start of line or non-alphanumeric
    my $postTopic = '$|\W';	# End of line or non-alphanumeric
    my $spacedTopic = TWiki::Search::spacedTopic( $oldTopic );

    while ( @refs ) {
       my $type = shift @refs;
       my $item = shift @refs;
       my( $itemWeb, $itemTopic ) = TWiki::Store::normalizeWebTopicName( "", $item );
       if ( &TWiki::Store::topicIsLockedBy( $itemWeb, $itemTopic ) ) {
          $lockFailure = 1;
       } else {
          my $resultText = "";
          $result .= ":$item: , "; 
          #open each file, replace $topic with $newTopic
          if ( &TWiki::Store::topicExists($itemWeb, $itemTopic) ) { 
             my $scantext = &TWiki::Store::readTopicRaw($itemWeb, $itemTopic);
             if( ! &TWiki::Access::checkAccessPermission( "change", $wikiUserName, $scantext,
                    $itemWeb, $itemTopic ) ) {
                 # This shouldn't happen, as search will not return, but check to be on the safe side
                 &TWiki::writeWarning( "rename: attempt to change $itemWeb.$itemTopic without permission" );
                 next;
             }
	     my $insidePRE = 0;
	     my $insideVERBATIM = 0;
             my $noAutoLink = 0;
	     foreach( split( /\n/, $scantext ) ) {
	        if( /^%META:TOPIC(INFO|MOVED)/ ) {
	            $resultText .= "$_\n";
	            next;
	        }
		# FIXME This code is in far too many places - also in Search.pm and Store.pm
		m|<pre>|i  && ( $insidePRE = 1 );
		m|</pre>|i && ( $insidePRE = 0 );
		if( m|<verbatim>|i ) {
		    $insideVERBATIM = 1;
		}
		if( m|</verbatim>|i ) {
		    $insideVERBATIM = 0;
		}
		m|<noautolink>|i   && ( $noAutoLink = 1 );
		m|</noautolink>|i  && ( $noAutoLink = 0 );

		if( ! ( $insidePRE || $insideVERBATIM || $noAutoLink ) ) {
		    if( $type eq "global" ) {
			my $insertWeb = ($itemWeb eq $newWeb) ? "" : "$newWeb.";
			s/($preTopic)\Q$oldWeb.$oldTopic\E(?=$postTopic)/$1$insertWeb$newTopic/g;
		    } else {
			# Only replace bare topic (i.e. not preceeded by web) if web of referring
			# topic is in original Web of topic that's being moved
			if( $oldWeb eq $itemWeb ) {
			    my $insertWeb = ($oldWeb eq $newWeb) ? "" : "$newWeb.";
			    s/($preTopic)\Q$oldTopic\E(?=$postTopic)/$1$insertWeb$newTopic/g;
			    s/\[\[($spacedTopic)\]\]/[[$newTopic][$1]]/gi;
			}
		    }
		}
	        $resultText .= "$_\n";
	     }
	     my ( $meta, $text ) = &TWiki::Store::_extractMetaData( $itemWeb, $itemTopic, $resultText );
	     &TWiki::Store::saveTopic( $itemWeb, $itemTopic, $text, $meta, "", "unlock", "dontNotify", "" );
          } else {
	    $result .= ";$item does not exist;";
          }
       }
    }
    return ( $lockFailure, $result );
}


=pod

---++ sub readTopicVersion (  $theWeb, $theTopic, $theRev  )

Read a specific version of a topic
<pre>view:     $text= &TWiki::Store::readTopicVersion( $topic, "1.$rev" );</pre>

=cut

sub readTopicVersion
{
    my( $theWeb, $theTopic, $theRev ) = @_;
    my $text = _readVersionNoMeta( $theWeb, $theTopic, $theRev );
    my $meta = "";
   
    ( $meta, $text ) = _extractMetaData( $theWeb, $theTopic, $text );
        
    return( $meta, $text );
}

=pod

---++ sub _readVersionNoMeta (  $theWeb, $theTopic, $theRev  )

Read a specific version of a topic

=cut

sub _readVersionNoMeta
{
    my( $theWeb, $theTopic, $theRev ) = @_;
    my $topicHandler = _getTopicHandler( $theWeb, $theTopic );
    
    $theRev =~ s/^1\.//o;
    return $topicHandler->getRevision( $theRev );
}

=pod

---++ sub readAttachmentVersion (  $theWeb, $theTopic, $theAttachment, $theRev  )

Not yet documented.

=cut

sub readAttachmentVersion
{
   my ( $theWeb, $theTopic, $theAttachment, $theRev ) = @_;
   
   my $topicHandler = _getTopicHandler( $theWeb, $theTopic, $theAttachment );
   $theRev =~ s/^1\.//o;
   return $topicHandler->getRevision( $theRev );
}

=pod

---++ sub getRevisionNumber (  $theWebName, $theTopic, $attachment  )

Use meta information if available ...

=cut

sub getRevisionNumber
{
    my( $theWebName, $theTopic, $attachment ) = @_;
    my $ret = getRevisionNumberX( $theWebName, $theTopic, $attachment );
    ##TWiki::writeDebug( "Store: rev = $ret" );
    if( ! $ret ) {
       $ret = "1.1"; # Temporary
    }
    
    return $ret;
}


=pod

---++ sub getRevisionNumberX (  $theWebName, $theTopic, $attachment  )

Latest revision number. <br/>
Returns "" if there is no revision.

=cut

sub getRevisionNumberX
{
    my( $theWebName, $theTopic, $attachment ) = @_;
    if( ! $theWebName ) {
        $theWebName = $TWiki::webName;
    }
    if( ! $attachment ) {
        $attachment = "";
    }
    
    my $topicHandler = _getTopicHandler( $theWebName, $theTopic, $attachment );
    my $revs = $topicHandler->numRevisions();
    $revs = "1.$revs" if( $revs );
    return $revs;
}


=pod

---++ sub getRevisionDiff (  $web, $topic, $rev1, $rev2  )

<pre>
rdiff:            $text = &TWiki::Store::getRevisionDiff( $webName, $topic, "1.$r2", "1.$r1" );
</pre>

=cut

sub getRevisionDiff
{
    my( $web, $topic, $rev1, $rev2 ) = @_;

    my $rcs = _getTopicHandler( $web, $topic );
    my $r1 = substr( $rev1, 2 );
    my $r2 = substr( $rev2, 2 );
    my( $error, $diff ) = $rcs->revisionDiff( $r1, $r2 );
    return $diff;
}


# =========================
# Call getRevisionInfoFromMeta for faster response for topics
# FIXME try and get rid of this it's a mess
# In direct calls changeToIsoDate always seems to be 1

=pod

---+++ getRevisionInfo($theWebName, $theTopic, $theRev, $attachment, $topicHandler) ==> ( $theWebName, $theTopic, $theRev, $attachment, $topicHandler ) 
| Description: | Get revision info of a topic |
| Parameter: =$theWebName= | Web name, optional, e.g. ="Main"= |
| Parameter: =$theTopic= | Topic name, required, e.g. ="TokyoOffice"= |
| Return: =( $date, $user, $rev, $comment )= | List with: ( last update date, login name of last user, minor part of top revision number ), e.g. =( 1234561, "phoeny", "5" )= |
| $date | in epochSec |
| $user | |
| $rev | TODO: this needs to be improves to contain the major number too (and what do we do is we have a different numbering system?) |
| $comment | WHAT COMMENT? |

=cut

sub getRevisionInfo
{
    my( $theWebName, $theTopic, $theRev, $attachment, $topicHandler ) = @_;
    if( ! $theWebName ) {
        $theWebName = $TWiki::webName;
    }

    $theRev = "" unless( $theRev );
    $theRev =~ s/^1\.//o;

    $topicHandler = _getTopicHandler( $theWebName, $theTopic, $attachment ) if( ! $topicHandler );
    my( $rcsOut, $rev, $date, $user, $comment ) = $topicHandler->getRevisionInfo( $theRev );
    
    return ( $date, $user, $rev, $comment );
}


=pod

---++ sub topicIsLockedBy (  $theWeb, $theTopic  )

| returns  ( $lockUser, $lockTime ) | ( "", 0 ) if not locked |

=cut

sub topicIsLockedBy
{
    my( $theWeb, $theTopic ) = @_;

    # pragmatic approach: Warn user if somebody else pressed the
    # edit link within a time limit e.g. 1 hour

    ( $theWeb, $theTopic ) = normalizeWebTopicName( $theWeb, $theTopic );

    my $lockFilename = "$TWiki::dataDir/$theWeb/$theTopic.lock";
    if( ( -e "$lockFilename" ) && ( $TWiki::editLockTime > 0 ) ) {
        my $tmp = readFile( $lockFilename );
        my( $lockUser, $lockTime ) = split( /\n/, $tmp );
        if( $lockUser ne $TWiki::userName ) {
            # time stamp of lock within editLockTime of current time?
            my $systemTime = time();
            # calculate remaining lock time in seconds
            $lockTime = $lockTime + $TWiki::editLockTime - $systemTime;
            if( $lockTime > 0 ) {
                # must warn user that it is locked
                return( $lockUser, $lockTime );
            }
        }
    }
    return( "", 0 );
}


=pod

---++ sub keyValue2list (  $args  )

Not yet documented.

=cut

sub keyValue2list
{
    my( $args ) = @_;
    
    my @res = ();
    
    # Format of data is name="value" name1="value1" [...]
    while( $args =~ s/\s*([^=]+)=\"([^"]*)\"//o ) { #" avoid confusing syntax highlighters
        push @res, $1;
        push @res, $2;
    }
    
    return @res;
}


=pod

---++ sub metaAddTopicData (  $web, $topic, $rev, $meta, $forceDate, $forceUser  )

Not yet documented.

=cut

sub metaAddTopicData
{
    my( $web, $topic, $rev, $meta, $forceDate, $forceUser ) = @_;

    my $time = $forceDate || time();
    my $user = $forceUser || $TWiki::userName;

    my @args = (
       "version" => "$rev",
       "date"    => "$time",
       "author"  => "$user",
       "format"  => $TWiki::formatVersion );
    $meta->put( "TOPICINFO", @args );
}


=pod

---++ sub saveTopicNew (  $web, $topic, $text, $metaData, $saveCmd, $doUnlock, $dontNotify, $dontLogSave  )

Not yet documented.

=cut

sub saveTopicNew
{
    my( $web, $topic, $text, $metaData, $saveCmd, $doUnlock, $dontNotify, $dontLogSave ) = @_;
    my $attachment = "";
    my $meta = TWiki::Meta->new();
    $meta->readArray( @$metaData );
    saveNew( $web, $topic, $text, $meta, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify );
}

=pod

---++ sub saveTopic (  $web, $topic, $text, $meta, $saveCmd, $doUnlock, $dontNotify, $dontLogSave, $forceDate  )

Not yet documented.

=cut

sub saveTopic
{
    my( $web, $topic, $text, $meta, $saveCmd, $doUnlock, $dontNotify, $dontLogSave, $forceDate ) = @_;
    my $attachment = "";
    my $comment = "";

    # FIXME: Inefficient code that hides meta data from Plugin callback
    $text = $meta->write( $text );  # add meta data for Plugin callback
    TWiki::Plugins::beforeSaveHandler( $text, $topic, $web );
    $meta = TWiki::Meta->remove();  # remove all meta data
    $text = $meta->read( $text );   # restore meta data

    my $error = saveNew( $web, $topic, $text, $meta, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $comment, $forceDate );
    $text = $meta->write( $text );  # add meta data for Plugin callback
    TWiki::Plugins::afterSaveHandler( $text, $topic, $web, $error );
    return $error;
}

=pod

---++ sub saveAttachment ()

Not yet documented.

=cut

sub saveAttachment
{
    my( $web, $topic, $text, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $theTmpFilename,
        $forceDate) = @_;
        
    my $topicHandler = _getTopicHandler( $web, $topic, $attachment );
    my $error = $topicHandler->addRevision( $theTmpFilename, $theComment, $TWiki::userName );
    $topicHandler->setLock( ! $doUnlock );
    
    return $error;
}


=pod

---++ sub save (  $web, $topic, $text, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $forceDate  )

Not yet documented.

=cut

sub save
{
    my( $web, $topic, $text, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $forceDate ) = @_;
    
    # FIXME get rid of this routine
    
    my $meta = TWiki::Meta->new();
    
    return saveNew( $web, $topic, $text, $meta, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $forceDate );
}


=pod

---++ sub _addMeta (  $web, $topic, $text, $attachment, $nextRev, $meta, $forceDate, $forceUser  )

Add meta data to the topic.

=cut

sub _addMeta
{
    my( $web, $topic, $text, $attachment, $nextRev, $meta, $forceDate, $forceUser ) = @_;
    
    if( ! $attachment ) {
        $nextRev = "1.1" if( ! $nextRev );
        metaAddTopicData(  $web, $topic, $nextRev, $meta, $forceDate, $forceUser );
        $text = $meta->write( $text );        
    }
    
    return $text;
}


=pod

---++ sub saveNew (  $web, $topic, $text, $meta, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $forceDate  )

Return non-null string if there is an (RCS) error. <br/>
FIXME: does rev info from meta work if user saves a topic with no change?

=cut

sub saveNew
{
    my( $web, $topic, $text, $meta, $saveCmd, $attachment, $dontLogSave, $doUnlock, $dontNotify, $theComment, $forceDate ) = @_;
    my $time = time();
    my $tmp = "";
    my $rcsError = "";
    my $dataError = "";
    
    my $topicHandler = _getTopicHandler( $web, $topic, $attachment );

    my $currentRev = $topicHandler->numRevisions();
    
    my $nextRev    = "";
    if( ! $currentRev ) {
        $nextRev = "1.1";
    } else {
        $nextRev = "1." . ($currentRev + 1);
    }
    $currentRev = "1." . $currentRev if( $currentRev );

    if( ! $attachment ) {
        # RCS requires a newline for the last line,
        # so add newline if needed
        $text =~ s/([^\n\r])$/$1\n/os;
    }
    
    if( ! $theComment ) {
       $theComment = "none";
    }

    #### Normal Save
    if( ! $saveCmd ) {
        $saveCmd = "";

        # get time stamp of existing file
        my $mtime1 = $topicHandler->getTimestamp();
        my $mtime2 = time();

        # how close time stamp of existing file to now?
        if( abs( $mtime2 - $mtime1 ) < $TWiki::editLockTime ) {
            # FIXME no previous topic?
            my( $date, $user ) = getRevisionInfo( $web, $topic, $currentRev, $attachment, $topicHandler );
            # TWiki::writeDebug( "Store::save date = $date" );
            # same user?
            if( ( $TWiki::doKeepRevIfEditLock ) && ( $user eq $TWiki::userName ) && $currentRev ) { # TODO shouldn't this also check to see if its still locked?
                # replace last repository entry
                $saveCmd = "repRev";
                if( $attachment ) {
                   $saveCmd = ""; # cmd option not supported for attachments.
                }
            }
        }
        
        if( $saveCmd ne "repRev" ) {
            $text = _addMeta( $web, $topic, $text, $attachment, $nextRev, $meta, $forceDate );

            $dataError = $topicHandler->addRevision( $text, $theComment, $TWiki::userName );
            return $dataError if( $dataError );
            
            $topicHandler->setLock( ! $doUnlock );

            if( ! $dontNotify ) {
                # update .changes
                my( $fdate, $fuser, $frev ) = getRevisionInfo( $web, $topic, "", $attachment, $topicHandler );
                $fdate = ""; # suppress warning
                $fuser = ""; # suppress warning

                my @foo = split( /\n/, &readFile( "$TWiki::dataDir/$TWiki::webName/.changes" ) );
                if( $#foo > 100 ) {
                    shift( @foo);
                }
                push( @foo, "$topic\t$TWiki::userName\t$time\t$frev" );
                open( FILE, ">$TWiki::dataDir/$TWiki::webName/.changes" );
                print FILE join( "\n", @foo )."\n";
                close(FILE);
            }

            if( ( $TWiki::doLogTopicSave ) && ! ( $dontLogSave ) ) {
                # write log entry
                my $extra = "";
                $extra   .= "dontNotify" if( $dontNotify );
                writeLog( "save", "$TWiki::webName.$topic", $extra );
            }
        }
    }

    #### Replace Revision Save
    if( $saveCmd eq "repRev" ) {
        # fix topic by replacing last revision, but do not update .changes

        # save topic with same userName and date
        # FIXME why should date be the same if same user replacing with editLockTime?
        my( $date, $user, $rev ) = getRevisionInfo( $web, $topic, "", $attachment, $topicHandler );
        $rev = "1.$rev";

        # Add one minute (make small difference, but not too big for notification)
        my $epochSec = $date + 60; #TODO: this seems wrong. if editLockTime == 3600, and i edit, 30 mins later... why would the recorded date be 29 mins too early?
        $text = _addMeta( $web, $topic, $text, $attachment, $rev,
                          $meta, $epochSec, $user );

        my $dataError = $topicHandler->replaceRevision( $text, $theComment, $user, $epochSec );
        return $dataError if( $dataError );
        $topicHandler->setLock( ! $doUnlock );

        if( ( $TWiki::doLogTopicSave ) && ! ( $dontLogSave ) ) {
            # write log entry
            my $extra = "repRev $rev ";
            $extra   .= &TWiki::userToWikiName( $user );
            $date = &TWiki::formatTime( $epochSec, "rcs", "gmtime" );
            $extra   .= " $date";
            $extra   .= " dontNotify" if( $dontNotify );
            writeLog( "save", "$TWiki::webName.$topic", $extra );
        }
    }

    #### Delete Revision
    if( $saveCmd eq "delRev" ) {
        # delete last revision

        # delete last entry in repository (unlock, delete revision, lock operation)
        my $rev = getRevisionNumber( $web, $topic );
        if( $rev eq "1.1" ) {
            # can't delete initial revision
            return;
        }
        my $dataError = $topicHandler->deleteRevision();
        return $dataError if( $dataError );

        # restore last topic from repository
        $topicHandler->restoreLatestRevision();
        $topicHandler->setLock( ! $doUnlock );

        # delete entry in .changes : FIXME

        if( $TWiki::doLogTopicSave ) {
            # write log entry
            writeLog( "cmd", "$TWiki::webName.$topic", "delRev $rev" );
        }
    }
    return ""; # all is well
}

=pod

---++ sub writeLog (  $action, $webTopic, $extra, $user  )

Not yet documented.

=cut

sub writeLog
{
    my( $action, $webTopic, $extra, $user ) = @_;

    # use local time for log, not UTC (gmtime)

    my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime( time() );
    my( $tmon) = $TWiki::isoMonth[$mon];
    $year = sprintf( "%.4u", $year + 1900 );  # Y2K fix
    my $time = sprintf( "%.2u ${tmon} %.2u - %.2u:%.2u", $mday, $year, $hour, $min );
    my $yearmonth = sprintf( "%.4u%.2u", $year, $mon+1 );

    my $wuserName = $user || $TWiki::userName;
    $wuserName = &TWiki::userToWikiName( $wuserName );
    my $remoteAddr = $ENV{'REMOTE_ADDR'} || "";
    my $text = "| $time | $wuserName | $action | $webTopic | $extra | $remoteAddr |";

    my $filename = $TWiki::logFilename;
    $filename =~ s/%DATE%/$yearmonth/go;
    open( FILE, ">>$filename");
    print FILE "$text\n";
    close( FILE);
}

=pod

---++ sub saveFile (  $name, $text  )

Not yet documented.

=cut

sub saveFile
{
    my( $name, $text ) = @_;
    
    umask( 002 );
    unless ( open( FILE, ">$name" ) )  {
	warn "Can't create file $name - $!\n";
	return;
    }
    print FILE $text;
    close( FILE);
}

=pod

---++ sub lockTopic (  $name, $doUnlock  )

Not yet documented.

=cut

sub lockTopic
{
   my ( $name, $doUnlock ) = @_;

   lockTopicNew( $TWiki::webName, $name, $doUnlock );
}

=pod

---++ sub lockTopicNew (  $theWeb, $theTopic, $doUnlock  )

Not yet documented. <br/>
Called from rename and =TWiki::Func=

=cut

sub lockTopicNew
{
    my( $theWeb, $theTopic, $doUnlock ) = @_;

    ( $theWeb, $theTopic ) = normalizeWebTopicName( $theWeb, $theTopic );
    
    my $topicHandler = _getTopicHandler( $theWeb, $theTopic );
    $topicHandler->setLock( ! $doUnlock );
}

=pod

---++ sub removeObsoleteTopicLocks (  $web  )

Not yet documented.

=cut

sub removeObsoleteTopicLocks
{
    my( $web ) = @_;

    # Clean all obsolete .lock files in a web.
    # This should be called regularly, best from a cron job (called from mailnotify)

    my $webDir = "$TWiki::dataDir/$web";
    opendir( DIR, "$webDir" );
    my @fileList = grep /\.lock$/, readdir DIR;
    closedir DIR;
    my $file = "";
    my $pathFile = "";
    my $lockUser = "";
    my $lockTime = 0;
    my $systemTime = time();
    foreach $file ( @fileList ) {
        $pathFile = "$webDir/$file";
        $pathFile =~ /(.*)/;
        $pathFile = $1;       # untaint file
        ( $lockUser, $lockTime ) = split( /\n/, readFile( "$pathFile" ) );
        $lockTime = 0 unless( $lockTime );

        # time stamp of lock over one hour of current time?
        if( abs( $systemTime - $lockTime ) > $TWiki::editLockTime ) {
            # obsolete, so delete file
            unlink "$pathFile";
        }
    }
}

=pod

---++ Functions: Content Handling

---+++ webExists( $web ) ==> $flag

| Description: | Test if web exists |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =$flag= | ="1"= if web exists, ="0"= if not |

=cut

sub webExists
{
    my( $theWeb ) = @_;
    return -e "$TWiki::dataDir/$theWeb";
}

=pod

---+++ topicExists( $web, $topic ) ==> $flag

| Description: | Test if topic exists |
| Parameter: =$web= | Web name, optional, e.g. ="Main"= |
| Parameter: =$topic= | Topic name, required, e.g. ="TokyoOffice"=, or ="Main.TokyoOffice"= |
| Return: =$flag= | ="1"= if topic exists, ="0"= if not |

=cut

sub topicExists
{
    my( $theWeb, $theTopic ) = @_;
    ( $theWeb, $theTopic ) = normalizeWebTopicName( $theWeb, $theTopic );
    return -e "$TWiki::dataDir/$theWeb/$theTopic.txt";
}

=pod

---++ sub getRevisionInfoFromMeta (  $web, $topic, $meta  )

Try and get from meta information in topic, if this can't be done then use RCS.
Note there is no "1." prefix to this data

=cut

sub getRevisionInfoFromMeta
{
    my( $web, $topic, $meta ) = @_;
    
    my( $date, $author, $rev );
    my %topicinfo = ();
    
    if( $meta ) {
        %topicinfo = $meta->findOne( "TOPICINFO" );
    }
        
    if( %topicinfo ) {
       # Stored as meta data in topic for faster access
       $date = $topicinfo{"date"} ;
       $author = $topicinfo{"author"};
       my $tmp = $topicinfo{"version"};
       $tmp =~ /1\.(.*)/o;
       $rev = $1;
    } else {
       # Get data from RCS
       ( $date, $author, $rev ) = getRevisionInfo( $web, $topic, "" );
    }
    
    # writeDebug( "rev = $rev" );
    
    return( $date, $author, $rev );
}

=pod

---++ sub convert2metaFormat (  $web, $topic, $text  )

Not yet documented.

=cut

sub convert2metaFormat
{
    my( $web, $topic, $text ) = @_;
    
    my $meta = TWiki::Meta->new();
    $text = $meta->read( $text );
     
    if ( $text =~ /<!--TWikiAttachment-->/ ) {
       $text = TWiki::Attach::migrateToFileAttachmentMacro( $meta, $text );
    }
    
    if ( $text =~ /<!--TWikiCat-->/ ) {
       $text = TWiki::Form::upgradeCategoryTable( $web, $topic, $meta, $text );    
    }
    
    return( $meta, $text );
}

=pod

---++ sub _extractMetaData (  $web, $topic, $fulltext  )

Expect meta data at top of file, but willing to accept it anywhere.
If we have an old file format without meta data, then convert.

=cut

sub _extractMetaData
{
    my( $web, $topic, $fulltext ) = @_;
    
    my $meta = TWiki::Meta->new();
    my $text = $meta->read( $fulltext );

    
    # If there is no meta data then convert
    if( ! $meta->count( "TOPICINFO" ) ) {
        ( $meta, $text ) = convert2metaFormat( $web, $topic, $text );
    } else {
        my %topicinfo = $meta->findOne( "TOPICINFO" );
        if( $topicinfo{"format"} eq "1.0beta" ) {
            # This format used live at DrKW for a few months
            if( $text =~ /<!--TWikiCat-->/ ) {
               $text = TWiki::Form::upgradeCategoryTable( $web, $topic, $meta, $text );
            }
            
            TWiki::Attach::upgradeFrom1v0beta( $meta );
            
            if( $meta->count( "TOPICMOVED" ) ) {
                 my %moved = $meta->findOne( "TOPICMOVED" );
                 $moved{"by"} = TWiki::wikiToUserName( $moved{"by"} );
                 $meta->put( "TOPICMOVED", %moved );
            }
        }
    }
    
    return( $meta, $text );
}

=pod

---++ sub getFileName (  $theWeb, $theTopic, $theAttachment  )

Not yet documented. <br/>
*FIXME - get rid of this because uses private part of handler*

=cut

sub getFileName
{
    my( $theWeb, $theTopic, $theAttachment ) = @_;

    my $topicHandler = _getTopicHandler( $theWeb, $theTopic, $theAttachment );
    return $topicHandler->{file};
}

=pod

---++ sub readTopMeta (  $theWeb, $theTopic  )

Just read the meta data at the top of the topic. <br/>
Generalise for Codev.DataFramework, but needs to be fast because
of use by view script.

=cut

sub readTopMeta
{
    my( $theWeb, $theTopic ) = @_;
    
    my $topicHandler = _getTopicHandler( $theWeb, $theTopic );
    my $filename = getFileName( $theWeb, $theTopic );
    
    my $data = "";
    my $line;
    $/ = "\n";     # read line by line
    open( IN_FILE, "<$filename" ) || return "";
    while( ( $line = <IN_FILE> ) ) {
        if( $line !~ /^%META:/ ) {
           last;
        } else {
           $data .= $line;
        }
    }
    
    my( $meta, $text ) = _extractMetaData( $theWeb, $theTopic, $data );
    
    close( IN_FILE );

    return $meta;
}

=pod

---++ readTopic( $web, $topic, $internal )
Return value: ( $metaObject, $topicText )

Reads the most recent version of a topic.  If $internal is false, view
permission will be required for the topic read to be successful.  A failed
topic read is indicated by setting $TWiki::readTopicPermissionFailed.

The metadata and topic text are returned separately, with the metadata in a
TWiki::Meta object.  (The topic text is, as usual, just a string.)

=cut

sub readTopic
{
    my( $theWeb, $theTopic, $internal ) = @_;
    
    my $fullText = readTopicRaw( $theWeb, $theTopic, "", $internal );
    
    my ( $meta, $text ) = _extractMetaData( $theWeb, $theTopic, $fullText );
    
    return( $meta, $text );
}

=pod

---++ sub readWebTopic (  $theWeb, $theName  )

Not yet documented.

=cut

sub readWebTopic
{
    my( $theWeb, $theName ) = @_;
    my $text = &readFile( "$TWiki::dataDir/$theWeb/$theName.txt" );
    
    return $text;
}

=pod

---++ readTopicRaw( $web, $topic, $version, $internal )
Return value: $topicText

Reads a topic; the most recent version is used unless $version is specified.
If $internal is false, view access permission will be checked.  If permission
is not granted, then an error message will be returned in $text, and set in
$TWiki::readTopicPermissionFailed.

=cut

sub readTopicRaw
{
    my( $theWeb, $theTopic, $theVersion, $internal ) = @_;

    #SVEN - test if theTopic contains a webName to override $theWeb
    ( $theWeb, $theTopic ) = normalizeWebTopicName( $theWeb, $theTopic );

    my $text = "";
    if( ! $theVersion ) {
        $text = &readFile( "$TWiki::dataDir/$theWeb/$theTopic.txt" );
    } else {
        $text = _readVersionNoMeta( $theWeb, $theTopic, $theVersion);
    }

    my $viewAccessOK = 1;
    unless( $internal ) {
        $viewAccessOK = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName, $text, $theTopic, $theWeb );
        # TWiki::writeDebug( "readTopicRaw $viewAccessOK $TWiki::wikiUserName $theWeb $theTopic" );
    }
    
    unless( $viewAccessOK ) {
        # FIXME: TWiki::Func::readTopicText will break if the following text changes
        $text = "No permission to read topic $theWeb.$theTopic\n";
        # Could note inability to read so can divert to viewauth or similar
        $TWiki::readTopicPermissionFailed = "$TWiki::readTopicPermissionFailed $theWeb.$theTopic";
    }

    return $text;
}


=pod

---++ sub readTemplateTopic (  $theTopicName  )

Not yet documented.

=cut

sub readTemplateTopic
{
    my( $theTopicName ) = @_;

    $theTopicName =~ s/$TWiki::securityFilter//go;    # zap anything suspicious

    # try to read in current web, if not read from TWiki web

    my $web = $TWiki::twikiWebname;
    if( topicExists( $TWiki::webName, $theTopicName ) ) {
        $web = $TWiki::webName;
    }
    return readTopic( $web, $theTopicName );
}

=pod

---++ _readTemplateFile (  $theName, $theSkin  )
Return value: raw template text, or "" if read fails

WARNING! THIS FUNCTION DEPENDS ON GLOBAL VARIABLES

PRIVATE Reads a template, constructing a candidate name for the template thus: $name.$skin.tmpl,
and looking for a file of that name first in templates/$web and then if that fails in templates/.
If a template is not found, tries to parse $name into a web name and a topic name, and
read topic $Web.${Skin}Skin${Topic}Template. If $name does not contain a web specifier,
$Web defaults to TWiki::twikiWebname. If no skin is specified, topic is ${Topic}Template.
If the topic exists, checks access permissions and reads the topic
without meta-data. In the event that the read fails (template not found, access permissions fail)
returns the empty string "". skin, web and topic names are forced to an upper-case first character
when composing user topic names.

=cut

sub _readTemplateFile
{
    my( $theName, $theSkin, $theWeb ) = @_;
    $theSkin = "" unless $theSkin; # prevent 'uninitialized value' warnings

    # CrisBailiff, PeterThoeny 13 Jun 2000: Add security
    $theName =~ s/$TWiki::securityFilter//go;    # zap anything suspicious
    $theName =~ s/\.+/\./g;                      # Filter out ".." from filename
    $theSkin =~ s/$TWiki::securityFilter//go;    # zap anything suspicious
    $theSkin =~ s/\.+/\./g;                      # Filter out ".." from filename

    my $tmplFile = "";

    # search first in twiki/templates/Web dir
    # for file script(.skin).tmpl
    my $tmplDir = "$TWiki::templateDir/$theWeb";
    if( opendir( DIR, $tmplDir ) ) {
        # for performance use readdir, not a row of ( -e file )
        my @filelist = grep /^$theName\..*tmpl$/, readdir DIR;
        closedir DIR;
        $tmplFile = "$theName.$theSkin.tmpl";
        if( ! grep { /^$tmplFile$/ } @filelist ) {
            $tmplFile = "$theName.tmpl";
            if( ! grep { /^$tmplFile$/ } @filelist ) {
                $tmplFile = "";
            }
        }
        if( $tmplFile ) {
            $tmplFile = "$tmplDir/$tmplFile";
        }
    }

    # if not found, search in twiki/templates dir
    $tmplDir = $TWiki::templateDir;
    if( ( ! $tmplFile ) && ( opendir( DIR, $tmplDir ) ) ) {
        my @filelist = grep /^$theName\..*tmpl$/, readdir DIR;
        closedir DIR;
        $tmplFile = "$theName.$theSkin.tmpl";
        if( ! grep { /^$tmplFile$/ } @filelist ) {
            $tmplFile = "$theName.tmpl";
            if( ! grep { /^$tmplFile$/ } @filelist ) {
                $tmplFile = "";
            }
        }
        if( $tmplFile ) {
            $tmplFile = "$tmplDir/$tmplFile";
        }
    }

    # See if it is a user topic. Search first in current web
    # twiki web. Note that neither web nor topic may be variables when used in a template.
    if ( ! $tmplFile ) {
	if ( $theSkin ne "" ) {
	    $theSkin = ucfirst( $theSkin ) . "Skin";
	}

	my $theTopic;
	my $theWeb;

	if ( $theName =~ /^(\w+)\.(\w+)$/ ) {
	    $theWeb = ucfirst( $1 );
	    $theTopic = ucfirst( $2 );
	} else {
	    $theWeb = $TWiki::webName;
	    $theTopic = $theSkin . ucfirst( $theName ) . "Template";
	    if ( !TWiki::Store::topicExists( $theWeb, $theTopic )) {
		$theWeb = $TWiki::twikiWebname;
	    }
	}

	if ( TWiki::Store::topicExists( $theWeb, $theTopic ) &&
		TWiki::Access::checkAccessPermission( "view",
		    $TWiki::wikiUserName, "", $theTopic, $theWeb )) {
	    my ( $meta, $text ) = TWiki::Store::readTopic( $theWeb, $theTopic, 1 );
	    return $text;
	}
    }

    # read the template file
    if( -e $tmplFile ) {
        return &readFile( $tmplFile );
    }
    return "";
}

=pod

---++ sub handleTmplP (  $theVar  )
Return value: expanded text of the named template, as found from looking in the global register of template definitions.

WARNING! THIS FUNCTION DEPENDS ON GLOBAL VARIABLES

If $theVar is the name of a previously defined template, returns the text of
that template after recursive expansion of any TMPL:P tags it contains.

=cut

sub handleTmplP
{
    # Print template variable, called by %TMPL:P{"$theVar"}%
    my( $theVar ) = @_;

    my $val = "";
    if( ( %templateVars ) && ( exists $templateVars{ $theVar } ) ) {
        $val = $templateVars{ $theVar };
        $val =~ s/%TMPL\:P{[\s\"]*(.*?)[\"\s]*}%/&handleTmplP($1)/geo;  # recursion
    }
    if( ( $theVar eq "sep" ) && ( ! $val ) ) {
        # set separator explicitely if not set
        $val = " | ";
    }
    return $val;
}

=pod

---++ sub readTemplate ( $theName, $theSkin, $theWeb )
Return value: expanded template text

WARNING! THIS IS A SIDE-EFFECTING FUNCTION

PUBLIC Reads a template, constructing a candidate name for the template as described in
_readTemplateFile.

If template text is found, extracts include statements and fully expands them.
Also extracts template definitions and adds them to the
global templateVars hash, overwriting any previous definition.

=cut

sub readTemplate
{
    my( $theName, $theSkin, $theWeb ) = @_;

    if( ! defined($theSkin) ) {
        $theSkin = &TWiki::getSkin();
    }

    if( ! defined( $theWeb ) ) {
      $theWeb = $TWiki::webName;
    }

    # recursively read template file(s)
    my $text = _readTemplateFile( $theName, $theSkin, $theWeb );
    while( $text =~ /%TMPL\:INCLUDE{[\s\"]*(.*?)[\"\s]*}%/s ) {
        $text =~ s/%TMPL\:INCLUDE{[\s\"]*(.*?)[\"\s]*}%/&_readTemplateFile( $1, $theSkin, $theWeb )/geo;
    }

    if( ! ( $text =~ /%TMPL\:/s ) ) {
        # no template processing
        $text =~ s|^(( {3})+)|"\t" x (length($1)/3)|geom;  # leading spaces to tabs
        return $text;
    }

    my $result = "";
    my $key  = "";
    my $val  = "";
    my $delim = "";
    foreach( split( /(%TMPL\:)/, $text ) ) {
        if( /^(%TMPL\:)$/ ) {
            $delim = $1;
        } elsif( ( /^DEF{[\s\"]*(.*?)[\"\s]*}%[\n\r]*(.*)/s ) && ( $1 ) ) {
            # handle %TMPL:DEF{"key"}%
            if( $key ) {
                $templateVars{ $key } = $val;
            }
            $key = $1;
            $val = $2 || "";

        } elsif( /^END%[\n\r]*(.*)/s ) {
            # handle %TMPL:END%
            $templateVars{ $key } = $val;
            $key = "";
            $val = "";
            $result .= $1 || "";

        } elsif( $key ) {
            $val    .= "$delim$_";

        } else {
            $result .= "$delim$_";
        }
    }

    # handle %TMPL:P{"..."}% recursively
    $result =~ s/%TMPL\:P{[\s\"]*(.*?)[\"\s]*}%/&handleTmplP($1)/geo;
    $result =~ s|^(( {3})+)|"\t" x (length($1)/3)|geom;  # leading spaces to tabs
    return $result;
}

=pod

---++ readFile( $filename )
Return value: $fileContents

Returns the entire contents of the given file, which can be specified in any
format acceptable to the Perl open() function.  SECURITY NOTE: make sure
any $filename coming from a user is stripped of special characters that might
change Perl's open() semantics.

=cut

sub readFile
{
    my( $name ) = @_;
    my $data = "";
    undef $/; # set to read to EOF
    open( IN_FILE, "<$name" ) || return "";
    $data = <IN_FILE>;
    $/ = "\n";
    close( IN_FILE );
    $data = "" unless $data; # no undefined
    return $data;
}


=pod

---++ sub readFileHead (  $name, $maxLines  )

Not yet documented.

=cut

sub readFileHead
{
    my( $name, $maxLines ) = @_;
    my $data = "";
    my $line;
    my $l = 0;
    $/ = "\n";     # read line by line
    open( IN_FILE, "<$name" ) || return "";
    while( ( $l < $maxLines ) && ( $line = <IN_FILE> ) ) {
        $data .= $line;
        $l += 1;
    }
    close( IN_FILE );
    return $data;
}


#AS 5 Dec 2000 collect all Web's topic names

=pod

---+++ getTopicNames( $web ) ==> @topics

| Description: | Get list of all topics in a web |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =@topics= | Topic list, e.g. =( "WebChanges",  "WebHome", "WebIndex", "WebNotify" )= |

=cut

sub getTopicNames {
    my( $web ) = @_ ;

    if( !defined $web ) {
	$web="";
    }

    #FIXME untaint web name?

    # get list of all topics by scanning $dataDir
    opendir DIR, "$TWiki::dataDir/$web" ;
    my @tmpList = readdir( DIR );
    closedir( DIR );

    # this is not magic, it just looks like it.
    my @topicList = sort
        grep { s#^.+/([^/]+)\.txt$#$1# }
        grep { ! -d }
        map  { "$TWiki::dataDir/$web/$_" }
        grep { ! /^\.\.?$/ } @tmpList;

    return @topicList ;    
}
#/AS


#AS 5 Dec 2000 collect immediate subWeb names

=pod

---++ sub getSubWebs (  $web  )

Not yet documented.

=cut

sub getSubWebs {
    my( $web ) = @_ ;
    
    if( !defined $web ) {
	$web="";
    }

    #FIXME untaint web name?

    # get list of all subwebs by scanning $dataDir
    opendir DIR, "$TWiki::dataDir/$web" ;
    my @tmpList = readdir( DIR );
    closedir( DIR );

    # this is not magic, it just looks like it.
    my @webList = sort
        grep { s#^.+/([^/]+)$#$1# }
        grep { -d }
        map  { "$TWiki::dataDir/$web/$_" }
        grep { ! /^\.\.?$/ } @tmpList;

    return @webList ;
}
#/AS


# =========================
#AS 26 Dec 2000 recursively collects all Web names
#FIXME: move var to TWiki.cfg ?
use vars qw ($subWebsAllowedP);

$subWebsAllowedP = 0; # 1 = subwebs allowed, 0 = flat webs

=pod

---++ sub getAllWebs (  $web  )

Not yet documented.

=cut

sub getAllWebs {
    # returns a list of subweb names
    my( $web ) = @_ ;
    
    if( !defined $web ) {
	$web="";
    }
    my @webList =   map { s/^\///o; $_ }
		    map { "$web/$_" }
		    &getSubWebs( $web );
    my $subWeb = "";
    if( $subWebsAllowedP ) {
        my @subWebs = @webList;
	foreach $subWeb ( @webList ) {
	    push @subWebs, &getAllWebs( $subWeb );
	}
	return @subWebs;
    }
    return @webList ;
}
#/AS


# =========================

1;

# EOF
