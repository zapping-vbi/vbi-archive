#
# Copyright (C) Motorola 2001 - All rights reserved
#
# TWiki extension that adds tags for the generation of tables of contents.
#
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
use strict;
use integer;

# Class of attribute sets
# An attribute set is a hash containing an entry for each parameter. The
# default parameter (quoted string) is named "__default__" in the hash.
{ package TWiki::Plugins::CommentPlugin::Attrs;

  # Parse a standard attribute string containing name=value pairs. The
  # value may be a word or a quoted string (no escapes!)
  sub new {
    my ( $class, $string ) = @_;
    my $this = {};

    if ( defined( $string ) ) {
      # name="value" pairs
      while ( $string =~ s/([a-z]\w+)\s*=\s*\"([^\"]*)\"//io ) {
        $this->{$1} = $2;
      }
      # name=value pairs
      while ( $string =~ s/([a-z]\w+)\s*=\s*([^\s,\}]*)//io ) {
        $this->{$1} = $2;
      }
      # simple quoted value with no name; only one allowed;
      # sets the key "__default__"
      if ( $string =~ s/\"(.*?)\"//o ) {
        $this->{"__default__"} = $1;
      }
      # simple name with no value (boolean)
      while ( $string =~ s/([a-z]\w+)\b//o ) {
        $this->{$1} = "on";
      }
    }
    return bless( $this, $class );
  }

  # PUBLIC Get an attr value; return undef if not set
  sub get {
    my ( $this, $attr ) = @_;
    return $this->{$attr};
  }

  # PUBLIC remove an attr value from the hash, return old value
  sub remove {
    my ( $this, $attr ) = @_;
    my $val = $this->{$attr};
    delete( $this->{$attr} ) if ( $val );
    return $val;
  }

  # PUBLIC generate a printed form for the hash, using standard
  # attribute syntax.
  sub toString {
    my $this = shift;
    my $key;
    my $ss = "";
    foreach $key ( keys %$this ) {
      if ( $key eq "__default__" ) {
	$ss = " \"" . $this->{$key} . "\"$ss";
      } else {
	$ss .= " $key=\"" . $this->{$key} . "\"";
      }
    }
    return "{$ss }";
  }

} # end of class Attrs

1;
