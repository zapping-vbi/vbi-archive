# Comment TWiki plugin
# Original author David Weller, reimplemented by Peter Masiar
# and again by Crawford Currie
#
# This version Copyright (C) 2004 Crawford Currie
#
use strict;

use TWiki::Plugins::CommentPlugin::Attrs;
use TWiki::Plugins::CommentPlugin::Templates;

{ package CommentPlugin::Comment;

  use vars qw( $inSave ); # recursion block

  # PUBLIC save the given comment. Note that this method is protected against
  # recursion. This is because the expansion of variables in the template
  # will invoke the commonTagsHandler in the plugin and therefore
  # this method.
  sub save {
    my ( $web, $topic, $query ) = @_;

    return if ( $inSave );

    my $url;

    my $wikiUserName = &TWiki::Func::getWikiUserName();
    if( ! TWiki::Func::checkAccessPermission( "change", $wikiUserName, "",
											  $topic, $web ) ) {
      # user has no permission to change the topic
      $url = TWiki::Func::getOopsUrl( $web, $topic, "oopsaccesschange" );
    } else {
      my( $oopsUrl, $lockUser ) =
		TWiki::Func::checkTopicEditLock( $web, $topic );
      if( $lockUser ) {
		$url = $oopsUrl;
      } else {
		TWiki::Func::setTopicEditLock( $web, $topic, 1 );
		
		$inSave = 1;
		my $error = _buildNewTopic( $web, $topic, $query );
		$inSave = 0;

		TWiki::Func::setTopicEditLock( $web, $topic, 0 );
		if( $error ) {
		  $url = $error;
		} else {
		  $url = TWiki::Func::getViewUrl( $web, $topic );
		}
      }
    }

    # shouldn't need this, but seem to.
    $query->param( -name=>'comment_action', -value=>'none' );
    TWiki::Func::redirectCgiQuery( $query, $url );
  }

  # PUBLIC STATIC convert COMMENT statements to form prompts
  sub prompt {
    #my ( $previewing, $text, $topic, $web ) = @_;

    my $defaultType = 
      TWiki::Func::getPreferencesValue("COMMENTPLUGIN_DEFAULT_TYPE") ||
	  "below";

    my $message = "";
    # Is commenting disabled?
    my $disable = "";
    if ( $_[0] ) {
      # We are in Preview mode
      $message  = "(Edit - Preview)";
      $disable = "disabled";
    }

    my $idx = 0;
    $_[1] =~ s/%COMMENT({.*?})?%/&_handleInput($1,$_[2],$_[3],\$idx,$message,$disable,$defaultType)/ego;
  }

  # PRIVATE generate an input form for a %COMMENT tag
  sub _handleInput {
    my ( $attributes, $topic, $web, $pidx, $message,
	 $disable, $defaultType ) = @_;

    $attributes =~ s/^{(.*)}$/$1/o if ( $attributes );
    my $attrs = new TWiki::Plugins::CommentPlugin::Attrs( $attributes );

    my $type =
      $attrs->remove( "type" ) || $attrs->remove( "mode" ) || $defaultType;
    my $silent = $attrs->remove( "nonotify" );
    my $location = $attrs->remove( "location" );

    # clean off whitespace
    $type =~ m/(\S*)/o;
    $type = $1;

    # Expand the template in the context of the web where the comment
    # box is (not the target of the comment!)
    my $input = _getTemplate( "PROMPT:$type", $topic, $web );

    return $input if $input =~ m/^%RED%/so;

    # Expand special attributes as required
    $input =~ s/%([a-z]\w+)\|(.*?)%/&_expandPromptParams($1, $2, $attrs)/iego;

    # see if this comment is targeted at a different topic, and
    # change the url if it is.
    my $anchor = undef;
    my $target = $attrs->remove( "target" );
    if ( $target ) {
      # extract web and anchor
      if ( $target =~ s/^(\w+)\.//o ) {
	$web = $1;
      }
      if ( $target =~ s/(#\w+)$//o ) {
	$anchor = $1;
      }
      if ( $target ne "" ) {
	$topic = $target;
      }
    }

    my $url = "";
    if ( $disable eq "" ) {
	  # invoke viewauth so we get authentication. When viewauth is run,
	  # the 'save' method in here will be invoked as the commenTagsHandler
	  # is run as flagged by the "comment_action" parameter.
      $url = TWiki::Func::getScriptUrl( "%INTURLENCODE{$web}%",
					"%INTURLENCODE{$topic}%",
					"viewauth" );

      my ( $oopsUrl, $lockUser, $lockTime ) =
	TWiki::Func::checkTopicEditLock( $web, $topic );

      if ( $lockUser ) {
	$message = "Commenting is locked out by <nop>$lockUser for at least $lockTime more minutes";
	$disable = "disabled";
      }
    }

    if ( $input !~ m/^%RED%/o ) {
      $input =~ s/%DISABLED%/$disable/go;
      $input =~ s/%MESSAGE%/$message/g;
      my $n = $$pidx + 0;
	
      $input = "<form name=\"${disable}$type$n\" " .
	"action=\"$disable$url\" method=\"${disable}post\">\n$input";
      if ( $disable eq "" ) {
	$input .= "<input name=\"comment_action\" " .
	"type=\"hidden\" value=\"save\" />\n";
	$input .= "<input name=\"comment_type\" " .
	  "type=\"hidden\" value=\"$type\" />\n";
	# remember to unlock the page
	$input .= "<input name=\"unlock\" " .
	"type=\"hidden\" value=\"1\" />\n";
	if( defined( $silent )) {
	  $input .= "<input name=\"comment_quietly\" " .
	    "type=\"hidden\" value=\"1\" />\n";
	}
	if ( $location ) {
	  $input .= "<input name=\"comment_location\" " .
	    "type=\"hidden\" value=\"$location\" />\n";
	} elsif ( $anchor ) {
	  $input .= "<input name=\"comment_anchor\" " .
	    "type=\"hidden\" value=\"$anchor\" />\n";
	} else {
	  $input .= "<input name=\"comment_index\" " .
	    "type=\"hidden\" value=\"$$pidx\" />\n";
	}
      }
      $input .= "</form>\n";
    }
    $$pidx++;
    return $input;
  }

  # PRIVATE get the given template and do standard expansions
  sub _getTemplate {
    my ( $name, $topic, $web ) = @_;

    # Get the templates.
    my $templateFile =
      TWiki::Func::getPreferencesValue("COMMENTPLUGIN_TEMPLATES") ||
	"comments";

    my $templates;

	if ( $TWiki::Plugins::VERSION < 1.020 ) {
	  $templates = CairoCompatibilityModule::readTemplate( $templateFile );
	} else {
	  $templates = TWiki::Store::readTemplate( $templateFile );
	}
    if (! $templates ) {
      TWiki::Func::writeWarning("No such template file '$templateFile'");
      return;
    }

    my $t =
      TWiki::Func::expandCommonVariables( "%TMPL:P{$name}%", $topic, $web );

    return "%RED%No such template def %<nop>TMPL:DEF{$name}%%ENDCOLOR%"
      unless ( defined($t) && $t ne "" );

    return $t;
  }

  # PRIVATE expand special %param|default% parameters in PROMPT template
  sub _expandPromptParams {
    my ( $name, $default, $attrs ) = @_;

    my $val = $attrs->get( $name );
    return $val if defined( $val );
    return $default;
  }

  # PRIVATE STATIC Performs comment insertion in the topic.
  sub _buildNewTopic {
    my ( $web, $topic, $query ) = @_;

    my $type = $query->param( 'comment_type' );
    my $index = $query->param( 'comment_index' );
    my $anchor = $query->param( 'comment_anchor' );
    my $location = $query->param( 'comment_location' );
    my $silent = $query->param( 'comment_quietly' );

    my $output = _getTemplate( "OUTPUT:$type", $topic, $web );
    if ( $output =~ m/^%RED%/o ) {
      return $output;
    }

    # Expand the template
    $output =~ s/%POS:(.*?)%//go;
    my $position = $1 || "AFTER";

    # Expand common variables in the template, but don't expand other
    # tags.
	if ( $TWiki::Plugins::VERSION < 1.020 ) {
	  eval "use TWiki::Plugins::CairoCompatibilityModule;";
	}
	$output = TWiki::expandVariablesOnTopicCreation($output);

	# CODE_SMELL: This should be part of TWiki::expandVariablesOnTopicCreation
    my @t = gmtime();
    my $now = sprintf( "%02d:%02d:%02d", $t[2], $t[1], $t[0] );
    $output =~ s/%TIME%/$now/go;

    my $bloody_hell = TWiki::Func::readTopicText( $web, $topic, undef, 1 );
    my $premeta = "";
    my $postmeta = "";
    my $inpost = 0;
    my $text = "";
    foreach my $line ( split( /\n/, $bloody_hell )) {
      if( $line =~ /^(%META:[^{]+{[^}]*}%)/ ) {
		if ( $inpost) {
		  $postmeta .= "$1\n";
		} else {
		  $premeta .= "$1\n";
		}
      } else {
		$text .= "$line\n";
		$inpost = 1;
      }
    }

    if ( $position eq "TOP" ) {
      $text = "$output$text";
    } elsif ( $position eq "BOTTOM" ) {
      $text .= "$output";
    } else {
      if ( $location ) {
		if ( $position eq "BEFORE" ) {
		  $text =~ s/($location)/$output$1/m;
		} else { # AFTER
		  $text =~ s/($location)/$1$output/m;
		}
      } elsif ( $anchor ) {
		# position relative to anchor
		if ( $position eq "BEFORE" ) {
		  $text =~ s/^($anchor)/$output$1/m;
		} else { # AFTER
		  $text =~ s/^($anchor)/$1$output/m;
		}
      } else {
		# Position relative to index'th comment
		my $idx = 0;
		$text =~ s/(%COMMENT({.*?})?%)/&_nth($1,\$idx,$position,$index,$output)/ego;
      }
    }
    $text =~ s/ {3}/\t/go;
    $text = $premeta . $text . $postmeta;
	return TWiki::Func::saveTopicText( $web, $topic, $text, 1, $silent );
  }

  # PRIVATE embed output if this comment is the interesting one
  sub _nth {
    my ( $tag, $pidx, $position, $index, $output ) = @_;

    if ( $$pidx == $index) {
      if ( $position eq "BEFORE" ) {
	$tag = "$output$tag";
      } else { # AFTER
	$tag .= "$output";
      }
    }
    $$pidx++;
    return $tag;
  }
}

1;
