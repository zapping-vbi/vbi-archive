# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2000-2004 Peter Thoeny, Peter@Thoeny.com
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
# - Upgrading TWiki is easy as long as you use Plugins.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log
#
# Note: Use the TWiki:Plugins/PerlDocPlugin to extract the documentation
#       Unlike in other modules, do not use a ---+ level one heading

=begin twiki

---++ Description

This module defines official funtions that [[%TWIKIWEB%.TWikiPlugins][Plugins]] and add-on 
scripts can use to interact with the TWiki engine and content.

Plugins should *only* use functions published in this module. If you use
functions in other TWiki libraries you might impose a security hole and 
you will likely need to change your Plugin when you upgrade TWiki.

Functions listing an "Introduced" field refer to the VERSION number of 
the TWiki::Plugins module.

=cut

package TWiki::Func;

use strict;

# =========================
=pod

---++ Functions: CGI Environment

---+++ getSessionValue( $key ) ==> $value

| Description: | Get a session value from the Session Plugin (if installed) |
| Parameter: =$key= | Session key |
| Return: =$value= | Value associated with key; empty string if not set; undef if session plugin is not installed |

=cut
# -------------------------
sub getSessionValue
{
#   my( $theKey ) = @_;
    return &TWiki::getSessionValue( @_ );
}


# =========================
=pod

---+++ setSessionValue( $key, $value ) ==> $result

| Description: | Set a session value via the Session Plugin (if installed) |
| Parameter: =$key= | Session key |
| Parameter: =$value= | Value associated with key |
| Return: =$result= | ="1"= if success; undef if session plugin is not installed |

=cut
# -------------------------
sub setSessionValue
{
#   my( $theKey, $theValue ) = @_;
    &TWiki::setSessionValue( @_ );
}

# =========================
=pod

---+++ getSkin( ) ==> $skin

| Description: | Get the name of the skin, set by the =SKIN= preferences variable or the =skin= CGI parameter |
| Return: =$skin= | Name of skin, e.g. ="gnu"=. Empty string if none |

=cut
# -------------------------
sub getSkin
{
    return &TWiki::getSkin();
}

# =========================
=pod

---+++ getUrlHost( ) ==> $host

| Description: | Get protocol, domain and optional port of script URL |
| Return: =$host= | URL host, e.g. ="http://example.com:80"= |

=cut
# -------------------------
sub getUrlHost
{
    return $TWiki::urlHost;
}

# =========================
=pod

---+++ getScriptUrl( $web, $topic, $script ) ==> $url

| Description: | Compose fully qualified URL |
| Parameter: =$web= | Web name, e.g. ="Main"= |
| Parameter: =$topic= | Topic name, e.g. ="WebNotify"= |
| Parameter: =$script= | Script name, e.g. ="view"= |
| Return: =$url= | URL, e.g. ="http://example.com:80/cgi-bin/view.pl/Main/WebNotify"= |

=cut
# -------------------------
sub getScriptUrl
{
#   my( $web, $topic, $script ) = @_;
    return &TWiki::getScriptUrl( @_ ); 
}

# =========================
=pod

---+++ getScriptUrlPath( ) ==> $path

| Description: | Get script URL path |
| Return: =$path= | URL path of TWiki scripts, e.g. ="/cgi-bin"= |

=cut
# -------------------------
sub getScriptUrlPath
{
    return $TWiki::scriptUrlPath;
}

# =========================
=pod

---+++ getViewUrl( $web, $topic ) ==> $url

| Description: | Compose fully qualified view URL |
| Parameter: =$web= | Web name, e.g. ="Main"=. The current web is taken if empty |
| Parameter: =$topic= | Topic name, e.g. ="WebNotify"= |
| Return: =$url= | URL, e.g. ="http://example.com:80/cgi-bin/view.pl/Main/WebNotify"= |

=cut
# -------------------------
sub getViewUrl
{
#   my( $web, $topic ) = @_;
    return &TWiki::getViewUrl( @_ );
}

# =========================
=pod

---+++ getOopsUrl( $web, $topic, $template, $param1, $param2, $param3, $param4 ) ==> $url

| Description: | Compose fully qualified "oops" dialog URL |
| Parameter: =$web= | Web name, e.g. ="Main"=. The current web is taken if empty |
| Parameter: =$topic= | Topic name, e.g. ="WebNotify"= |
| Parameter: =$template= | Oops template name, e.g. ="oopslocked"= |
| Parameter: =$param1= ... =$param4= | Parameter values for %<nop>PARAM1% ... %<nop>PARAM4% variables in template, optional |
| Return: =$url= | URL, e.g. ="http://example.com:80/cgi-bin/oops.pl/ Main/WebNotify?template=oopslocked&amp;param1=joe"= |

=cut
# -------------------------
sub getOopsUrl
{
#   my( $web, $topic, $template, @params ) = @_;
    # up to 4 parameters in @theParams
    return &TWiki::getOopsUrl( @_ );
}

# =========================
=pod

---+++ getPubUrlPath( ) ==> $path

| Description: | Get pub URL path |
| Return: =$path= | URL path of pub directory, e.g. ="/pub"= |

=cut
# -------------------------
sub getPubUrlPath
{
    return &TWiki::getPubUrlPath();
}

# =========================
=pod

---+++ getCgiQuery( ) ==> $query

| Description: | Get CGI query object. Important: Plugins cannot assume that scripts run under CGI, Plugins must always test if the CGI query object is set |
| Return: =$query= | CGI query object; or 0 if script is called as a shell script |

=cut
# -------------------------
sub getCgiQuery
{
    return &TWiki::getCgiQuery();
}

# =========================
=pod

---+++ writeHeader( $query )

| Description: | Prints a basic content-type HTML header for text/html to standard out |
| Parameter: =$query= | CGI query object |
| Return: | none |

=cut
# -------------------------
sub writeHeader
{
#   my( $theQuery ) = @_;
    return &TWiki::writeHeader( @_ );
}

# =========================
=pod

---+++ redirectCgiQuery( $query, $url )

| Description: | Redirect to URL |
| Parameter: =$query= | CGI query object |
| Parameter: =$url= | URL to redirect to |
| Return: | none, never returns |

=cut
# -------------------------
sub redirectCgiQuery
{
#   my( $theQuery, $theUrl ) = @_;
    return &TWiki::redirect( @_ );
}

# =========================
=pod

---++ Functions: Preferences

---+++ extractNameValuePair( $attr, $name ) ==> $value

| Description: | Extract a named or unnamed value from a variable parameter string |
| Parameter: =$attr= | Attribute string |
| Parameter: =$name= | Name, optional |
| Return: =$value= | Extracted value |

   * Example:
      * Variable: =%<nop>TEST{ "nameless" name1="val1" name2="val2" }%=
      * First extract text between ={...}= to get: ="nameless" name1="val1" name2="val2"=
      * Then call this on the text: <br />
        =my $noname = TWiki::Func::extractNameValuePair( $text );= <br />
        =my $val1  = TWiki::Func::extractNameValuePair( $text, "name1" );= <br />
        =my $val2  = TWiki::Func::extractNameValuePair( $text, "name2" );=

=cut
# -------------------------
sub extractNameValuePair
{
#   my( $theAttr, $theName ) = @_;
    return &TWiki::extractNameValuePair( @_ );
}

# =========================
=pod

---+++ getPreferencesValue( $key, $web ) ==> $value

| Description: | Get a preferences value from TWiki or from a Plugin |
| Parameter: =$key= | Preferences key |
| Parameter: =$web= | Name of web, optional. Current web if not specified; does not apply to settings of Plugin topics |
| Return: =$value= | Preferences value; empty string if not set |

   * Example for Plugin setting:
      * MyPlugin topic has: =* Set COLOR = red=
      * Use ="MYPLUGIN_COLOR"= for =$key=
      * =my $color = TWiki::Func::getPreferencesValue( "MYPLUGIN_COLOR" );=

   * Example for preferences setting:
      * WebPreferences topic has: =* Set WEBBGCOLOR = #FFFFC0=
      * =my $webColor = TWiki::Func::getPreferencesValue( "WEBBGCOLOR", "Sandbox" );=

=cut
# -------------------------
sub getPreferencesValue
{
#   my( $theKey, $theWeb ) = @_;
    return &TWiki::Prefs::getPreferencesValue( @_ );
}

# =========================
=pod

---+++ getPreferencesFlag( $key, $web ) ==> $value

| Description: | Get a preferences flag from TWiki or from a Plugin |
| Parameter: =$key= | Preferences key |
| Parameter: =$web= | Name of web, optional. Current web if not specified; does not apply to settings of Plugin topics |
| Return: =$value= | Preferences flag ="1"= (if set), or ="0"= (for preferences values ="off"=, ="no"= and ="0"=) |

   * Example for Plugin setting:
      * MyPlugin topic has: =* Set SHOWHELP = off=
      * Use ="MYPLUGIN_SHOWHELP"= for =$key=
      * =my $showHelp = TWiki::Func::getPreferencesFlag( "MYPLUGIN_SHOWHELP" );=

=cut
# -------------------------
sub getPreferencesFlag
{
#   my( $theKey, $theWeb ) = @_;
    return &TWiki::Prefs::getPreferencesFlag( @_ );
}

# =========================
=pod

---+++ getWikiToolName( ) ==> $name

| Description: | Get toolname as defined in TWiki.cfg |
| Return: =$name= | Name of tool, e.g. ="TWiki"= |

=cut
# -------------------------
sub getWikiToolName
{
    return $TWiki::wikiToolName;
}

# =========================
=pod

---+++ getMainWebname( ) ==> $name

| Description: | Get name of Main web as defined in TWiki.cfg |
| Return: =$name= | Name, e.g. ="Main"= |

=cut
# -------------------------
sub getMainWebname
{
    return $TWiki::mainWebname;
}

# =========================
=pod

---+++ getTwikiWebname( ) ==> $name

| Description: | Get name of TWiki documentation web as defined in TWiki.cfg |
| Return: =$name= | Name, e.g. ="TWiki"= |

=cut
# -------------------------
sub getTwikiWebname
{
    return $TWiki::twikiWebname;
}

# =========================
=pod

---++ Functions: User Handling and Access Control

---+++ getDefaultUserName( ) ==> $loginName

| Description: | Get default user name as defined in TWiki.cfg's =$defaultUserName= |
| Return: =$loginName= | Default user name, e.g. ="guest"= |

=cut
# -------------------------
sub getDefaultUserName
{
    return $TWiki::defaultUserName;
}

# =========================
=pod

---+++ getWikiName( ) ==> $wikiName

| Description: | Get Wiki name of logged in user |
| Return: =$wikiName= | Wiki Name, e.g. ="JohnDoe"= |

=cut
# -------------------------
sub getWikiName
{
    return $TWiki::wikiName;
}

# =========================
=pod

---+++ getWikiUserName( $text ) ==> $wikiName

| Description: | Get Wiki name of logged in user with web prefix |
| Return: =$wikiName= | Wiki Name, e.g. ="Main.JohnDoe"= |

=cut
# -------------------------
sub getWikiUserName
{
    return $TWiki::wikiUserName;
}

# =========================
=pod

---+++ wikiToUserName( $wikiName ) ==> $loginName

| Description: | Translate a Wiki name to a login name based on [[%MAINWEB%.TWikiUsers]] topic |
| Parameter: =$wikiName= | Wiki name, e.g. ="Main.JohnDoe"= or ="JohnDoe"= |
| Return: =$loginName= | Login name of user, e.g. ="jdoe"= |

=cut
# -------------------------
sub wikiToUserName
{
#   my( $wiki ) = @_;
    return &TWiki::wikiToUserName( @_ );
}

# =========================
=pod

---+++ userToWikiName( $loginName, $dontAddWeb ) ==> $wikiName

| Description: | Translate a login name to a Wiki name based on [[%MAINWEB%.TWikiUsers]] topic |
| Parameter: =$loginName= | Login name, e.g. ="jdoe"= |
| Parameter: =$dontAddWeb= | Do not add web prefix if ="1"= |
| Return: =$wikiName= | Wiki name of user, e.g. ="Main.JohnDoe"= or ="JohnDoe"= |

=cut
# -------------------------
sub userToWikiName
{
#   my( $loginName, $dontAddWeb ) = @_;
    return &TWiki::userToWikiName( @_ );
}

# =========================
=pod

---+++ isGuest( ) ==> $flag

| Description: | Test if logged in user is a guest |
| Return: =$flag= | ="1"= if yes, ="0"= if not |

=cut
# -------------------------
sub isGuest
{
    return &TWiki::isGuest();
}

# =========================
=pod

---+++ permissionsSet( $web ) ==> $flag

| Description: | Test if any access restrictions are set for this web, ignoring settings on individual pages |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =$flag= | ="1"= if yes, ="0"= if no |

=cut
# -------------------------
sub permissionsSet
{
#   my( $web ) = @_;
    return &TWiki::Access::permissionsSet( @_ );
}

# =========================
=pod

---+++ checkAccessPermission( $type, $wikiName, $text, $topic, $web ) ==> $flag

| Description: | Check access permission for a topic based on the [[%TWIKIWEB%.TWikiAccessControl]] rules |
| Parameter: =$type= | Access type, e.g. ="VIEW"=, ="CHANGE"=, ="CREATE"= |
| Parameter: =$wikiName= | WikiName of remote user, i.e. ="Main.PeterThoeny"= |
| Parameter: =$text= | Topic text, optional. If empty, topic =$web.$topic= is consulted |
| Parameter: =$topic= | Topic name, required, e.g. ="PrivateStuff"= |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =$flag= | ="1"= if access may be granted, ="0"= if not |

=cut
# -------------------------
sub checkAccessPermission
{
#   my( $type, $user, $text, $topic, $web ) = @_;
    return &TWiki::Access::checkAccessPermission( @_ );
}

# =========================
=pod

---++ Functions: Content Handling

---+++ webExists( $web ) ==> $flag

| Description: | Test if web exists |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =$flag= | ="1"= if web exists, ="0"= if not |

=cut
# -------------------------
sub webExists
{
#   my( $theWeb ) = @_;
    return &TWiki::Store::webExists( @_ );
}

# =========================
=pod

---+++ topicExists( $web, $topic ) ==> $flag

| Description: | Test if topic exists |
| Parameter: =$web= | Web name, optional, e.g. ="Main"= |
| Parameter: =$topic= | Topic name, required, e.g. ="TokyoOffice"=, or ="Main.TokyoOffice"= |
| Return: =$flag= | ="1"= if topic exists, ="0"= if not |

=cut
# -------------------------
sub topicExists
{
#   my( $web, $topic ) = @_;
    return &TWiki::Store::topicExists( @_ );
}

# =========================
=pod

---+++ getRevisionInfo( $web, $topic ) ==> ( $date, $loginName, $rev )

| Description: | Get revision info of a topic |
| Parameter: =$web= | Web name, optional, e.g. ="Main"= |
| Parameter: =$topic= | Topic name, required, e.g. ="TokyoOffice"= |
| Return: =( $date, $loginName, $rev )= | List with: ( last update date, login name of last user, minor part of top revision number ), e.g. =( 12345 "phoeny", "5" )= |
| | $date is in epochSeconds |

=cut
# -------------------------
sub getRevisionInfo
{
#   my( $web, $topic );
    return TWiki::Store::getRevisionInfoFromMeta( @_ );
}

# =========================
=pod

---+++ checkTopicEditLock( $web, $topic ) ==> ( $oopsUrl, $loginName, $unlockTime )

| Description: | Check if topic has an edit lock by a user |
| Parameter: =$web= | Web name, e.g. ="Main"=, or empty |
| Parameter: =$topic= | Topic name, e.g. ="MyTopic"=, or ="Main.MyTopic"= |
| Return: =( $oopsUrl, $loginName, $unlockTime )= | The =$oopsUrl= for calling redirectCgiQuery(), user's =$loginName=, and estimated =$unlockTime= in minutes. The =$oopsUrl= and =$loginName= is empty if topic has no edit lock. |

=cut
# -------------------------
sub checkTopicEditLock
{
    my( $web, $topic ) = @_;
    my( $loginName, $lockTime ) = TWiki::Store::topicIsLockedBy( $web, $topic );
    my $oopsUrl = "";
    if( $loginName ) {
        use integer;
        $lockTime = ( $lockTime / 60 ) + 1;           # convert to minutes
        my $editLockTime = $TWiki::editLockTime / 60; # max lock time
        my $wikiUser = TWiki::Func::userToWikiName( $loginName );
        $oopsUrl = &TWiki::Func::getOopsUrl( $web, $topic, "oopslocked", $wikiUser, $editLockTime, $lockTime );
    }
    return( $oopsUrl, $loginName, $lockTime );
}

# =========================
=pod

---+++ setTopicEditLock( $web, $topic, $lock ) ==> $oopsUrl

| Description: | Lock topic for editing, or unlock when done |
| Parameter: =$web= | Web name, e.g. ="Main"=, or empty |
| Parameter: =$topic= | Topic name, e.g. ="MyTopic"=, or ="Main.MyTopic"= |
| Parameter: =$lock= | Set to =1= to lock topic, =0= to unlock |
| Return: =$oopsUrl= | Empty string if OK; the =$oopsUrl= for calling redirectCgiQuery() in case lock is already taken when trying to lock topic |

=cut
# -------------------------
sub setTopicEditLock
{
    my( $web, $topic, $lock ) = @_;
    if( $lock ) {
        my( $oopsUrl ) = checkTopicEditLock( $web, $topic );
        return $oopsUrl if( $oopsUrl );
    }
    TWiki::Store::lockTopicNew( $web, $topic, ! $lock );    # reverse $lock parameter is correct!
    return "";
}

# =========================
=pod

---+++ readTopicText( $web, $topic, $rev, $ignorePermissions ) ==> $text

| Description: | Read topic text, including meta data |
| Parameter: =$web= | Web name, e.g. ="Main"=, or empty |
| Parameter: =$topic= | Topic name, e.g. ="MyTopic"=, or ="Main.MyTopic"= |
| Parameter: =$rev= | Topic revision to read, optional. Specify the minor part of the revision, e.g. ="5"=, not ="1.5"=; the top revision is returned if omitted or empty. |
| Parameter: =$ignorePermissions=  | Set to ="1"= if checkAccessPermission() is already performed and OK; an oops URL is returned if user has no permission |
| Return: =$text= | Topic text with embedded meta data; an oops URL for calling redirectCgiQuery() is returned in case of an error |

=cut
# -------------------------
sub readTopicText
{
    my( $web, $topic, $rev, $ignorePermissions ) = @_;

    my $text = TWiki::Store::readTopicRaw( $web, $topic, $rev, $ignorePermissions );
    # FIXME: The following breaks if spec of readTopicRaw() changes
    if( $text =~ /^No permission to read topic/ ) {
        $text = TWiki::getOopsUrl( $web, $topic, "oopsaccessview" );
    }
    return $text;
}

# =========================
=pod

---+++ saveTopicText( $web, $topic, $text, $ignorePermissions, $dontNotify ) ==> $oopsUrl

| Description: | Save topic text, typically obtained by readTopicText(). Topic data usually includes meta data; the file attachment meta data is replaced by the meta data from the topic file if it exists. |
| Parameter: =$web= | Web name, e.g. ="Main"=, or empty |
| Parameter: =$topic= | Topic name, e.g. ="MyTopic"=, or ="Main.MyTopic"= |
| Parameter: =$text= | Topic text to save, assumed to include meta data |
| Parameter: =$ignorePermissions=  | Set to ="1"= if checkAccessPermission() is already performed and OK |
| Parameter: =$dontNotify= | Set to ="1"= if not to notify users of the change |
| Return: =$oopsUrl= | Empty string if OK; the =$oopsUrl= for calling redirectCgiQuery() in case of error |

   * Example: <br />
     =my $oopsUrl = TWiki::Func::setTopicEditLock( $web, $topic, 1 );= <br />
     =if( $oopsUrl ) {= <br />
     =&nbsp;   TWiki::Func::redirectCgiQuery( $query, $oopsUrl );   # assuming valid query= <br />
     =&nbsp;   return;= <br />
     =}= <br />
     =my $text = TWiki::Func::readTopicText( $web, $topic );        # read topic text= <br />
     =# check for oops URL in case of error:= <br />
     =if( $text =~ /^http.*?\/oops/ ) {= <br />
     =&nbsp;   TWiki::Func::redirectCgiQuery( $query, $text );= <br />
     =&nbsp;   return;= <br />
     =}= <br />
     =# do topic text manipulation like:= <br />
     =$text =~ s/old/new/g;= <br />
     =# do meta data manipulation like:= <br />
     =$text =~ s/(META\:FIELD.*?name\=\"TopicClassification\".*?value\=\")[^\"]*/$1BugResolved/;= <br />
     =$oopsUrl = TWiki::Func::saveTopicText( $web, $topic, $text ); # save topic text= <br />
     =TWiki::Func::setTopicEditLock( $web, $topic, 0 );             # unlock topic= <br />
     =if( $oopsUrl ) {= <br />
     =&nbsp;   TWiki::Func::redirectCgiQuery( $query, $oopsUrl );= <br />
     =&nbsp;   return;= <br />
     =}=

=cut
# -------------------------
sub saveTopicText
{
    my( $web, $topic, $text, $ignorePermissions, $dontNotify ) = @_;

    my( $mirrorSite, $mirrorViewURL ) = TWiki::readOnlyMirrorWeb( $web );
    return TWiki::getOopsUrl( $web, $topic, "oopsmirror", $mirrorSite, $mirrorViewURL ) if( $mirrorSite );

    # check access permission
    unless( $ignorePermissions ||
            TWiki::Access::checkAccessPermission( "change", $TWiki::wikiUserName, "", $topic, $web )
          ) {
        return TWiki::getOopsUrl( $web, $topic, "oopsaccesschange" );
    }

    return TWiki::getOopsUrl( $web, $topic, "oopssave" )  unless( defined $text );
    return TWiki::getOopsUrl( $web, $topic, "oopsempty" ) unless( $text ); # empty topic not allowed

    # extract meta data and merge old attachment meta data
    my $meta = "";
    ( $meta, $text ) = TWiki::Store::_extractMetaData( $web, $topic, $text );
    my( $oldMeta, $oldText ) = TWiki::Store::readTopic( $web, $topic );
    $meta->copyFrom( $oldMeta, "FILEATTACHMENT" );

    # save topic
    my $error = TWiki::Store::saveTopic( $web, $topic, $text, $meta, "", 0, $dontNotify );
    return TWiki::getOopsUrl( $web, $topic, "oopssaveerr", $error ) if( $error );
    return "";
}

# =========================
=pod

---+++ getPublicWebList( ) ==> @webs

| Description: | Get list of all public webs, e.g. all webs that do not have the =NOSEARCHALL= flag set in the WebPreferences |
| Return: =@webs= | List of all public webs, e.g. =( "Main",  "Know", "TWiki" )= |

=cut
# -------------------------
sub getPublicWebList
{
    return &TWiki::getPublicWebList();
}

# =========================
=pod

---+++ getTopicList( $web ) ==> @topics

| Description: | Get list of all topics in a web |
| Parameter: =$web= | Web name, required, e.g. ="Sandbox"= |
| Return: =@topics= | Topic list, e.g. =( "WebChanges",  "WebHome", "WebIndex", "WebNotify" )= |

=cut
# -------------------------
sub getTopicList
{
#   my( $web ) = @_;
    return &TWiki::Store::getTopicNames ( @_ );
}

# =========================
=pod

---++ Functions: Rendering

---+++ expandCommonVariables( $text, $topic, $web ) ==> $text

| Description: | Expand all common =%<nop>VARIABLES%= |
| Parameter: =$text= | Text with variables to expand, e.g. ="Current user is %<nop>WIKIUSER%"= |
| Parameter: =$topic= | Current topic name, e.g. ="WebNotify"= |
| Parameter: =$web= | Web name, optional, e.g. ="Main"=. The current web is taken if missing |
| Return: =$text= | Expanded text, e.g. ="Current user is <nop>TWikiGuest"= |

=cut
# -------------------------
sub expandCommonVariables
{
#   my( $text, $topic, $web ) = @_;
    return &TWiki::handleCommonTags( @_ );
}

# =========================
=pod

---+++ renderText( $text, $web ) ==> $text

| Description: | Render text from TWiki markup into XHTML as defined in [[%TWIKIWEB%.TextFormattingRules]] |
| Parameter: =$text= | Text to render, e.g. ="*bold* text and =fixed font="= |
| Parameter: =$web= | Web name, optional, e.g. ="Main"=. The current web is taken if missing |
| Return: =$text= | XHTML text, e.g. ="&lt;b>bold&lt;/b> and &lt;code>fixed font&lt;/code>"= |

=cut
# -------------------------
sub renderText
{
#   my( $text, $web ) = @_;
    return &TWiki::getRenderedVersion( @_ );
}

# =========================
=pod

---+++ internalLink( $pre, $web, $topic, $label, $anchor, $createLink ) ==> $text

| Description: | Render topic name and link label into an XHTML link. Normally you do not need to call this funtion, it is called internally by =renderText()= |
| Parameter: =$pre= | Text occuring before the TWiki link syntax, optional |
| Parameter: =$web= | Web name, required, e.g. ="Main"= |
| Parameter: =$topic= | Topic name to link to, required, e.g. ="WebNotify"= |
| Parameter: =$label= | Link label, required. Usually the same as =$topic=, e.g. ="notify"= |
| Parameter: =$anchor= | Anchor, optional, e.g. ="#Jump"= |
| Parameter: =$createLink= | Set to ="1"= to add question linked mark after topic name if topic does not exist;<br /> set to ="0"= to suppress link for non-existing topics |
| Return: =$text= | XHTML anchor, e.g. ="&lt;a href="/cgi-bin/view/Main/WebNotify#Jump">notify&lt;/a>"= |

=cut
# -------------------------
sub internalLink
{
#   my( $pre, $web, $topic, $label, $anchor, $anchor, $createLink ) = @_;
    return &TWiki::internalLink( @_ );
}

# =========================
=pod

---+++ search text( $text ) ==> $text

| Description: | This is not a function, just a how-to note. Use: =expandCommonVariables("%<nop>SEARCH{...}%" );= |
| Parameter: =$text= | Search variable |
| Return: ="$text"= | Search result in [[%TWIKIWEB%.FormattedSearch]] format |

=cut

# =========================
=pod

---+++ formatTime( $time, $format, $timezone ) ==> $text

| Description: | Format the time int seconds into the desired time string |
| Parameter: =$time= | Time in epoc seconds |
| Parameter: =$format= | Format type, optional. Default e.g. ="31 Dec 2002 - 19:30"=, can be ="iso"= (e.g. ="2002-12-31T19:30Z"=), ="rcs"= (e.g. ="2001/12/31 23:59:59"=, ="http"= for HTTP header format (e.g. ="Thu, 23 Jul 1998 07:21:56 GMT"=) |
| Parameter: =$timezone= | either not defined (uses the displaytime setting), "gmtime", or "servertime" |
| Return: =$text= | Formatted time string |
| Note: | if you used the removed formatGmTime, add a third parameter "gmtime" |

=cut
# -------------------------
sub formatTime
{
#   my $epSecs = @_;
    return &TWiki::formatTime( @_ );
}

# =========================
=pod

---++ Functions: File I/O

---+++ getDataDir( ) ==> $dir

| Description: | Get data directory (topic file root) |
| Return: =$dir= | Data directory, e.g. ="/twiki/data"= |

=cut
# -------------------------
sub getDataDir
{
    return &TWiki::getDataDir();
}

# =========================
=pod

---+++ getPubDir( ) ==> $dir

| Description: | Get pub directory (file attachment root). Attachments are in =$dir/Web/TopicName= |
| Return: =$dir= | Pub directory, e.g. ="/htdocs/twiki/pub"= |

=cut
# -------------------------
sub getPubDir
{
    return &TWiki::getPubDir();
}

# =========================
# NOTE: The following function is deprecated and should not be used. Use readTopicText() instead
# ---+++ readTopic( $web, $topic ) ==> ( $meta, $text )
# | Description: | Read topic text and meta data, regardless of access permissions. |
# | Parameter: =$web= | Web name, required, e.g. ="Main"= |
# | Parameter: =$topic= | Topic name, required, e.g. ="TokyoOffice"= |
# | Return: =( $meta, $text )= | Meta data object and topic text |
# -------------------------
sub readTopic
{
#   my( $web, $topic ) = @_;
    return &TWiki::Store::readTopic( @_ );
}

# =========================
=pod

---+++ readTemplate( $name, $skin ) ==> $text

| Description: | Read a template or skin file. Embedded [[%TWIKIWEB%.TWikiTemplates][template directives]] get expanded |
| Parameter: =$name= | Template name, e.g. ="view"= |
| Parameter: =$skin= | Skin name, optional, e.g. ="print"= |
| Return: =$text= | Template text |

=cut
# -------------------------
sub readTemplate
{
#   my( $name, $skin ) = @_;
    return &TWiki::Store::readTemplate( @_ );
}

# =========================
=pod

---+++ readFile( $filename ) ==> $text

| Description: | Read text file, low level. NOTE: For topics use readTopicText() |
| Parameter: =$filename= | Full path name of file |
| Return: =$text= | Content of file |

=cut
# -------------------------
sub readFile
{
#   my( $filename ) = @_;
    return &TWiki::Store::readFile( @_ );
}

# =========================
=pod

---+++ saveFile( $filename, $text )

| Description: | Save text file, low level. NOTE: For topics use saveTopicText() |
| Parameter: =$filename= | Full path name of file |
| Parameter: =$text= | Text to save |
| Return: | none |

=cut
# -------------------------
sub saveFile
{
#   my( $filename, $text ) = @_;
    return &TWiki::Store::saveFile( @_ );
}

# =========================
=pod

---+++ writeWarning( $text )

| Description: | Log Warning that may require admin intervention to data/warning.txt |
| Parameter: =$text= | Text to write; timestamp gets added |
| Return: | none |

=cut
# -------------------------
sub writeWarning
{
#   my( $theText ) = @_;
    return &TWiki::writeWarning( @_ );
}

# =========================
=pod

---+++ writeDebug( $text )

| Description: | Log debug message to data/debug.txt |
| Parameter: =$text= | Text to write; timestamp gets added |
| Return: | none |

=cut
# -------------------------
sub writeDebug
{
#   my( $theText ) = @_;
    return &TWiki::writeDebug( @_ );
}

# =========================
=pod

---++ Functions: I18N related

---+++ getRegularExpression( $regexName ) ==> $pattern

| Description: | Retrieves a TWiki predefined regular expression |
| Parameter: =$regexName= | Name of the regular expression to retrieve.  See notes below |
| Return: | String or precompiled regular expression matching as described below |
| Introduced: | VERSION 1.020 (Feb 2004) |

__Notes:__ TWiki internally precompiles several regular expressions to represent various string entities
in an I18N-compatible manner.  Plugins are encouraged to use these in matching where appropriate.
The following are guaranteed to be present; others may exist, but their use is unsupported and
they may be removed in future TWiki versions.  Those which are marked "CC" are for use within
character classes and may not produce the desired results outside of them.

| *Name* | *Matches* | *CC* |
| upperAlpha | Upper case characters | Y |
| lowerAlpha | Lower case characters | Y |
| mixedAlpha | Alphabetic characters | Y |
| mixedAlphaNum | Alphanumeric charactecs | Y |
| wikiWordRegex | WikiWords | N |

Example:
<pre>
   my $upper = TWiki::Func::getRegularExpression("upperAlpha");
   my $alpha = TWiki::Func::getRegularExpression("mixedAlpha");
   my $capitalized = qr/[$upper][$alpha]+/;
</pre>

=cut

sub getRegularExpression
{
    my ( $regexName ) = @_;
    return $TWiki::regex{$regexName};
}

# =========================
=pod

---++ Copyright and License

Copyright (C) 2000-2004 Peter Thoeny, Peter@Thoeny.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details, published at 
http://www.gnu.org/copyleft/gpl.html

=cut

1;

# EOF
