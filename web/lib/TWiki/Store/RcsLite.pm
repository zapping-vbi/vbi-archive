# Module of TWiki Collaboration Platform, http://TWiki.org/
#
# Copyright (C) 2002 John Talintyre, john.talintyre@btinternet.com
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
#
# Functions used by both Rcs and RcsFile - they both inherit from this Class
#
# Simple interface to RCS.  Doesn't support:
#    branches
#    locking
#
# This modules doesn't know anything about the content of the topic e.g. it doesn't know
# about the meta data.
#
# FIXME:
#  - need to tidy up dealing with \n for differences
#  - still have difficulty on line ending at end of sequences, consequence of doing a line based diff
#  - most serious is when having multiple line ends on one seq but not other - this needs fixing
#  - tidyup us of 1. for revisions
#  - cleaner dealing with errors/warnings

=begin twiki

---+ TWiki::Store::RcsLite Module

This module implements rcs (without calling it)

=cut

package TWiki::Store::RcsLite;

use TWiki::Store::RcsFile;
@ISA = qw(TWiki::Store::RcsFile);

use strict;
use Algorithm::Diff;
use FileHandle;
use TWiki;

my $DIFF_DEBUG = 0;
my $DIFFEND_DEBUG = 0;

# ======================
=pod

---++ sub new (  $proto, $web, $topic, $attachment, %settings  )

Not yet documented.

=cut to implementation

sub new
{
   my( $proto, $web, $topic, $attachment, %settings ) = @_;
   my $class = ref($proto) || $proto;
   my $self = TWiki::Store::RcsFile->new( $web, $topic, $attachment, %settings );
   bless( $self, $class );
   $self->_init();
   $self->{head} = 0;
   return $self;
}

# ======================
=pod

---++ sub _trace ()

Not yet documented.

=cut to implementation

sub _trace
{
#   my( $text ) = @_;
#   TWiki::writeDebug( "RcsLite $text" );
}


# Process an RCS file

# File format information:
#
#rcstext    ::=  admin {delta}* desc {deltatext}*
#admin      ::=  head {num};
#                { branch   {num}; }
#                access {id}*;
#                symbols {sym : num}*;
#                locks {id : num}*;  {strict  ;}
#                { comment  {string}; }
#                { expand   {string}; }
#                { newphrase }*
#delta      ::=  num
#                date num;
#                author id;
#                state {id};
#                branches {num}*;
#                next {num};
#                { newphrase }*
#desc       ::=  desc string
#deltatext  ::=  num
#                log string
#                { newphrase }*
#                text string
#num        ::=  {digit | .}+
#digit      ::=  0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
#id         ::=  {num} idchar {idchar | num }*
#sym        ::=  {digit}* idchar {idchar | digit }*
#idchar     ::=  any visible graphic character except special
#special    ::=  $ | , | . | : | ; | @
#string     ::=  @{any character, with @ doubled}*@
#newphrase  ::=  id word* ;
#word       ::=  id | num | string | :
#
# Identifiers are case sensitive. Keywords are in lower case only. The sets of keywords and 
# identifiers can overlap. In most environments RCS uses the ISO 8859/1 encoding: 
# visible graphic characters are codes 041-176 and 240-377, and white space characters are 
# codes 010-015 and 040. 
#
# Dates, which appear after the date keyword, are of the form Y.mm.dd.hh.mm.ss, 
# where Y is the year, mm the month (01-12), dd the day (01-31), hh the hour (00-23), 
# mm the minute (00-59), and ss the second (00-60). Y contains just the last two digits of 
# the year for years from 1900 through 1999, and all the digits of years thereafter. 
# Dates use the Gregorian calendar; times use UTC. 
#
# The newphrase productions in the grammar are reserved for future extensions to the format 
# of RCS files. No newphrase will begin with any keyword already in use. 

# ======================
=pod

---++ sub _readTo (  $file, $char  )

Not yet documented.

=cut to implementation

sub _readTo
{
    my( $file, $char ) = @_;
    my $buf = "";
    my $ch;
    my $space = 0;
    my $string = "";
    my $state = "";
    while( read( $file, $ch, 1 ) ) {
       if( $ch eq "@" ) {
          if( $state eq "@" ) {
             $state = "e";
             next;
          } elsif( $state eq "e" ) {
             $state = "@";
             $string .= "@";
             next;
          } else {
             $state = "@";
             next;
          }
       } else {
          if( $state eq "e" ) {
             $state = "";
             if( $char eq "@" ) {
                last;
             }
             # End of string
          } elsif ( $state eq "@" ) {
             $string .= $ch;
             next;
          }
       }
       
       if( $ch =~ /\s/ ) {
          if( length( $buf ) == 0 ) {
              next;
          } elsif( $space ) {
              next;
          } else {
              $space = 1;
              $ch = " ";
          }
       } else {
          $space = 0;
       }
       $buf .= $ch;
       if( $ch eq $char ) {
           last;
       }
    }
    return( $buf, $string );
}

# ======================
# Called by routines that must make sure RCS file has been read in
=pod

---++ sub _ensureProcessed (  $self  )

Not yet documented.

=cut to implementation

sub _ensureProcessed
{
    my( $self ) = @_;
    if( ! $self->{where} ) {
        $self->_process();
    }
}

# ======================
# Read in the whole RCS file
=pod

---++ sub _process (  $self  )

Not yet documented.

=cut to implementation

sub _process
{
    my( $self ) = @_;
    my $rcsFile = $self->rcsFile();
    if( ! -e $rcsFile ) {
        $self->{where} = "nofile";
        return;
    }
    my $fh = new FileHandle;
    if( ! $fh->open( $rcsFile ) ) {
        $self->_warn( "Couldn't open file $rcsFile" );
        $self->{where} = "nofile";
        return;
    }
    my $where = "admin.head";
    binmode( $fh );
    my $lastWhere = "";
    my $going = 1;
    my $term = ";";
    my $string = "";
    my $num = "";
    my $headNum = "";
    my @date = ();
    my @author = ();
    my @log = ();
    my @text = ();
    my $dnum = "";
    while( $going ) {
       ($_, $string) = _readTo( $fh, $term );
       last if( ! $_ );
      
       my $lastWhere = $where;
       #print "\"$where -- $_\"\n";
       if( $where eq "admin.head" ) {
          if( /^head\s+([0-9]+)\.([0-9]+);$/o ) {
             die( "Only support start of version being 1" ) if( $1 ne "1" );
             $headNum = $2;
             $where = "admin.access"; # Don't support branch
          } else {
             last;
          }
       } elsif( $where eq "admin.access" ) {
          if( /^access\s*(.*);$/o ) {
             $where = "admin.symbols";
             $self->{access} = $1;
          } else {
             last;
          }
       } elsif( $where eq "admin.symbols" ) {
          if( /^symbols(.*);$/o ) {
             $where = "admin.locks";
             $self->{symbols} = $1;
          } else {
             last;
          }
       } elsif( $where eq "admin.locks" ) {
          if( /^locks.*;$/o ) {
             $where = "admin.postLocks";
          } else {
             last;
          }
       } elsif( $where eq "admin.postLocks" ) {
          if( /^strict\s*;/o ) {
             $where = "admin.postStrict";
          }
       } elsif( $where eq "admin.postStrict" &&
                /^comment\s.*$/o ) {
             $where = "admin.postComment";
             $self->{comment} = $string;
       } elsif( ( $where eq "admin.postStrict" || $where eq "admin.postComment" )  &&
                /^expand\s/o ) {
             $where = "admin.postExpand";
             $self->{expand} = $string;         
       } elsif( $where eq "admin.postStrict" || $where eq "admin.postComment" || 
                $where eq "admin.postExpand" || $where eq "delta.date") {
          if( /^([0-9]+)\.([0-9]+)\s+date\s+(\d\d(\d\d)?(\.\d\d){5}?);$/o ) {
             $where = "delta.author";
             $num = $2;
             $date[$num] = TWiki::Store::RcsFile::_rcsDateTimeToEpoch ($3 );
          }
       } elsif( $where eq "delta.author" ) {
          if( /^author\s+(.*);$/o ) {
             $author[$num] = $1;
             if( $num == 1 ) {
                $where = "desc";
                $term = "@";
             } else {
                $where = "delta.date";
             }
          }
       } elsif( $where eq "desc" ) {
          if( /desc\s*$/o ) {
             $self->{"description"} = $string;
             $where = "deltatext.log";
          }
       } elsif( $where eq "deltatext.log" ) {
          if( /\d+\.(\d+)\s+log\s+$/o ) {
             $dnum = $1;
             $log[$dnum] = $string;
             $where = "deltatext.text";
          }
       } elsif( $where eq "deltatext.text" ) {
          if( /text\s*$/o ) {
             $where = "deltatext.log";
             $text[$dnum] = $string;
             if( $dnum == 1 ) {
                $where = "done";
                last;
             }
          }
       }
    }
    
    $self->{"head"} = $headNum;
    $self->{"author"} = \@author;
    $self->{"date"} = \@date;   #TODO: i hitnk i need to make this into epochSecs
    $self->{"log"} = \@log;
    $self->{"delta"} = \@text;
    $self->{"status"} = $dnum;
    $self->{where} = $where;
    
    close( $fh );
}

# ======================
=pod

---++ sub _formatString (  $str  )

Not yet documented.

=cut to implementation

sub _formatString
{
    my( $str ) = @_;
    $str =~ s/@/@@/go;
    return "\@$str\@";
}

# ======================
# Write content of the RCS file
=pod

---++ sub _write (  $self, $file  )

Not yet documented.

=cut to implementation

sub _write
{
    my( $self, $file ) = @_;
    
    # admin
    print $file "head\t1." . $self->numRevisions() . ";\n";
    print $file "access" . $self->access() . ";\n";
    print $file "symbols" . $self->{symbols} . ";\n";
    print $file "locks; strict;\n";
    printf $file "comment\t%s;\n", ( _formatString( $self->comment() ) );
    printf $file "expand\t@%s@;\n", ( $self->{expand} ) if ( $self->{expand} );
    
    print $file "\n";
    
    # delta
    for( my $i=$self->numRevisions(); $i>0; $i--) {
       printf $file "\n1.%d\ndate\t%s;\tauthor %s;\tstate Exp;\nbranches;\n", 
              ($i, TWiki::Store::RcsFile::_epochToRcsDateTime( ${$self->{date}}[$i] ), $self->author($i) );
       if( $i == 1 ) {
           print $file "next\t;\n";
       } else {
           printf $file "next\t1.%d;\n", ($i - 1);
       }
    }
    
    printf $file "\n\ndesc\n%s\n\n", ( _formatString( $self->description() ) );
    
    for( my $i=$self->numRevisions(); $i>0; $i--) {
       printf $file "\n1.$i\nlog\n%s\ntext\n%s\n",
              ( _formatString( $self->log($i) ), _formatString( $self->delta($i) ) );
    }
}

# ======================
=pod

---++ sub _binaryChange (  $self  )

Not yet documented.

=cut to implementation

sub _binaryChange
{
   my( $self ) = @_;
   # Nothing to be done but note for re-writing
   $self->{expand} = "b" if( $self->{binary} );
   # FIXME: unless we have to not do diffs for binary files
}

# ======================
=pod

---++ sub numRevisions (  $self  )

Not yet documented.

=cut to implementation

sub numRevisions
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return $self->{"head"};
}

# ======================
=pod

---++ sub access (  $self  )

Not yet documented.

=cut to implementation

sub access
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return $self->{access};
}

# ======================
=pod

---++ sub comment (  $self  )

Not yet documented.

=cut to implementation

sub comment
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return $self->{"comment"};
}

# ======================
=pod

---++ sub date (  $self, $version  )

Not yet documented.
| $date | in epoch seconds |

=cut to implementation

sub date
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    my $date = ${$self->{"date"}}[$version];
    if( $date ) {
#        $date = TWiki::Store::RcsFile::_rcsDateTimeToEpoch( $date );
    } else {
        $date = 0;#MMMM, should this be 0, or now()?
    }
    return $date;
}

# ======================
=pod

---++ sub description (  $self  )

Not yet documented.

=cut to implementation

sub description
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return $self->{"description"};
}

# ======================
=pod

---++ sub author (  $self, $version  )

Not yet documented.

=cut to implementation

sub author
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    return ${$self->{"author"}}[$version];
}

# ======================
=pod

---++ sub log (  $self, $version  )

Not yet documented.

=cut to implementation

sub log
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    return ${$self->{"log"}}[$version];
}

# ======================
=pod

---++ sub delta (  $self, $version  )

Not yet documented.

=cut to implementation

sub delta
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    return ${$self->{"delta"}}[$version];
}

# ======================
=pod

---++ sub addRevision (  $self, $text, $log, $author, $date  )

Not yet documented.
| $date | in epoch seconds |

=cut to implementation

sub addRevision
{
    my( $self, $text, $log, $author, $date ) = @_;
    _trace( "::addRevision date=\"$date\"" );
    $self->_ensureProcessed();
    
    $self->_save( $self->file(), \$text );
    $text = $self->_readFile( $self->{file} ) if( $self->{attachment} );
    my $head = $self->numRevisions();
    if( $head ) {
        my $delta = _diffText( \$text, \$self->delta($head), "" );
        ${$self->{"delta"}}[$head] = $delta;
    }   
    $head++;
    ${$self->{"delta"}}[$head] = $text;
    $self->{"head"} = $head;
    ${$self->{"log"}}[$head] = $log;
    ${$self->{"author"}}[$head] = $author;
    if( $date ) {
 #       $date =~ s/[ \/\:]/\./go;
    } else {
        $date = time();
    }
#    $date = TWiki::Store::RcsFile::_epochToRcsDateTime( $date );


    _trace("::addRevision date now=\"$date\"" );
    ${$self->{"date"}}[$head] = $date;

    return $self->_writeMe();
}

# ======================
=pod

---++ sub _writeMe (  $self  )

Not yet documented.

=cut to implementation

sub _writeMe
{
    my( $self ) = @_;
    my $dataError = "";
    my $out = new FileHandle;
    
    chmod( 0644, $self->rcsFile()  ); # FIXME move permission to config or similar
    if( ! $out->open( "> " . $self->rcsFile() ) ) {
       $dataError = "Problem opening " . $self->rcsFile() . " for writing";
    } else {
       binmode( $out );
       $self->_write( $out );
       close( $out );
    }
    chmod( 0444, $self->rcsFile()  ); # FIXME as above
    return $dataError;    
}

# ======================
=pod

---++ sub replaceRevision (  $self, $text, $comment, $user, $date  )

Not yet documented.
# Replace the top revision
# Return non empty string with error message if there is a problem
| $date | is on epoch seconds |

=cut to implementation

sub replaceRevision
{
    my( $self, $text, $comment, $user, $date ) = @_;
    _trace( "::replaceRevision date=\"$date\"" );
    $self->_ensureProcessed();
    $self->_delLastRevision();
    $self->addRevision( $text, $comment, $user, $date );    
}

# ======================
# Delete the last revision - do nothing if there is only one revision
=pod

---++ sub deleteRevision (  $self  )

Not yet documented.

=cut to implementation

sub deleteRevision
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return "" if( $self->numRevisions() <= 1 );
    $self->_delLastRevision();
    return $self->_writeMe();
}

# ======================
=pod

---++ sub _delLastRevision (  $self  )

Not yet documented.

=cut to implementation

sub _delLastRevision
{
    my( $self ) = @_;
    my $numRevisions = $self->numRevisions();
    if( $numRevisions > 1 ) {
        # Need to recover text for last revision
        my $lastText = $self->getRevision( $numRevisions - 1 );
        $numRevisions--;
        $self->{"delta"}->[$numRevisions] = $lastText;
    } else {
        $numRevisions--;
    }
    $self->{head} = $numRevisions;
}

# ======================
=pod

---++ sub revisionDiff (  $self, $rev1, $rev2  )

Not yet documented.

=cut to implementation

sub revisionDiff
{
    my( $self, $rev1, $rev2 ) = @_;
    $self->_ensureProcessed();
    my $text1 = $self->getRevision( $rev1 );
    $text1 =~ s/%META:TOPICINFO{[^\n]*}%\n//o;
    my $text2 = $self->getRevision( $rev2 );
    $text2 =~ s/%META:TOPICINFO{[^\n]*}%\n//o;
    my $diff = _diffText( \$text1, \$text2, "diff" );
    return ("", $diff);
}

# ======================
=pod

---++ sub getRevision (  $self, $version  )

Not yet documented.

=cut to implementation

sub getRevision
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    my $head = $self->numRevisions();
    if( $version == $head ) {
        return $self->delta( $version );
    } else {
        my $headText = $self->delta( $head );
        my @text = _mySplit( \$headText, 1 );
        return $self->_patchN( \@text, $head-1, $version );
    }
}

# ======================
# If revision file is missing, information based on actual file is returned.
# Date is in epoch based seconds
=pod

---++ sub getRevisionInfo (  $self, $version  )

Not yet documented.

=cut to implementation

sub getRevisionInfo
{
    my( $self, $version ) = @_;
    $self->_ensureProcessed();
    $version = $self->numRevisions() if( ! $version );
    if( $self->{where} && $self->{where} ne "nofile" ) {
        return ( "", $version, $self->date( $version ), $self->author( $version ), $self->comment( $version ) );
    } else {
        return $self->_getRevisionInfoDefault();
    }
}


# ======================
# Apply delta (patch) to text.  Note that RCS stores reverse deltas, the is text for revision x
# is patched to produce text for revision x-1.
# It is fiddly dealing with differences in number of line breaks after the end of the
# text.
=pod

---++ sub _patch (  $text, $delta  )

Not yet documented.

=cut to implementation

sub _patch
{
   # Both params are references to arrays
   my( $text, $delta ) = @_;
   my $adj = 0;
   my $pos = 0;
   my $last = "";
   my $d;
   my $extra = "";
   my $max = $#$delta;
   while( $pos <= $max ) {
       $d = $delta->[$pos];
       if( $d =~ /^([ad])(\d+)\s(\d+)\n(\n*)/ ) {
          $last = $1;
          $extra = $4;
          my $offset = $2;
          my $length = $3;
          if( $last eq "d" ) {
             my $start = $offset + $adj - 1;
             my @removed = splice( @$text, $start, $length );
             $adj -= $length;
             $pos++;
          } elsif( $last eq "a" ) {
             my @toAdd = @${delta}[$pos+1..$pos+$length];
             if( $extra ) {
                 if( @toAdd ) {
                     $toAdd[$#toAdd] .= $extra;
                 } else {
                     @toAdd = ( $extra );
                 }
             }
             splice( @$text, $offset + $adj, 0, @toAdd );
             $adj += $length;
             $pos += $length + 1;
          }
       } else {
          warn( "wrong! - should be \"[ad]<num> <num>\" and was: \"" . $d . "\"\n\n" ); #FIXME remove die
          return;
       }
   }
}


# ======================
=pod

---++ sub _patchN (  $self, $text, $version, $target  )

Not yet documented.

=cut to implementation

sub _patchN
{
    my( $self, $text, $version, $target ) = @_;
    my $deltaText= $self->delta( $version );
    my @delta = _mySplit( \$deltaText );
    _patch( $text, \@delta );
    if( $version == $target ) {
        return join( "", @$text );
    } else {
        return $self->_patchN( $text, $version-1, $target );
    }
}

# ======================
# Split and make sure we have trailing carriage returns
=pod

---++ sub _mySplit (  $text, $addEntries  )

Not yet documented.

=cut to implementation

sub _mySplit
{
    my( $text, $addEntries ) = @_;

    my $ending = "";
    if( $$text =~ /(\n+)$/o ) {
        $ending = $1;
    }

    my @list = split( /\n/o, $$text );
    for( my $i = 0; $i<$#list; $i++ ) {
        $list[$i] .= "\n";
    }

    if( $ending ) {
        if( $addEntries ) {
            my $len = length($ending);
            if( @list ) {
               $len--;
               $list[$#list] .= "\n";
            }
            for( my $i=0; $i<$len; $i++ ) {
                push @list, ("\n");
            }
        } else {
            if( @list ) {
                $list[$#list] .= $ending;
            } else {
                @list = ( $ending );
            }
        }
    }
    # TODO: deal with Mac style line ending??

    return @list; # FIXME would it be more efficient to return a reference?
}

# ======================
# Way of dealing with trailing \ns feels clumsy
=pod

---++ sub _diffText (  $new, $old, $type  )

Not yet documented.

=cut to implementation

sub _diffText
{
    my( $new, $old, $type ) = @_;
    
    my @lNew = _mySplit( $new );
    my @lOld = _mySplit( $old );
    return _diff( \@lNew, \@lOld, $type );
}

# ======================
=pod

---++ sub _lastNoEmptyItem (  $items  )

Not yet documented.

=cut to implementation

sub _lastNoEmptyItem
{
   my( $items ) = @_;
   my $pos = $#$items;
   my $count = 0;
   my $item;
   while( $pos >= 0 ) {
      $item = $items->[$pos];
      last if( $item );
      $count++;
      $pos--;
   }
   return( $pos, $count );
}

# ======================
# Deal with trailing carriage returns - Algorithm doesn't give output that RCS format is too happy with
=pod

---++ sub _diffEnd (  $new, $old, $type  )

Not yet documented.

=cut to implementation

sub _diffEnd
{
   my( $new, $old, $type ) = @_;
   return if( $type ); # FIXME
   
   my( $posNew, $countNew ) = _lastNoEmptyItem( $new );
   my( $posOld, $countOld ) = _lastNoEmptyItem( $old );

   return "" if( $countNew == $countOld );
   
   if( $DIFFEND_DEBUG ) {
     print( "countOld, countNew, posOld, posNew, lastOld, lastNew, lenOld: " .
            "$countOld, $countNew, $posOld, $posNew, " . $#$old . ", " . $#$new . 
            "," . @$old . "\n" );
   }
   
   $posNew++;
   my $toDel = ( $countNew < 2 ) ? 1 : $countNew;
   my $startA = @${new} - ( ( $countNew > 0 ) ? 1 : 0 );
   my $toAdd = ( $countOld < 2 ) ? 1 : $countOld;
   my $theEnd = "d$posNew $toDel\na$startA $toAdd\n";
   for( my $i=$posOld; $i<@${old}; $i++ ) {
       $theEnd .= $old->[$i] ? $old->[$i] : "\n";
   }
   
   for( my $i=0; $i<$countNew; $i++ ) {pop @$new;}
   pop @$new;
   for( my $i=0; $i<$countOld; $i++ ) {pop @$old;}
   pop @$old;
   
   print "--$theEnd--\n"  if( $DIFFEND_DEBUG );
      
   return $theEnd;
}

# ======================
# no type means diff for putting in rcs file, diff means normal diff output
=pod

---++ sub _diff (  $new, $old, $type  )

Not yet documented.

=cut to implementation

sub _diff
{
    my( $new, $old, $type ) = @_;
    # Work out diffs to change new to old, params are refs to lists
    my $diffs = Algorithm::Diff::diff( $new, $old );

    my $adj = 0;
    my @patch = ();
    my @del = ();
    my @ins = ();
    my $out = "";
    my $start = 0;
    my $start1;
    my $chunkSign = "";
    my $count = 0;
    my $numChunks = @$diffs;
    my $last = 0;
    my $lengthNew = @$new - 1;
    foreach my $chunk ( @$diffs ) {
       $count++;
       print "[\n" if( $DIFF_DEBUG );
       $chunkSign = "";
       my @lines = ();
       foreach my $line ( @$chunk ) {
           my( $sign, $pos, $what ) = @$line;
           print "$sign $pos \"$what\"\n" if( $DIFF_DEBUG );
           if( $chunkSign ne $sign && $chunkSign ne "") {
               if( $chunkSign eq "-" && $type eq "diff" ) {
                  # Might be change of lines
                  my $chunkLength = @$chunk;
                  my $linesSoFar = @lines;
                  if( $chunkLength == 2 * $linesSoFar ) {
                     $chunkSign = "c";
                     $start1 = $pos;
                  }
               }
               $adj += _addChunk( $chunkSign, \$out, \@lines, $start, $adj, $type, $start1, $last ) if( $chunkSign ne "c" );
           }
           if( ! @lines ) {
               $start = $pos;
           }
           $chunkSign = $sign if( $chunkSign ne "c" );
           push @lines, ( $what );
       }

       $last = 1 if( $count == $numChunks );
       if( $last && $chunkSign eq "+" ) {
           my $endings = 0;
           for( my $i=$#$old; $i>=0; $i-- ) {
               if( $old->[$i] ) {
                   last;
               } else {
                   $endings++;
               }
           }
           my $has = 0;
           for( my $i=$#lines; $i>=0; $i-- ) {
               if( $lines[$i] ) {
                   last;
               } else {
                   $has++;
               }
           }
           for( my $i=0; $i<$endings-$has; $i++ ) {
               push @lines, ("");
           }
       }
       $adj += _addChunk( $chunkSign, \$out, \@lines, $start, $adj, $type, $start1, $last, $lengthNew );
       print "]\n" if( $DIFF_DEBUG );
    }
    # Make sure we have the correct number of carriage returns at the end
    
    print "pre end: \"$out\"\n" if( $DIFFEND_DEBUG );
    return $out; # . $theEnd;
}


# ======================
=pod

---++ sub _range (  $start, $end  )

Not yet documented.

=cut to implementation

sub _range
{
   my( $start, $end ) = @_;
   if( $start == $end ) {
      return "$start";
   } else {
      return "$start,$end";
   }
}

# ======================
=pod

---++ sub _addChunk (  $chunkSign, $out, $lines, $start, $adj, $type, $start1, $last, $newLines  )

Not yet documented.

=cut to implementation

sub _addChunk
{
   my( $chunkSign, $out, $lines, $start, $adj, $type, $start1, $last, $newLines ) = @_;
   my $nLines = @$lines;
   if( $lines->[$#$lines] =~ /(\n+)$/o ) {
      $nLines += ( ( length( $1 ) == 0 ) ? 0 : length( $1 ) -1 );
   }
   if( $nLines > 0 ) {
       print "addChunk chunkSign=$chunkSign start=$start adj=$adj type=$type start1=$start1 " .
             "last=$last newLines=$newLines nLines=$nLines\n" if( $DIFF_DEBUG );
       $$out .= "\n" if( $$out && $$out !~ /\n$/o );
       if( $chunkSign eq "c" ) {
          $$out .= _range( $start+1, $start+$nLines/2 );
          $$out .= "c";
          $$out .= _range( $start1+1, $start1+$nLines/2 );
          $$out .= "\n";
          $$out .= "< " . join( "< ", @$lines[0..$nLines/2-1] );
          $$out .= "\n" if( $lines->[$nLines/2-1] !~ /\n$/o );
          $$out .= "---\n";
          $$out .= "> " . join( "> ", @$lines[$nLines/2..$nLines-1] );
          $nLines = 0;
       } elsif( $chunkSign eq "+" ) {
          if( $type eq "diff" ) {
              $$out .= $start-$adj . "a";
              $$out .= _range( $start+1, $start+$nLines ) . "\n";
              $$out .= "> " . join( "> ", @$lines );
          } else {
              $$out .= "a";
              $$out .= $start-$adj;
              $$out .= " $nLines\n";
              $$out .= join( "", @$lines );
          }
       } else {
          print "Start nLines newLines: $start $nLines $newLines\n" if( $DIFF_DEBUG );
          if( $type eq "diff" ) {
              $$out .= _range( $start+1, $start+$nLines );
              $$out .= "d";
              $$out .= $start + $adj . "\n";
              $$out .= "< " . join( "< ", @$lines );
          } else {
              $$out .= "d";
              $$out .= $start+1;
              $$out .= " $nLines";
              $$out .= "\n" if( $last );
          }
          $nLines *= -1;
       }
       @$lines = ();
   }
   return $nLines;
}



# ======================
=pod

---++ sub validTo (  $self  )

Not yet documented.

=cut to implementation

sub validTo
{
    my( $self ) = @_;
    $self->_ensureProcessed();
    return $self->{"status"};
}

1;
