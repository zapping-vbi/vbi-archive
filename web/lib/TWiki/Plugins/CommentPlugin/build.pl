#!/usr/bin/perl -w
#
# Example build class. Copy this file to the equivalent place in your
# plugin and edit.
#
# Requires the environment variable TWIKI_SHARED to be
# set to point at the shared code repository
# Usage: ./build.pl [-n] [-v] [target]
# where [target] is the optional build target (build, test,
# install, release, uninstall), test is the default.`
# Two command-line options are supported:
# -n Don't actually do anything, just print commands
# -v Be verbose
#
# Read the comments at the top of lib/TWiki/Plugins/Build.pm for
# details of how the build process works, and what files you
# have to provide and where.
#
# Standard preamble
BEGIN {
  use File::Spec;
  my $cwd = `dirname $0`; chop($cwd);
  my $basedir = File::Spec->rel2abs("../../../..", $cwd);
  die "TWIKI_SHARED not set" unless ($ENV{TWIKI_SHARED});
  unshift @INC, "$ENV{TWIKI_SHARED}/lib";
  unshift @INC, $basedir;
  unshift @INC, $cwd;
}
use TWiki::Plugins::Build;

# Declare our build package
{ package CommentPluginBuild;

  @CommentPluginBuild::ISA = ( "TWiki::Plugins::Build" );

  sub new {
    my $class = shift;
    return bless( $class->SUPER::new( "CommentPlugin" ), $class );
  }
}

# Create the build object
$build = new CommentPluginBuild();

# Build the target on the command line, or the default target
$build->build($build->{target});

