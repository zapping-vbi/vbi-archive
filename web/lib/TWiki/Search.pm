# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Search engine of TWiki.
#
# Copyright (C) 2000-2004 Peter Thoeny, peter@thoeny.com
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
#
# 20000501 Kevin Kinnell  : Many many many changes, best view is to
#                           run a diff.
# 20000605 Kevin Kinnell  : Bug hunting.  Fixed to allow web colors
#                           spec'd as "word" instead of hex only.
#                           Found a lovely bug that screwed up the
#                           search limits because Perl (as we all know
#                           but may forget) doesn't clear the $n match
#                           params if a match fails... *^&$#!!!
# PTh 03 Nov 2000: Performance improvements

=begin twiki

---+ TWiki::Search Module

This module implements all the search functionality.

=cut

package TWiki::Search;
use strict;

use vars qw(
    $cacheRev1webTopic $cacheRev1date $cacheRev1user
);


# 'Use locale' for internationalisation of Perl sorting and searching - 
# main locale settings are done in TWiki::setupLocale
BEGIN {
    # Do a dynamic 'use locale' for this module
    if( $TWiki::useLocale ) {
        require locale;
	import locale ();
    }
    $cacheRev1webTopic = "";
}

# ===========================
# Normally writes no output, uncomment writeDebug line to get output of all RCS etc command to debug file
=pod

---++ sub _traceExec (  $cmd, $result  )

Not yet documented.

=cut

sub _traceExec
{
   my( $cmd, $result ) = @_;
   
   #TWiki::writeDebug( "Search exec: $cmd -> $result" );
}

# ===========================
=pod

---++ sub _translateSpace (  $theText  )

Not yet documented.

=cut

sub _translateSpace
{
    my( $theText ) = @_;
    $theText =~ s/\s+/$TWiki::TranslationToken/go;
    return $theText;
}

# ===========================
=pod

---++ sub _tokensFromSearchString (  $theSearchVal, $theType  )

Not yet documented.

=cut

sub _tokensFromSearchString
{
    my( $theSearchVal, $theType ) = @_;

    my @tokens = ();
    if( $theType eq "regex" ) {
        # regular expression search Example: soap;wsdl;web service;!shampoo
        @tokens = split( /;/, $theSearchVal );

    } elsif( $theType eq "literal" ) {
        # literal search
        $tokens[0] = $theSearchVal;

    } else {
        # keyword search. Example: soap +wsdl +"web service" -shampoo
        $theSearchVal =~ s/(\".*?)\"/&_translateSpace($1)/geo;  # hide spaces in "literal text"
        $theSearchVal =~ s/[\+\-]\s+//go;

        # build pattern of stop words
        my $stopWords = &TWiki::Prefs::getPreferencesValue( "SEARCHSTOPWORDS" ) || "";
        $stopWords =~ s/[\s\,]+/\|/go;
        $stopWords =~ s/[\(\)]//go;

        # read from bottom to up:
        @tokens =
            map { s/^\+//o; s/^\-/\!/o; s/^\"//o; $_ }    # remove +, change - to !, remove "
            grep { ! /^($stopWords)$/i }                  # remove stopwords
            map { s/$TWiki::TranslationToken/ /go; $_ }   # restore space
            split( /[\s]+/, $theSearchVal );              # split on spaces
    }

    return @tokens;
}

# =========================
=pod

---++ sub _searchTopicsInWeb (  $theWeb, $theTopic, $theScope, $theType, $caseSensitive, @theTokens  )

Not yet documented.

=cut

sub _searchTopicsInWeb
{
    my( $theWeb, $theTopic, $theScope, $theType, $caseSensitive, @theTokens ) = @_;

    my @topicList = ();
    return @topicList unless( @theTokens );                        # bail out if no search string

    if( $theTopic ) {                                              # limit search to topic list
        if( $theTopic =~ /^\^\([$TWiki::regex{mixedAlphaNum}\|]+\)\$$/ ) { # topic list without wildcards
            my $topics = $theTopic;                                # for speed, do not get all topics in web
            $topics =~ s/^\^\(//o;                                 # but convert topic pattern into topic list
            $topics =~ s/\)\$//o;                                  #
            @topicList = split( /\|/, $topics );                   # build list from topic pattern
        } else {                                                   # topic list with wildcards
            @topicList = _getTopicList( $theWeb );                 # get all topics in web
            if( $caseSensitive ) {
                @topicList = grep( /$theTopic/, @topicList );      # limit by topic name,
            } else {                                               # Codev.SearchTopicNameAndTopicText
                @topicList = grep( /$theTopic/i, @topicList );
            }
        }
    } else {
        @topicList = _getTopicList( $theWeb );                     # get all topics in web
    }

    my $sDir = "$TWiki::dataDir/$theWeb";
    $theScope = "text" unless( $theScope =~ /^(topic|all)$/ );     # default scope is "text"

    foreach my $token ( @theTokens ) {                             # search each token
        my $invertSearch = ( $token =~ s/^\!//o );                 # flag for AND NOT search
        my @scopeTextList = ();
        my @scopeTopicList = ();
        return @topicList unless( @topicList );                    # bail out if no topics left

        # scope can be "topic" (default), "text" or "all"
        # scope="text", e.g. Perl search on topic name:
        unless( $theScope eq "text" ) {
            my $qtoken = $token;
            $qtoken = quotemeta( $qtoken ) if( $theType ne "regex" ); # FIXME I18N
            if( $caseSensitive ) {                                 # fix for Codev.SearchWithNoPipe
                @scopeTopicList = grep( /$qtoken/, @topicList );
            } else {
                @scopeTopicList = grep( /$qtoken/i, @topicList );
            }
        }

        # scope="text", e.g. grep search on topic text:
        unless( $theScope eq "topic" ) {
            # Construct command line with 'grep'.  I18N: 'grep' must use locales if needed,
            # for case-insensitive searching.  See TWiki::setupLocale.
            my $cmd = "";
            if( $theType eq "regex" ) {
                $cmd .= $TWiki::egrepCmd;
            } else {
                $cmd .= $TWiki::fgrepCmd;
            }
            $cmd .= " -i" unless( $caseSensitive );
            $cmd .= " -l -- $TWiki::cmdQuote%TOKEN%$TWiki::cmdQuote %FILES%";

            my $result = "";
            if( $sDir ) {
                chdir( "$sDir" );
                _traceExec( "chdir to $sDir", "" );
                $sDir = "";  # chdir only once
            }

            # process topics in sets,  fix for Codev.ArgumentListIsTooLongForSearch
            my $maxTopicsInSet = 512;                              # max number of topics for a grep call
            my @take = @topicList;
            my @set = splice( @take, 0, $maxTopicsInSet );
            while( @set ) {
                @set = map { "$_.txt" } @set;                      # add ".txt" extension to topic names
                my $acmd = $cmd;
                $acmd =~ s/%TOKEN%/$token/o;
                $acmd =~ s/%FILES%/@set/o;
                $acmd =~ /(.*)/;
                $acmd = "$1";                                      # untaint variable (FIXME: Needs a better check!)
                $result = `$acmd`;
                _traceExec( $acmd, $result );
                @set = split( /\n/, $result );
                @set = map { /(.*)\.txt$/; $_ = $1; } @set;        # cut ".txt" extension
                my %seen = ();
                foreach my $topic ( @set ) {
                    $seen{$topic}++;                               # make topics unique
                }
                push( @scopeTextList, sort keys %seen );           # add hits to found list
                @set = splice( @take, 0, $maxTopicsInSet );
            }
        }

        if( @scopeTextList && @scopeTopicList ) {
            push( @scopeTextList, @scopeTopicList );               # join "topic" and "text" lists
            my %seen = ();
            @scopeTextList = sort grep { ! $seen{$_} ++ } @scopeTextList;  # make topics unique
        } elsif( @scopeTopicList ) {
            @scopeTextList =  @scopeTopicList;
        }

        if( $invertSearch ) {                                      # do AND NOT search
            my %seen = ();
            foreach my $topic ( @scopeTextList ) {
                $seen{$topic} = 1;
            }
            @scopeTextList = ();
            foreach my $topic ( @topicList ) {
                push( @scopeTextList, $topic ) unless( $seen{$topic} );
            }
        }
        @topicList = @scopeTextList;                               # reduced topic list for next token
    }
    return @topicList;
}

# =========================
=pod

---++ sub _getTopicList (  $web  )

Not yet documented.

=cut

sub _getTopicList
{
    my( $web ) = @_ ;
    opendir DIR, "$TWiki::dataDir/$web" ;
    my @topicList = sort map { s/\.txt$//o; $_ } grep { /\.txt$/ } readdir( DIR );
    closedir( DIR );
    return @topicList;
}

# =========================
=pod

---++ sub _makeTopicPattern (  $theTopic  )

Not yet documented.

=cut

sub _makeTopicPattern
{
    my( $theTopic ) = @_ ;
    return "" unless( $theTopic );
    # "Web*, FooBar" ==> ( "Web*", "FooBar" ) ==> ( "Web.*", "FooBar" )
    my @arr = map { s/[^\*\_$TWiki::regex{mixedAlphaNum}]//go; s/\*/\.\*/go; $_ }
              split( /,\s*/, $theTopic );
    return "" unless( @arr );
    # ( "Web.*", "FooBar" ) ==> "^(Web.*|FooBar)$"
    return '^(' . join( "|", @arr ) . ')$';
}

# =========================
=pod

---++ sub searchWeb ()

Not yet documented.

=cut

sub searchWeb
{
    my %params = @_;
    my $doInline =      $params{"inline"} || 0;
    my $emptySearch =   "something.Very/unLikelyTo+search-for;-)";
    my $theSearchVal =  $params{"search"} || $emptySearch;
    my $theWebName =    $params{"web"} || "";
    my $theTopic =      $params{"topic"} || "";
    my $theExclude =    $params{"excludetopic"} || "";
    my $theScope =      $params{"scope"} || "";
    my $theOrder =      $params{"order"} || "";
    my $theType =       $params{"type"} || "";
    my $theRegex =      $params{"regex"} || "";
    my $theLimit =      $params{"limit"} || "";
    my $revSort =       $params{"reverse"} || "";
    my $caseSensitive = $params{"casesensitive"} || "";
    my $noSummary =     $params{"nosummary"} || "";
    my $noSearch =      $params{"nosearch"} || "";
    my $noHeader =      $params{"noheader"} || "";
    my $noTotal =       $params{"nototal"} || "";
    my $doBookView =    $params{"bookview"} || "";
    my $doRenameView =  $params{"renameview"} || "";
    my $doShowLock =    $params{"showlock"} || "";
    my $doExpandVars =  $params{"expandvariables"} || "";
    my $noEmpty =       $params{"noempty"} || "";
    my $theTemplate =   $params{"template"} || "";
    my $theHeader =     $params{"header"} || "";
    my $theFormat =     $params{"format"} || "";
    my $doMultiple =    $params{"multiple"} || "";
    my $theSeparator =  $params{"separator"} || "";

    ##TWiki::writeDebug "Search locale is $TWiki::siteLocale";

    ## 0501 kk : vvv new option to limit results
    # process the result limit here, this is the 'global' limit for
    # all webs in a multi-web search

    ## #############
    ## 0605 kk : vvv This code broke due to changes in the wiki.pm
    ##               file; it used to rely on the value of $1 being
    ##               a null string if there was no match.  What a pity
    ##               Perl doesn't do The Right Thing, but whatever--it's
    ##               fixed now.
    if ($theLimit =~ /(^\d+$)/o) { # only digits, all else is the same as
        $theLimit = $1;            # an empty string.  "+10" won't work.
    } else {
        $theLimit = 0;             # change "all" to 0, then to big number
    }
    if (! $theLimit ) {            # PTh 03 Nov 2000:
        $theLimit = 32000;         # Big number, needed for performance improvements
    }

    $theType = "regex" if( $theRegex );

    if( $theSeparator ) {
        $theSeparator =~ s/\$n/\n/gos;
        $theSeparator =~ s/\$n\(\)/\n/gos;  # expand "$n()" to new line
    }

    my $searchResult = ""; 
    my $topic = $TWiki::mainTopicname;

    my @webList = ();

    # A value of 'all' or 'on' by itself gets all webs,
    # otherwise ignored (unless there is a web called "All".)
    my $searchAllFlag = ( $theWebName =~ /(^|[\,\s])(all|on)([\,\s]|$)/i );

    # Search what webs?  "" current web, list gets the list, all gets
    # all (unless marked in WebPrefs as NOSEARCHALL)

    if( $theWebName ) {
        foreach my $web ( split( /[\,\s]+/, $theWebName ) ) {
            # the web processing loop filters for valid web names, so don't do it here.

            if( $web =~ /^(all|on)$/i  ) {
                # get list of all webs by scanning $dataDir
                opendir DIR, $TWiki::dataDir;
                my @tmpList = readdir(DIR);
                closedir(DIR);
                @tmpList = sort
                   grep { s#^.+/([^/]+)$#$1# }
                   grep { -d }
                   map  { "$TWiki::dataDir/$_" }
                   grep { ! /^[._]/ } @tmpList;

                   # what that does (looking from the bottom up) is take the file
                   # list, filter out the dot directories and dot files, turn the
                   # list into full paths instead of just file names, filter out
                   # any non-directories, strip the path back off, and sort
                   # whatever was left after all that (which should be merely a
                   # list of directory's names.)

                foreach my $aweb ( @tmpList ) {
                    push( @webList, $aweb ) unless( grep { /^$aweb$/ } @webList );
                }

            } else {
                push( @webList, $web ) unless( grep { /^$web$/ } @webList );
            }
        }

    } else {
        #default to current web
        push @webList, $TWiki::webName;
    }

    $theTopic   = _makeTopicPattern( $theTopic );    # E.g. "Bug*, *Patch" ==> "^(Bug.*|.*Patch)$"
    $theExclude = _makeTopicPattern( $theExclude );  # E.g. "Web*, FooBar" ==> "^(Web.*|FooBar)$"

    my $tempVal = "";
    my $tmpl = "";
    my $topicCount = 0; # JohnTalintyre
    my $originalSearch = $theSearchVal;
    my $renameTopic;
    my $renameWeb = "";
    my $spacedTopic;
    $theTemplate = "searchformat" if( $theFormat );

    if( $theTemplate ) {
        $tmpl = &TWiki::Store::readTemplate( "$theTemplate" );
        # FIXME replace following with this @@@
    } elsif( $doBookView ) {
        $tmpl = &TWiki::Store::readTemplate( "searchbookview" );
    } elsif ($doRenameView ) {
	# Rename view, showing where topics refer to topic being renamed.
        $tmpl = &TWiki::Store::readTemplate( "searchrenameview" ); # JohnTalintyre

        # Create full search string from topic name that is passed in
        $renameTopic = $theSearchVal;
        if( $renameTopic =~ /(.*)\\\.(.*)/o ) {
            $renameWeb = $1;
            $renameTopic = $2;
        }
        $spacedTopic = spacedTopic( $renameTopic );
        $spacedTopic = $renameWeb . '\.' . $spacedTopic if( $renameWeb );

	# I18N: match non-alpha before and after topic name in renameview searches
	# This regex must work under grep, i.e. if using Perl 5.6 or higher
	# the POSIX character classes will be used in grep as well.
        my $alphaNum = $TWiki::regex{mixedAlphaNum};
        $theSearchVal = "(^|[^${alphaNum}_])$theSearchVal" . 
			"([^${alphaNum}_]" . '|$)|' .
                        '(\[\[' . $spacedTopic . '\]\])';
    } else {
        $tmpl = &TWiki::Store::readTemplate( "search" );
    }

    $tmpl =~ s/\%META{.*?}\%//go;  # remove %META{"parent"}%

    my( $tmplHead, $tmplSearch,
        $tmplTable, $tmplNumber, $tmplTail ) = split( /%SPLIT%/, $tmpl );
    $tmplHead   = &TWiki::handleCommonTags( $tmplHead, $topic );
    $tmplSearch = &TWiki::handleCommonTags( $tmplSearch, $topic );
    $tmplNumber = &TWiki::handleCommonTags( $tmplNumber, $topic );
    $tmplTail   = &TWiki::handleCommonTags( $tmplTail, $topic );

    if( ! $tmplTail ) {
        print "<html><body>";
        print "<h1>TWiki Installation Error</h1>";
        # Might not be search.tmpl FIXME
        print "Incorrect format of search.tmpl (missing %SPLIT% parts)";
        print "</body></html>";
        return;
    }

    if( ! $doInline ) {
        # print first part of full HTML page
        $tmplHead = &TWiki::getRenderedVersion( $tmplHead );
        $tmplHead =~ s|</*nop/*>||goi;   # remove <nop> tags (PTh 06 Nov 2000)
        print $tmplHead;
    }

    if( ! $noSearch ) {
        # print "Search:" part
        my $searchStr = $theSearchVal;
        $searchStr = "" if( $theSearchVal eq $emptySearch );
        $searchStr =~ s/&/&amp;/go;
        $searchStr =~ s/</&lt;/go;
        $searchStr =~ s/>/&gt;/go;
        $searchStr =~ s/^\.\*$/Index/go;
        $tmplSearch =~ s/%SEARCHSTRING%/$searchStr/go;
        if( $doInline ) {
            $searchResult .= $tmplSearch;
        } else {
            $tmplSearch = &TWiki::getRenderedVersion( $tmplSearch );
            $tmplSearch =~ s|</*nop/*>||goi;   # remove <nop> tag
            print $tmplSearch;
        }
    }

    my @tokens = &_tokensFromSearchString( $theSearchVal, $theType );

    # write log entry
    # FIXME: Move log entry further down to log actual webs searched
    if( ( $TWiki::doLogTopicSearch ) && ( ! $doInline ) ) {
        # 0501 kk : vvv Moved from search
        # PTh 17 May 2000: reverted to old behaviour,
        #     e.g. do not log inline search
        # PTh 03 Nov 2000: Moved out of the 'foreach $thisWebName' loop
        $tempVal = join( ' ', @webList );
        &TWiki::Store::writeLog( "search", $tempVal, $theSearchVal );
    }

    # loop through webs
    foreach my $thisWebName ( @webList ) {

        # PTh 03 Nov 2000: Add security check
        $thisWebName =~ s/$TWiki::securityFilter//go;
        $thisWebName =~ /(.*)/;
        $thisWebName = $1;  # untaint variable

        next unless &TWiki::Store::webExists( $thisWebName );  # can't process what ain't thar

        my $thisWebBGColor     = &TWiki::Prefs::getPreferencesValue( "WEBBGCOLOR", $thisWebName ) || "\#FF00FF";
        my $thisWebNoSearchAll = &TWiki::Prefs::getPreferencesValue( "NOSEARCHALL", $thisWebName );

        # make sure we can report this web on an 'all' search
        # DON'T filter out unless it's part of an 'all' search.
        # PTh 18 Aug 2000: Need to include if it is the current web
        next if (   ( $searchAllFlag )
                 && ( ( $thisWebNoSearchAll =~ /on/i ) || ( $thisWebName =~ /^[\.\_]/ ) )
                 && ( $thisWebName ne $TWiki::webName ) );

        # search topics in this web
        my @topicList = _searchTopicsInWeb( $thisWebName, $theTopic, $theScope, $theType, $caseSensitive, @tokens );

        # exclude topics, Codev.ExcludeWebTopicsFromSearch
        if( $caseSensitive ) {
            @topicList = grep( !/$theExclude/, @topicList ) if( $theExclude );
        } else {
            @topicList = grep( !/$theExclude/i, @topicList ) if( $theExclude );
        }
        next if( $noEmpty && ! @topicList ); # Nothing to show for this web

        # use hash tables for date, author, rev number and view permission
        my %topicRevDate = ();
        my %topicRevUser = ();
        my %topicRevNum = ();
        my %topicAllowView = ();

        # sort the topic list by date, author or topic name
        if( $theOrder eq "modified" ) {
            # PTh 03 Nov 2000: Performance improvement
            # Dates are tricky. For performance we do not read, say,
            # 2000 records of author/date, sort and then use only 50.
            # Rather we 
            #   * sort by file timestamp (to get a rough list)
            #   * shorten list to the limit + some slack
            #   * sort by rev date on shortened list to get the acurate list

            # Do performance exercise only if it pays off
            if(  $theLimit + 20 < scalar(@topicList) ) {
                # sort by file timestamp, Schwartzian Transform
                my @tmpList = ();
                if( $revSort ) {
                    @tmpList = map { $_->[1] }
                               sort {$b->[0] <=> $a->[0] }
                               map { [ (stat "$TWiki::dataDir\/$thisWebName\/$_.txt")[9], $_ ] }
                               @topicList;
                } else {
                    @tmpList = map { $_->[1] }
                               sort {$a->[0] <=> $b->[0] }
                               map { [ (stat "$TWiki::dataDir\/$thisWebName\/$_.txt")[9], $_ ] }
                               @topicList;
                }

                # then shorten list and build the hashes for date and author
                my $idx = $theLimit + 10;  # slack on limit
                @topicList = ();
                foreach( @tmpList ) {
                    push( @topicList, $_ );
                    $idx -= 1;
                    last if $idx <= 0;
                }
            }

            # build the hashes for date and author
            foreach( @topicList ) {
                $tempVal = $_;
                # Permission check done below, so force this read to succeed with "internal" parameter
                my( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $tempVal, "", "internal" );
                my ( $revdate, $revuser, $revnum ) = &TWiki::Store::getRevisionInfoFromMeta( $thisWebName, $tempVal, $meta );
                $topicRevUser{ $tempVal }   = &TWiki::userToWikiName( $revuser );
                $topicRevDate{ $tempVal }   = $revdate;  # keep epoc sec for sorting
                $topicRevNum{ $tempVal }    = $revnum;
                $topicAllowView{ $tempVal } = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName,
                                                  $text, $tempVal, $thisWebName );
            }

            # sort by date (second time if exercise), Schwartzian Transform
            my $dt = "";
            if( $revSort ) {
                @topicList = map { $_->[1] }
                             sort {$b->[0] <=> $a->[0] }
                             map { $dt = $topicRevDate{$_}; $topicRevDate{$_} = TWiki::formatTime( $dt ); [ $dt, $_ ] }
                             @topicList;
            } else {
                @topicList = map { $_->[1] }
                             sort {$a->[0] <=> $b->[0] }
                             map { $dt = $topicRevDate{$_}; $topicRevDate{$_} = TWiki::formatTime( $dt ); [ $dt, $_ ] }
                             @topicList;
            }

        } elsif( $theOrder =~ /^creat/ ) {
            # sort by topic creation time

            # first we need to build the hashes for modified date, author, creation time
            my %topicCreated = (); # keep only temporarily for sort
            foreach( @topicList ) {
                $tempVal = $_;
                # Permission check done below, so force this read to succeed with "internal" parameter
                my( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $tempVal, "", "internal" );
                my( $revdate, $revuser, $revnum ) = &TWiki::Store::getRevisionInfoFromMeta( $thisWebName, $tempVal, $meta );
                $topicRevUser{ $tempVal }   = &TWiki::userToWikiName( $revuser );
                $topicRevDate{ $tempVal }   = &TWiki::formatTime( $revdate );
                $topicRevNum{ $tempVal }    = $revnum;
                $topicAllowView{ $tempVal } = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName,
                                                  $text, $tempVal, $thisWebName );
                my ( $createdate ) = &TWiki::Store::getRevisionInfo( $thisWebName, $tempVal, "1.1" );
                $topicCreated{ $tempVal } = $createdate;  # Sortable epoc second format
            }

            # sort by creation time, Schwartzian Transform
            if( $revSort ) {
                @topicList = map { $_->[1] }
                             sort {$b->[0] <=> $a->[0] }
                             map { [ $topicCreated{$_}, $_ ] }
                             @topicList;
            } else {
                @topicList = map { $_->[1] }
                             sort {$a->[0] <=> $b->[0] }
                             map { [ $topicCreated{$_}, $_ ] }
                             @topicList;
            }

        } elsif( $theOrder eq "editby" ) {
            # sort by author

            # first we need to build the hashes for date and author
            foreach( @topicList ) {
                $tempVal = $_;
                # Permission check done below, so force this read to succeed with "internal" parameter
                my( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $tempVal, "", "internal" );
                my( $revdate, $revuser, $revnum ) = &TWiki::Store::getRevisionInfoFromMeta( $thisWebName, $tempVal, $meta );
                $topicRevUser{ $tempVal }   = &TWiki::userToWikiName( $revuser );
                $topicRevDate{ $tempVal }   = &TWiki::formatTime( $revdate );
                $topicRevNum{ $tempVal }    = $revnum;
                $topicAllowView{ $tempVal } = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName,
                                                  $text, $tempVal, $thisWebName );
            }

            # sort by author, Schwartzian Transform
            if( $revSort ) {
                @topicList = map { $_->[1] }
                             sort {$b->[0] cmp $a->[0] }
                             map { [ $topicRevUser{$_}, $_ ] }
                             @topicList;
            } else {
                @topicList = map { $_->[1] }
                             sort {$a->[0] cmp $b->[0] }
                             map { [ $topicRevUser{$_}, $_ ] }
                             @topicList;
            }

        } elsif( $theOrder =~ m/^formfield\((.*)\)$/ ) {
            # sort by TWikiForm field
            my $sortfield = $1;
            my %fieldVals= ();
            # first we need to build the hashes for fields
            foreach( @topicList ) {
                $tempVal = $_;
                # Permission check done below, so force this read to succeed with "internal" parameter
                my( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $tempVal, "", "internal" );
                my( $revdate, $revuser, $revnum ) = &TWiki::Store::getRevisionInfoFromMeta( $thisWebName, $tempVal, $meta );
                $topicRevUser{ $tempVal }   = &TWiki::userToWikiName( $revuser );
                $topicRevDate{ $tempVal }   = &TWiki::formatTime( $revdate );
                $topicRevNum{ $tempVal }    = $revnum;
                $topicAllowView{ $tempVal } = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName,
                                                  $text, $tempVal, $thisWebName );
                $fieldVals{ $tempVal } = getMetaFormField( $meta, $sortfield );
            }
 
            # sort by field, Schwartzian Transform
            if( $revSort ) {
                @topicList = map { $_->[1] }
                sort {$b->[0] cmp $a->[0] }
                map { [ $fieldVals{$_}, $_ ] }
                @topicList;
            } else {
                @topicList = map { $_->[1] }
                sort {$a->[0] cmp $b->[0] }
                map { [ $fieldVals{$_}, $_ ] }
                @topicList;
            }

        } else {
            # simple sort, suggested by RaymondLutz in Codev.SchwartzianTransformMisused
	    ##TWiki::writeDebug "Topic list before sort = @topicList";
            if( $revSort ) {
                @topicList = sort {$b cmp $a} @topicList;
            } else {
                @topicList = sort {$a cmp $b} @topicList;
            }
	    ##TWiki::writeDebug "Topic list after sort = @topicList";
        }

        # header and footer of $thisWebName
        my( $beforeText, $repeatText, $afterText ) = split( /%REPEAT%/, $tmplTable );
        if( $theHeader ) {
            $theHeader =~ s/\$n\(\)/\n/gos;          # expand "$n()" to new line
	    # I18N fix
	    my $mixedAlpha = $TWiki::regex{mixedAlpha};
	    $theHeader =~ s/\$n([^$mixedAlpha])/\n$1/gos; # expand "$n" to new line
            $beforeText = $theHeader;
            $beforeText =~ s/\$web/$thisWebName/gos;
            if( $theSeparator ) {
                $beforeText .= $theSeparator;
            } else {
                $beforeText =~ s/([^\n])$/$1\n/os;  # add new line at end of needed
            }
        }

        # output the list of topics in $thisWebName
        my $ntopics = 0;
        my $headerDone = 0;
        my $topic = "";
        my $head = "";
        my $revDate = "";
        my $revUser = "";
        my $revNum = "";
        my $revNumText = "";
        my $allowView = "";
        my $locked = "";
        foreach( @topicList ) {
          $topic = $_;

          my $meta = "";
          my $text = "";
          my $forceRendering = 0;

          # make sure we have date and author
          if( exists( $topicRevUser{$topic} ) ) {
              $revDate = $topicRevDate{$topic};
              $revUser = $topicRevUser{$topic};
              $revNum  = $topicRevNum{$topic};
              $allowView = $topicAllowView{$topic};
          } else {
              # lazy query, need to do it at last
              ( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $topic );
              $text =~ s/%WEB%/$thisWebName/gos;
              $text =~ s/%TOPIC%/$topic/gos;
              $allowView = &TWiki::Access::checkAccessPermission( "view", $TWiki::wikiUserName, $text, $topic, $thisWebName );
              ( $revDate, $revUser, $revNum ) = &TWiki::Store::getRevisionInfoFromMeta( $thisWebName, $topic, $meta );
              $revDate = &TWiki::formatTime( $revDate );
              $revUser = &TWiki::userToWikiName( $revUser );
          }

          $locked = "";
          if( $doShowLock ) {
              ( $tempVal ) = &TWiki::Store::topicIsLockedBy( $thisWebName, $topic );
              if( $tempVal ) {
                  $revUser = &TWiki::userToWikiName( $tempVal );
                  $locked = "(LOCKED)";
              }
          }

          # Check security
          # FIXME - how do we deal with user login not being available if
          # coming from search script?
          if( ! $allowView ) {
              next;
          }

          # Special handling for format="..."
          if( $theFormat ) {
              unless( $text ) {
                  ( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $topic );
                  $text =~ s/%WEB%/$thisWebName/gos;
                  $text =~ s/%TOPIC%/$topic/gos;
              }
              if( $doExpandVars ) {
                  if( $topic eq $TWiki::topicName ) {
                      # primitive way to prevent recursion
                      $text =~ s/%SEARCH/%<nop>SEARCH/g;
                  }
                  $text = &TWiki::handleCommonTags( $text, $topic, $thisWebName );
              }
          }

          my @multipleHitLines = ();
          if( $doMultiple ) {
              my $pattern = $tokens[$#tokens]; # last token in an AND search
              $pattern = quotemeta( $pattern ) if( $theType ne "regex" );
              ( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $topic ) unless $text;
              if( $caseSensitive ) {
                  @multipleHitLines = reverse grep { /$pattern/ } split( /[\n\r]+/, $text );
              } else {
                  @multipleHitLines = reverse grep { /$pattern/i } split( /[\n\r]+/, $text );
              }
          }

          do {    # multiple=on loop

            $text = pop( @multipleHitLines ) if( scalar( @multipleHitLines ) );

            if( $theFormat ) {
                $tempVal = $theFormat;
                if( $theSeparator ) {
                    $tempVal .= $theSeparator;
                } else {
                    $tempVal =~ s/([^\n])$/$1\n/os;       # add new line at end of needed
                }
                $tempVal =~ s/\$n\(\)/\n/gos;          # expand "$n()" to new line
		# I18N fix
		my $mixedAlpha = $TWiki::regex{mixedAlpha};
                $tempVal =~ s/\$n([^$mixedAlpha])/\n$1/gos; # expand "$n" to new line
                $tempVal =~ s/\$web/$thisWebName/gos;
                $tempVal =~ s/\$topic\(([^\)]*)\)/breakName( $topic, $1 )/geos;
                $tempVal =~ s/\$topic/$topic/gos;
                $tempVal =~ s/\$locked/$locked/gos;
                $tempVal =~ s/\$date/$revDate/gos;
                $tempVal =~ s/\$isodate/&TWiki::revDate2ISO($revDate)/geos;
                $tempVal =~ s/\$rev/1.$revNum/gos;
                $tempVal =~ s/\$wikiusername/$revUser/gos;
                $tempVal =~ s/\$wikiname/wikiName($revUser)/geos;
                $tempVal =~ s/\$username/&TWiki::wikiToUserName($revUser)/geos;
                $tempVal =~ s/\$createdate/_getRev1Info( $thisWebName, $topic, "date" )/geos;
                $tempVal =~ s/\$createusername/_getRev1Info( $thisWebName, $topic, "username" )/geos;
                $tempVal =~ s/\$createwikiname/_getRev1Info( $thisWebName, $topic, "wikiname" )/geos;
                $tempVal =~ s/\$createwikiusername/_getRev1Info( $thisWebName, $topic, "wikiusername" )/geos;
                if( $tempVal =~ m/\$text/ ) {
                    # expand topic text
                    ( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $topic ) unless $text;
                    if( $topic eq $TWiki::topicName ) {
                        # defuse SEARCH in current topic to prevent loop
                        $text =~ s/%SEARCH{.*?}%/SEARCH{...}/go;
                    }
                    $tempVal =~ s/\$text/$text/gos;
                    $forceRendering = 1 unless( $doMultiple );
                }
            } else {
                $tempVal = $repeatText;
            }
            $tempVal =~ s/%WEB%/$thisWebName/go;
            $tempVal =~ s/%TOPICNAME%/$topic/go;
            $tempVal =~ s/%LOCKED%/$locked/o;
            $tempVal =~ s/%TIME%/$revDate/o;
            if( $revNum > 1 ) {
                $revNumText = "r1.$revNum";
            } else {
                $revNumText = "<b>NEW</b>";
            }
            $tempVal =~ s/%REVISION%/$revNumText/o;
            $tempVal =~ s/%AUTHOR%/$revUser/o;

            if( ( $doInline || $theFormat ) && ( ! ( $forceRendering ) ) ) {
                # print at the end if formatted search because of table rendering
                # do nothing
            } else {
                $tempVal = &TWiki::handleCommonTags( $tempVal, $topic );
                $tempVal = &TWiki::getRenderedVersion( $tempVal );
            }

            if( $doRenameView ) { # added JET 19 Feb 2000
                # Permission check done below, so force this read to succeed with "internal" parameter
                my $rawText = &TWiki::Store::readTopicRaw( $thisWebName, $topic, "", "internal" );
                my $changeable = "";
                my $changeAccessOK = &TWiki::Access::checkAccessPermission( "change", $TWiki::wikiUserName, $text, $topic, $thisWebName );
                if( ! $changeAccessOK ) {
                   $changeable = "(NO CHANGE PERMISSION)";
                   $tempVal =~ s/%SELECTION%.*%SELECTION%//o;
                } else {
                   $tempVal =~ s/%SELECTION%//go;
                }
                $tempVal =~ s/%CHANGEABLE%/$changeable/o;

                $tempVal =~ s/%LABEL%/$doRenameView/go;
                my $reducedOutput = "";
                
                # Remove lines that don't contain the topic and highlight matched string
                my $insidePRE = 0;
                my $insideVERBATIM = 0;
                my $noAutoLink = 0;
                
                foreach( split( /\n/, $rawText ) ) {
                
                   next if( /^%META:TOPIC(INFO|MOVED)/ );
                   s/</&lt;/go;
                   s/>/&gt;/go;

                   # This code is in far too many places
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
                       # Case insensitive option is required to get [[spaced Word]] to match
		       # I18N: match non-alpha before and after topic name in renameview searches
		       my $alphaNum = $TWiki::regex{mixedAlphaNum};
                       my $match =  "(^|[^${alphaNum}_.])($originalSearch)(?=[^${alphaNum}]|\$)";
		       # NOTE: Must *not* use /o here, since $match is based on
		       # search string that will vary during lifetime of
		       # compiled code with mod_perl.
                       my $subs = s|$match|$1<font color="red">$2</font>&nbsp;|g;
                       $match = '(\[\[)' . "($spacedTopic)" . '(?=\]\])';
                       $subs += s|$match|$1<font color="red">$2</font>&nbsp;|gi;
                       if( $subs ) {
                           $topicCount++ if( ! $reducedOutput );
                           $reducedOutput .= "$_<br />\n" if( $subs );
                       }
                   }
                }
                $tempVal =~ s/%TOPIC_NUMBER%/$topicCount/go;
                $tempVal =~ s/%TEXTHEAD%/$reducedOutput/go;
                next if ( ! $reducedOutput );

            } elsif( $doBookView ) {
                # BookView, added PTh 20 Jul 2000
                if( ! $text ) {
                    ( $meta, $text ) = &TWiki::Store::readTopic( $thisWebName, $topic );
                }

                $text = &TWiki::handleCommonTags( $text, $topic, $thisWebName );
                $text = &TWiki::getRenderedVersion( $text, $thisWebName );
                # FIXME: What about meta data rendering?
                $tempVal =~ s/%TEXTHEAD%/$text/go;

            } elsif( $theFormat ) {
                # free format, added PTh 10 Oct 2001
                $tempVal =~ s/\$summary/&TWiki::makeTopicSummary( $text, $topic, $thisWebName )/geos;
                $tempVal =~ s/\$formfield\(\s*([^\)]*)\s*\)/getMetaFormField( $meta, $1 )/geos;
                $tempVal =~ s/\$formname/_getMetaFormName( $meta )/geos;
                $tempVal =~ s/\$pattern\((.*?\s*\.\*)\)/getTextPattern( $text, $1 )/geos;
                $tempVal =~ s/\$nop(\(\))?//gos;      # remove filler, useful for nested search
                $tempVal =~ s/\$quot(\(\))?/\"/gos;   # expand double quote
                $tempVal =~ s/\$percnt(\(\))?/\%/gos; # expand percent
                $tempVal =~ s/\$dollar(\(\))?/\$/gos; # expand dollar

            } elsif( $noSummary ) {
                $tempVal =~ s/%TEXTHEAD%//go;
                $tempVal =~ s/&nbsp;//go;

            } else {
                # regular search view
                if( $text ) {
                    $head = $text;
                } else {
                    $head = &TWiki::Store::readFileHead( "$TWiki::dataDir\/$thisWebName\/$topic.txt", 16 );
                }
                $head = &TWiki::makeTopicSummary( $head, $topic, $thisWebName );
                $tempVal =~ s/%TEXTHEAD%/$head/go;
            }

            # lazy output of header (only if needed for the first time)
            unless( $headerDone || $noHeader ) {
                $headerDone = 1;
                $beforeText =~ s/%WEBBGCOLOR%/$thisWebBGColor/go;
                $beforeText =~ s/%WEB%/$thisWebName/go;
                $beforeText = &TWiki::handleCommonTags( $beforeText, $topic );
                if( $doInline || $theFormat ) {
                    # print at the end if formatted search because of table rendering
                    $searchResult .= $beforeText;
                } else {
                    $beforeText = &TWiki::getRenderedVersion( $beforeText, $thisWebName );
                    $beforeText =~ s|</*nop/*>||goi;   # remove <nop> tag
                    print $beforeText;
                }
            }

            # output topic (or line if multiple=on)
            if( $doInline || $theFormat ) {
                # print at the end if formatted search because of table rendering
                $searchResult .= $tempVal;
            } else {
                $tempVal = &TWiki::getRenderedVersion( $tempVal, $thisWebName );
                $tempVal =~ s|</*nop/*>||goi;   # remove <nop> tag
                print $tempVal;
            }

          } while( @multipleHitLines ); # multiple=on loop

          $ntopics += 1;
          last if( $ntopics >= $theLimit );
        } # end topic loop in a web

        # output footer only if hits in web
        if( $ntopics ) {
            # output footer of $thisWebName
            $afterText  = &TWiki::handleCommonTags( $afterText, $topic );
            if( $doInline || $theFormat ) {
                # print at the end if formatted search because of table rendering
                $afterText =~ s/\n$//os;  # remove trailing new line
                $searchResult .= $afterText;
            } else {
                $afterText = &TWiki::getRenderedVersion( $afterText, $thisWebName );
                $afterText =~ s|</*nop/*>||goi;   # remove <nop> tag
                print $afterText;
            }
        }

        # output number of topics (only if hits in web or if search only one web)
        if( $ntopics || @webList < 2 ) {
            unless( $noTotal ) {
                my $thisNumber = $tmplNumber;
                $thisNumber =~ s/%NTOPICS%/$ntopics/go;
                if( $doInline || $theFormat ) {
                    # print at the end if formatted search because of table rendering
                    $searchResult .= $thisNumber;
                } else {
                    $thisNumber = &TWiki::getRenderedVersion( $thisNumber, $thisWebName );
                    $thisNumber =~ s|</*nop/*>||goi;   # remove <nop> tag
                    print $thisNumber;
                }
            }
        }
    }

    if( $theFormat ) {
        if( $theSeparator ) {
            $searchResult =~ s/$theSeparator$//s;  # remove separator at end
        } else {
            $searchResult =~ s/\n$//os;            # remove trailing new line
        }
    }
    if( $doInline ) {
        # return formatted search result
        return $searchResult;

    } else {
        if( $theFormat ) {
            # finally print $searchResult which got delayed because of formatted search
            $tmplTail = "$searchResult$tmplTail";
        }

        # print last part of full HTML page
        $tmplTail = &TWiki::getRenderedVersion( $tmplTail );
        $tmplTail =~ s|</*nop/*>||goi;   # remove <nop> tag
        print $tmplTail;
    }
    return $searchResult;
}

#=========================
=pod

---++ sub _getRev1Info( $theWeb, $theTopic, $theAttr )

Returns the topic revision info of version 1.1, attributes are "date", "username", "wikiname",
"wikiusername". Revision info is cached for speed

=cut

sub _getRev1Info
{
    my( $theWeb, $theTopic, $theAttr ) = @_;

    unless( $cacheRev1webTopic eq "$theWeb.$theTopic" ) {
        # refresh cache
        $cacheRev1webTopic = "$theWeb.$theTopic";
        ( $cacheRev1date, $cacheRev1user ) = &TWiki::Store::getRevisionInfo( $theWeb, $theTopic, "1.1" );
    }
    if( $theAttr eq "username" ) {
        return $cacheRev1user;
    }
    if( $theAttr eq "wikiname" ) {
        return &TWiki::userToWikiName( $cacheRev1user, 1 );
    }
    if( $theAttr eq "wikiusername" ) {
        return &TWiki::userToWikiName( $cacheRev1user );
    }
    if( $theAttr eq "date" ) {
        return &TWiki::formatTime( $cacheRev1date );
    }
    # else assume attr "key"
    return "1.1";
}

#=========================
=pod

---++ sub getMetaFormField (  $theMeta, $theParams  )

Not yet documented.

=cut

sub getMetaFormField
{
    my( $theMeta, $theParams ) = @_;

    my $name = $theParams;
    my $break = "";
    my @params = split( /\,\s*/, $theParams, 2 );
    if( @params > 1 ) {
        $name = $params[0] || "";
        $break = $params[1] || 1;
    }
    my $title = "";
    my $value = "";
    my @fields = $theMeta->find( "FIELD" );
    foreach my $field ( @fields ) {
        $title = $field->{"title"};
        $value = $field->{"value"};
        $value =~ s/^\s*(.*?)\s*$/$1/go;
        if( $title eq $name ) {
            $value = breakName( $value, $break );
            return $value;
        }
    }
    return "";
}

#=========================
=pod

---++ sub _getMetaFormName (  $theMeta )

Returns the name of the form attached to the topic

=cut

sub _getMetaFormName
{
    my( $theMeta ) = @_;

    my %aForm = $theMeta->findOne( "FORM" );
    if( %aForm ) {
        return $aForm{"name"};
    }
    return "";
}

#=========================
=pod

---++ sub getTextPattern (  $theText, $thePattern  )

Not yet documented.

=cut

sub getTextPattern
{
    my( $theText, $thePattern ) = @_;

    $thePattern =~ s/([^\\])([\$\@\%\&\#\'\`\/])/$1\\$2/go;  # escape some special chars
    $thePattern =~ /(.*)/;     # untaint
    $thePattern = $1;
    my $OK = 0;
    eval {
       $OK = ( $theText =~ s/$thePattern/$1/is );
    };
    $theText = "" unless( $OK );

    return $theText;
}

#=========================
=pod

---++ sub wikiName (  $theWikiUserName  )

Not yet documented.

=cut

sub wikiName
{
    my( $theWikiUserName ) = @_;

    $theWikiUserName =~ s/^.*\.//o;
    return $theWikiUserName;
}

#=========================
=pod

---++ sub breakName (  $theText, $theParams  )

Not yet documented.

=cut

sub breakName
{
    my( $theText, $theParams ) = @_;

    my @params = split( /[\,\s]+/, $theParams, 2 );
    if( @params ) {
        my $len = $params[0] || 1;
        $len = 1 if( $len < 1 );
        my $sep = "- ";
        $sep = $params[1] if( @params > 1 );
        if( $sep =~ /^\.\.\./i ) {
            # make name shorter like "ThisIsALongTop..."
            $theText =~ s/(.{$len})(.+)/$1.../;

        } else {
            # split and hyphenate the topic like "ThisIsALo- ngTopic"
            $theText =~ s/(.{$len})/$1$sep/g;
            $theText =~ s/$sep$//;
        }
    }
    return $theText;
}

#=========================
# Turn a topic into a spaced-out topic, with space before each part of
# the WikiWord.
=pod

---++ sub spacedTopic (  $topic  )

Not yet documented.

=cut

sub spacedTopic
{
    my( $topic ) = @_;
    # FindMe -> Find\s*Me
    # I18N fix
    my $upperAlpha = $TWiki::regex{singleUpperAlphaRegex};
    my $lowerAlpha = $TWiki::regex{singleLowerAlphaRegex};
    $topic =~ s/($lowerAlpha)($upperAlpha)/$1 *$2/go;
    return $topic;
}

#=========================

1;

# EOF
