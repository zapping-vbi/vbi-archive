# Main Module of TWiki Collaboration Platform, http://TWiki.org/
# ($wikiversion has version info)
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
#
# Notes:
# - Latest version at http://twiki.org/
# - Installation instructions in $dataDir/TWiki/TWikiDocumentation.txt
# - Customize variables in TWiki.cfg when installing TWiki.
# - Optionally create a new plugin or customize DefaultPlugin.pm for
#   custom rendering rules.
# - Upgrading TWiki is easy as long as you only customize DefaultPlugin.pm.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log
#
# 20000501 Kevin Kinnell : changed beta0404 to have many new search
#                          capabilities.  This file had a new hash added
#                          for month name-to-number look-ups, a slight
#                          change in the parameter list for the search
#                          script call in &handleSearchWeb, and a new
#                          sub -- &revDate2EpSecs -- for calculating the
#                          epoch seconds from a rev date (the only way
#                          to sort dates.)

=begin twiki

---+ TWiki Package
This package stores all TWiki subroutines that haven't been modularized
into any of the others.

=cut


package TWiki;

use strict;

use Time::Local;	# Added for revDate2EpSecs
use Cwd qw( cwd ); 	# Added for getTWikiLibDir

require 5.005;		# For regex objects and internationalisation

# ===========================
# TWiki config variables from TWiki.cfg:
use vars qw(
        $defaultUserName $wikiHomeUrl $defaultUrlHost
        $scriptUrlPath $pubUrlPath $pubDir $templateDir $dataDir $logDir
        $siteWebTopicName $wikiToolName $securityFilter $uploadFilter
        $debugFilename $warningFilename $htpasswdFilename
        $logFilename $remoteUserFilename $wikiUsersTopicname
        $userListFilename $doMapUserToWikiName
        $twikiWebname $mainWebname $mainTopicname $notifyTopicname
        $wikiPrefsTopicname $webPrefsTopicname
        $statisticsTopicname $statsTopViews $statsTopContrib $doDebugStatistics
        $numberOfRevisions $editLockTime $scriptSuffix
        $safeEnvPath $mailProgram $noSpamPadding $mimeTypesFilename
        $doKeepRevIfEditLock $doGetScriptUrlFromCgi $doRemovePortNumber
        $doRemoveImgInMailnotify $doRememberRemoteUser $doPluralToSingular
        $doHidePasswdInRegistration $doSecureInclude
        $doLogTopicView $doLogTopicEdit $doLogTopicSave $doLogRename
        $doLogTopicAttach $doLogTopicUpload $doLogTopicRdiff
        $doLogTopicChanges $doLogTopicSearch $doLogRegistration
        $superAdminGroup $doSuperAdminGroup $OS
        $disableAllPlugins $attachAsciiPath $displayTimeValues
        $dispScriptUrlPath $dispViewPath
    );

# Internationalisation (I18N) config from TWiki.cfg:
use vars qw(
	$useLocale $localeRegexes $siteLocale $siteCharsetOverride 
	$upperNational $lowerNational
    );

# TWiki::Store config:
use vars qw(
        $rcsDir $rcsArg $nullDev $endRcsCmd $storeTopicImpl $keywordMode
        $storeImpl @storeSettings
    );

# TWiki::Search config:
use vars qw(
        $cmdQuote $lsCmd $egrepCmd $fgrepCmd
    );

# ===========================
# Global variables

# Refactoring Note: these are split up by "site" globals and "request"
# globals so that the latter may latter be placed inside a Perl object
# instead of being globals as now.

# ---------------------------
# Site-Wide Global Variables

# Misc. Globals
use vars qw(
	@isoMonth @weekDay %userToWikiList %wikiToUserList $wikiversion
	$TranslationToken %mon2num $viewScript $twikiLibDir $formatVersion
	@publicWebList %regex
    );

# Internationalisation (I18N) setup:
use vars qw(
	$siteCharset $useUnicode $siteLang $siteFullLang $urlCharEncoding 
    );

# ---------------------------
# Per-Request "Global" Variables
use vars qw(
        $webName $topicName $includingWebName $includingTopicName
	$userName $wikiName $wikiUserName $urlHost $isList @listTypes
	@listElements $debugUserTime $debugSystemTime $script
        $newTopicFontColor $newTopicBgColor $linkToolTipInfo $noAutoLink
	$pageMode $readTopicPermissionFailed $cgiQuery $basicInitDone
    );

# ===========================
# TWiki version:
$wikiversion      = "20 Mar 2004";

# ===========================
# Key Global variables, required for writeDebug
# (new variables must be declared in "use vars qw(..)" above)
@isoMonth = ( "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" );
@weekDay = ("Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat");

{ 
    my $count = 0;
    %mon2num = map { $_ => $count++ } @isoMonth; 
}

# ===========================
# Read the configuration file at compile time in order to set locale
BEGIN {
    do "TWiki.cfg";

    # Do a dynamic 'use locale' for this module
    if( $useLocale ) {
        require locale;
	import locale ();
    }
}

sub writeDebug;
sub writeWarning;


# ===========================
# use TWiki and other modules
use TWiki::Prefs;     # preferences
use TWiki::Search;    # search engine
use TWiki::Access;    # access control
use TWiki::Meta;      # Meta class - topic meta data
use TWiki::Store;     # file I/O and rcs related functions
use TWiki::Attach;    # file attachment functions
use TWiki::Form;      # forms for topics
use TWiki::Func;      # official TWiki functions for plugins
use TWiki::Plugins;   # plugins handler  #AS
use TWiki::Net;       # SMTP, get URL
use TWiki::User;


# ===========================
# Other Global variables

# Token character that must not occur in any normal text - converted
# to a flag character if it ever does occur (very unlikely)
$TranslationToken= "\0";	# Null not allowed in charsets used with TWiki

# The following are also initialized in initialize, here for cases where
# initialize not called.
$cgiQuery = 0;
@publicWebList = ();
$noAutoLink = 0;
$viewScript = "view";

$regex{linkProtocolPattern} = "(file|ftp|gopher|http|https|irc|news|nntp|telnet)";

# Header patterns based on '+++'. The '###' are reserved for numbered headers
$regex{headerPatternDa} = '^---+(\++|\#+)\s*(.+)\s*$';       # '---++ Header', '---## Header'
$regex{headerPatternSp} = '^\t(\++|\#+)\s*(.+)\s*$';         # '   ++ Header', '   + Header'
$regex{headerPatternHt} = '^<h([1-6])>\s*(.+?)\s*</h[1-6]>'; # '<h6>Header</h6>
$regex{headerPatternNoTOC} = '(\!\!+|%NOTOC%)';  # '---++!! Header' or '---++ Header %NOTOC% ^top'

$debugUserTime   = 0;
$debugSystemTime = 0;

$formatVersion = "1.0";

$basicInitDone = 0;		# basicInitialize not yet done

$pageMode = 'html';		# Default is to render as HTML


=pod

---++ writeWarning( $text )

Prints date, time, and contents $text to $warningFilename, typically
'warnings.txt'.  Use for warnings and errors that may require admin
intervention.  Not using Store::writeLog; log file is more of an audit/usage
file.  Use this for defensive programming warnings (e.g. assertions).

=cut

sub writeWarning {
    my( $text ) = @_;
    if( $warningFilename ) {
        my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime( time() );
	my( $tmon) = $isoMonth[$mon];
        $year = sprintf( "%.4u", $year + 1900 );
        my $time = sprintf( "%.2u ${tmon} %.2u - %.2u:%.2u",
			   $mday, $year, $hour, $min );
        open( FILE, ">>$warningFilename" );
        print FILE "$time $text\n";
        close( FILE );
    }
}

=pod

---++ writeDebug( $text )

Prints date, time, and contents of $text to $debugFilename, typically
'debug.txt'.  Use for debugging messages.

=cut

sub writeDebug {
    my( $text ) = @_;
    open( FILE, ">>$debugFilename" );
    
    my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime( time() );
    my( $tmon) = $isoMonth[$mon];
    $year = sprintf( "%.4u", $year + 1900 );
    my $time = sprintf( "%.2u ${tmon} %.2u - %.2u:%.2u", $mday, $year, $hour, $min );

    print FILE "$time $text\n";
    close(FILE);
}

=pod

---++ writeDebugTimes( $text )

Dumps user and system time spent, with deltas from last call, followed
by contents of $text, to debug log using writeDebug above.  Use for
performance monitoring/debugging.

=cut

sub writeDebugTimes
{
    my( $text ) = @_;

    if( ! $debugUserTime ) {
        writeDebug( "===      sec (delta:)     sec (delta:)     sec   function:" );
    }
    my( $puser, $psystem, $cuser, $csystem ) = times();
    my $duser = $puser - $debugUserTime;
    my $dsystem = $psystem - $debugSystemTime;
    my $times = sprintf( "usr %1.2f (%1.2f), sys %1.2f (%1.2f), sum %1.2f",
                  $puser, $duser, $psystem, $dsystem, $puser+$psystem );
    $debugUserTime   = $puser;
    $debugSystemTime = $psystem;

    writeDebug( "==> $times,  $text" );
}

=pod

---++ initialize( $pathInfo, $remoteUser, $topic, $url, $query )
Return value: ( $topicName, $webName, $scriptUrlPath, $userName, $dataDir )

Per-web initialization of all aspects of TWiki.  Initializes the
Store, User, Access, and Prefs modules.  Contains two plugin
initialization hooks: 'initialize1' to allow plugins to interact
for authentication, and 'initialize2' once the authenticated username
is available.

Also parses $theTopic to determine whether it's a URI, a "Web.Topic"
pair, a "Web." WebHome shorthand, or just a topic name.  Note that
if $pathInfo is set, this overrides $theTopic.

=cut

sub initialize
{
    my ( $thePathInfo, $theRemoteUser, $theTopic, $theUrl, $theQuery ) = @_;
    
    if( not $basicInitDone ) {
	basicInitialize();
    }

    ##writeDebug( "\n---------------------------------" );

    $cgiQuery = $theQuery;
    
    # Initialise vars here rather than at start of module,
    # so compatible with modPerl
    @publicWebList = ();
    &TWiki::Store::initialize();

    &TWiki::User::initialize();

    # Make %ENV safer for CGI
    if( $safeEnvPath ) {
        $ENV{'PATH'} = $safeEnvPath;
    }
    delete @ENV{ qw( IFS CDPATH ENV BASH_ENV ) };

    # initialize lib directory early because of later 'cd's
    getTWikiLibDir();

    # initialize access control
    &TWiki::Access::initializeAccess();
    $readTopicPermissionFailed = ""; # Will be set to name(s) of topic(s) that can't be read

    # initialize $webName and $topicName from URL
    $topicName = "";
    $webName   = "";
    if( $theTopic ) {
        if(( $theTopic =~ /^$regex{linkProtocolPattern}\:\/\//o ) && ( $cgiQuery ) ) {
            # redirect to URI
            print $cgiQuery->redirect( $theTopic );
            return; # should never return here
        } elsif( $theTopic =~ /(.*)[\.\/](.*)/ ) {
            # is "bin/script?topic=Webname.SomeTopic"
            $webName   = $1 || "";
            $topicName = $2 || "";
            # jump to WebHome if ""bin/script?topic=Webname."
            $topicName = $mainTopicname if( $webName && ( ! $topicName ) );
        } else {
            # assume "bin/script/Webname?topic=SomeTopic"
            $topicName = $theTopic;
        }
    }

    # Clean up PATH_INFO problems, e.g.  Support.CobaltRaqInstall.  A valid
    # PATH_INFO is '/Main/WebHome', i.e. the text after the script name;
    # invalid PATH_INFO is often a full path starting with '/cgi-bin/...'.
    $thePathInfo =~ s!$scriptUrlPath/[\-\.A-Z]+$scriptSuffix/!/!i;
    ## writeDebug( "===== thePathInfo after cleanup = $thePathInfo" );

    # Get the web and topic names from PATH_INFO
    if( $thePathInfo =~ /\/(.*)[\.\/](.*)/ ) {
        # is "bin/script/Webname/SomeTopic" or "bin/script/Webname/"
        $webName   = $1 || "" if( ! $webName );
        $topicName = $2 || "" if( ! $topicName );
    } elsif( $thePathInfo =~ /\/(.*)/ ) {
        # is "bin/script/Webname" or "bin/script/"
        $webName   = $1 || "" if( ! $webName );
    }
    ( $topicName =~ /\.\./ ) && ( $topicName = $mainTopicname );

    # Refuse to work with character sets that allow TWiki syntax
    # to be recognised within multi-byte characters.  Only allow 'oops'
    # page to be displayed (redirect causes this code to be re-executed).
    if ( invalidSiteCharset() and $theUrl !~ m!$scriptUrlPath/oops! ) {  
	writeWarning "Cannot use this multi-byte encoding ('$siteCharset') as site character encoding";
	writeWarning "Please set a different character encoding in the \$siteLocale setting in TWiki.cfg.";
        my $url = &TWiki::getOopsUrl( $webName, $topicName, "oopsbadcharset" );
	print $cgiQuery->redirect( $url );
        return;
    }

    # Convert UTF-8 web and topic name from URL into site charset 
    # if necessary - no effect if URL is not in UTF-8
    ( $webName, $topicName ) = convertUtf8URLtoSiteCharset ( $webName, $topicName );

    # Filter out dangerous or unwanted characters
    $topicName =~ s/$securityFilter//go;
    $topicName =~ /(.*)/;
    $topicName = $1 || $mainTopicname;  # untaint variable
    $webName   =~ s/$securityFilter//go;
    $webName   =~ /(.*)/;
    $webName   = $1 || $mainWebname;  # untaint variable
    $includingTopicName = $topicName;
    $includingWebName = $webName;

    # initialize $urlHost and $scriptUrlPath 
    if( ( $theUrl ) && ( $theUrl =~ m!^([^:]*://[^/]*)(.*)/.*$! ) && ( $2 ) ) {
        if( $doGetScriptUrlFromCgi ) {
            $scriptUrlPath = $2;
        }
        $urlHost = $1;
        if( $doRemovePortNumber ) {
            $urlHost =~ s/\:[0-9]+$//;
        }
    } else {
        $urlHost = $defaultUrlHost;
    }
    # PTh 15 Jul 2001: Removed init of $scriptUrlPath based on $theUrl because
    # $theUrl has incorrect URI after failed authentication

    # initialize preferences, first part for site and web level
    &TWiki::Prefs::initializePrefs( $webName );

    # initialize user name and user to WikiName list
    userToWikiListInit();
    if( !$disableAllPlugins ) {
            # Early plugin initialization, allow plugins like SessionPlugin
	    # to set the user.  This must be done before preferences are set,
	    # as we need to get user preferences
            $userName = &TWiki::Plugins::initialize1( $topicName, $webName, $theRemoteUser, $theUrl, $thePathInfo );
    }
    $wikiName     = userToWikiName( $userName, 1 );      # i.e. "JonDoe"
    $wikiUserName = userToWikiName( $userName );         # i.e. "Main.JonDoe"

    # initialize preferences, second part for user level
    &TWiki::Prefs::initializeUserPrefs( $wikiUserName );

    # some remaining init
    $viewScript = "view";
    if( ( $ENV{'SCRIPT_NAME'} ) && ( $ENV{'SCRIPT_NAME'} =~ /^.*\/viewauth$/ ) ) {
        # Needed for TOC
        $viewScript = "viewauth";
    }

    # Add background color and font color (AlWilliams - 18 Sep 2000)
    # PTh: Moved from internalLink to initialize ('cause of performance)
    $newTopicBgColor   = TWiki::Prefs::getPreferencesValue("NEWTOPICBGCOLOR")   || "#FFFFCE";
    $newTopicFontColor = TWiki::Prefs::getPreferencesValue("NEWTOPICFONTCOLOR") || "#0000FF";
    $linkToolTipInfo   = TWiki::Prefs::getPreferencesValue("LINKTOOLTIPINFO")   || "";
    # Prevent autolink of WikiWords
    $noAutoLink        = TWiki::Prefs::getPreferencesValue("NOAUTOLINK") || 0;

#AS
    if( !$disableAllPlugins ) {
        # Normal plugin initialization - userName is known and preferences available
        &TWiki::Plugins::initialize2( $topicName, $webName, $userName );
    }
#/AS

    return ( $topicName, $webName, $scriptUrlPath, $userName, $dataDir );
}

=pod

---++ basicInitialize()

Sets up POSIX locale and precompiled regexes - for use from scripts
that handle multiple webs (e.g. mailnotify) and need regexes or
isWebName/isWikiName to work before the per-web initialize() is called.
Also called from initialize() if not necessary beforehand.

=cut

sub basicInitialize() {
    # Set up locale for internationalisation and pre-compile regexes
    setupLocale();
    setupRegexes();
    
    $basicInitDone = 1;
}

=pod

---++ setupLocale()

Run-time locale setup - If $useLocale is set, this function parses $siteLocale
from TWiki.cfg and passes it to the POSIX::setLocale function to change TWiki's
operating environment.

mod_perl compatibility note: If TWiki is running under Apache, won't this play
with the Apache process's locale settings too?  What effects would this have?

Note that 'use locale' must be done in BEGIN block for regexes and sorting to
work properly, although regexes can still work without this in
'non-locale regexes' mode (see setupRegexes routine).

=cut

sub setupLocale {
 
    $siteCharset = 'ISO-8859-1';	# Default values if locale mis-configured
    $siteLang = 'en';
    $siteFullLang = 'en-us';

    if ( $useLocale ) {
	if ( not defined $siteLocale or $siteLocale !~ /[a-z]/i ) {
	    writeWarning "Locale $siteLocale unset or has no alphabetic characters";
	    return;
	}
	# Extract the character set from locale and use in HTML templates
	# and HTTP headers
	$siteLocale =~ m/\.([a-z0-9_-]+)$/i;
	$siteCharset = $1 if defined $1;
	$siteCharset =~ s/^utf8$/utf-8/i;	# For convenience, avoid overrides
	$siteCharset =~ s/^eucjp$/euc-jp/i;

	# Override charset - used when locale charset not supported by Perl
	# conversion modules
	$siteCharset = $siteCharsetOverride || $siteCharset;
	$siteCharset = lc $siteCharset;

	# Extract the default site language - ignores '@euro' part of
	# 'fr_BE@euro' type locales.
	$siteLocale =~ m/^([a-z]+)_([a-z]+)/i;
	$siteLang = (lc $1) if defined $1;	# Not including country part
	$siteFullLang = (lc "$1-$2" ) 		# Including country part
		if defined $1 and defined $2;

	# Set environment variables for grep 
	$ENV{'LC_CTYPE'}= $siteLocale;

	# Load POSIX for I18N support 
	require POSIX;
	import POSIX qw( locale_h LC_CTYPE );

	# Set new locale
	my $locale = setlocale(&LC_CTYPE, $siteLocale);
	##writeDebug "New locale is $locale";
    }
}

=pod

---++ setupRegexes()

Set up pre-compiled regexes for use in rendering.  All regexes with
unchanging variables in match should use the '/o' option, even if not in a
loop, to help mod_perl, where the same code can be executed many times
without recompilation.

=cut

sub setupRegexes {

    # Build up character class components for use in regexes.
    # Depends on locale mode and Perl version, and finally on
    # whether locale-based regexes are turned off.
    if ( not $useLocale or $] < 5.006 or not $localeRegexes ) {
	# No locales needed/working, or Perl 5.005_03 or lower, so just use
	# any additional national characters defined in TWiki.cfg
	$regex{upperAlpha} = "A-Z$upperNational";
	$regex{lowerAlpha} = "a-z$lowerNational";
	$regex{numeric} = '\d';
	$regex{mixedAlpha} = "$regex{upperAlpha}$regex{lowerAlpha}";
    } else {
	# Perl 5.6 or higher with working locales
	$regex{upperAlpha} = "[:upper:]";
	$regex{lowerAlpha} = "[:lower:]";
	$regex{numeric} = "[:digit:]";
	$regex{mixedAlpha} = "[:alpha:]";
    }
    $regex{mixedAlphaNum} = "$regex{mixedAlpha}$regex{numeric}";
    $regex{lowerAlphaNum} = "$regex{lowerAlpha}$regex{numeric}";

    # Compile regexes for efficiency and ease of use
    # Note: qr// locks in regex modes (i.e. '-xism' here) - see Friedl
    # book at http://regex.info/. 

    # TWiki concept regexes
    $regex{wikiWordRegex} = qr/[$regex{upperAlpha}]+[$regex{lowerAlpha}]+[$regex{upperAlpha}]+[$regex{mixedAlphaNum}]*/;
    $regex{webNameRegex} = qr/[$regex{upperAlpha}]+[$regex{mixedAlphaNum}]*/;
    $regex{defaultWebNameRegex} = qr/_[$regex{mixedAlphaNum}_]+/;
    $regex{anchorRegex} = qr/\#[$regex{mixedAlphaNum}_]+/;
    $regex{abbrevRegex} = qr/[$regex{upperAlpha}]{3,}/;

    # Simplistic email regex, e.g. for WebNotify processing - no i18n
    # characters allowed
    $regex{emailAddrRegex} = qr/([A-Za-z0-9\.\+\-\_]+\@[A-Za-z0-9\.\-]+)/;

    # Filename regex, for attachments
    $regex{filenameRegex} = qr/[$regex{mixedAlphaNum}\.]+/;

    # Single-character alpha-based regexes
    $regex{singleUpperAlphaRegex} = qr/[$regex{upperAlpha}]/;
    $regex{singleLowerAlphaRegex} = qr/[$regex{lowerAlpha}]/;
    $regex{singleUpperAlphaNumRegex} = qr/[$regex{upperAlpha}$regex{numeric}]/;
    $regex{singleMixedAlphaNumRegex} = qr/[$regex{upperAlpha}$regex{lowerAlpha}$regex{numeric}]/;

    $regex{singleMixedNonAlphaRegex} = qr/[^$regex{upperAlpha}$regex{lowerAlpha}]/;
    $regex{singleMixedNonAlphaNumRegex} = qr/[^$regex{upperAlpha}$regex{lowerAlpha}$regex{numeric}]/;

    # Multi-character alpha-based regexes
    $regex{mixedAlphaNumRegex} = qr/[$regex{mixedAlphaNum}]*/;

    # Character encoding regexes

    # 7-bit ASCII only
    $regex{validAsciiStringRegex} = qr/^[\x00-\x7F]+$/;
    
    # Regex to match only a valid UTF-8 character, taking care to avoid
    # security holes due to overlong encodings by excluding the relevant
    # gaps in UTF-8 encoding space - see 'perldoc perlunicode', Unicode
    # Encodings section.  Tested against Markus Kuhn's UTF-8 test file
    # at http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt.
    $regex{validUtf8CharRegex} = qr{
				# Single byte - ASCII
				[\x00-\x7F] 
				|

				# 2 bytes
				[\xC2-\xDF][\x80-\xBF] 
				|

				# 3 bytes

				    # Avoid illegal codepoints - negative lookahead
				    (?!\xEF\xBF[\xBE\xBF])	

				    # Match valid codepoints
				    (?:
					([\xE0][\xA0-\xBF])|
					([\xE1-\xEC\xEE-\xEF][\x80-\xBF])|
					([\xED][\x80-\x9F])
				    )
				    [\x80-\xBF]
				|

				# 4 bytes 
				    (?:
					([\xF0][\x90-\xBF])|
					([\xF1-\xF3][\x80-\xBF])|
					([\xF4][\x80-\x8F])
				    )
				    [\x80-\xBF][\x80-\xBF]
			    }x;

    $regex{validUtf8StringRegex} = qr/^ (?: $regex{validUtf8CharRegex} )+ $/x;

}

=pod

---++ invalidSiteCharset()
Return value: boolean $isCharsetInvalid

Check for unusable multi-byte encodings as site character set
- anything that enables a single ASCII character such as '[' to be
matched within a multi-byte character cannot be used for TWiki.

=cut

sub invalidSiteCharset {
    # FIXME: match other problematic multi-byte character sets 
    return ( $siteCharset =~ /^(?:iso-2022-?|hz-?|.*big5|.*shift_?jis|ms.kanji)/i );
}


=pod

---++ convertUtf8URLtoSiteCharset( $webName, $topicName )
Return value: ( string $convertedWebName, string $convertedTopicName)
Auto-detect UTF-8 vs. site charset in URL, and convert UTF-8 into site charset.

TODO: remove dependence on webname and topicname, i.e. generic encoding
subroutine.

=cut

sub convertUtf8URLtoSiteCharset {
    my ( $webName, $topicName ) = @_;

    ##writeDebug "URL web.topic is $webName.$topicName";
    my $fullTopicName = "$webName.$topicName";
    my $charEncoding;

    # Detect character encoding of the full topic name from URL
    if ( $fullTopicName =~ $regex{validAsciiStringRegex} ) {
	$urlCharEncoding = 'ASCII';
    } elsif ( $fullTopicName =~ $regex{validUtf8StringRegex} ) {
	$urlCharEncoding = 'UTF-8';

	# Convert into ISO-8859-1 if it is the site charset
	if ( $siteCharset =~ /^iso-?8859-?1$/i ) {
	    # ISO-8859-1 maps onto first 256 codepoints of Unicode
	    # (conversion from 'perldoc perluniintro')
	    $fullTopicName =~ s/ ([\xC2\xC3]) ([\x80-\xBF]) / 
				 chr( ord($1) << 6 & 0xC0 | ord($2) & 0x3F )
				 /egx;
	} elsif ( $siteCharset eq "utf-8" ) {
	    # Convert into internal Unicode characters if on Perl 5.8 or higher.
	    if( $] >= 5.008 ) {
		require Encode;			# Perl 5.8 or higher only
		$fullTopicName = Encode::decode("utf8", $fullTopicName);	# 'decode' into UTF-8
	    } else {
		writeWarning "UTF-8 not supported on Perl $] - use Perl 5.8 or higher.";
	    }
	    writeWarning "UTF-8 not yet supported as site charset - TWiki is likely to have problems";
	} else {
	    # Convert from UTF-8 into some other site charset
	    writeDebug "Converting from UTF-8 to $siteCharset";

	    # Use conversion modules depending on Perl version
	    if( $] >= 5.008 ) {
		require Encode;			# Perl 5.8 or higher only
                import Encode qw(:fallbacks);
		# Map $siteCharset into real encoding name
		$charEncoding = Encode::resolve_alias( $siteCharset );
		if( not $charEncoding ) {
		    writeWarning "Conversion to \$siteCharset '$siteCharset' not supported, or name not recognised - check 'perldoc Encode::Supported'";
		} else {
		    ##writeDebug "Converting with Encode, valid 'to' encoding is '$charEncoding'";
		    # Convert text using Encode:
		    # - first, convert from UTF8 bytes into internal (UTF-8) characters
		    $fullTopicName = Encode::decode("utf8", $fullTopicName);	
		    # - then convert into site charset from internal UTF-8,
		    # inserting \x{NNNN} for characters that can't be converted
                    $fullTopicName = Encode::encode( $charEncoding, $fullTopicName, &FB_PERLQQ );
		    ##writeDebug "Encode result is $fullTopicName";
		}

	    } else {
		require Unicode::MapUTF8;	# Pre-5.8 Perl versions
		$charEncoding = $siteCharset;
		if( not Unicode::MapUTF8::utf8_supported_charset($charEncoding) ) {
		    writeWarning "Conversion to \$siteCharset '$siteCharset' not supported, or name not recognised - check 'perldoc Unicode::MapUTF8'";
		} else {
		    # Convert text
		    ##writeDebug "Converting with Unicode::MapUTF8, valid encoding is '$charEncoding'";
		    $fullTopicName = Unicode::MapUTF8::from_utf8({ 
			    			-string => $fullTopicName, 
		    			 	-charset => $charEncoding });
		    # FIXME: Check for failed conversion?
		}
	    }
	}
	($webName, $topicName) = split /\./, $fullTopicName;

    } else {
	# Non-ASCII and non-UTF-8 - assume in site character set, 
	# no conversion required
	$urlCharEncoding = 'Native';
	$charEncoding = $siteCharset;
    }
    ##writeDebug "Final web and topic are $webName $topicName ($urlCharEncoding URL -> $siteCharset)";

    return ($webName, $topicName);
}

=pod

---++ writeHeader ( $query )

Simple header setup for most scripts.  Calls writeHeaderFull, assuming
'basic' type and 'text/html' content-type.

=cut

sub writeHeader
{
    my( $query ) = @_;

    # FIXME: Pass real content-length to make persistent connections work
    # in HTTP/1.1 (performance improvement for browsers and servers). 
    # Requires significant but easy changes in various places.

    # Just write a basic content-type header for text/html
    writeHeaderFull( $query, 'basic', 'text/html', 0);
}


=pod

---++ writeHeaderFull( $query, $pageType, $contentType, $contentLength )

Builds and outputs HTTP headers.  $pageType should (currently) be either
"edit" or "basic".  $query is the object from the CGI module, not the actual
query string.

"edit" will cause headers to be generated that force caching for 24 hours, to
prevent Codev.BackFromPreviewLosesText bug, which caused data loss with IE5 and
IE6.

"basic" will cause only the Content-Type header to be set (from the
parameter), plus any headers set by plugins.  Hopefully, further types will
be used to improve cacheability for other pages in future.

Implements the post-Dec2001 release plugin API, which requires the
writeHeaderHandler in plugin to return a string of HTTP headers, CR/LF
delimited.  Filters out headers that the core code needs to generate for
whatever reason, and any illegal headers.

=cut

sub writeHeaderFull
{
    my( $query, $pageType, $contentType, $contentLength ) = @_;

    # Handle Edit pages - future versions will extend to caching
    # of other types of page, with expiry time driven by page type.
    my( $pluginHeaders, $coreHeaders );


    $contentType .= "; charset=$siteCharset";

    if ($pageType eq 'edit') {
	# Get time now in HTTP header format
	my $lastModifiedString = formatTime(time, 'http', "gmtime");

	# Expiry time is set high to avoid any data loss.  Each instance of 
	# Edit page has a unique URL with time-string suffix (fix for 
	# RefreshEditPage), so this long expiry time simply means that the 
	# browser Back button always works.  The next Edit on this page 
	# will use another URL and therefore won't use any cached 
	# version of this Edit page.
	my $expireHours = 24;
	my $expireSeconds = $expireHours * 60 * 60;

	# Set content length, to enable HTTP/1.1 persistent connections 
	# (aka HTTP keepalive), and cache control headers, to ensure edit page 
	# is cached until required expiry time.
	$coreHeaders = $query->header( 
			    -content_type => $contentType,
			    -content_length => $contentLength,
			    -last_modified => $lastModifiedString,
			    -expires => "+${expireHours}h",
			    -cache_control => "max-age=$expireSeconds",
			 );
    } elsif ($pageType eq 'basic') {
	$coreHeaders = $query->header(
	    		    -content_type => $contentType,
			 );
    } else {
	writeWarning( "===== invalid page type in TWiki.pm, writeHeaderFull(): $pageType" );
    }

    # Delete extra CR/LF to allow suffixing more headers
    $coreHeaders =~ s/\r\n\r\n$/\r\n/s;
    ##writeDebug( "===== After trim, Headers are:\n$coreHeaders" );

    # Wiki Plugin Hook - get additional headers from plugin
    $pluginHeaders = &TWiki::Plugins::writeHeaderHandler( $query ) || '';

    # Delete any trailing blank line
    $pluginHeaders =~ s/\r\n\r\n$/\r\n/s;

    # Add headers supplied by plugin, omitting any already in core headers
    my $finalHeaders = $coreHeaders;
    if( $pluginHeaders ) {
	# Build hash of all core header names, lower-cased
	my ($headerLine, $headerName, %coreHeaderSeen);
	for $headerLine (split /\r\n/, $coreHeaders) {
	    $headerLine =~ m/^([^ ]+): /i;		# Get header name
	    $headerName = lc($1);
	    ##writeDebug("==== core header name $headerName");
	    $coreHeaderSeen{$headerName}++;
	}
	# Append plugin headers if legal and not seen in core headers
	for $headerLine (split /\r\n/, $pluginHeaders) {
	    $headerLine =~ m/^([^ ]+): /i;		# Get header name
	    $headerName = lc($1);
	    if ( $headerName =~ m/[\-a-z]+/io ) {	# Skip bad headers
		##writeDebug("==== plugin header name $headerName");
		##writeDebug("Saw $headerName already ") if $coreHeaderSeen{$headerName};
		$finalHeaders .= $headerLine . "\r\n"
		    unless $coreHeaderSeen{$headerName};
	    }

	}
    }
    $finalHeaders .= "\r\n" if ( $finalHeaders);

    ##writeDebug( "===== Final Headers are:\n$finalHeaders" );
    print $finalHeaders;

}

=pod

---++ setPageMode( $mode )

Set page rendering mode:
   * 'rss' - encode 8-bit characters as XML entities
   * 'html' - (default) no encoding of 8-bit characters
   
=cut

sub setPageMode
{
    $pageMode = shift;
}

=pod

---++ getPageMode()
Return value: string $mode

Returns current page mode, 'html' unless set via setPageMode
FIXME: This function is currently unused.  Remove on some non
documentation-only commit, unless use is planned in future.

=cut

sub getPageMode
{
    return $pageMode;
}

=pod

---++ getCgiQuery()
Retrun value: string $query

Returns the CGI query portion (i.e. the bit after the '?') of the
current request.

=cut

sub getCgiQuery
{
    return $cgiQuery;
}

=pod

---++ redirect( $query, $url )

Redirects the request to $url, via the CGI module object $query unless
overridden by a plugin.  Note that this is currently only called by
Func::redirectCgiQuery() at the request of a plugin!  All of the redirects
done internally by TWiki are not overridable.

=cut

sub redirect
{
    my( $query, $url ) = @_;
    if( ! &TWiki::Plugins::redirectCgiQueryHandler( $query, $url ) ) {
        print $query->redirect( $url );
    }
}


=pod

---++ getEmailNotifyList( $webName, $topicName )
Return value: @emailNotifyList

Get email list from WebNotify page - this now handles entries of the form:
   * Main.UserName 
   * UserName 
   * Main.GroupName
   * GroupName
The 'UserName' format (i.e. no Main webname) is supported in any web, but
is not recommended since this may make future data conversions more
complicated, especially if used outside the Main web.  %<nop>MAINWEB% is OK
instead of 'Main'.  The user's email address(es) are fetched from their
user topic (home page) as long as they are listed in the '* Email:
fred@example.com' format.  Nested groups are supported.

=cut

sub getEmailNotifyList
{
    my( $web, $topicname ) = @_;

    $topicname = $notifyTopicname unless $topicname;
    return() unless &TWiki::Store::topicExists( $web, $topicname );

    # Allow %MAINWEB% as well as 'Main' in front of users/groups -
    # non-capturing regex.
    my $mainWebPattern = qr/(?:$mainWebname|%MAINWEB%)/;

    my @list = ();
    my %seen;			# Incremented when email address is seen
    foreach ( split ( /\n/, TWiki::Store::readWebTopic( $web, $topicname ) ) ) {
        if ( /^\s+\*\s(?:$mainWebPattern\.)?($regex{wikiWordRegex})\s+\-\s+($regex{emailAddrRegex})/o ) {
	    # Got full form:   * Main.WikiName - email@domain
	    # (the 'Main.' part is optional, non-capturing)
	    if ( $1 ne 'TWikiGuest' ) {
		# Add email address to list if non-guest and non-duplicate
		push (@list, $2) unless $seen{$1}++;
            }
        } elsif ( /^\s+\*\s(?:$mainWebPattern\.)?($regex{wikiWordRegex})\s*$/o ) { 
	    # Got short form:   * Main.WikiName
	    # (the 'Main.' part is optional, non-capturing)
            my $userWikiName = $1;
            foreach ( getEmailOfUser($userWikiName) ) {
		# Add email address to list if it's not a duplicate
                push (@list, $_) unless $seen{$_}++;
            }
        }
    }
    ##writeDebug "list of emails: @list";
    return( @list);
}

=pod

---++ getEmailOfUser( $wikiName )
Return value: ( $userEmail ) or @groupEmailList

Get e-mail address for a given WikiName from the user's home page, or
list of e-mail addresses for a group.  Nested groups are supported.
$wikiName must contain _only_ the WikiName; do *not* pass names of the
form "Main.JohnSmith".

=cut

sub getEmailOfUser
{
    my( $wikiName ) = @_;		# WikiName without web prefix

    my @list = ();
    # Ignore guest entry and non-existent pages
    if ( $wikiName ne "TWikiGuest" && 
		TWiki::Store::topicExists( $mainWebname, $wikiName ) ) {
        if ( $wikiName =~ /Group$/ ) {
            # Page is for a group, get all users in group
	    ##writeDebug "using group: $mainWebname . $wikiName";
	    my @userList = TWiki::Access::getUsersOfGroup( $wikiName ); 
	    foreach my $user ( @userList ) {
		$user =~ s/^.*\.//;	# Get rid of 'Main.' part.
		foreach my $email ( getEmailOfUser($user) ) {
		    push @list, $email;
		}
	    }
        } else {
	    # Page is for a user
	    ##writeDebug "reading home page: $mainWebname . $wikiName";
            foreach ( split ( /\n/, &TWiki::Store::readWebTopic( 
					    $mainWebname, $wikiName ) ) ) {
                if (/^\s\*\sEmail:\s+([\w\-\.\+]+\@[\w\-\.\+]+)/) {   
		    # Add email address to list
                    push @list, $1;
                }
            }
        }
    }
    return (@list);
}

=pod

---++ initializeRemoteUser( $remoteUser )
Return value: $remoteUser

Acts as a filter for $remoteUser.  If set, $remoteUser is filtered for
insecure characters and untainted.

If $doRememberRemoteUser and $remoteUser are both set, it also caches
$remoteUser as belonging to the IP address of the current request.

If $doRememberRemoteUser is set and $remoteUser is not, then it sets
$remoteUser to the last authenticated user to make a request with the
current request's IP address, or $defaultUserName if no cached name
is available.

If neither are set, then it sets $remoteUser to $defaultUserName.

=cut

sub initializeRemoteUser
{
    my( $theRemoteUser ) = @_;

    my $remoteUser = $theRemoteUser || $defaultUserName;
    $remoteUser =~ s/$securityFilter//go;
    $remoteUser =~ /(.*)/;
    $remoteUser = $1;  # untaint variable

    my $remoteAddr = $ENV{'REMOTE_ADDR'} || "";

    if( $ENV{'REDIRECT_STATUS'} && $ENV{'REDIRECT_STATUS'} eq '401' ) {
        # bail out if authentication failed
        $remoteAddr = "";
    }

    if( ( ! $doRememberRemoteUser ) || ( ! $remoteAddr ) ) {
        # do not remember IP address
        return $remoteUser;
    }

    my $text = &TWiki::Store::readFile( $remoteUserFilename );
    # Assume no I18N characters in userids, as for email addresses
    # FIXME: Needs fixing for IPv6?
    my %AddrToName = map { split( /\|/, $_ ) }
                     grep { /^[0-9\.]+\|[A-Za-z0-9]+\|$/ }
                     split( /\n/, $text );

    my $rememberedUser = "";
    if( exists( $AddrToName{ $remoteAddr } ) ) {
        $rememberedUser = $AddrToName{ $remoteAddr };
    }

    if( $theRemoteUser ) {
        if( $theRemoteUser ne $rememberedUser ) {
            $AddrToName{ $remoteAddr } = $theRemoteUser;
            # create file as "$remoteAddr|$theRemoteUser|" lines
            $text = "# This is a generated file, do not modify.\n";
            foreach my $usrAddr ( sort keys %AddrToName ) {
                my $usrName = $AddrToName{ $usrAddr };
                # keep $userName unique
                if(  ( $usrName ne $theRemoteUser )
                  || ( $usrAddr eq $remoteAddr ) ) {
                    $text .= "$usrAddr|$usrName|\n";
                }
            }
            &TWiki::Store::saveFile( $remoteUserFilename, $text );
        }
    } else {
        # get user name from AddrToName table
        $remoteUser = $rememberedUser || $defaultUserName;
    }

    return $remoteUser;
}

=pod

---++ userToWikiListInit()

Build hashes to translate in both directions between username (e.g. jsmith) 
and WikiName (e.g. JaneSmith).  Only used for sites where authentication is
managed by external Apache configuration, instead of via TWiki's .htpasswd
mechanism.

=cut

sub userToWikiListInit
{
    %userToWikiList = ();
    %wikiToUserList = ();

    # fix for Codev.SecurityAlertGainAdminRightWithTWikiUsersMapping
    # bail out in .htpasswd authenticated sites
    return unless( $doMapUserToWikiName );

    my @list = split( /\n/, &TWiki::Store::readFile( $userListFilename ) );

    # Get all entries with two '-' characters on same line, i.e.
    # 'WikiName - userid - date created'
    @list = grep { /^\s*\* $regex{wikiWordRegex}\s*-\s*[^\-]*-/o } @list;
    my $wUser;
    my $lUser;
    foreach( @list ) {
	# Get the WikiName and userid, and build hashes in both directions
        if(  ( /^\s*\* ($regex{wikiWordRegex})\s*\-\s*([^\s]*).*/o ) && $2 ) {
            $wUser = $1;	# WikiName
            $lUser = $2;	# userid
            $lUser =~ s/$securityFilter//go;	# FIXME: Should filter in for security...
            $userToWikiList{ $lUser } = $wUser;
            $wikiToUserList{ $wUser } = $lUser;
        }
    }
}

=pod

---++ userToWikiName( $loginUser, $dontAddWeb )
Return value: $wikiName

Translates intranet username (e.g. jsmith) to WikiName (e.g. JaneSmith)
userToWikiListInit must be called before this function is used.

Unless $dontAddWeb is set, "Main." is prepended to the returned WikiName.

=cut

sub userToWikiName
{
    my( $loginUser, $dontAddWeb ) = @_;
    
    if( !$loginUser ) {
        return "";
    }

    $loginUser =~ s/$securityFilter//go;
    my $wUser = $userToWikiList{ $loginUser } || $loginUser;
    if( $dontAddWeb ) {
        return $wUser;
    }
    return "$mainWebname.$wUser";
}

=pod

---++ wikiToUserName( $wikiName )
Return value: $loginUser

Translates WikiName (e.g. JaneSmith) to an intranet username (e.g. jsmith)
userToWikiListInit must be called before this function is used.

=cut

sub wikiToUserName
{
    my( $wikiUser ) = @_;
    $wikiUser =~ s/^.*\.//g;
    my $userName =  $wikiToUserList{"$wikiUser"} || $wikiUser;
    ##writeDebug( "TWiki::wikiToUserName: $wikiUser->$userName" );
    return $userName;
}

=pod

---++ isGuest()

Returns whether the current user is TWikiGuest or equivalent.

=cut

sub isGuest
{
   return ( $userName eq $defaultUserName );
}

# =========================
=pod

---++ sub getWikiUserTopic ()

Not yet documented.

=cut

sub getWikiUserTopic
{
    # Topic without Web name
    return $wikiName;
}

# =========================
# Check for a valid WikiWord or WikiName
=pod

---++ sub isWikiName (  $name  )

Not yet documented.

=cut

sub isWikiName
{
    my( $name ) = @_;

    $name ||= "";	# Default value if undef
    return ( $name =~ m/^$regex{wikiWordRegex}$/o )
}

# =========================
# Check for a valid ABBREV (acronym)
=pod

---++ sub isAbbrev (  $name  )

Not yet documented.

=cut

sub isAbbrev
{
    my( $name ) = @_;

    $name ||= "";	# Default value if undef
    return ( $name =~ m/^$regex{abbrevRegex}$/o )
}

# =========================
# Check for a valid web name
=pod

---++ sub isWebName (  $name  )

Not yet documented.

=cut

sub isWebName
{
    my( $name ) = @_;

    $name ||= "";	# Default value if undef
    return ( $name =~ m/^$regex{webNameRegex}$/o )
}

# =========================
=pod

---++ sub readOnlyMirrorWeb (  $theWeb  )

Not yet documented.

=cut

sub readOnlyMirrorWeb
{
    my( $theWeb ) = @_;

    my @mirrorInfo = ( "", "", "", "" );
    if( $siteWebTopicName ) {
        my $mirrorSiteName = &TWiki::Prefs::getPreferencesValue( "MIRRORSITENAME", $theWeb );
        if( $mirrorSiteName && $mirrorSiteName ne $siteWebTopicName ) {
            my $mirrorViewURL  = &TWiki::Prefs::getPreferencesValue( "MIRRORVIEWURL", $theWeb );
            my $mirrorLink = &TWiki::Store::readTemplate( "mirrorlink" );
            $mirrorLink =~ s/%MIRRORSITENAME%/$mirrorSiteName/g;
            $mirrorLink =~ s/%MIRRORVIEWURL%/$mirrorViewURL/g;
            $mirrorLink =~ s/\s*$//g;
            my $mirrorNote = &TWiki::Store::readTemplate( "mirrornote" );
            $mirrorNote =~ s/%MIRRORSITENAME%/$mirrorSiteName/g;
            $mirrorNote =~ s/%MIRRORVIEWURL%/$mirrorViewURL/g;
            $mirrorNote = getRenderedVersion( $mirrorNote, $theWeb );
            $mirrorNote =~ s/\s*$//g;
            @mirrorInfo = ( $mirrorSiteName, $mirrorViewURL, $mirrorLink, $mirrorNote );
        }
    }
    return @mirrorInfo;
}


# =========================
=pod

---++ sub getDataDir ()

Not yet documented.

=cut

sub getDataDir
{
    return $dataDir;
}

# =========================
=pod

---++ sub getPubDir ()

Not yet documented.

=cut

sub getPubDir
{
    return $pubDir;
}

# =========================
=pod

---++ sub getPubUrlPath ()

Not yet documented.

=cut

sub getPubUrlPath
{
    return $pubUrlPath;
}

=pod

---++ getTWikiLibDir()

If necessary, finds the full path of the directory containing TWiki.pm,
and sets the variable $twikiLibDir so that this process is only performed
once per invocation.  (mod_perl safe: lib dir doesn't change.)

=cut

sub getTWikiLibDir
{
    if( $twikiLibDir ) {
        return $twikiLibDir;
    }

    # FIXME: Should just use $INC{"TWiki.pm"} to get path used to load this
    # module.
    my $dir = "";
    foreach $dir ( @INC ) {
        if( -e "$dir/TWiki.pm" ) {
            $twikiLibDir = $dir;
            last;
        }
    }

    # fix relative path
    if( $twikiLibDir =~ /^\./ ) {
        my $curr = cwd();
        $twikiLibDir = "$curr/$twikiLibDir/";
        # normalize "/../" and "/./"
        while ( $twikiLibDir =~ s|([\\/])[^\\/]+[\\/]\.\.[\\/]|$1| ) {};
        $twikiLibDir =~ s|([\\/])\.[\\/]|$1|g;
    }
    $twikiLibDir =~ s|([\\/])[\\/]*|$1|g; # reduce "//" to "/"
    $twikiLibDir =~ s|[\\/]$||;           # cut trailing "/"

    return $twikiLibDir;
}

# =========================
=pod

---++ sub revDate2ISO ()

Not yet documented.

=cut

sub revDate2ISO
{
    my $epochSec = revDate2EpSecs( $_[0] );
    return formatTime( $epochSec, 1, "gmtime");
}

# =========================
=pod

---++ sub revDate2EpSecs ()

Not yet documented.

=cut

sub revDate2EpSecs
# Convert RCS revision date/time to seconds since epoch, for easier sorting 
{
    my( $date ) = @_;
    # NOTE: This routine *will break* if input is not one of below formats!
    
    # FIXME - why aren't ifs around pattern match rather than $5 etc
    # try "31 Dec 2001 - 23:59"  (TWiki date)
    if ($date =~ /([0-9]+)\s+([A-Za-z]+)\s+([0-9]+)[\s\-]+([0-9]+)\:([0-9]+)/) {
        my $year = $3;
        $year -= 1900 if( $year > 1900 );
        return timegm( 0, $5, $4, $1, $mon2num{$2}, $year );
    }

    # try "2001/12/31 23:59:59" or "2001.12.31.23.59.59" (RCS date)
    if ($date =~ /([0-9]+)[\.\/\-]([0-9]+)[\.\/\-]([0-9]+)[\.\s\-]+([0-9]+)[\.\:]([0-9]+)[\.\:]([0-9]+)/) {
        my $year = $1;
        $year -= 1900 if( $year > 1900 );
        return timegm( $6, $5, $4, $3, $2-1, $year );
    }

    # try "2001/12/31 23:59" or "2001.12.31.23.59" (RCS short date)
    if ($date =~ /([0-9]+)[\.\/\-]([0-9]+)[\.\/\-]([0-9]+)[\.\s\-]+([0-9]+)[\.\:]([0-9]+)/) {
        my $year = $1;
        $year -= 1900 if( $year > 1900 );
        return timegm( 0, $5, $4, $3, $2-1, $year );
    }

    # try "2001-12-31T23:59:59Z" or "2001-12-31T23:59:59+01:00" (ISO date)
    # FIXME: Calc local to zulu time "2001-12-31T23:59:59+01:00"
    if ($date =~ /([0-9]+)\-([0-9]+)\-([0-9]+)T([0-9]+)\:([0-9]+)\:([0-9]+)/ ) {
        my $year = $1;
        $year -= 1900 if( $year > 1900 );
        return timegm( $6, $5, $4, $3, $2-1, $year );
    }

    # try "2001-12-31T23:59Z" or "2001-12-31T23:59+01:00" (ISO short date)
    # FIXME: Calc local to zulu time "2001-12-31T23:59+01:00"
    if ($date =~ /([0-9]+)\-([0-9]+)\-([0-9]+)T([0-9]+)\:([0-9]+)/ ) {
        my $year = $1;
        $year -= 1900 if( $year > 1900 );
        return timegm( 0, $5, $4, $3, $2-1, $year );
    }

    # give up, return start of epoch (01 Jan 1970 GMT)
    return 0;
}

# =========================
=pod

---++ sub getSessionValue ()

Not yet documented.

=cut

sub getSessionValue
{
#   my( $key ) = @_;
    return &TWiki::Plugins::getSessionValueHandler( @_ );
}

# =========================
=pod

---++ sub setSessionValue ()

Not yet documented.

=cut

sub setSessionValue
{
#   my( $key, $value ) = @_;
    return &TWiki::Plugins::setSessionValueHandler( @_ );
}

# =========================
=pod

---++ sub getSkin ()

Not yet documented.

=cut

sub getSkin
{
    my $skin = "";
    $skin = $cgiQuery->param( 'skin' ) if( $cgiQuery );
    $skin = &TWiki::Prefs::getPreferencesValue( "SKIN" ) unless( $skin );
    return $skin;
}

# =========================
=pod

---++ sub getViewUrl (  $web, $topic  )

Returns a fully-qualified URL to the specified topic, which must be normalized
into separate specified =$web= and =$topic= parts.

=cut

sub getViewUrl
{
    my( $theWeb, $theTopic ) = @_;
    # PTh 20 Jun 2000: renamed sub viewUrl to getViewUrl, added $theWeb
    # WM 14 Feb 2004: Removed support for old syntax not specifying $theWeb

    $theTopic =~ s/\s*//gs; # Illegal URL, remove space

    # PTh 24 May 2000: added $urlHost, needed for some environments
    # see also Codev.PageRedirectionNotWorking
    return "$urlHost$dispScriptUrlPath$dispViewPath$scriptSuffix/$theWeb/$theTopic";
}

=pod

---++ getScriptURL( $web, $topic, $script )
Return value: $absoluteScriptURL

Returns the absolute URL to a TWiki script, providing the wub and topic as
"path info" parameters.  The result looks something like this:
"http://host/twiki/bin/$script/$web/$topic"

=cut

sub getScriptUrl
{
    my( $theWeb, $theTopic, $theScript ) = @_;
    
    my $url = "$urlHost$dispScriptUrlPath/$theScript$scriptSuffix/$theWeb/$theTopic";

    # FIXME consider a plugin call here - useful for certificated logon environment
    
    return $url;
}

=pod

---++ getOopsUrl( $web, $topic, $template, @scriptParams )
Return Value: $absoluteOopsURL

Composes a URL for an "oops" error page.  The last parameters depend on the
specific oops template in use, and are passed in the URL as 'param1..paramN'.

The returned URL ends up looking something like:
"http://host/twiki/bin/oops/$web/$topic?template=$template&param1=$scriptParams[0]..."

=cut

sub getOopsUrl
{
    my( $theWeb, $theTopic, $theTemplate,
        $theParam1, $theParam2, $theParam3, $theParam4 ) = @_;
    # PTh 20 Jun 2000: new sub
    my $web = $webName;  # current web
    if( $theWeb ) {
        $web = $theWeb;
    }
    my $url = "";
    # $urlHost is needed, see Codev.PageRedirectionNotWorking
    $url = getScriptUrl( $web, $theTopic, "oops" );
    $url .= "\?template=$theTemplate";
    $url .= "\&amp;param1=" . handleUrlEncode( $theParam1 ) if ( $theParam1 );
    $url .= "\&amp;param2=" . handleUrlEncode( $theParam2 ) if ( $theParam2 );
    $url .= "\&amp;param3=" . handleUrlEncode( $theParam3 ) if ( $theParam3 );
    $url .= "\&amp;param4=" . handleUrlEncode( $theParam4 ) if ( $theParam4 );

    return $url;
}

# =========================
=pod

---++ sub makeTopicSummary (  $theText, $theTopic, $theWeb  )

Not yet documented.

=cut

sub makeTopicSummary
{
    my( $theText, $theTopic, $theWeb ) = @_;
    # called by search, mailnotify & changes after calling readFileHead

    my $htext = $theText;
    # Format e-mail to add spam padding (HTML tags removed later)
    $htext =~ s/([\s\(])(?:mailto\:)*([a-zA-Z0-9\-\_\.\+]+)\@([a-zA-Z0-9\-\_\.]+)\.([a-zA-Z0-9\-\_]+)(?=[\s\.\,\;\:\!\?\)])/$1 . &mailtoLink( $2, $3, $4 )/ge;
    $htext =~ s/<\!\-\-.*?\-\->//gs;  # remove all HTML comments
    $htext =~ s/<\!\-\-.*$//s;        # cut HTML comment
    $htext =~ s/<[^>]*>//g;           # remove all HTML tags
    $htext =~ s/\&[a-z]+;/ /g;        # remove entities
    $htext =~ s/%WEB%/$theWeb/g;      # resolve web
    $htext =~ s/%TOPIC%/$theTopic/g;  # resolve topic
    $htext =~ s/%WIKITOOLNAME%/$wikiToolName/g; # resolve TWiki tool name
    $htext =~ s/%META:.*?%//g;        # remove meta data variables
    $htext =~ s/\[\[([^\]]*\]\[|[^\s]*\s)(.*?)\]\]/$2/g; # keep only link text of [[][]]
    $htext =~ s/[\%\[\]\*\|=_\&\<\>]/ /g; # remove Wiki formatting chars & defuse %VARS%
    $htext =~ s/\-\-\-+\+*\s*\!*/ /g; # remove heading formatting
    $htext =~ s/\s+[\+\-]*/ /g;       # remove newlines and special chars

    # FIXME I18N: Avoid splitting within multi-byte characters (e.g. EUC-JP
    # encoding) by encoding bytes as Perl UTF-8 characters in Perl 5.8+. 
    # This avoids splitting within a Unicode codepoint (or a UTF-16
    # surrogate pair, which is encoded as a single Perl UTF-8 character),
    # but we ideally need to avoid splitting closely related Unicode codepoints.
    # Specifically, this means Unicode combining character sequences (e.g.
    # letters and accents) - might be better to split on word boundary if
    # possible.

    # limit to 162 chars 
    $htext =~ s/(.{162})($regex{mixedAlphaNumRegex})(.*?)$/$1$2 \.\.\./g;

    # Encode special chars into XML &#nnn; entities for use in RSS feeds
    # - no encoding for HTML pages, to avoid breaking international 
    # characters. FIXME: Only works for ISO-8859-1 characters, where the
    # Unicode encoding (&#nnn;) is identical.
    if( $pageMode eq 'rss' ) {
	# FIXME: Issue for EBCDIC/UTF-8
	$htext =~ s/([\x7f-\xff])/"\&\#" . unpack( "C", $1 ) .";"/ge;
    }

    # prevent text from getting rendered in inline search and link tool 
    # tip text by escaping links (external, internal, Interwiki)
    $htext =~ s/([\s\(])(?=\S)/$1<nop>/g;
    $htext =~ s/([\-\*\s])($regex{linkProtocolPattern}\:)/$1<nop>$2/go;
    $htext =~ s/@([a-zA-Z0-9\-\_\.]+)/@<nop>$1/g;	# email address

    return $htext;
}

# =========================
=pod

---++ sub extractNameValuePair (  $str, $name  )

Not yet documented.

=cut

sub extractNameValuePair
{
    my( $str, $name ) = @_;

    my $value = "";
    return $value unless( $str );
    $str =~ s/\\\"/\\$TranslationToken/g;  # escape \"

    if( $name ) {
        # format is: %VAR{ ... name = "value" }%
        if( $str =~ /(^|[^\S])$name\s*=\s*\"([^\"]*)\"/ ) {
            $value = $2 if defined $2;  # distinguish between "" and "0"
        }

    } else {
        # test if format: { "value" ... }
        if( $str =~ /(^|\=\s*\"[^\"]*\")\s*\"(.*?)\"\s*(\w+\s*=\s*\"|$)/ ) {
            # is: %VAR{ "value" }%
            # or: %VAR{ "value" param="etc" ... }%
            # or: %VAR{ ... = "..." "value" ... }%
            # Note: "value" may contain embedded double quotes
            $value = $2 if defined $2;  # distinguish between "" and "0";

        } elsif( ( $str =~ /^\s*\w+\s*=\s*\"([^\"]*)/ ) && ( $1 ) ) {
            # is: %VAR{ name = "value" }%
            # do nothing, is not a standalone var

        } else {
            # format is: %VAR{ value }%
            $value = $str;
        }
    }
    $value =~ s/\\$TranslationToken/\"/go;  # resolve \"
    return $value;
}

# =========================
=pod

---++ sub fixN (  $theTag  )

Not yet documented.

=cut

sub fixN
{
    my( $theTag ) = @_;
    $theTag =~ s/[\r\n]+//gs;
    return $theTag;
}

# =========================
=pod

---++ sub fixURL (  $theHost, $theAbsPath, $theUrl  )

Not yet documented.

=cut

sub fixURL
{
    my( $theHost, $theAbsPath, $theUrl ) = @_;

    my $url = $theUrl;
    if( $url =~ /^\// ) {
        # fix absolute URL
        $url = "$theHost$url";
    } elsif( $url =~ /^\./ ) {
        # fix relative URL
        $url = "$theHost$theAbsPath/$url";
    } elsif( $url =~ /^$regex{linkProtocolPattern}\:/ ) {
        # full qualified URL, do nothing
    } elsif( $url ) {
        # FIXME: is this test enough to detect relative URLs?
        $url = "$theHost$theAbsPath/$url";
    }

    return $url;
}

# =========================
=pod

---++ sub fixIncludeLink (  $theWeb, $theLink, $theLabel  )

Not yet documented.

=cut

sub fixIncludeLink
{
    my( $theWeb, $theLink, $theLabel ) = @_;

    if( $theLabel ) {
        # [[...][...]] link
        if( $theLink =~ /^($regex{webNameRegex}\.|$regex{defaultWebNameRegex}\.|$regex{linkProtocolPattern}\:)/ ) {
            return "[[$theLink][$theLabel]]";  # no change
        }
        # add 'Web.' prefix
        return "[[$theWeb.$theLink][$theLabel]]";

    } else {
        # [[...]] link
        if( $theLink =~ /^($regex{webNameRegex}\.|$regex{defaultWebNameRegex}\.|$regex{linkProtocolPattern}\:)/ ) {
            return "[[$theLink]]";  # no change
        }
        # add 'Web.' prefix
        return "[[$theWeb.$theLink][$theLink]]";
    }
}

# =========================
=pod

---++ sub handleIncludeUrl (  $theUrl, $thePattern  )

Not yet documented.

=cut

sub handleIncludeUrl
{
    my( $theUrl, $thePattern ) = @_;
    my $text = "";
    my $host = "";
    my $port = 80;
    my $path = "";
    my $user = "";
    my $pass = "";

    # RNF 22 Jan 2002 Handle http://user:pass@host
    if( $theUrl =~ /http\:\/\/(.+)\:(.+)\@([^\:]+)\:([0-9]+)(\/.*)/ ) {
        $user = $1;
        $pass = $2;
        $host = $3;
        $port = $4;
        $path = $5;

    } elsif( $theUrl =~ /http\:\/\/(.+)\:(.+)\@([^\/]+)(\/.*)/ ) {
        $user = $1;
        $pass = $2;
        $host = $3;
        $path = $4;

    } elsif( $theUrl =~ /http\:\/\/([^\:]+)\:([0-9]+)(\/.*)/ ) {
        $host = $1;
        $port = $2;
        $path = $3;

    } elsif( $theUrl =~ /http\:\/\/([^\/]+)(\/.*)/ ) {
        $host = $1;
        $path = $2;

    } else {
        $text = showError( "Error: Unsupported protocol. (Must be 'http://domain/...')" );
        return $text;
    }

    $text = &TWiki::Net::getUrl( $host, $port, $path, $user, $pass );
    $text =~ s/\r\n/\n/gs;
    $text =~ s/\r/\n/gs;
    $text =~ s/^(.*?\n)\n(.*)/$2/s;
    my $httpHeader = $1;
    my $contentType = "";
    if( $httpHeader =~ /content\-type\:\s*([^\n]*)/ois ) {
        $contentType = $1;
    }
    if( $contentType =~ /^text\/html/ ) {
        $path =~ s/(.*)\/.*/$1/; # build path for relative address
        $host = "http://$host";   # build host for absolute address
        if( $port != 80 ) {
            $host .= ":$port";
        }

        # FIXME: Make aware of <base> tag

        $text =~ s/^.*?<\/head>//is;            # remove all HEAD
        $text =~ s/<script.*?<\/script>//gis;   # remove all SCRIPTs
        $text =~ s/^.*?<body[^>]*>//is;         # remove all to <BODY>
        $text =~ s/(?:\n)<\/body>//is;          # remove </BODY>
        $text =~ s/(?:\n)<\/html>//is;          # remove </HTML>
        $text =~ s/(<[^>]*>)/&fixN($1)/ges;     # join tags to one line each
        $text =~ s/(\s(href|src|action)\=[\"\']?)([^\"\'\>\s]*)/$1 . &fixURL( $host, $path, $3 )/geois;

    } elsif( $contentType =~ /^text\/plain/ ) {
        # do nothing

    } else {
        $text = showError( "Error: Unsupported content type: $contentType."
              . " (Must be text/html or text/plain)" );
    }

    if( $thePattern ) {
        $thePattern =~ s/([^\\])([\$\@\%\&\#\'\`\/])/$1\\$2/g;  # escape some special chars
        $thePattern =~ /(.*)/;     # untaint
        $thePattern = $1;
        $text = "" unless( $text =~ s/$thePattern/$1/is );
    }

    return $text;
}

=pod

---++ handleIncludeFile( $includeCommandAttribs, $topic, $web, \@verbatimBuffer, @processedTopics )
Return value: $includedText

Processes a specific instance %<nop>INCLUDE{...}% syntax.  Returns the text to be
inserted in place of the INCLUDE command.  $topic and $web should be for the
immediate parent topic in the include hierarchy. @verbatimBuffer is the request-
global buffer for storing removed verbatim blocks, and @processedTopics is a
list of topics already %<nop>INCLUDE%'ed -- these are not allowed to be included
again to prevent infinte recursive inclusion.

=cut

sub handleIncludeFile
{
    my( $theAttributes, $theTopic, $theWeb, $verbatim, @theProcessedTopics ) = @_;
    my $incfile = extractNameValuePair( $theAttributes );
    my $pattern = extractNameValuePair( $theAttributes, "pattern" );
    my $rev     = extractNameValuePair( $theAttributes, "rev" );
    my $warn    = extractNameValuePair( $theAttributes, "warn" );

    if( $incfile =~ /^http\:/ ) {
        # include web page
        return handleIncludeUrl( $incfile, $pattern );
    }

    # CrisBailiff, PeterThoeny 12 Jun 2000: Add security
    $incfile =~ s/$securityFilter//go;    # zap anything suspicious
    if( $doSecureInclude ) {
        # Filter out ".." from filename, this is to
        # prevent includes of "../../file"
        $incfile =~ s/\.+/\./g;
    } else {
        # danger, could include .htpasswd with relative path
        $incfile =~ s/passwd//gi;    # filter out passwd filename
    }

    # test for different usage
    my $fileName = "$dataDir/$theWeb/$incfile";       # TopicName.txt
    if( ! -e $fileName ) {
        $fileName = "$dataDir/$theWeb/$incfile.txt";  # TopicName
        if( ! -e $fileName ) {
            $fileName = "$dataDir/$incfile";              # Web/TopicName.txt
            if( ! -e $fileName ) {
                $incfile =~ s/\.([^\.]*)$/\/$1/g;
                $fileName = "$dataDir/$incfile.txt";      # Web.TopicName
                if( ! -e $fileName ) {
                    # give up, file not found
                    $warn = TWiki::Prefs::getPreferencesValue( "INCLUDEWARNING" ) unless( $warn );
                    if( $warn =~ /^on$/i ) {
                        return showError( "Warning: Can't INCLUDE <nop>$incfile, topic not found" );
                    } elsif( $warn && $warn !~ /^(off|no)$/i ) {
                        $incfile =~ s/\//\./go;
                        $warn =~ s/\$topic/$incfile/go;
                        return $warn;
                    } # else fail silently
                    return "";
                }
            }
        }
    }

    # prevent recursive loop
    if( ( @theProcessedTopics ) && ( grep { /^$fileName$/ } @theProcessedTopics ) ) {
        # file already included
        if( $warn || TWiki::Prefs::getPreferencesFlag( "INCLUDEWARNING" ) ) {
            unless( $warn =~ /^(off|no)$/i ) {
                return showError( "Warning: Can't INCLUDE <nop>$incfile twice, topic is already included" );
            }
        }
        return "";
    } else {
        # remember for next time
        push( @theProcessedTopics, $fileName );
    }

    my $text = "";
    my $meta = "";
    my $isTopic = 0;

    # set include web/filenames and current web/filenames
    $includingWebName = $theWeb;
    $includingTopicName = $theTopic;
    $fileName =~ s/\/([^\/]*)\/([^\/]*)(\.txt)$/$1/g;
    if( $3 ) {
        # identified "/Web/TopicName.txt" filename, e.g. a Wiki topic
        # so save the current web and topic name
        $theWeb = $1;
        $theTopic = $2;
        $isTopic = 1;

        if( $rev ) {
            $rev = "1.$rev" unless( $rev =~ /^1\./ );
            ( $meta, $text ) = &TWiki::Store::readTopicVersion( $theWeb, $theTopic, $rev );
        } else {
            ( $meta, $text ) = &TWiki::Store::readTopic( $theWeb, $theTopic );
        }
        # remove everything before %STARTINCLUDE% and after %STOPINCLUDE%
        $text =~ s/.*?%STARTINCLUDE%//s;
        $text =~ s/%STOPINCLUDE%.*//s;

    } # else is a file with relative path, e.g. $dataDir/../../path/to/non-twiki/file.ext

    if( $pattern ) {
        $pattern =~ s/([^\\])([\$\@\%\&\#\'\`\/])/$1\\$2/g;  # escape some special chars
        $pattern =~ /(.*)/;     # untaint
        $pattern = $1;
        $text = "" unless( $text =~ s/$pattern/$1/is );
    }

    # handle all preferences and internal tags (for speed: call by reference)
    $text = takeOutVerbatim( $text, $verbatim );

    # handle all preferences and internal tags
    &TWiki::Prefs::handlePreferencesTags( $text );
    handleInternalTags( $text, $theTopic, $theWeb );

    # Wiki Plugin Hook (4th parameter tells plugin that its called from an include)
    &TWiki::Plugins::commonTagsHandler( $text, $theTopic, $theWeb, 1 );

    # handle tags again because of plugin hook
    &TWiki::Prefs::handlePreferencesTags( $text );
    handleInternalTags( $text, $theTopic, $theWeb );

    # If needed, fix all "TopicNames" to "Web.TopicNames" to get the right context
    if( ( $isTopic ) && ( $theWeb ne $webName ) ) {
        # "TopicName" to "Web.TopicName"
        $text =~ s/(^|[\s\(])($regex{webNameRegex}\.$regex{wikiWordRegex})/$1$TranslationToken$2/go;
        $text =~ s/(^|[\s\(])($regex{wikiWordRegex}|$regex{abbrevRegex})/$1$theWeb\.$2/go;
        $text =~ s/(^|[\s\(])$TranslationToken/$1/go;
        # "[[TopicName]]" to "[[Web.TopicName][TopicName]]"
        $text =~ s/\[\[([^\]]+)\]\]/fixIncludeLink( $theWeb, $1 )/geo;
        # "[[TopicName][...]]" to "[[Web.TopicName][...]]"
        $text =~ s/\[\[([^\]]+)\]\[([^\]]+)\]\]/fixIncludeLink( $theWeb, $1, $2 )/geo;
        # FIXME: Support for <noautolink>
    }
    
    # FIXME What about attachments?

    # recursively process multiple embedded %INCLUDE% statements and prefs
    $text =~ s/%INCLUDE{(.*?)}%/&handleIncludeFile($1, $theTopic, $theWeb, $verbatim, @theProcessedTopics )/ge;

    return $text;
}

# =========================
# Only does simple search for topicmoved at present, can be expanded when required
=pod

---++ sub handleMetaSearch (  $attributes  )

Not yet documented.

=cut

sub handleMetaSearch
{
    my( $attributes ) = @_;
    
    my $attrWeb           = extractNameValuePair( $attributes, "web" );
    my $attrTopic         = extractNameValuePair( $attributes, "topic" );
    my $attrType          = extractNameValuePair( $attributes, "type" );
    my $attrTitle         = extractNameValuePair( $attributes, "title" );
    
    my $searchVal = "XXX";
    
    if( ! $attrType ) {
       $attrType = "";
    }
    
    
    my $searchWeb = "all";
    
    if( $attrType eq "topicmoved" ) {
       $searchVal = "%META:TOPICMOVED[{].*from=\\\"$attrWeb\.$attrTopic\\\".*[}]%";
    } elsif ( $attrType eq "parent" ) {
       $searchWeb = $attrWeb;
       $searchVal = "%META:TOPICPARENT[{].*name=\\\"($attrWeb\\.)?$attrTopic\\\".*[}]%";
    }
    
    my $text = &TWiki::Search::searchWeb(
        "inline"        => "1",
        "search"        => $searchVal,
        "web"           => $searchWeb,
        "type"          => "regex",
        "nosummary"     => "on",
        "nosearch"      => "on",
        "noheader"      => "on",
        "nototal"       => "on",
        "noempty"       => "on",
        "template"      => "searchmeta",
    );    
    
    if( $text !~ /^\s*$/ ) {
       $text = "$attrTitle$text";
    }
    
    return $text;
}

# =========================
=pod

---++ sub handleSearchWeb (  $attributes  )

Not yet documented.

=cut

sub handleSearchWeb
{
    my( $attributes ) = @_;

    return &TWiki::Search::searchWeb(
        "inline"        => "1",
        "search"        => extractNameValuePair( $attributes ) || extractNameValuePair( $attributes, "search" ),
        "web"           => extractNameValuePair( $attributes, "web" ),
        "topic"         => extractNameValuePair( $attributes, "topic" ),
        "excludetopic"  => extractNameValuePair( $attributes, "excludetopic" ),
        "scope"         => extractNameValuePair( $attributes, "scope" ),
        "order"         => extractNameValuePair( $attributes, "order" ),
        "type"          => extractNameValuePair( $attributes, "type" )
                        || TWiki::Prefs::getPreferencesValue( "SEARCHVARDEFAULTTYPE" ),
        "regex"         => extractNameValuePair( $attributes, "regex" ),
        "limit"         => extractNameValuePair( $attributes, "limit" ),
        "reverse"       => extractNameValuePair( $attributes, "reverse" ),
        "casesensitive" => extractNameValuePair( $attributes, "casesensitive" ),
        "nosummary"     => extractNameValuePair( $attributes, "nosummary" ),
        "nosearch"      => extractNameValuePair( $attributes, "nosearch" ),
        "noheader"      => extractNameValuePair( $attributes, "noheader" ),
        "nototal"       => extractNameValuePair( $attributes, "nototal" ),
        "bookview"      => extractNameValuePair( $attributes, "bookview" ),
        "renameview"    => extractNameValuePair( $attributes, "renameview" ),
        "showlock"      => extractNameValuePair( $attributes, "showlock" ),
        "expandvariables" => extractNameValuePair( $attributes, "expandvariables" ),
        "noempty"       => extractNameValuePair( $attributes, "noempty" ),
        "template"      => extractNameValuePair( $attributes, "template" ),
        "header"        => extractNameValuePair( $attributes, "header" ),
        "format"        => extractNameValuePair( $attributes, "format" ),
        "multiple"      => extractNameValuePair( $attributes, "multiple" ),
        "separator"     => extractNameValuePair( $attributes, "separator" ),
    );
}

# =========================
#TODO: this seems like a duplication with formatGmTime and formatLocTime
#remove any 2.
=pod

---++ sub handleTime (  $theAttributes, $theZone  )

Not yet documented.

=cut

sub handleTime
{
    my( $theAttributes, $theZone ) = @_;
    # format examples:
    #   28 Jul 2000 15:33:59 is "$day $month $year $hour:$min:$sec"
    #   001128               is "$ye$mo$day"

    my $format = extractNameValuePair( $theAttributes );

    my $value = "";
    my $time = time();

#    if( $format ) {
        $value = formatTime($time, $format, $theZone);
 #   } else {
 #       if( $theZone eq "gmtime" ) {
 #           $value = gmtime( $time );
 #       } elsif( $theZone eq "servertime" ) {
 #           $value = localtime( $time );
 #       }
 #   }

#    if( $theZone eq "gmtime" ) {
#		$value = $value." GMT";
#	}

    return $value;
}

# =========================
=pod
---++ sub formatTime ($epochSeconds, $formatString, $outputTimeZone) ==> $value
| $epochSeconds | epochSecs GMT |
| $formatString | twiki time date format |
| $outputTimeZone | timezone to display. (not sure this will work)(gmtime or servertime) |

=cut
sub formatTime 
{
    my ($epochSeconds, $formatString, $outputTimeZone) = @_;
    my $value = $epochSeconds;

    # use default TWiki format "31 Dec 1999 - 23:59" unless specified
    $formatString = "\$day \$month \$year - \$hour:\$min" unless( $formatString );
    $outputTimeZone = $displayTimeValues unless( $outputTimeZone );

    my( $sec, $min, $hour, $day, $mon, $year, $wday) = gmtime( $epochSeconds );
      ( $sec, $min, $hour, $day, $mon, $year, $wday ) = localtime( $epochSeconds ) if( $outputTimeZone eq "servertime" );

    #standard twiki date time formats
    if( $formatString =~ /rcs/i ) {
        # RCS format, example: "2001/12/31 23:59:59"
        $formatString = "\$year/\$mo/\$day \$hour:\$min:\$sec";
    } elsif ( $formatString =~ /http|email/i ) {
        # HTTP header format, e.g. "Thu, 23 Jul 1998 07:21:56 EST"
 	    # - based on RFC 2616/1123 and HTTP::Date; also used
        # by TWiki::Net for Date header in emails.
        $formatString = "\$wday, \$day \$month \$year \$hour:\$min:\$sec \$tz";
    } elsif ( $formatString =~ /iso/i ) {
        # ISO Format, see spec at http://www.w3.org/TR/NOTE-datetime
        # e.g. "2002-12-31T19:30Z"
        $formatString = "\$year-\$mo-\$dayT\$hour:\$min";
        if( $outputTimeZone eq "gmtime" ) {
            $formatString = $formatString."Z";
        } else {
            #TODO:            $formatString = $formatString.  # TZD  = time zone designator (Z or +hh:mm or -hh:mm) 
        }
    } 
    
    $value = $formatString;
    $value =~ s/\$sec[o]?[n]?[d]?[s]?/sprintf("%.2u",$sec)/geoi;
    $value =~ s/\$min[u]?[t]?[e]?[s]?/sprintf("%.2u",$min)/geoi;
    $value =~ s/\$hou[r]?[s]?/sprintf("%.2u",$hour)/geoi;
    $value =~ s/\$day/sprintf("%.2u",$day)/geoi;
    my @weekDay = ("Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat");
    $value =~ s/\$wday/$weekDay[$wday]/geoi;
    $value =~ s/\$mon[t]?[h]?/$isoMonth[$mon]/goi;
    $value =~ s/\$mo/sprintf("%.2u",$mon+1)/geoi;
    $value =~ s/\$yea[r]?/sprintf("%.4u",$year+1900)/geoi;
    $value =~ s/\$ye/sprintf("%.2u",$year%100)/geoi;
        
#TODO: how do we get the different timezone strings (and when we add usertime, then what?)    
    my $tz_str = "GMT";
    $tz_str = "Local" if ( $outputTimeZone eq "servertime" );
    $value =~ s/\$tz/$tz_str/geoi;
 
    return $value;        
}

#AS
# =========================
=pod

---++ sub showError (  $errormessage  )

Not yet documented.

=cut

sub showError
{
    my( $errormessage ) = @_;
    return "<font size=\"-1\" color=\"#FF0000\">$errormessage</font>" ;
}

=pod

---++ handleToc( $text, $topic, $web, $tocAttributes )
Parameters:
   * $text          : the text of the current topic
   * $topic         : the topic we are in
   * $web           : the web we are in
   * $tocAttributes : "Topic" [web="Web"] [depth="N"]
Return value: $tableOfContents

Andrea Sterbini 22-08-00 / PTh 28 Feb 2001

Handles %<nop>TOC{...}% syntax.  Creates a table of contents using TWiki bulleted
list markup, linked to the section headings of a topic. A section heading is
entered in one of the following forms:
   * $headingPatternSp : \t++... spaces section heading
   * $headingPatternDa : ---++... dashes section heading
   * $headingPatternHt : &lt;h[1-6]> HTML section heading &lt;/h[1-6]>

=cut

sub handleToc
{
    ##     $_[0]     $_[1]      $_[2]    $_[3]
    ## my( $theText, $theTopic, $theWeb, $attributes ) = @_;

    # get the topic name attribute
    my $topicname = extractNameValuePair( $_[3] )  || $_[1];

    # get the web name attribute
    my $web = extractNameValuePair( $_[3], "web" ) || $_[2];
    $web =~ s/\//\./g;
    my $webPath = $web;
    $webPath =~ s/\./\//g;

    # get the depth limit attribute
    my $depth = extractNameValuePair( $_[3], "depth" ) || 6;

    #get the title attribute
    my $title = extractNameValuePair( $_[3], "title" ) || "";
    $title = "\n<span class=\"title\">$title</span>" if( $title );

    my $result  = "";
    my $line  = "";
    my $level = "";
    my @list  = ();

    if( "$web.$topicname" eq "$_[2].$_[1]" ) {
        # use text from parameter
        @list = split( /\n/, $_[0] );

    } else {
        # read text from file
        if ( ! &TWiki::Store::topicExists( $web, $topicname ) ) {
            return showError( "TOC: Cannot find topic \"$web.$topicname\"" );
        }
        @list = split( /\n/, handleCommonTags( 
            &TWiki::Store::readWebTopic( $web, $topicname ), $topicname, $web ) );
    }

    @list = grep { /(<\/?pre>)|($regex{headerPatternDa})|($regex{headerPatternSp})|($regex{headerPatternHt})/ } @list;
    my $insidePre = 0;
    my $i = 0;
    my $tabs = "";
    my $anchor = "";
    my $highest = 99;
    foreach $line ( @list ) {
        if( $line =~ /^.*<pre>.*$/io ) {
            $insidePre = 1;
            $line = "";
        }
        if( $line =~ /^.*<\/pre>.*$/io ) {
            $insidePre = 0;
            $line = "";
        }
        if (!$insidePre) {
            $level = $line ;
            if ( $line =~  /$regex{headerPatternDa}/o ) {
                $level =~ s/$regex{headerPatternDa}/$1/go;
                $level = length $level;
                $line  =~ s/$regex{headerPatternDa}/$2/go;
            } elsif
               ( $line =~  /$regex{headerPatternSp}/o ) {
                $level =~ s/$regex{headerPatternSp}/$1/go;
                $level = length $level;
                $line  =~ s/$regex{headerPatternSp}/$2/go;
            } elsif
               ( $line =~  /$regex{headerPatternHt}/io ) {
                $level =~ s/$regex{headerPatternHt}/$1/gio;
                $line  =~ s/$regex{headerPatternHt}/$2/gio;
            }
            if( ( $line ) && ( $level <= $depth ) ) {
                $anchor = makeAnchorName( $line );
                # cut TOC exclude '---+ heading !! exclude'
                $line  =~ s/\s*$regex{headerPatternNoTOC}.+$//go;
                $line  =~ s/[\n\r]//go;
                next unless $line;
                $highest = $level if( $level < $highest );
                $tabs = "";
                for( $i=0 ; $i<$level ; $i++ ) {
                    $tabs = "\t$tabs";
                }
                # Remove *bold* and _italic_ formatting
                $line =~ s/(^|[\s\(])\*([^\s]+?|[^\s].*?[^\s])\*($|[\s\,\.\;\:\!\?\)])/$1$2$3/g;
                $line =~ s/(^|[\s\(])_+([^\s]+?|[^\s].*?[^\s])_+($|[\s\,\.\;\:\!\?\)])/$1$2$3/g;
                # Prevent WikiLinks
                $line =~ s/\[\[.*?\]\[(.*?)\]\]/$1/g;  # '[[...][...]]'
                $line =~ s/\[\[(.*?)\]\]/$1/ge;        # '[[...]]'
                $line =~ s/([\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})/$1<nop>$3/g;  # 'Web.TopicName'
                $line =~ s/([\s\(])($regex{wikiWordRegex})/$1<nop>$2/g;  # 'TopicName'
                $line =~ s/([\s\(])($regex{abbrevRegex})/$1<nop>$2/g;  # 'TLA'
                # create linked bullet item
                # AB change
		# $line = "$tabs* <a href=\"$dispScriptUrlPath/$viewScript$scriptSuffix/$webPath/$topicname#$anchor\">$line</a>";
		if ( $viewScript eq 'view' ) {
		    $line = "$tabs* <a href=\"$dispScriptUrlPath$dispViewPath/$webPath/$topicname#$anchor\">$line</a>";
		} else {
		    $line = "$tabs* <a href=\"$dispScriptUrlPath/$viewScript$scriptSuffix/$webPath/$topicname#$anchor\">$line</a>";
		}
                $result .= "\n$line";
            }
        }
    }
    if( $result ) {
        if( $highest > 1 ) {
            # left shift TOC
            $highest--;
            $result =~ s/^\t{$highest}//gm;
        }
        $result = "<div id=\"twikitoc\">$title$result\n</div>";
        return $result;

    } else {
        return showError("TOC: No TOC in \"$web.$topicname\"");
    }
}

# =========================
=pod

---++ sub getPublicWebList ()

Not yet documented.

=cut

sub getPublicWebList
{
    # FIXME: Should this go elsewhere?
    # (Not in Store because Store should not be dependent on Prefs.)

    if( ! @publicWebList ) {
        # build public web list, e.g. exclude hidden webs, but include current web
        my @list = &TWiki::Store::getAllWebs( "" );
        my $item = "";
        my $hidden = "";
        foreach $item ( @list ) {
            $hidden = &TWiki::Prefs::getPreferencesValue( "NOSEARCHALL", $item );
            # exclude topics that are hidden or start with . or _ unless current web
            if( ( $item eq $TWiki::webName  ) || ( ( ! $hidden ) && ( $item =~ /^[^\.\_]/ ) ) ) {
                push( @publicWebList, $item );
            }
        }
    }
    return @publicWebList;
}

# =========================
=pod

---++ sub expandVariablesOnTopicCreation ( $text )

Expand limited set of variables with a topic during topic creation

=cut

sub expandVariablesOnTopicCreation {
  my ( $theText, $theUser, $theWikiName, $theWikiUserName ) = @_;

  my $today = formatTime(time(), "\$day \$mon \$year", "gmtime");
  $theUser         = $userName                     unless $theUser;
  $theWikiName     = userToWikiName( $theUser, 1 ) unless $theWikiName;
  $theWikiUserName = userToWikiName( $theUser )    unless $theWikiUserName;

  $theText =~ s/%DATE%/$today/go;
  $theText =~ s/%USERNAME%/$theUser/go;                     # "jdoe"
  $theText =~ s/%WIKINAME%/$theWikiName/go;                 # "JonDoe"
  $theText =~ s/%WIKIUSERNAME%/$theWikiUserName/go;         # "Main.JonDoe"
  $theText =~ s/%URLPARAM{(.*?)}%/&handleUrlParam($1)/geo;  # expand URL parameters
  $theText =~ s/%NOP{.*?}%//gos;  # Remove filler: Use it to remove access control at time of
  $theText =~ s/%NOP%//go;        # topic instantiation or to prevent search from hitting a template

  return $theText;
}

# =========================
=pod

---++ sub handleWebAndTopicList (  $theAttr, $isWeb  )

Not yet documented.

=cut

sub handleWebAndTopicList
{
    my( $theAttr, $isWeb ) = @_;

    my $format = extractNameValuePair( $theAttr ) || extractNameValuePair( $theAttr, "format" );
    $format .= '$name' unless( $format =~ /\$name/ );
    my $separator = extractNameValuePair( $theAttr, "separator" ) || "\n";
    my $web = extractNameValuePair( $theAttr, "web" ) || "";
    my $webs = extractNameValuePair( $theAttr, "webs" ) || "public";
    my $selection = extractNameValuePair( $theAttr, "selection" ) || "";
    $selection =~ s/\,/ /g;
    $selection = " $selection ";
    my $marker    = extractNameValuePair( $theAttr, "marker" ) || 'selected="selected"';

    my @list = ();
    if( $isWeb ) {
        my @webslist = split( /,\s?/, $webs );
        foreach my $aweb ( @webslist ) {
            if( $aweb eq "public" ) {
                push( @list, getPublicWebList() );
            } elsif( $aweb eq "webtemplate" ) {
                push( @list, grep { /^\_/o } &TWiki::Store::getAllWebs( "" ) );
            } else{
                push( @list, $aweb ) if( &TWiki::Store::webExists( $aweb ) );
            }
        }
    } else {
        $web = $webName if( ! $web );
        my $hidden = &TWiki::Prefs::getPreferencesValue( "NOSEARCHALL", $web );
        if( ( $web eq $TWiki::webName  ) || ( ! $hidden ) ) {
            @list = &TWiki::Store::getTopicNames( $web );
        }
    }
    my $text = "";
    my $item = "";
    my $line = "";
#$text .= "format: $format, selection: $selection, marker: $marker\n";
    my $mark = "";
    foreach $item ( @list ) {
        $line = $format;
        $line =~ s/\$web/$web/goi;
        $line =~ s/\$name/$item/goi;
        $line =~ s/\$qname/"$item"/goi;
        $mark = ( $selection =~ / $item / ) ? $marker : "";
#$text .= "item: $item, mark: $mark.";
        $line =~ s/\$marker/$mark/goi;
        $text .= "$line$separator";
    }
    $text =~ s/$separator$//s;  # remove last separator
    return $text;
}

# =========================
=pod

---++ sub handleUrlParam (  $theArgs  )

Not yet documented.

=cut

sub handleUrlParam
{
    my( $theArgs ) = @_;

    my $param     = extractNameValuePair( $theArgs );
    my $newLine   = extractNameValuePair( $theArgs, "newline" ) || "";
    my $encode    = extractNameValuePair( $theArgs, "encode" ) || "";
    my $multiple  = extractNameValuePair( $theArgs, "multiple" ) || "";
    my $separator = extractNameValuePair( $theArgs, "separator" ) || "\n";
    my $value = "";
    if( $cgiQuery ) {
        if( $multiple ) {
            my @valueArray = $cgiQuery->param( $param );
            if( @valueArray ) {
                unless( $multiple =~ m/^on$/i ) {
                    my $item = "";
                    @valueArray = map {
                        $item = $_;
                        $_ = $multiple;
                        $_ .= $item unless( s/\$item/$item/go );
                        $_
                    } @valueArray;
                }
                $value = join ( $separator, @valueArray );
            }
        } else {
            $value = $cgiQuery->param( $param );
            $value = "" unless( defined $value );
        }
    }
    $value = handleUrlEncode( "\"$value\" type=\"$encode\"", 1 ) if( $encode );
    $value =~ s/\r?\n/$newLine/go if( $newLine );
    unless( $value ) {
        $value = extractNameValuePair( $theArgs, "default" ) || "";
    }
    return $value;
}

# =========================
# Encode to URL parameter or HTML entity
# TODO: For non-ISO-8859-1 $siteCharset, need to convert to Unicode 
# for use in entity, or to UTF-8 before URL encoding.

=pod

---++ sub handleUrlEncode (  $theArgs, $doExtract  )

Not yet documented.

=cut

sub handleUrlEncode
{
    my( $theArgs, $doExtract ) = @_;

    my $text = $theArgs;
    my $type = "";
    if( $doExtract ) {
        $text = extractNameValuePair( $theArgs );
        $type = extractNameValuePair( $theArgs, "type" ) || "";
    }
    if( $type =~ /^entit(y|ies)$/i ) {
        # HTML entity encoding
	# TODO: Encode characters > 0x7F to Unicode first
        $text =~ s/\"/\&\#034;/g;
        $text =~ s/\%/\&\#037;/g;
        $text =~ s/\*/\&\#042;/g;
        $text =~ s/\_/\&\#095;/g;
        $text =~ s/\=/\&\#061;/g;
        $text =~ s/\[/\&\#091;/g;
        $text =~ s/\]/\&\#093;/g;
        $text =~ s/\</\&\#060;/g;
        $text =~ s/\>/\&\#062;/g;
        $text =~ s/\|/\&\#124;/g;
    } else {
        # URL encoding
        $text =~ s/[\n\r]/\%3Cbr\%20\%2F\%3E/g;
        $text =~ s/\s+/\%20/g;
        $text =~ s/\"/\%22/g;
        $text =~ s/\&/\%26/g;
        $text =~ s/\+/\%2B/g;
        $text =~ s/\</\%3C/g;
        $text =~ s/\>/\%3E/g;
        $text =~ s/\\/\%5C/g;
        # Encode characters > 0x7F (ASCII-derived charsets only)
	# TODO: Encode to UTF-8 first
        $text =~ s/([\x7f-\xff])/'%' . unpack( "H*", $1 ) /ge;
    }
    return $text;
}


=pod

---++ sub handleNativeUrlEncode ( $theStr, $doExtract )

Perform URL encoding into native charset ($siteCharset) - for use when
viewing attachments via browsers that generate UTF-8 URLs, on sites running
with non-UTF-8 (Native) character sets.  Aim is to prevent UTF-8 URL
encoding.  For mainframes, we assume that UTF-8 URLs will be translated
by the web server to an EBCDIC character set.

=cut

sub handleNativeUrlEncode {
    my( $theStr, $doExtract ) = @_;

    my $isEbcdic = ( 'A' eq chr(193) ); 	# True if Perl is using EBCDIC

    if( $siteCharset eq "utf-8" or $isEbcdic ) {
	# Just strip double quotes, no URL encoding - let browser encode to
	# UTF-8 or EBCDIC based $siteCharset as appropriate
	$theStr =~ s/^"(.*)"$/$1/;	
	return $theStr;
    } else {
	return handleUrlEncode( $theStr, $doExtract );
    }
}

=pod

---++ sub handleIntUrlEncode ( $theStr, $doExtract )

This routine was introduced to URL encode Mozilla's UTF-8 POST URLs in the
TWiki Feb2003 release - encoding is no longer needed since UTF-URLs are now
directly supported, but it is provided for backward compatibility with
skins that may still be using the deprecated %INTURLENCODE%.

=cut

sub handleIntUrlEncode
{
    my( $theStr ) = @_;

    # Just strip double quotes, no URL encoding - Mozilla UTF-8 URLs
    # directly supported now
    $theStr =~ s/^"(.*)"$/$1/;	
    return $theStr;
}

=pod

---++ sub handleEnvVariable (  $theVar  )

Not yet documented.

=cut

sub handleEnvVariable
{
    my( $theVar ) = @_;
    my $value = $ENV{$theVar} || "";
    return $value;
}

=pod

---++ sub handleTmplP (  $theParam  )

Not yet documented.

=cut

sub handleTmplP
{
    my( $theParam ) = @_;

    $theParam = extractNameValuePair( $theParam );
    my $value = &TWiki::Store::handleTmplP( $theParam );
    return $value;
}

# =========================
# Create spaced-out topic name for Ref-By search 
=pod

---++ sub handleSpacedTopic (  $theTopic  )

Not yet documented.

=cut

sub handleSpacedTopic
{
    my( $theTopic ) = @_;
    my $spacedTopic = $theTopic;
    $spacedTopic =~ s/($regex{singleLowerAlphaRegex}+)($regex{singleUpperAlphaNumRegex}+)/$1%20*$2/go;   # "%20*" is " *" - I18N: only in ASCII-derived charsets
    return $spacedTopic;
}

# =========================
=pod

---++ sub handleIcon (  $theParam  )

Not yet documented.

=cut

sub handleIcon
{
    my( $theParam ) = @_;

    $theParam = extractNameValuePair( $theParam );
    my $value = &TWiki::Attach::filenameToIcon( "file.$theParam" );
    return $value;
}

=pod

---++ sub handleRelativeTopicPath ( $styleTopic, $web )

Not yet documented.

=cut

sub handleRelativeTopicPath
{
       my( $theStyleTopic, $theWeb ) = @_;

       if ( !$theStyleTopic ) {
               return "";
       }
       my $theRelativePath;
       # if there is no dot in $theStyleTopic, no web has been specified
       if ( index( $theStyleTopic, "." ) == -1 ) {
               # add local web
               $theRelativePath = $theWeb . "/" . $theStyleTopic;
       } else {
               $theRelativePath = $theStyleTopic; #including dot
       }
       # replace dot by slash is not necessary; TWiki.MyTopic is a valid url
       # add ../ if not already present to make a relative file reference
       if ( index( $theRelativePath, "../" ) == -1 ) {
               $theRelativePath = "../" . $theRelativePath;
       }
       return $theRelativePath;
}

=pod

---++ handleInternalTags( $text, $topic, $web )

Modifies $text in-place, replacing variables internal to TWiki with their
values.  Some example variables: %<nop>TOPIC%, %<nop>SCRIPTURL%, %<nop>WIKINAME%, etc.

=cut

sub handleInternalTags
{
    # modify arguments directly, i.e. call by reference
    # $_[0] is text
    # $_[1] is topic
    # $_[2] is web

    # Make Edit URL unique for every edit - fix for RefreshEditPage.
    $_[0] =~ s!%EDITURL%!"$dispScriptUrlPath/edit$scriptSuffix/%WEB%/%TOPIC%\?t=" . time()!ge;

    $_[0] =~ s/%NOP{(.*?)}%/$1/gs;  # remove NOP tag in template topics but show content
    $_[0] =~ s/%NOP%/<nop>/g;
    $_[0] =~ s/%TMPL\:P{(.*?)}%/&handleTmplP($1)/ge;
    $_[0] =~ s/%SEP%/&handleTmplP('"sep"')/ge;

    $_[0] =~ s/%HTTP_HOST%/&handleEnvVariable('HTTP_HOST')/ge;
    $_[0] =~ s/%REMOTE_ADDR%/&handleEnvVariable('REMOTE_ADDR')/ge;
    $_[0] =~ s/%REMOTE_PORT%/&handleEnvVariable('REMOTE_PORT')/ge;
    $_[0] =~ s/%REMOTE_USER%/&handleEnvVariable('REMOTE_USER')/ge;

    $_[0] =~ s/%TOPIC%/$_[1]/g;
    $_[0] =~ s/%BASETOPIC%/$topicName/g;
    $_[0] =~ s/%INCLUDINGTOPIC%/$includingTopicName/g;
    $_[0] =~ s/%SPACEDTOPIC%/&handleSpacedTopic($_[1])/ge;
    $_[0] =~ s/%WEB%/$_[2]/g;
    $_[0] =~ s/%BASEWEB%/$webName/g;
    $_[0] =~ s/%INCLUDINGWEB%/$includingWebName/g;

    # I18N information
    $_[0] =~ s/%CHARSET%/$siteCharset/g;
    $_[0] =~ s/%SHORTLANG%/$siteLang/g;
    $_[0] =~ s/%LANG%/$siteFullLang/g;

    $_[0] =~ s/%TOPICLIST{(.*?)}%/&handleWebAndTopicList($1,'0')/ge;
    $_[0] =~ s/%WEBLIST{(.*?)}%/&handleWebAndTopicList($1,'1')/ge;

    # URLs and paths
    $_[0] =~ s/%WIKIHOMEURL%/$wikiHomeUrl/g;
    $_[0] =~ s/%SCRIPTURL%/$urlHost$dispScriptUrlPath/g;
    $_[0] =~ s/%SCRIPTURLPATH%/$dispScriptUrlPath/g;
    $_[0] =~ s/%SCRIPTSUFFIX%/$scriptSuffix/g;
    $_[0] =~ s/%PUBURL%/$urlHost$pubUrlPath/g;
    $_[0] =~ s/%PUBURLPATH%/$pubUrlPath/g;
    $_[0] =~ s/%RELATIVETOPICPATH{(.*?)}%/&handleRelativeTopicPath($1,$_[2])/ge;

    # Attachments
    $_[0] =~ s!%ATTACHURL%!$urlHost%ATTACHURLPATH%!g;
    # I18N: URL-encode full web, topic and filename to the native
    # $siteCharset for attachments viewed from browsers that use UTF-8 URL,
    # unless we are in UTF-8 mode or working on EBCDIC mainframe.
    # Include the filename suffixed to %ATTACHURLPATH% - a hack, but required
    # for migration purposes
    $_[0] =~ s!%ATTACHURLPATH%/($regex{filenameRegex})!&handleNativeUrlEncode("$pubUrlPath/$_[2]/$_[1]/$1",1)!ge;
    $_[0] =~ s!%ATTACHURLPATH%!&handleNativeUrlEncode("$pubUrlPath/$_[2]/$_[1]",1)!ge;	# No-filename case
    $_[0] =~ s/%ICON{(.*?)}%/&handleIcon($1)/ge;

    # URL encoding
    $_[0] =~ s/%URLPARAM{(.*?)}%/&handleUrlParam($1)/ge;
    $_[0] =~ s/%(URL)?ENCODE{(.*?)}%/&handleUrlEncode($2,1)/ge; 	# ENCODE is documented, URLENCODE is legacy
    $_[0] =~ s/%INTURLENCODE{(.*?)}%/&handleIntUrlEncode($1)/ge;	# Deprecated - not needed with UTF-8 URL support
    
    # Dates and times
    $_[0] =~ s/%DATE%/&formatTime(time(), "\$day \$mon \$year", "gmtime")/ge; 					# Deprecated, but used in signatures
    $_[0] =~ s/%GMTIME%/&handleTime("","gmtime")/ge;
    $_[0] =~ s/%GMTIME{(.*?)}%/&handleTime($1,"gmtime")/ge;
    $_[0] =~ s/%SERVERTIME%/&handleTime("","servertime")/ge;
    $_[0] =~ s/%SERVERTIME{(.*?)}%/&handleTime($1,"servertime")/ge;
    $_[0] =~ s/%DISPLAYTIME%/&handleTime("", $displayTimeValues)/ge;
    $_[0] =~ s/%DISPLAYTIME{(.*?)}%/&handleTime($1, $displayTimeValues)/ge;

    $_[0] =~ s/%WIKIVERSION%/$wikiversion/g;
    $_[0] =~ s/%USERNAME%/$userName/g;
    $_[0] =~ s/%WIKINAME%/$wikiName/g;
    $_[0] =~ s/%WIKIUSERNAME%/$wikiUserName/g;
    $_[0] =~ s/%WIKITOOLNAME%/$wikiToolName/g;
    $_[0] =~ s/%MAINWEB%/$mainWebname/g;
    $_[0] =~ s/%TWIKIWEB%/$twikiWebname/g;
    $_[0] =~ s/%HOMETOPIC%/$mainTopicname/g;
    $_[0] =~ s/%WIKIUSERSTOPIC%/$wikiUsersTopicname/g;
    $_[0] =~ s/%WIKIPREFSTOPIC%/$wikiPrefsTopicname/g;
    $_[0] =~ s/%WEBPREFSTOPIC%/$webPrefsTopicname/g;
    $_[0] =~ s/%NOTIFYTOPIC%/$notifyTopicname/g;
    $_[0] =~ s/%STATISTICSTOPIC%/$statisticsTopicname/g;
    $_[0] =~ s/%STARTINCLUDE%//g;
    $_[0] =~ s/%STOPINCLUDE%//g;
    $_[0] =~ s/%SECTION{(.*?)}%//g;
    $_[0] =~ s/%ENDSECTION%//g;
    $_[0] =~ s/%SEARCH{(.*?)}%/&handleSearchWeb($1)/ge; # can be nested
    $_[0] =~ s/%SEARCH{(.*?)}%/&handleSearchWeb($1)/ge if( $_[0] =~ /%SEARCH/o );
    $_[0] =~ s/%METASEARCH{(.*?)}%/&handleMetaSearch($1)/ge;

}

=pod

---++ takeOutVerbatim( $text, \@verbatimBuffer )
Return value: $textWithoutVerbatim

Searches through $text and extracts &lt;verbatim> blocks, appending each
onto the end of the @verbatimBuffer array and replacing it with a token
string which is not affected by TWiki rendering.  The text after these
substitutions is returned.

This function is designed to preserve the contents of verbatim blocks
through some rendering operation.  The general sequence of calls for
this use is something like this:

   $textToRender = takeOutVerbatim($inputText, \@verbatimBlocks);
   $renderedText = performSomeRendering($textToRender);
   $resultText = putBackVerbatim($renderedText, "pre", @verbatimBlocks);

Note that some changes are made to verbatim blocks here: &lt; and > are replaced
by their HTML entities &amp;lt; and &amp;gt;, and the actual &lt;verbatim>
tags are replaced with &lt;pre> tags so that the text is rendered truly
"verbatim" by a browser.  If this is not desired, pass "verbatim" as the
second parameter of putBackVerbatim instead of "pre".

=cut

sub takeOutVerbatim
{
    my( $intext, $verbatim ) = @_;	# $verbatim is ref to array
    
    if( $intext !~ /<verbatim>/oi ) {
        return( $intext );
    }
    
    # Exclude text inside verbatim from variable substitution
    
    my $tmp = "";
    my $outtext = "";
    my $nesting = 0;
    my $verbatimCount = $#{$verbatim} + 1;
    
    foreach( split( /\n/, $intext ) ) {
        if( /^(\s*)<verbatim>\s*$/i ) {
            $nesting++;
            if( $nesting == 1 ) {
                $outtext .= "$1%_VERBATIM$verbatimCount%\n";
                $tmp = "";
                next;
            }
        } elsif( m|^\s*</verbatim>\s*$|i ) {
            $nesting--;
            if( ! $nesting ) {
                $verbatim->[$verbatimCount++] = $tmp;
                next;
            }
        }

        if( $nesting ) {
            $tmp .= "$_\n";
        } else {
            $outtext .= "$_\n";
        }
    }
    
    # Deal with unclosed verbatim
    if( $nesting ) {
        $verbatim->[$verbatimCount] = $tmp;
    }
       
    return $outtext;
}

=pod

---++putBackVerbatim( $textWithoutVerbatim, $putBackType, @verbatimBuffer )
Return value: $textWithVerbatim

This function reverses the actions of takeOutVerbatim above.  See the text for
takeOutVerbatim for a more thorough description.

Set $putBackType to 'verbatim' to get back original text, or to 'pre' to
convert to HTML readable verbatim text.

=cut

sub putBackVerbatim
{
    my( $text, $type, @verbatim ) = @_;
    
    for( my $i=0; $i<=$#verbatim; $i++ ) {
        my $val = $verbatim[$i];
        if( $type ne "verbatim" ) {
            $val =~ s/</&lt;/g;
            $val =~ s/</&gt;/g;
            $val =~ s/\t/   /g; # A shame to do this, but been in TWiki.org have converted
                                # 3 spaces to tabs since day 1
        }
        $text =~ s|%_VERBATIM$i%|<$type>\n$val</$type>|;
    }

    return $text;
}

=pod

---++ handleCommonTags( $text, $topic, $web, @processedTopics )
Return value: $handledText

Processes %<nop>VARIABLE%, %<nop>TOC%, and %<nop>INCLUDE% syntax; also includes
"commonTagsHandler" plugin hook.  If processing an included topic,
@processedTopics should be a list of topics already included, or in
the process of being included.

Returns the text of the topic, after file inclusion, variable substitution,
table-of-contents generation, and any plugin changes from commonTagsHandler.

=cut

sub handleCommonTags
{
    my( $text, $theTopic, $theWeb, @theProcessedTopics ) = @_;

    # PTh 22 Jul 2000: added $theWeb for correct handling of %INCLUDE%, %SEARCH%
    if( !$theWeb ) {
        $theWeb = $webName;
    }
    
    my @verbatim = ();
    $text = takeOutVerbatim( $text, \@verbatim );

    # handle all preferences and internal tags (for speed: call by reference)
    $includingWebName = $theWeb;
    $includingTopicName = $theTopic;
    &TWiki::Prefs::handlePreferencesTags( $text );
    handleInternalTags( $text, $theTopic, $theWeb );

    # recursively process multiple embedded %INCLUDE% statements and prefs
    $text =~ s/%INCLUDE{(.*?)}%/&handleIncludeFile($1, $theTopic, $theWeb, \@verbatim, @theProcessedTopics )/ge;

    # Wiki Plugin Hook
    &TWiki::Plugins::commonTagsHandler( $text, $theTopic, $theWeb, 0 );

    # handle tags again because of plugin hook
    &TWiki::Prefs::handlePreferencesTags( $text );
    handleInternalTags( $text, $theTopic, $theWeb );

    $text =~ s/%FORMFIELD{(.*?)}%/&getFormField($theWeb,$theTopic,$1)/ge;
    $text =~ s/%TOC{([^}]*)}%/&handleToc($text,$theTopic,$theWeb,$1)/ge;
    $text =~ s/%TOC%/&handleToc($text,$theTopic,$theWeb,"")/ge;
#SVEN
    $text =~ s/%GROUPS%/join( ", ", &TWiki::Access::getListOfGroups() )/ge;
    
    # Ideally would put back in getRenderedVersion rather than here which would save removing
    # it again!  But this would mean altering many scripts to pass back verbatim
    $text = putBackVerbatim( $text, "verbatim", @verbatim );

    return $text;
}

# =========================
=pod

---++ sub handleMetaTags (  $theWeb, $theTopic, $text, $meta, $isTopRev  )

Not yet documented.

=cut

sub handleMetaTags
{
    my( $theWeb, $theTopic, $text, $meta, $isTopRev ) = @_;

    $text =~ s/%META{\s*"form"\s*}%/&renderFormData( $theWeb, $theTopic, $meta )/ge;
    $text =~ s/%META{\s*"formfield"\s*(.*?)}%/&renderFormField( $meta, $1 )/ge;
    $text =~ s/%META{\s*"attachments"\s*(.*)}%/&TWiki::Attach::renderMetaData( $theWeb,
                                                $theTopic, $meta, $1, $isTopRev )/ge;
    $text =~ s/%META{\s*"moved"\s*}%/&renderMoved( $theWeb, $theTopic, $meta )/ge;
    $text =~ s/%META{\s*"parent"\s*(.*)}%/&renderParent( $theWeb, $theTopic, $meta, $1 )/ge;

    $text = &TWiki::handleCommonTags( $text, $theTopic );

    return $text;
}

# ========================
=pod

---++ sub renderParent (  $web, $topic, $meta, $args  )

Not yet documented.

=cut

sub renderParent
{
    my( $web, $topic, $meta, $args ) = @_;
    
    my $text = "";

    my $dontRecurse = 0;
    my $noWebHome = 0;
    my $prefix = "";
    my $suffix = "";
    my $usesep = "";

    if( $args ) {
       $dontRecurse = extractNameValuePair( $args, "dontrecurse" );
       $noWebHome =   extractNameValuePair( $args, "nowebhome" );
       $prefix =      extractNameValuePair( $args, "prefix" );
       $suffix =      extractNameValuePair( $args, "suffix" );
       $usesep =      extractNameValuePair( $args, "separator" );
    }

    if( ! $usesep ) {
       $usesep = " &gt; ";
    }

    my %visited = ();
    $visited{"$web.$topic"} = 1;

    my $sep = "";
    my $cWeb = $web;

    while( 1 ) {
        my %parent = $meta->findOne( "TOPICPARENT" );
        if( %parent ) {
            my $name = $parent{"name"};
            my $pWeb = $cWeb;
            my $pTopic = $name;
            if( $name =~ /^(.*)\.(.*)$/ ) {
               $pWeb = $1;
               $pTopic = $2;
            }
            if( $noWebHome && ( $pTopic eq $mainTopicname ) ) {
               last;  # exclude "WebHome"
            }
            $text = "[[$pWeb.$pTopic][$pTopic]]$sep$text";
            $sep = $usesep;
            if( $dontRecurse || ! $name ) {
               last;
            } else {
               my $dummy;
               if( $visited{"$pWeb.$pTopic"} ) {
                  last;
               } else {
                  $visited{"$pWeb.$pTopic"} = 1;
               }
               if( TWiki::Store::topicExists( $pWeb, $pTopic ) ) {
                   ( $meta, $dummy ) = TWiki::Store::readTopMeta( $pWeb, $pTopic );
               } else {
                   last;
               }
               $cWeb = $pWeb;
            }
        } else {
            last;
        }
    }

    if( $text && $prefix ) {
       $text = "$prefix$text";
    }

    if( $text && $suffix ) {
       $text .= $suffix;
    }

    if( $text ) {
        $text = handleCommonTags( $text, $topic, $web );
        $text = getRenderedVersion( $text, $web );
    }

    return $text;
}

# ========================
=pod

---++ sub renderMoved (  $web, $topic, $meta  )

Not yet documented.

=cut

sub renderMoved
{
    my( $web, $topic, $meta ) = @_;
    
    my $text = "";
    
    my %moved = $meta->findOne( "TOPICMOVED" );
    
    if( %moved ) {
        my $from = $moved{"from"};
        $from =~ /(.*)\.(.*)/;
        my $fromWeb = $1;
        my $fromTopic = $2;
        my $to   = $moved{"to"};
        $to =~ /(.*)\.(.*)/;
        my $toWeb = $1;
        my $toTopic = $2;
        my $by   = $moved{"by"};
        $by = userToWikiName( $by );
        my $date = $moved{"date"};
        $date = formatTime( $date, , "gmtime" );
        
        # Only allow put back if current web and topic match stored information
        my $putBack = "";
        if( $web eq $toWeb && $topic eq $toTopic ) {
            $putBack  = " - <a title=\"Click to move topic back to previous location, with option to change references.\"";
            $putBack .= " href=\"$dispScriptUrlPath/rename/$web/$topic?newweb=$fromWeb&newtopic=$fromTopic&";
            $putBack .= "confirm=on\">put it back</a>";
        }
        $text = "<p><i><nop>$to moved from <nop>$from on $date by $by </i>$putBack</p>";
    }
    
    $text = handleCommonTags( $text, $topic, $web );
    $text = getRenderedVersion( $text, $web );

    
    return $text;
}


# ========================
=pod

---++ sub renderFormField (  $meta, $args  )

Not yet documented.

=cut

sub renderFormField
{
    my( $meta, $args ) = @_;
    my $text = "";
    if( $args ) {
        my $name = extractNameValuePair( $args, "name" );
        $text = TWiki::Search::getMetaFormField( $meta, $name ) if( $name );
    }
    return $text;
}

# =========================
=pod

---++ sub renderFormData (  $web, $topic, $meta  )

Not yet documented.

=cut

sub renderFormData
{
    my( $web, $topic, $meta ) = @_;

    my $metaText = "";
    
    my %form = $meta->findOne( "FORM" );
    if( %form ) {
        my $name = $form{"name"};
        $metaText = "\n|*[[$name]]*||\n";
        
        my @fields = $meta->find( "FIELD" );
        foreach my $field ( @fields ) {
            my $title = $field->{"title"};
            my $value = $field->{"value"};
            $value =~ s/\n/<br \/>/g;      # undo expansion
            $metaText .= "|  $title:|$value  |\n";
        }

        $metaText = getRenderedVersion( $metaText, $web );
    }

    return $metaText;
}

# Before including topic text in a hidden field in web form, encode
# characters that would break the field
=pod

---++ sub encodeSpecialChars (  $text  )

Not yet documented.

=cut

sub encodeSpecialChars
{
    my( $text ) = @_;
    
    $text =~ s/&/%_A_%/g;
    $text =~ s/\"/%_Q_%/g;
    $text =~ s/>/%_G_%/g;
    $text =~ s/</%_L_%/g;
    # PTh, JoachimDurchholz 22 Nov 2001: Fix for Codev.OperaBrowserDoublesEndOfLines
    $text =~ s/(\r*\n|\r)/%_N_%/g;

    return $text;
}

=pod

---++ sub decodeSpecialChars (  $text  )

Not yet documented.

=cut

sub decodeSpecialChars
{
    my( $text ) = @_;
    
    $text =~ s/%_N_%/\r\n/g;
    $text =~ s/%_L_%/</g;
    $text =~ s/%_G_%/>/g;
    $text =~ s/%_Q_%/\"/g;
    $text =~ s/%_A_%/&/g;

    return $text;
}


# =========================
# Render bulleted and numbered lists, including nesting.
# Called from several places.  Accumulates @listTypes and @listElements
# to track nested lists.
=pod

---++ sub emitList (  $theType, $theElement, $theDepth, $theOlType  )

Not yet documented.

=cut

sub emitList {
    my( $theType, $theElement, $theDepth, $theOlType ) = @_;

    my $result = "";
    $isList = 1;

    # Ordered list type
    $theOlType = "" unless( $theOlType );
    $theOlType =~ s/^(.).*/$1/;
    $theOlType = "" if( $theOlType eq "1" );

    if( @listTypes < $theDepth ) {
        my $firstTime = 1;
        while( @listTypes < $theDepth ) {
            push( @listTypes, $theType );
            push( @listElements, $theElement );
            $result .= "<$theElement>\n" unless( $firstTime );
            if( $theOlType ) {
                $result .= "<$theType type=\"$theOlType\">\n";
            } else {
                $result .= "<$theType>\n";
            }
            $firstTime = 0;
        }

    } elsif( @listTypes > $theDepth ) {
        while( @listTypes > $theDepth ) {
            local($_) = pop @listElements;
            $result .= "</$_>\n";
            local($_) = pop @listTypes;
            $result .= "</$_>\n";
        }
        $result .= "</$listElements[$#listElements]>\n" if( @listElements );

    } elsif( @listElements ) {
        $result = "</$listElements[$#listElements]>\n";
    }

    if( ( @listTypes ) && ( $listTypes[$#listTypes] ne $theType ) ) {
        $result .= "</$listTypes[$#listTypes]>\n<$theType>\n";
        $listTypes[$#listTypes] = $theType;
        $listElements[$#listElements] = $theElement;
    }

    return $result;
}

# ========================
=pod

---++ sub emitTR (  $thePre, $theRow, $insideTABLE  )

Not yet documented.

=cut

sub emitTR {
    my ( $thePre, $theRow, $insideTABLE ) = @_;

    my $text = "";
    my $attr = "";
    my $l1 = 0;
    my $l2 = 0;
    if( $insideTABLE ) {
        $text = "$thePre<tr>";
    } else {
        $text = "$thePre<table border=\"1\" cellspacing=\"0\" cellpadding=\"1\"> <tr>";
    }
    $theRow =~ s/\t/   /g;  # change tabs to space
    $theRow =~ s/\s*$//;    # remove trailing spaces
    $theRow =~ s/(\|\|+)/$TranslationToken . length($1) . "\|"/ge;  # calc COLSPAN

    foreach( split( /\|/, $theRow ) ) {
        $attr = "";
        #AS 25-5-01 Fix to avoid matching also single columns
        if ( s/$TranslationToken([0-9]+)//o ) { 
            $attr = " colspan=\"$1\"" ;
        }
        s/^\s+$/ &nbsp; /;
        /^(\s*).*?(\s*)$/;
        $l1 = length( $1 || "" );
        $l2 = length( $2 || "" );
        if( $l1 >= 2 ) {
            if( $l2 <= 1 ) {
                $attr .= ' align="right"';
            } else {
                $attr .= ' align="center"';
            }
        }
        if( /^\s*(\*.*\*)\s*$/ ) {
            $text .= "<th$attr bgcolor=\"#99CCCC\"> $1 </th>";
        } else {
            $text .= "<td$attr> $_ </td>";
        }
    }
    $text .= "</tr>";
    return $text;
}

# =========================
=pod

---++ sub fixedFontText (  $theText, $theDoBold  )

Not yet documented.

=cut

sub fixedFontText
{
    my( $theText, $theDoBold ) = @_;
    # preserve white space, so replace it by "&nbsp; " patterns
    $theText =~ s/\t/   /g;
    $theText =~ s|((?:[\s]{2})+)([^\s])|'&nbsp; ' x (length($1) / 2) . "$2"|eg;
    if( $theDoBold ) {
        return "<code><b>$theText</b></code>";
    } else {
        return "<code>$theText</code>";
    }
}

# =========================
# Build an HTML &lt;Hn> element with suitable anchor for linking from %<nop>TOC%
=pod

---++ sub makeAnchorHeading (  $theText, $theLevel  )

Not yet documented.

=cut

sub makeAnchorHeading
{
    my( $theText, $theLevel ) = @_;

    # - Need to build '<nop><h1><a name="atext"> text </a></h1>'
    #   type markup.
    # - Initial '<nop>' is needed to prevent subsequent matches.
    # - Need to make sure that <a> tags are not nested, i.e. in
    #   case heading has a WikiName or ABBREV that gets linked
    # - filter out $regex{headerPatternNoTOC} ( '!!' and '%NOTOC%' )

    my $text = $theText;
    my $anchorName =       makeAnchorName( $text, 0 );
    my $compatAnchorName = makeAnchorName( $text, 1 );
    $text =~ s/$regex{headerPatternNoTOC}//o; # filter '!!', '%NOTOC%'
    my $hasAnchor = 0;  # text contains potential anchor
    $hasAnchor = 1 if( $text =~ m/<a /i );
    $hasAnchor = 1 if( $text =~ m/\[\[/ );

    $hasAnchor = 1 if( $text =~ m/(^|[\s\(])($regex{abbrevRegex})/ );
    $hasAnchor = 1 if( $text =~ m/(^|[\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})/ );
    $hasAnchor = 1 if( $text =~ m/(^|[\s\(])($regex{wikiWordRegex})/ );

    # FIXME: '<h1><a name="atext"></a></h1> WikiName' has an
    #        empty <a> tag, which is not HTML conform
    my $prefix = "<nop><h$theLevel><a name=\"$compatAnchorName\"> </a> " .
		 "<a name=\"$anchorName\"> ";
    if( $hasAnchor ) {
        $text = "$prefix </a> $text <\/h$theLevel>";
    } else {
        $text = "$prefix $text <\/a><\/h$theLevel>";
    }

    return $text;
}

# =========================
# Build a valid HTML anchor name
=pod

---++ sub makeAnchorName (  $anchorName, $compatibilityMode  )

Not yet documented.

=cut

sub makeAnchorName
{
    my( $anchorName, $compatibilityMode ) = @_;

    if ( ! $compatibilityMode && $anchorName =~ /^$regex{anchorRegex}$/ ) {
	# accept, already valid -- just remove leading #
	return substr($anchorName, 1);
    }

    if ( $compatibilityMode ) {
	# remove leading/trailing underscores first, allowing them to be
	# reintroduced
	$anchorName =~ s/^[\s\#\_]*//;
        $anchorName =~ s/[\s\_]*$//;
    }
    $anchorName =~ s/<\w[^>]*>//gi;         # remove HTML tags
    $anchorName =~ s/\&\#?[a-zA-Z0-9]*;//g; # remove HTML entities
    $anchorName =~ s/^(.+?)\s*$regex{headerPatternNoTOC}.*/$1/o; # filter TOC excludes if not at beginning
    $anchorName =~ s/$regex{headerPatternNoTOC}//o; # filter '!!', '%NOTOC%'
    # FIXME: More efficient to match with '+' on next line:
    $anchorName =~ s/$regex{singleMixedNonAlphaNumRegex}/_/g;      # only allowed chars
    $anchorName =~ s/__+/_/g;               # remove excessive '_'
    if ( !$compatibilityMode ) {
        $anchorName =~ s/^[\s\#\_]*//;      # no leading space nor '#', '_'
    }
    $anchorName =~ s/^(.{32})(.*)$/$1/;     # limit to 32 chars - FIXME: Use Unicode chars before truncate
    if ( !$compatibilityMode ) {
        $anchorName =~ s/[\s\_]*$//;        # no trailing space, nor '_'
    }

    # No need to encode 8-bit characters in anchor due to UTF-8 URL support

    return $anchorName;
}

# =========================
=pod

---++ sub internalCrosswebLink (  $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink  )

Not yet documented.

=cut

sub internalCrosswebLink
{
    my( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink ) = @_;
    if ( $theTopic eq $mainTopicname && $theWeb ne $webName ) {
        return &internalLink( $thePreamble, $theWeb, $theTopic, $theWeb, $theAnchor, $doLink );
    } else {
        return &internalLink( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink );
    }
}

# =========================
=pod

---++ sub linkToolTipInfo ( $theWeb, $theTopic )

Returns =title="..."= tooltip info in case LINKTOOLTIPINFO perferences variable is set. 
Warning: Slower performance if enabled.

=cut

sub linkToolTipInfo
{
    my( $theWeb, $theTopic ) = @_;
    return "" unless( $linkToolTipInfo );

    # FIXME: This is slow, it can be improved by caching topic rev info and summary
    my( $date, $user, $rev ) = TWiki::Store::getRevisionInfo( $theWeb, $theTopic );
    my $text = $linkToolTipInfo;
    $text =~ s/\$web/<nop>$theWeb/g;
    $text =~ s/\$topic/<nop>$theTopic/g;
    $text =~ s/\$rev/1.$rev/g;
    $text =~ s/\$date/&formatTime( $date )/ge;
    $text =~ s/\$username/<nop>$user/g;                                     # "jsmith"
    $text =~ s/\$wikiname/"<nop>" . &TWiki::userToWikiName( $user, 1 )/ge;  # "JohnSmith"
    $text =~ s/\$wikiusername/"<nop>" . &TWiki::userToWikiName( $user )/ge; # "Main.JohnSmith"
    if( $text =~ /\$summary/ ) {
        my $summary = &TWiki::Store::readFileHead( "$TWiki::dataDir\/$theWeb\/$theTopic.txt", 16 );
        $summary = &TWiki::makeTopicSummary( $summary, $theTopic, $theWeb );
        $summary =~ s/[\"\']/<nop>/g;       # remove quotes (not allowed in title attribute)
        $text =~ s/\$summary/$summary/g;
    }
    return " title=\"$text\"";
}

# =========================
=pod

---++ sub internalLink (  $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink  )

Not yet documented.

=cut

sub internalLink {
    my( $thePreamble, $theWeb, $theTopic, $theLinkText, $theAnchor, $doLink ) = @_;
    # $thePreamble is text used before the TWiki link syntax
    # $doLink is boolean: false means suppress link for non-existing pages

    # Get rid of leading/trailing spaces in topic name
    $theTopic =~ s/^\s*//;
    $theTopic =~ s/\s*$//;

    # Turn spaced-out names into WikiWords - upper case first letter of
    # whole link, and first of each word. TODO: Try to turn this off,
    # avoiding spaces being stripped elsewhere - e.g. $doPreserveSpacedOutWords 
    $theTopic =~ s/^(.)/\U$1/;
    $theTopic =~ s/\s($regex{singleMixedAlphaNumRegex})/\U$1/go;	

    # Add <nop> before WikiWord inside link text to prevent double links
    $theLinkText =~ s/([\s\(])($regex{singleUpperAlphaRegex})/$1<nop>$2/go;

    my $exist = &TWiki::Store::topicExists( $theWeb, $theTopic );
    # I18N - Only apply plural processing if site language is English, or
    # if a built-in English-language web (Main, TWiki or Plugins).  Plurals
    # apply to names ending in 's', where topic doesn't exist with plural
    # name.
    if(  ( $doPluralToSingular ) and ( $siteLang eq 'en' 
					or $theWeb eq $mainWebname
					or $theWeb eq $twikiWebname
					or $theWeb eq 'Plugins' 
				     ) 
	    and ( $theTopic =~ /s$/ ) and not ( $exist ) ) {
        # Topic name is plural in form and doesn't exist as written
        my $tmp = $theTopic;
        $tmp =~ s/ies$/y/;       # plurals like policy / policies
        $tmp =~ s/sses$/ss/;     # plurals like address / addresses
        $tmp =~ s/([Xx])es$/$1/; # plurals like box / boxes
        $tmp =~ s/([A-Za-rt-z])s$/$1/; # others, excluding ending ss like address(es)
        if( &TWiki::Store::topicExists( $theWeb, $tmp ) ) {
            $theTopic = $tmp;
            $exist = 1;
        }
    }

    my $text = $thePreamble;
    if( $exist) {
        if( $theAnchor ) {
            my $anchor = makeAnchorName( $theAnchor );
            $text .= "<a href=\"$dispScriptUrlPath$dispViewPath"
		  .  "$scriptSuffix/$theWeb/$theTopic\#$anchor\""
                  .  &linkToolTipInfo( $theWeb, $theTopic )
                  .  ">$theLinkText<\/a>";
            return $text;
        } else {
            $text .= "<a href=\"$dispScriptUrlPath$dispViewPath"
		  .  "$scriptSuffix/$theWeb/$theTopic\""
                  .  &linkToolTipInfo( $theWeb, $theTopic )
                  .  ">$theLinkText<\/a>";
            return $text;
        }

    } elsif( $doLink ) {
        $text .= "<span style='background : $newTopicBgColor;'>"
              .  "<font color=\"$newTopicFontColor\">$theLinkText</font></span>"
              .  "<a href=\"$dispScriptUrlPath/edit$scriptSuffix/$theWeb/$theTopic?topicparent=$webName.$topicName\">?</a>";
        return $text;

    } else {
        $text .= $theLinkText;
        return $text;
    }
}

# =========================
# Handle most internal and external links
=pod

---++ sub specificLink (  $thePreamble, $theWeb, $theTopic, $theText, $theLink  )

Not yet documented.

=cut

sub specificLink
{
    my( $thePreamble, $theWeb, $theTopic, $theText, $theLink ) = @_;

    # format: $thePreamble[[$theText]]
    # format: $thePreamble[[$theLink][$theText]]
    #
    # Current page's $theWeb and $theTopic are also used

    # Strip leading/trailing spaces
    $theLink =~ s/^\s*//;
    $theLink =~ s/\s*$//;

    if( $theLink =~ /^$regex{linkProtocolPattern}\:/ ) {

        # External link: add <nop> before WikiWord and ABBREV 
	# inside link text, to prevent double links
	$theText =~ s/([\s\(])($regex{singleUpperAlphaRegex})/$1<nop>$2/go;
        return "$thePreamble<a href=\"$theLink\" target=\"_top\">$theText</a>";

    } else {

	# Internal link: get any 'Web.' prefix, or use current web
	$theLink =~ s/^($regex{webNameRegex}|$regex{defaultWebNameRegex})\.//;
	my $web = $1 || $theWeb;
	(my $baz = "foo") =~ s/foo//;       # reset $1, defensive coding

	# Extract '#anchor'
	# FIXME and NOTE: Had '-' as valid anchor character, removed
	# $theLink =~ s/(\#[a-zA-Z_0-9\-]*$)//;
	$theLink =~ s/($regex{anchorRegex}$)//;
	my $anchor = $1 || "";

	# Get the topic name
	my $topic = $theLink || $theTopic;  # remaining is topic
	$topic =~ s/\&[a-z]+\;//gi;        # filter out &any; entities
	$topic =~ s/\&\#[0-9]+\;//g;       # filter out &#123; entities
	$topic =~ s/[\\\/\#\&\(\)\{\}\[\]\<\>\!\=\:\,\.]//g;
	$topic =~ s/$securityFilter//go;    # filter out suspicious chars
	if( ! $topic ) {
	    return "$thePreamble$theText"; # no link if no topic
	}

	return internalLink( $thePreamble, $web, $topic, $theText, $anchor, 1 );
    }

}

# =========================
=pod

---++ sub externalLink (  $pre, $url  )

Not yet documented.

=cut

sub externalLink
{
    my( $pre, $url ) = @_;
    if( $url =~ /\.(gif|jpg|jpeg|png)$/i ) {
        my $filename = $url;
        $filename =~ s@.*/([^/]*)@$1@go;
        return "$pre<img src=\"$url\" alt=\"$filename\" />";
    }

    return "$pre<a href=\"$url\" target=\"_top\">$url</a>";
}

# =========================
=pod

---++ sub mailtoLink (  $theAccount, $theSubDomain, $theTopDomain  )

Not yet documented.

=cut

sub mailtoLink
{
    my( $theAccount, $theSubDomain, $theTopDomain ) = @_;

    my $addr = "$theAccount\@$theSubDomain$TWiki::noSpamPadding\.$theTopDomain";
    return "<a href=\"mailto\:$addr\">$addr</a>";
}

# =========================
=pod

---++ sub mailtoLinkFull (  $theAccount, $theSubDomain, $theTopDomain, $theLinkText  )

Not yet documented.

=cut

sub mailtoLinkFull
{
    my( $theAccount, $theSubDomain, $theTopDomain, $theLinkText ) = @_;

    my $addr = "$theAccount\@$theSubDomain$TWiki::noSpamPadding\.$theTopDomain";
    return "<a href=\"mailto\:$addr\">$theLinkText</a>";
}

# =========================
=pod

---++ sub mailtoLinkSimple (  $theMailtoString, $theLinkText  )

Not yet documented.

=cut

sub mailtoLinkSimple
{
    # Does not do any anti-spam padding, because address will not include '@'
    my( $theMailtoString, $theLinkText ) = @_;	

    # Defensive coding
    if ($theMailtoString =~ s/@//g ) {
    	writeWarning("mailtoLinkSimple called with an '\@' in string - internal TWiki error");
    }
    return "<a href=\"mailto\:$theMailtoString\">$theLinkText</a>";
}

=pod

---++ sub getFormField ( $web, $topic, $args )

Handles %FORMFIELD{}% tags in topics, used in handleInternalTags

TODO: use Prefs subsystem to read form info, instead of rereadeing
topic every time a form field value is requested.

=cut

sub getFormField
{
    my( $web, $topic, $args ) = @_;
    
    my ( $altText, $found, $format );
    my ( $formField, $formTopic, $default );
    
    $formField = extractNameValuePair( $args );
    $formTopic = extractNameValuePair( $args, "topic" );
    $altText   = extractNameValuePair( $args, "alttext" );
    $default   = extractNameValuePair( $args, "default" ) || undef;
    $format    = extractNameValuePair( $args, "format" ) || '$value';

    my $meta;
    if ($formTopic) {
       my $formTopicWeb;
       if ($topic =~ /^([^.]+)\.([^.]+)/o) {
	   ( $formTopicWeb, $topic ) = ( $1, $2 );
       } else {
	   $formTopicWeb = extractNameValuePair( $args, "web" );
       }
       $formTopicWeb = $web unless defined $formTopicWeb;
       ( $meta, my $dummyText ) = &TWiki::Store::readTopic( $formTopicWeb, $formTopic );
    } else {
       ( $meta, my $dummyText ) = &TWiki::Store::readTopic( $web, $topic );
    }

    my $text = "";
    my @fields = $meta->find( "FIELD" );
    foreach my $field ( @fields ) {
	my $title = $field->{"title"};
	my $name = $field->{"name"};
	if( $title eq $formField || $name eq $formField ) {
	    my $value = $field->{"value"};
	    if (length $value) {
		$found = 1;
		$text = $format;
		$text =~ s/\$value/$value/g;
	    } elsif (defined $altText) {
		$found = 1;
		$text = $altText;
	    }
	    last; #one hit suffices
	}
    }          
    if (!$found) {
	if (defined $default) {
	    $text = $format;
	    $text =~ s/\$value/$default/g;
	} else {
	    $text = $altText;
	}
    }
    $text = getRenderedVersion( $text, $web );
    return $text;
} 


# =========================
=pod

---++ sub getRenderedVersion (  $text, $theWeb, $meta  )

Not yet documented.

=cut

sub getRenderedVersion {
    my( $text, $theWeb, $meta ) = @_;
    my( $head, $result, $extraLines, $insidePRE, $insideTABLE, $insideNoAutoLink );

    return "" unless $text;  # nothing to do

    # FIXME: Get $theTopic from parameter to handle [[#anchor]] correctly
    # (fails in %INCLUDE%, %SEARCH%)
    my $theTopic = $topicName;

    # PTh 22 Jul 2000: added $theWeb for correct handling of %INCLUDE%, %SEARCH%
    if( !$theWeb ) {
        $theWeb = $webName;
    }

    $head = "";
    $result = "";
    $insidePRE = 0;
    $insideTABLE = 0;
    $insideNoAutoLink = 0;      # PTh 02 Feb 2001: Added Codev.DisableWikiWordLinks
    $isList = 0;
    @listTypes = ();
    @listElements = ();

    # Initial cleanup
    $text =~ s/\r//g;
    $text =~ s/(\n?)$/\n<nop>\n/s; # clutch to enforce correct rendering at end of doc
    # Convert any occurrences of token (very unlikely - details in
    # Codev.NationalCharTokenClash)
    $text =~ s/$TranslationToken/!/go;	

    my @verbatim = ();
    $text = takeOutVerbatim( $text, \@verbatim );
    $text =~ s/\\\n//g;  # Join lines ending in "\"

    # do not render HTML head, style sheets and scripts
    if( $text =~ m/<body[\s\>]/i ) {
        my $bodyTag = "";
        my $bodyText = "";
        ( $head, $bodyTag, $bodyText ) = split( /(<body)/i, $text, 3 );
        $text = $bodyTag . $bodyText;
    }
    
    # Wiki Plugin Hook
    &TWiki::Plugins::startRenderingHandler( $text, $theWeb, $meta );

    # $isList is tested and set by this loop and 'emitList' function
    $isList = 0;		# True when within a list

    foreach( split( /\n/, $text ) ) {

        # change state:
        m|<pre>|i  && ( $insidePRE = 1 );
        m|</pre>|i && ( $insidePRE = 0 );
        m|<noautolink>|i   && ( $insideNoAutoLink = 1 );
        m|</noautolink>|i  && ( $insideNoAutoLink = 0 );

        if( $insidePRE ) {
            # inside <PRE>

            # close list tags if any
            if( @listTypes ) {
                $result .= &emitList( "", "", 0 );
                $isList = 0;
            }

# Wiki Plugin Hook
            &TWiki::Plugins::insidePREHandler( $_ );

            s/(.*)/$1\n/;
            s/\t/   /g;		# Three spaces
            $result .= $_;

        } else {
          # normal state, do Wiki rendering

# Wiki Plugin Hook
          &TWiki::Plugins::outsidePREHandler( $_ );
          $extraLines = undef;   # Plugins might introduce extra lines
          do {                   # Loop over extra lines added by plugins
            $_ = $extraLines if( defined $extraLines );
            s/^(.*?)\n(.*)$/$1/s;
            $extraLines = $2;    # Save extra lines, need to parse each separately

# Blockquoted email (indented with '> ')
            s/^>(.*?)$/> <cite> $1 <\/cite><br \/>/g;

# Embedded HTML
            s/\<(\!\-\-)/$TranslationToken$1/g;  # Allow standalone "<!--"
            s/(\-\-)\>/$1$TranslationToken/g;    # Allow standalone "-->"
	    # FIXME: next 2 lines are redundant since s///g's below do same
	    # thing
            s/(\<\<+)/"&lt\;" x length($1)/ge;
            s/(\>\>+)/"&gt\;" x length($1)/ge;
            s/\<nop\>/nopTOKEN/g;  # defuse <nop> inside HTML tags
            s/\<(\S.*?)\>/$TranslationToken$1$TranslationToken/g;
            s/</&lt\;/g;
            s/>/&gt\;/g;
            s/$TranslationToken(\S.*?)$TranslationToken/\<$1\>/go;
            s/nopTOKEN/\<nop\>/g;
            s/(\-\-)$TranslationToken/$1\>/go;
            s/$TranslationToken(\!\-\-)/\<$1/go;

# Handle embedded URLs
            s!(^|[\-\*\s\(])($regex{linkProtocolPattern}\:([^\s\<\>\"]+[^\s\.\,\!\?\;\:\)\<]))!&externalLink($1,$2)!geo;

# Entities
            s/&(\w+?)\;/$TranslationToken$1\;/g;      # "&abc;"
            s/&(\#[0-9]+)\;/$TranslationToken$1\;/g;  # "&#123;"
            s/&/&amp;/g;                              # escape standalone "&"
            s/$TranslationToken/&/go;

# Headings
            # '<h6>...</h6>' HTML rule
            s/$regex{headerPatternHt}/&makeAnchorHeading($2,$1)/geoi;
            # '\t+++++++' rule
            s/$regex{headerPatternSp}/&makeAnchorHeading($2,(length($1)))/geo;
            # '----+++++++' rule
            s/$regex{headerPatternDa}/&makeAnchorHeading($2,(length($1)))/geo;

# Horizontal rule
            s/^---+/<hr \/>/;
            s!^([a-zA-Z0-9]+)----*!<table width=\"100%\"><tr><td valign=\"bottom\"><h2>$1</h2></td><td width=\"98%\" valign=\"middle\"><hr /></td></tr></table>!o;

# Table of format: | cell | cell |
            # PTh 25 Jan 2001: Forgiving syntax, allow trailing white space
            if( $_ =~ /^(\s*)\|.*\|\s*$/ ) {
                s/^(\s*)\|(.*)/&emitTR($1,$2,$insideTABLE)/e;
                $insideTABLE = 1;
            } elsif( $insideTABLE ) {
                $result .= "</table>\n";
                $insideTABLE = 0;
            }

# Lists and paragraphs
            s/^\s*$/<p \/>/o                 && ( $isList = 0 );
            m/^(\S+?)/o                      && ( $isList = 0 );
	    # Definition list
            s/^(\t+)\$\s(([^:]+|:[^\s]+)+?):\s/<dt> $2 <\/dt><dd> /o && ( $result .= &emitList( "dl", "dd", length $1 ) );
            s/^(\t+)(\S+?):\s/<dt> $2<\/dt><dd> /o && ( $result .= &emitList( "dl", "dd", length $1 ) );
	    # Unnumbered list
            s/^(\t+)\* /<li> /o              && ( $result .= &emitList( "ul", "li", length $1 ) );
	    # Numbered list
            s/^(\t+)([1AaIi]\.|\d+\.?) ?/<li> /o && ( $result .= &emitList( "ol", "li", length $1, $2 ) );
	    # Finish the list
            if( ! $isList ) {
                $result .= &emitList( "", "", 0 );
                $isList = 0;
            }

# '#WikiName' anchors
            s/^(\#)($regex{wikiWordRegex})/ '<a name="' . &makeAnchorName( $2 ) . '"><\/a>'/ge;

# enclose in white space for the regex that follow
             s/(.*)/\n$1\n/;

# Emphasizing
            # PTh 25 Sep 2000: More relaxed rules, allow leading '(' and trailing ',.;:!?)'
            s/([\s\(])==([^\s]+?|[^\s].*?[^\s])==([\s\,\.\;\:\!\?\)])/$1 . &fixedFontText( $2, 1 ) . $3/ge;
            s/([\s\(])__([^\s]+?|[^\s].*?[^\s])__([\s\,\.\;\:\!\?\)])/$1<strong><em>$2<\/em><\/strong>$3/g;
            s/([\s\(])\*([^\s]+?|[^\s].*?[^\s])\*([\s\,\.\;\:\!\?\)])/$1<strong>$2<\/strong>$3/g;
            s/([\s\(])_([^\s]+?|[^\s].*?[^\s])_([\s\,\.\;\:\!\?\)])/$1<em>$2<\/em>$3/g;
            s/([\s\(])=([^\s]+?|[^\s].*?[^\s])=([\s\,\.\;\:\!\?\)])/$1 . &fixedFontText( $2, 0 ) . $3/ge;

# Mailto
	    # Email addresses must always be 7-bit, even within I18N sites

	    # RD 27 Mar 02: Mailto improvements - FIXME: check security...
	    # Explicit [[mailto:... ]] link without an '@' - hence no 
	    # anti-spam padding needed.
            # '[[mailto:string display text]]' link (no '@' in 'string'):
            s/\[\[mailto\:([^\s\@]+)\s+(.+?)\]\]/&mailtoLinkSimple( $1, $2 )/ge;

	    # Explicit [[mailto:... ]] link including '@', with anti-spam 
	    # padding, so match name@subdom.dom.
            # '[[mailto:string display text]]' link
            s/\[\[mailto\:([a-zA-Z0-9\-\_\.\+]+)\@([a-zA-Z0-9\-\_\.]+)\.(.+?)(\s+|\]\[)(.*?)\]\]/&mailtoLinkFull( $1, $2, $3, $5 )/ge;

	    # Normal mailto:foo@example.com ('mailto:' part optional)
	    # FIXME: Should be '?' after the 'mailto:'...
            s/([\s\(])(?:mailto\:)*([a-zA-Z0-9\-\_\.\+]+)\@([a-zA-Z0-9\-\_\.]+)\.([a-zA-Z0-9\-\_]+)(?=[\s\.\,\;\:\!\?\)])/$1 . &mailtoLink( $2, $3, $4 )/ge;

# Make internal links
	    # Spaced-out Wiki words with alternative link text
            # '[[Web.odd wiki word#anchor][display text]]' link:
            s/\[\[([^\]]+)\]\[([^\]]+)\]\]/&specificLink("",$theWeb,$theTopic,$2,$1)/ge;
            # RD 25 Mar 02: Codev.EasierExternalLinking
            # '[[URL#anchor display text]]' link:
            s/\[\[([a-z]+\:\S+)\s+(.*?)\]\]/&specificLink("",$theWeb,$theTopic,$2,$1)/ge;
	    # Spaced-out Wiki words
            # '[[Web.odd wiki word#anchor]]' link:
            s/\[\[([^\]]+)\]\]/&specificLink("",$theWeb,$theTopic,$1,$1)/ge;

            # do normal WikiWord link if not disabled by <noautolink> or NOAUTOLINK preferences variable
            unless( $noAutoLink || $insideNoAutoLink ) {

                # 'Web.TopicName#anchor' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})($regex{anchorRegex})/&internalLink($1,$2,$3,"$TranslationToken$3$4$TranslationToken",$4,1)/geo;
                # 'Web.TopicName' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{wikiWordRegex})/&internalCrosswebLink($1,$2,$3,"$TranslationToken$3$TranslationToken","",1)/geo;

                # 'TopicName#anchor' link:
                s/([\s\(])($regex{wikiWordRegex})($regex{anchorRegex})/&internalLink($1,$theWeb,$2,"$TranslationToken$2$3$TranslationToken",$3,1)/geo;

                # 'TopicName' link:
		s/([\s\(])($regex{wikiWordRegex})/&internalLink($1,$theWeb,$2,$2,"",1)/geo;

		# Handle acronyms/abbreviations of three or more letters
                # 'Web.ABBREV' link:
                s/([\s\(])($regex{webNameRegex})\.($regex{abbrevRegex})/&internalLink($1,$2,$3,$3,"",0)/geo;
                # 'ABBREV' link:
		s/([\s\(])($regex{abbrevRegex})/&internalLink($1,$theWeb,$2,$2,"",0)/geo;
                # (deprecated <link> moved to DefaultPlugin)

                s/$TranslationToken(\S.*?)$TranslationToken/$1/go;
            }

            s/^\n//;
            s/\t/   /g;
            $result .= $_;

          } while( defined( $extraLines ) );  # extra lines produced by plugins
        }
    }
    if( $insideTABLE ) {
        $result .= "</table>\n";
    }
    $result .= &emitList( "", "", 0 );
    if( $insidePRE ) {
        $result .= "</pre>\n";
    }

    # Wiki Plugin Hook
    &TWiki::Plugins::endRenderingHandler( $result );

    $result = putBackVerbatim( $result, "pre", @verbatim );

    $result =~ s|\n?<nop>\n$||o; # clean up clutch
    return "$head$result";
}

=end twiki

=cut

1;
