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
# - Files wiki[a-z]+.pm are included by wiki.pm
# - Upgrading TWiki is easy as long as you only customize wikicfg.pm.
# - Check web server error logs for errors, i.e. % tail /var/log/httpd/error_log
use strict;

=begin twiki

---+ TWiki::Prefs Module

This module reads TWiki preferences of site-level, web-level and user-level
topics and implements routines to access those preferences.

=cut

$TWiki::Prefs::finalPrefsName = "FINALPREFERENCES";
$TWiki::Prefs::formPrefPrefix = "FORM_";

package TWiki::Prefs::TopicPrefs;

=pod

---++ TopicPrefs Object

This Prefs-internal class is used to cache preferences read in from a single
topic.  It is available via 'use TWiki::Prefs', but not via
'use TWiki::Prefs::TopicPrefs'.

---+++ sub new( $web, $topic )

Reads preferences from the specified topic into a new TopicPrefs object.

=cut

sub new
{
    my ($class, $theWeb, $theTopic) = @_;
    my $self = {};
    bless $self, $class;

    $self->{web} = $theWeb;
    $self->{topic} = $theTopic;

    $self->readPrefs();

    return $self;
}

=pod

---+++ sub readPrefs()

Rereads preferences from the topic, updating the TopicPrefs object.

=cut

sub readPrefs
{
    my $self = shift;
    
    my $theWeb = $self->{web};
    my $theTopic = $self->{topic};
    
    $self->{prefsHash} = {};
    $self->{prefsKeys} = [];
    $self->{prefsVals} = [];

    my( $meta, $text ) = &TWiki::Store::readTopic( $theWeb, $theTopic, 1 );
    $text =~ s/\r/\n/g;
    $text =~ s/\n+/\n/g;

    my $key = "";
    my $value ="";
    my $isKey = 0;
    foreach( split( /\n/, $text ) ) {
        if( /^\t+\*\sSet\s([a-zA-Z0-9_]*)\s\=\s*(.*)/ ) {
            if( $isKey ) {
                $self->insertPrefsValue( $key, $value );
            }
            $key = $1;
            $value = defined $2 ? $2 : "";
            $isKey = 1;
        } elsif ( $isKey ) {
            if( ( /^\t+/ ) && ( ! /^\t+\*/ ) ) {
                # follow up line, extending value
                $value .= "\n$_";
            } else {
                $self->insertPrefsValue( $key, $value );
                $isKey = 0;
            }
        }
    }
    if( $isKey ) {
        $self->insertPrefsValue( $key, $value );
    }
    
    my %form = $meta->findOne( "FORM" );
    if( %form ) {
        my @fields = $meta->find( "FIELD" );
        foreach my $field ( @fields ) {
            $key = $field->{"name"};
            $value = $field->{"value"};
	    my $title = $field->{"title"};
	    my $prefixedTitle = $TWiki::Prefs::formPrefPrefix . $title;
	    $self->insertPrefsValue( $prefixedTitle, $value );
            my $attributes = $field->{"attributes"};
            if( $attributes && $attributes =~ /[S]/o ) {
                $self->insertPrefsValue( $key, $value );
            }
        }
    }
}

=pod

---+++ sub insertPrefsValue( $key, $value )

Adds a key-value pair to the TopicPrefs object.

=cut

sub insertPrefsValue {
    my ( $self, $theKey, $theValue ) = @_;

    return if exists $self->{finalHash}{$theKey}; # key is final, may not be overridden

    $theValue =~ s/\t/ /g;                 # replace TAB by space
    $theValue =~ s/([^\\])\\n/$1\n/g;      # replace \n by new line
    $theValue =~ s/([^\\])\\\\n/$1\\n/g;   # replace \\n by \n
    $theValue =~ s/`//g;                   # filter out dangerous chars

    if (exists $self->{prefsHash}{$theKey}) {
	# key exists, need to deal with existing preference
	my $valueRef = $self->{prefsHash}{$theKey};
	if ($theKey eq $TWiki::Prefs::finalPrefsName) {
	    $$valueRef .= ", $theValue"; # merge final preferences lists
	} else {
	    $$valueRef = $theValue; # simply replace all other values
	}
    } else {
	# new preference setting, no previous value
	my $newIndex = scalar @{ $self->{prefsKeys} };
	$self->{prefsKeys}[$newIndex] = $theKey;
	$self->{prefsVals}[$newIndex] = $theValue;
	$self->{prefsHash}{$theKey} = \$self->{prefsVals}[$newIndex];
    }
}

=pod

---+++ sub getTopicPrefs()

Returns a list  =( \@keys, \@values )= listing the preference settings in the
current topic.  Changing entries in the =@values= array will change cached
preference settings.  Changing entries in the =@keys= array will result in
unspecified behavior -- don't do that.

=cut

sub getTopicPrefs
{
    my $self = shift;
    return ($self->{prefsKeys}, $self->{prefsVals}) if wantarray;
    return [$self->{prefsKeys}, $self->{prefsVals}]; # return array ref in scalar context
}


# =============================================================================
package TWiki::Prefs::PrefsCache;

use vars qw( %topicCache );

=pod

---++ PrefsCache Package Functions

The PrefsCache package holds a cache of topics that have been read in, using
the TopicPrefs class.  These functions manage that cache.

---+++ sub clearCache()

This non-member function clears cached topic preferences, forcing all settings
to be reread.

---+++ sub invalidateCache( $web, $topic )

This non-member function invalidates the cache on a particular topic.

=cut

sub clearCache { undef %topicCache; }
sub invalidateCache { delete $topicCache{$_[0]}{$_[1]}; }

=pod

---++ PrefsCache Object

This defines an object used internally by the functions in TWiki::Prefs to hold
preferences.  This object handles the cascading of preferences from site, to
web, to topic/user.

---+++ sub new( $type, $parent, @target )

| Description: | Creates a new Prefs object. |
| Parameter: =$type= | Type of prefs object to create, see notes. |
| Parameter: =$parent= | Prefs object from which to inherit higher-level settings. |
| Parameter: =@target= | What this object stores preferences for, see notes. |

*Notes:* =$type= should be one of "global", "web", "request", or "copy". If the
type is "global", no parent or target should be specified; the object will
cache sitewide preferences.  If the type is "web", =$parent= should hold global
preferences, and @target should contain only the web's name.  If the type is
"request", then $parent should be a "web" preferences object for the current
web, and =@target= should be ( $topicName, $userName ).  $userName should be
just the WikiName, with no web specifier.  If the type is "copy", the result is
a simple copy of =$parent=; no =@target= is needed.

Call like this: =$mainWebPrefs = TWiki::Prefs->new("web", "Main");=

=cut

sub new
{
    my ($theClass, $theType, $theParent, @theTarget) = @_;
    
    my $self;
    
    if ($theType eq "copy") {
	$self = { %$theParent };
	bless $self, $theClass;

	$self->inheritPrefs($theParent);
    } else {
	$self = {};
	bless $self, $theClass;

	$self->{type} = $theType;
	$self->{parent} = $theParent;
	$self->{web} = $theTarget[0] if ($theType eq "web");
	
	if ($theType eq "request") {
	    $self->{topic} = $theTarget[0];
	    $self->{user} = $theTarget[1];
	}

	$self->loadPrefs( 1 );
    }

    return $self;
}

=pod

---+++ sub loadPrefs( $allowCache )

Requests for Prefs object to load preferences from its defining topics,
re-cascading the overrides.  If =$allowCache= is set, the topic cache will be
used to load preferences when applicable.  Topics that must be read will be
placed in the cache regardless of the setting of $allowCache.

=cut

sub loadPrefs
{
    my ($self, $allowCache) = @_;
    
    $self->{prefsKeys} = [];
    $self->{prefsVals} = [];
    $self->{prefsHash} = {};
    $self->{finalHash} = {};

    $self->inheritPrefs($self->{parent}) if defined $self->{parent};
    
    if ($self->{type} eq "global") {
	# global prefs
        $self->loadPrefsFromTopic($TWiki::twikiWebname, $TWiki::wikiPrefsTopicname, "", $allowCache);
        $self->loadPrefsFromTopic($TWiki::mainWebname, $TWiki::wikiPrefsTopicname, "", $allowCache);

    } elsif ($self->{type} eq "web") {
	# web prefs
	$self->loadPrefsFromTopic( $self->{web}, $TWiki::webPrefsTopicname, "", $allowCache);

    } elsif ($self->{type} eq "request") {
	# request prefs - read topic and user
	my $parent = $self->{parent};
	my $topicPrefsSetting = $parent->getPreferenceFlag("READTOPICPREFS");
	my $topicPrefsOverride = $parent->getPreferenceFlag("TOPICOVERRIDESUSER");
	if ($topicPrefsSetting && !$topicPrefsOverride) {
	    # topic prefs overridden by user prefs
	    $self->loadPrefsFromTopic( $parent->{web}, $self->{topic}, "", $allowCache);
	}
	$self->loadPrefsFromTopic( $TWiki::mainWebname, $self->{user}, "", $allowCache );
	if ($topicPrefsSetting && $topicPrefsOverride) {
	    # topic prefs override user prefs
	    $self->loadPrefsFromTopic( $parent->{web}, $self->{topic}, "", $allowCache );
	}
    }
}

=pod

---+++ sub loadPrefsFromTopic( $web, $topic, $keyPrefix, $allowCache )

Loads preferences from a topic.  If =$allowCache= is set then cached
settings are used where available.  All settings loaded are prefixed
with =$keyPrefix=.

=cut

sub loadPrefsFromTopic
{
    my ($self, $theWeb, $theTopic, $theKeyPrefix, $allowCache) = @_;

    my $topicPrefs;
    
    if ($allowCache && exists( $topicCache{$theWeb}{$theTopic} )) {
	$topicPrefs = $topicCache{$theWeb}{$theTopic};
    } else {
	$topicPrefs = TWiki::Prefs::TopicPrefs->new($theWeb, $theTopic);
    }

    my ($topicKeys, $topicVals) = $topicPrefs->getTopicPrefs();
    
    $theKeyPrefix = "" unless defined $theKeyPrefix;
    for ( my $i = 0; $i < @$topicVals; $i++) {
	$self->_insertPreference($theKeyPrefix . $topicKeys->[$i], $topicVals->[$i]);
    }

    if (exists($self->{prefsHash}{$TWiki::Prefs::finalPrefsName})) {
	my $finalPrefsRef = $self->{prefsHash}{$TWiki::Prefs::finalPrefsName};
	my @finalPrefsList = split /[\s,]+/, $$finalPrefsRef;
	$self->{finalHash} = { map { $_ => 1 } @finalPrefsList };
    }
    
}

# Private function to insert a value into a PrefsCache object
sub _insertPreference
{
    my ($self, $theKey, $theValue) = @_;

    return if (exists $self->{finalHash}{$theKey}); # $theKey is in FINALPREFERENCES, don't update it
    
    if (exists $self->{prefsHash}{$theKey}) {
	# key exists, need to deal with existing preference
	my $valueRef = $self->{prefsHash}{$theKey};
	if ($theKey eq $TWiki::Prefs::finalPrefsName) {
	    $$valueRef .= ", $theValue"; # merge final preferences lists
	} else {
	    $$valueRef = $theValue; # simply replace all other values
	}
    } else {
	# new preference setting, no previous value
	my $newIndex = scalar @{ $self->{prefsKeys} };
	$self->{prefsKeys}[$newIndex] = $theKey;
	$self->{prefsVals}[$newIndex] = $theValue;
	$self->{prefsHash}{$theKey} = \$self->{prefsVals}[$newIndex];
    }
    
}

=pod

---+++ sub inheritPrefs( $otherPrefsObject )

Simply copies the preferences contained in the $otherPrefsObject into the
current one, discarding anything that may currently be there.

=cut

sub inheritPrefs
{
    my ($self, $otherPrefsObject) = @_;
    $self->{prefsKeys} = [ @{ $otherPrefsObject->{prefsKeys} } ];
    $self->{prefsVals} = [ @{ $otherPrefsObject->{prefsVals} } ];
    $self->{finalHash} = { %{ $otherPrefsObject->{finalHash} } };
    
    for (my $i = 0; $i < @{ $self->{prefsKeys} }; $i++) {
	$self->{prefsHash}{$self->{prefsKeys}[$i]} = \$self->{prefsVals}[$i];
    }
}

=pod

---+++ sub replacePreferencesTags( $text )

Substitutes preferences values for =%PREF%= tags in =$text=, modifying that
parameter in-place.

=cut

sub replacePreferencesTags
{
    my $self = shift;

    my $x;
    my $term;

    my $keys = $self->{prefsKeys};
    my $vals = $self->{prefsVals};
    
    for( $x = 0; $x < @$keys; $x++ ) {
        $term = '%' . $keys->[$x] . '%';
        $_[0] =~ s/$term/$vals->[$x]/ge;
    }
}

=pod

---+++ sub getPreferenceValue( $key )

Returns the stored preference with key =$key=, or "" if no such preference
exists.

=cut

sub getPreferenceValue
{
    my ($self, $theKey) = @_;
    if (exists($self->{prefsHash}{$theKey})) {
	return ${ $self->{prefsHash}{$theKey} }; #double dereference
    } else {
	return "";
    }
}

=pod

---+++ sub getPreferenceFlag( $key )

Returns a preference as a flag.  See
=[[#sub_formatAsFlag_prefValue][Prefs::formatAsFlag]]= for details on how
preference values are converted to flags.

=cut

sub getPreferenceFlag
{
    my ( $self, $theKey  ) = @_;

    my $value = $self->getPreferenceValue( $theKey );
    return TWiki::Prefs::formatAsFlag( $value );
}

# =============================================================================
package TWiki::Prefs;

use vars qw(
    $globalPrefs %webPrefs $requestPrefs $requestWeb
    $finalPrefsName
);


=pod

---++ TWiki::Prefs package

This is the external interface to the Prefs module, and is how the rest of the
TWiki code accesses preferences.

---+++ sub initializePrefs( $webName )

Resets all preference globals (for mod_perl compatibility), and reads
preferences from TWiki::TWikiPreferences, Main::TWikiPreferences, and
$webName::WebPreferences.

=cut

sub initializePrefs
{
    my( $theWebName ) = @_;
    
    TWiki::Prefs::PrefsCache::clearCache(); # for mod_perl compatibility

    $requestWeb = $theWebName;
    $globalPrefs = TWiki::Prefs::PrefsCache->new("global");
    $webPrefs{$requestWeb} = TWiki::Prefs::PrefsCache->new("web", $globalPrefs, $requestWeb);
    $requestPrefs = TWiki::Prefs::PrefsCache->new("copy", $webPrefs{$requestWeb});

    return;
}

# =========================

=pod

---+++ sub initializeUserPrefs( $userPrefsTopic )

Called after user is known (potentially by Plugin), this function reads
preferences from the user's personal topic.  The parameter is the topic to read
user-level preferences from (Generally "Main.CurrentUserName").

=cut

sub initializeUserPrefs
{
    my( $theWikiUserName ) = @_;

    $theWikiUserName = "Main.TWikiGuest" unless $theWikiUserName;

    if( $theWikiUserName =~ /^(.*)\.(.*)$/ ) {
	$requestPrefs = TWiki::Prefs::PrefsCache->new("request", $webPrefs{$requestWeb}, $TWiki::topicName, $2);
    }

    return;
}


# =========================

=pod

---+++ sub getPrefsFromTopic (  $web, $topic, $keyPrefix  )

Reads preferences from the topic at =$theWeb.$theTopic=, prefixes them with
=$theKeyPrefix= if one is provided, and adds them to the preference cache.

=cut

sub getPrefsFromTopic
{
    my ($web, $topic, $keyPrefix) = @_;
    $requestPrefs->loadPrefsFromTopic($web, $topic, $keyPrefix, 1);
}

# =========================

=pod

---+++ sub updateSetFromForm (  $meta, $text  )
Return value: $newText

If there are any settings "Set SETTING = value" in =$text= for a setting
that is set in form metadata in =$meta=, these are changed so that the
value in the =$text= setting is the same as the one set in the =$meta= form.
=$text= is not modified; rather, a new copy with these changes is returned.

=cut

sub updateSetFromForm
{
    my( $meta, $text ) = @_;
    my( $key, $value );
    my $ret = "";
    
    my %form = $meta->findOne( "FORM" );
    if( %form ) {
        my @fields = $meta->find( "FIELD" );
        foreach my $line ( split( /\n/, $text ) ) {
            foreach my $field ( @fields ) {
                $key = $field->{"name"};
                $value = $field->{"value"};
                my $attributes = $field->{"attributes"};
                if( $attributes && $attributes =~ /[S]/o ) {
                    $value =~ s/\n/\\\n/o;
                    TWiki::writeDebug( "updateSetFromForm: \"$key\"=\"$value\"" );
                    # Worry about verbatim?  Multi-lines
                    if ( $line =~ s/^(\t+\*\sSet\s$key\s\=\s*).*$/$1$value/g ) {
                        last;
                    }
                }
            }
            $ret .= "$line\n";
        }
    } else {
        $ret = $text;
    }
    
    return $ret;
}

# =========================

=pod

---+++ sub handlePreferencesTags ( $text )

Replaces %PREF% and %<nop>VAR{"pref"}% syntax in $text, modifying that parameter in-place.

=cut

sub handlePreferencesTags
{
    my $textRef = \$_[0];

    $requestPrefs->replacePreferencesTags($$textRef);
    
    if( $$textRef =~ /\%VAR{(.*?)}\%/ ) {
        # handle web specific variables
        $$textRef =~ s/\%VAR{(.*?)}\%/prvGetWebVariable($1)/ge;
    }
}

=pod

---+++ sub prvGetWebVariable( $attributeString )

Returns the value for a %<nop>VAR{"foo" web="bar"}% syntax, given the stuff inside the {}'s.

=cut

sub prvGetWebVariable
{
    my ( $attributeString ) = @_;
    
    my $key = &TWiki::extractNameValuePair( $attributeString );
    my $attrWeb = &TWiki::extractNameValuePair( $attributeString, "web" );
    if( $attrWeb =~ /%[A-Z]+%/ ) { # handle %MAINWEB%-type cases 
        &TWiki::handleInternalTags( $attrWeb, $requestWeb, "dummy" );
    }
    
    return getPreferencesValue( $key, $attrWeb);
}

=pod

---+++ sub formatAsFlag( $prefValue )

Returns 1 if the =$prefValue= is "on", and 0 otherwise.  "On" means set to
something with a true Perl-truth-value, with the special cases that "off" and
"no" are forced to false.  (Both of the latter are case-insensitive.)  Note
also that leading and trailing whitespace on =$prefValue= will be stripped
prior to this conversion.

=cut

sub formatAsFlag
{
    my ( $value  ) = @_;

    $value =~ s/^\s*(.*?)\s*$/$1/gi;
    $value =~ s/off//gi;
    $value =~ s/no//gi;
    if( $value ) {
        return 1;
    } else {
        return 0;
    }
}

=pod

---+++ sub formatAsNumber( $prefValue )

Converts the string =$prefValue= to a number.  First any whitespace and commas
are removed.  <em>L10N note: assumes thousands separator is comma and decimal
point is period.</em>  Then, if the first character is a zero, the value is
passed to oct(), which will interpret hex (0x prefix), octal (leading zero
only), or binary (0b prefix) numbers.  If the first character is a digit
greater than zero, the value is assumed to be a decimal number and returned.
If the =$prefValue= is empty or not a number, zero is returned.  Finally, if
=$prefValue= is undefined, an undefined value is returned.  <strong>Undefined
preferences are automatically converted to empty strings, and so this function
will always return zero for these, rather than 'undef'.</strong>

=cut

sub formatAsNumber
{
    my ( $strValue ) = @_;
    return undef unless defined( $strValue ); 
    
    $strValue =~ s/[,\s]+//g;    
    
    if ($strValue =~ /^0/) {
	return oct($strValue); # hex/octal/binary
    } elsif ($strValue =~ /^(\d|\.\d)/) {
	return $strValue;      # decimal
    } else {
	return 0;              # empty/non-numeric
    }
}

=pod

---+++ sub getPreferencesValue (  $theKey, $theWeb  )

Returns the value of the preference =$theKey=.  If =$theWeb= is also specified,
looks up the value with respect to that web instead of the current one; also,
in this case user/topic preferences are not considered.

In any case, if a plugin supports sessions and provides a value for =$theKey=,
this value overrides all preference settings in any web.

=cut

sub getPreferencesValue
{
    my ( $theKey, $theWeb ) = @_;

    my $sessionValue = &TWiki::getSessionValue( $theKey );
    if( defined( $sessionValue ) ) {
        return $sessionValue;
    }

    if ($theWeb) {
	if (!exists $webPrefs{$theWeb}) {
	    $webPrefs{$theWeb} = TWiki::Prefs::PrefsCache->new("web", $globalPrefs, $theWeb);
	}
	return $webPrefs{$theWeb}->getPreferenceValue($theKey);
    } else {
	return $requestPrefs->getPreferenceValue($theKey) if defined $requestPrefs;
	if (exists $webPrefs{$requestWeb}) {
	  return $webPrefs{$requestWeb}->getPreferenceValue($theKey); # user/topic prefs not yet init'd
	}
    }    
}

# =========================

=pod

---+++ sub getPreferencesFlag (  $theKey, $theWeb  )

Returns the preference =$theKey= from =$theWeb= as a flag.  See
=getPreferencesValue= for the semantics of the parameters, and
=[[#sub_formatAsFlag_prefValue][formatAsFlag]]= for the method of interpreting
a value as a flag.

=cut

sub getPreferencesFlag
{
    my ( $theKey, $theWeb ) = @_;

    my $value = getPreferencesValue( $theKey, $theWeb );
    return formatAsFlag($value);
}

=pod

---+++ sub getPreferencesNumber (  $theKey, $theWeb  )

Returns the preference =$theKey= from =$theWeb= as a flag.  See
=getPreferencesValue= for the semantics of the parameters, and
=[[#sub_formatAsNumber_prefValue][formatAsNumber]]= for the method of
interpreting a value as a number.

=cut

sub getPreferencesNumber
{
    my ( $theKey, $theWeb ) = @_;

    my $value = getPreferencesValue( $theKey, $theWeb );
    return formatAsNumber($value);
}

# =========================

1;

=end twiki

=cut

# EOF

