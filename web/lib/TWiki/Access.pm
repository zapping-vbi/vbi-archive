# Module of TWiki Collaboration Platform, http://TWiki.org/
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
# - Installation instructions in $dataDir/TWiki/TWikiDocumentation.txt
# - Customize variables in wikicfg.pm when installing TWiki.
# - Optionally change wikicfg.pm for custom extensions of rendering rules.
# - Upgrading TWiki is easy as long as you only customize wikicfg.pm.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log

=begin twiki

---+ TWiki::Access Package

This package manages access control to view and change topics. Plugins
should only use the equivalent interface in TWiki::Func.

=cut

package TWiki::Access;

use strict;

use vars qw(
    %allGroups @processedGroups
);

# =========================
=pod

---++ initializeAccess()
| Description: | Basic module initialization, called from TWiki::initialize |

=cut

sub initializeAccess
{
    %allGroups = ();
    @processedGroups = ();
}

# =========================
# Are there any security restrictions for this Web
# (ignoring settings on individual pages).
=pod

---++ sub permissionsSet (  $web  )

Not yet documented.

=cut

sub permissionsSet
{
    my( $web ) = @_;
    
    my $permSet = 0;
    
    my @types = qw/ALLOW DENY/;
    my @actions = qw/CHANGE VIEW RENAME/;
    
    OUT: foreach my $type ( @types ) {
        foreach my $action ( @actions ) {
            my $pref = $type . "WEB" . $action;
            my $prefValue = TWiki::Prefs::getPreferencesValue( $pref, $web ) || "";
            if( $prefValue !~ /^\s*$/ ) {
                $permSet = 1;
                last OUT;
            }
        }
    }
    
    return $permSet;
}

# =========================
=pod

---++ checkAccessPermission( $action, $user, $text, $topic, $web ) ==> $ok
| Description:          | Check if user is allowed to access topic |
| Parameter: =$action=  | "VIEW", "CHANGE", "CREATE", etc. |
| Parameter: =$user=    | Remote WikiName, e.g. "Main.PeterThoeny" |
| Parameter: =$text=    | If empty: Read "$theWebName.$theTopicName" to check permissions |
| Parameter: =$topic=   | Topic name to check, e.g. "SomeTopic" |
| Parameter: =$web=     | Web, e.g. "Know" |
| Return:    =$ok=      | 1 if OK to access, 0 if no permission |

=cut

sub checkAccessPermission
{
    my( $theAccessType, $theUserName,
        $theTopicText, $theTopicName, $theWebName ) = @_;

#AS 2001-11-04 see Codev.UnchangeableTopicBug
    if ( $TWiki::doSuperAdminGroup && 
	 $TWiki::superAdminGroup ) {
	if ( &userIsInGroup( $theUserName, $TWiki::superAdminGroup ) ) {
	    return 1;
	}
    }
#/AS

    $theAccessType = uc( $theAccessType );  # upper case
    if( ! $theWebName ) {
        $theWebName = $TWiki::webName;
    }
    if( ! $theTopicText ) {
        # text not supplied as parameter, so read topic
        $theTopicText = &TWiki::Store::readWebTopic( $theWebName, $theTopicName );
    }
    ##&TWiki::writeDebug( "checkAccessPermission: Type $theAccessType, user $theUserName, topic $theTopicName" );

    # parse the " * Set (ALLOWTOPIC|DENYTOPIC)$theAccessType = " in body text
    my @denyList = ();
    my @allowList = ();
    foreach( split( /\n/, $theTopicText ) ) {
        if( /^\s+\*\sSet\s(ALLOWTOPIC|DENYTOPIC)$theAccessType\s*\=\s*(.*)/ ) {
            if( $2 ) {
                my $allowOrDeny = $1;        # "ALLOWTOPIC" or "DENYTOPIC"
                my @tmpList = map { getUsersOfGroup( $_ ) }
                              prvGetUserList( $2 );
                ##my $tmp = join( ', ', @tmpList );
                ##&TWiki::writeDebug( "  Topic $allowOrDeny$theAccessType: {$tmp}" );
                if( $allowOrDeny eq "DENYTOPIC" ) {
                    @denyList = @tmpList;
                } else {
                    @allowList = @tmpList;
                }
            }
        }
    }
    
    # if empty, get access permissions from preferences
    if( ! @denyList ) {
        my $tmpVal = &TWiki::Prefs::getPreferencesValue( "DENYWEB$theAccessType", $theWebName );
        @denyList  = map { getUsersOfGroup( $_ ) }
                     prvGetUserList( $tmpVal );
        ##my $tmp = join( ', ', @denyList );
        ##&TWiki::writeDebug( "  Prefs DENYWEB$theAccessType: {$tmp}" );
    }
    if( ! @allowList ) {
        my $tmpVal = &TWiki::Prefs::getPreferencesValue( "ALLOWWEB$theAccessType", $theWebName );
        @allowList  = map { getUsersOfGroup( $_ ) }
                      prvGetUserList( $tmpVal );
        ##my $tmp = join( ', ', @allowList );
        ##&TWiki::writeDebug( "  Prefs ALLOWWEB$theAccessType: {$tmp}" );
    }

    # access permission logic
    if( @denyList ) {
        if( grep { /^$theUserName$/ } @denyList  ) {
            # user is on deny list
            ##&TWiki::writeDebug( "  return 0, user is on deny list" );
            return 0;
        }
    }
    if( @allowList ) {
        if( grep { /^$theUserName$/ } @allowList  ) {
            # user is on allow list
            ##&TWiki::writeDebug( "  return 1, user is on allow list" );
            return 1;
        } else {
            # user is not on allow list
            ##&TWiki::writeDebug( "  return 0, user is not on allow list" );
            return 0;
        }
    }
    # allow is undefined, so grant access
    ##&TWiki::writeDebug( "  return 1, allow is undefined" );
    return 1;
}

# =========================
=pod

---++ getListOfGroups(  ) ==> @listOfGroups
| Description:        | get a list of groups definedin this TWiki |
| Return:    =@listOfGroups=    | list of all the groups |

=cut

sub getListOfGroups
{
    my $text = &TWiki::Search::searchWeb(
         "inline"        => "1",
         "search"        => "Set GROUP =",
         "web"           => "all",
         "topic"         => "*Group",
         "type"          => "regex",
         "nosummary"     => "on",
         "nosearch"      => "on",
         "noheader"      => "on",
         "nototal"       => "on",
         "noempty"       => "on",
	 "format"	 => "\$web.\$topic",
     );

    my ( @list ) =  split ( /\n/, $text );	
    return @list;
}

# =========================
=pod

---++ getGroupsUserIsIn( $user ) ==> @listOfGroups
| Description:        | get a list of groups a user is in |
| Parameter: =$user=  | Remote WikiName, e.g. "Main.PeterThoeny" |
| Return:    =@listOfGroups=    | list os all the WikiNames for a group |

=cut

sub getGroupsUserIsIn
{
    my( $theUserName ) = @_;

    my $userTopic = prvGetWebTopicName( $TWiki::mainWebname, $theUserName );
    my @grpMembers = ();
    my @listOfGroups = getListOfGroups();
    my $group;

	&TWiki::writeDebug("Checking [$userTopic]");
    foreach $group ( @listOfGroups) {
        if ( userIsInGroup ( $userTopic, $group )) {
	    	push ( @grpMembers, $group );
		}
    }

    return @grpMembers;
}

# =========================
=pod

---++ userIsInGroup( $user, $group ) ==> $ok
| Description:        | Check if user is a member of a group |
| Parameter: =$user=  | Remote WikiName, e.g. "Main.PeterThoeny" |
| Parameter: =$group= | Group name, e.g. "Main.EngineeringGroup" |
| Return:    =$ok=    | 1 user is in group, 0 if not |
| TODO: | what are we checking if we are not specifying a Group? |
| | more detailed documentation@! |

=cut

sub userIsInGroup
{
    my( $theUserName, $theGroupTopicName ) = @_;

    my $usrTopic = prvGetWebTopicName( $TWiki::mainWebname, $theUserName );
    my $grpTopic = prvGetWebTopicName( $TWiki::mainWebname, $theGroupTopicName );
    my @grpMembers = ();

    if( $grpTopic !~ /.*Group$/ ) {
        # not a group, so compare user to user
        push( @grpMembers, $grpTopic );
    } elsif( ( %allGroups ) && ( exists $allGroups{ $grpTopic } ) ) {
        # group is allready known
        @grpMembers = @{ $allGroups{ $grpTopic } };
    } else {
        @grpMembers = prvGetUsersOfGroup( $grpTopic, 1 );
    }

    my $isInGroup = grep { /^$usrTopic$/ } @grpMembers;
    return $isInGroup;
}

# =========================
=pod

---++ getUsersOfGroup( $group ) ==> @users
| Description:         | Get all members of a group; groups are expanded recursively |
| Parameter: =$group=  | Group topic name, e.g. "Main.EngineeringGroup" |
| Return:    =@users=  | List of users, e.g. ( "Main.JohnSmith", "Main.JaneMiller" ) |

=cut

sub getUsersOfGroup
{
    my( $theGroupTopicName ) = @_;
    ##TWiki::writeDebug( "group is $theGroupTopicName" );
    return prvGetUsersOfGroup( $theGroupTopicName, 1 );
}

# =========================
=pod

---++ sub prvGetUsersOfGroup (  $theGroupTopicName, $theFirstCall  )

Not yet documented.

=cut

sub prvGetUsersOfGroup
{
    my( $theGroupTopicName, $theFirstCall ) = @_;

    my @resultList = ();
    # extract web and topic name
    my $topic = $theGroupTopicName;
    my $web = $TWiki::mainWebname;
    $topic =~ /^([^\.]*)\.(.*)$/;
    if( $2 ) {
        $web = $1;
        $topic = $2;
    }
    ##TWiki::writeDebug( "Web is $web, topic is $topic" );

    if( $topic !~ /.*Group$/ ) {
        # return user, is not a group
        return ( "$web.$topic" );
    }

    # check if group topic is already processed
    if( $theFirstCall ) {
        # FIXME: Get rid of this global variable
        @processedGroups = ();
    } elsif( grep { /^$web\.$topic$/ } @processedGroups ) {
        # do nothing, already processed
        return ();
    }
    push( @processedGroups, "$web\.$topic" );

    # read topic
    my $text = &TWiki::Store::readWebTopic( $web, $topic );

    # reset variables, defensive coding needed for recursion
    (my $baz = "foo") =~ s/foo//;

    # extract users
    my $user = "";
    my @glist = ();
    foreach( split( /\n/, $text ) ) {
        if( /^\s+\*\sSet\sGROUP\s*\=\s*(.*)/ ) {
            if( $1 ) {
                @glist = prvGetUserList( $1 );
            }
        }
    }
    foreach( @glist ) {
        if( /.*Group$/ ) {
            # $user is actually a group
            my $group = $_;
            if( ( %allGroups ) && ( exists $allGroups{ $group } ) ) {
                # allready known, so add to list
                push( @resultList, @{ $allGroups{ $group } } );
            } else {
                # call recursively
                my @userList = prvGetUsersOfGroup( $group, 0 );
                # add group to allGroups hash
                $allGroups{ $group } = [ @userList ];
                push( @resultList, @userList );
            }
        } else {
            # add user to list
            push( @resultList, $_ );
        }
    }
    ##TWiki::writeDebug( "Returning group member list of @resultList" );
    return @resultList;
}

# =========================
=pod

---++ sub prvGetWebTopicName (  $theWebName, $theTopicName  )

Not yet documented.

=cut

sub prvGetWebTopicName
{
    my( $theWebName, $theTopicName ) = @_;
    $theTopicName =~ s/%MAINWEB%/$theWebName/go;
    $theTopicName =~ s/%TWIKIWEB%/$theWebName/go;
    if( $theTopicName =~ /[\.]/ ) {
        $theWebName = "";  # to suppress warning
    } else {
        $theTopicName = "$theWebName\.$theTopicName";
    }
    return $theTopicName;
}

# =========================
=pod

---++ sub prvGetUserList (  $theItems  )

Not yet documented.

=cut

sub prvGetUserList
{
    my( $theItems ) = @_;
    # comma delimited list of users or groups
    # i.e.: "%MAINWEB%.UserA, UserB, Main.UserC  # something else"
    $theItems =~ s/(<[^>]*>)//go;     # Remove HTML tags
    # TODO: i18n fix for user name
    $theItems =~ s/\s*([a-zA-Z0-9_\.\,\s\%]*)\s*(.*)/$1/go; # Limit list
    my @list = map { prvGetWebTopicName( $TWiki::mainWebname, $_ ) }
               split( /[\,\s]+/, $theItems );
    return @list;
}

# =========================

1;

# EOF
