#!/usr/bin/perl
#
#  Copyright (C) 2002-2003 Michael H. Schimek 
#  inspired by a LXR script http://lxr.linux.no/
#
#  GPL2 blah blah blah.
#
#  This script turns a C header file into functions printing
#  and checking ioctl arguments. It's part of the debugging
#  routines of the Zapping tv viewer http://zapping.sf.net.
#
#  Perl and C gurus cover your eyes. This is one of my first
#  attempts in this funny tongue and far from a proper C parser.

# $Id: structpr_gen.pl,v 1.1.2.3 2003-07-29 03:48:01 mschimek Exp $

$number		= '[0-9]+';
$ident		= '\~?_*[a-zA-Z][a-zA-Z0-9_]*';
$signed		= '((signed)?(char|short|int|long))|__s8|__s16|__s32|signed';
$unsigned	= '(((unsigned\s*)|u|u_)(char|short|int|long))|__u8|__u16|__u32|unsigned';
$define		= '^\s*\#\s*define\s+';

while (@ARGV) {
    # Syntax of arguments, in brief:
    # struct.field=SYM_
    #   says struct.field contains symbols starting with SYM_
    # struct.field=string|hex|fourcc
    #   print that field appropriately. Try first without
    #   a hint, there's some wisdom built into this script.
    # typedef=see above
    # struct={ fprintf(fp, "<$s>", t->narf); }
    #   print like this.
    # todo:
    # * select symbol mode 0,1,2,3
    # * select union member (e.g. struct.field=FOO:u.foo)
    $arg = shift (@ARGV);
    if ($arg =~ m/(($ident)\.($ident))\=($ident)/) {
	$symbolic{$1} = $4;
    } elsif ($arg =~ m/($ident)\=($ident)/) {
	$symbolic{$1} = $2;
    } elsif ($arg =~ m/(($ident)(\.$ident)?)\={(.*)}/) {
    	$print_func{$1} = $4;
    }
}

$_ = $/; 
undef($/); 
$contents = <>;
$/ = $_;

#
#  Step I - comb the source and filter out #defines
#

sub wash {
    my $t = $_[0];
    $t =~ s/[^\n]+//gs;
    return ($t);
}

# Remove comments.
$contents =~ s/\/\*(.*?)\*\//&wash($1)/ges;
$contents =~ s/\/\/[^\n]*//g; # C++

# Unwrap continuation lines.
$contents =~ s/\\\s*\n/$1\05/gs;
while ($contents =~ s/\05([^\n\05]+)\05/$1\05\05/gs) {}
$contents =~ s/(\05+)([^\n]*)/"$2"."\n" x length($1)/ges;

sub add_ioctl_check {
    my ($name, $dir, $type) = @_;

    $ioctl_check .= "static __inline__ void IOCTL_ARG_TYPE_CHECK_$name ";
    
    if ($dir eq "W") {
	$ioctl_check .= "(const $type *arg) {}\n";
    } else {
	$ioctl_check .= "($type *arg) {}\n";
    }
}

sub add_ioctl {
    my ($name, $dir, $type) = @_;

    $ioctl_cases{$type} .= "\tcase $name:\n"
	. "\t\tif (!arg) { fputs (\"$name\", fp); return; }\n";

    &add_ioctl_check ($name, $dir, $type);
}

# Find macro definitions, create ioctl & symbol table.
$t = "";
$skip = 0;
foreach ($contents =~ /^(.*)/gm) {
    if ($skip) {
	if (/^\s*#\s*endif/) {
	    $skip = 0;
	}

	next;
    }

    # #if 0
    if (/^\s*#\s*if\s+0/) {
	$skip = 1;
    # Ioctls
    } elsif (/$define($ident)\s+_IO(WR|R|W).*\(.*,\s*$number\s*,\s*(struct|union)\s*($ident)\s*\)\s*$/) {
	&add_ioctl ($1, $2, "$3 $4");
    } elsif (/$define($ident)\s+_IO(WR|R|W).*\(.*,\s*$number\s*,\s*(($signed)|($unsigned))\s*\)\s*$/) {
	&add_ioctl ($1, $2, "$3");
    } elsif (/$define($ident)\s+_IO(WR|R|W).*\(.*,\s*$number\s*,\s*([^*]+)\s*\)\s*$/) {
	&add_ioctl_check ($1, $2, $3);
    # Define 
    } elsif (/$define($ident)/) {
	push @global_symbols, $1;
    # Other text
    } elsif (!/^\s*\#/) {
	$_ =~ s/\s+/ /g;
	$t="$t$_ ";
    }
}

# Split field lists: struct { ... } foo, bar; int x, y;
$t =~ s/({|;)\s*((struct\s*{[^}]*})\s*($ident))\s*,/\1 \2; \3 /gm;
$t =~ s/({|;)\s*(([^,;}]*)\s+($ident))\s*,/\1 \2; \3 /gm;

# Function pointers are just pointers.
$t =~ s/\(\s*\*\s*($ident)\s*\)\s*\([^)]*\)\s*;/void *\1;/gm;

# Split after ,;{
$t =~ s/(,|;|{)/\1\n/gm;
@contents = split ('\n', $t);

#
#  Step II - parse structs, unions and enums
#

sub add_arg {
    my ($type, $ref, $field, $template) = @_;

    $templ .= "$field=$template ";
    $args .= "($type) $ref$field, ";
}

sub add_arg_func {
    my ($text, $deps, $type, $ref, $field) = @_;
    my $lp = "";
    my $rp = "";

    if ($type =~ m/^(struct|union)/) {
	$lp = "{";
	$rp = "}";
    }

    if ($funcs{$type}) {
	push @$deps, $type;
	$type =~ s/ /_/g;
	$templ .= "$field=$lp";
	$$text .= &flush_args() . "\tfprintf_$type (fp, $ref$field);\n";
	$templ .= "$rp ";
    } else {
	$templ .= "$field=? ";
    }
}

sub add_symbolic {
    my ($text, $deps, $mode, $ref, $field, $prefix) = @_;
    my $sbody, $count;

    $count = 0;

    foreach (@global_symbols) {
	if (/^$prefix/) {
	    $str = $_;
	    $str =~ s/^$prefix//;
	    $sbody .= "\t\t\"$str\", $_,\n";
	    ++$count;
	}
    }

    $prefix = lc $prefix;

    if ($count > 3) {
	my $type = "symbol $prefix";

	$funcs{$type} = {
	    text => "static void\n"
		. "fprintf_symbol_$prefix (FILE *fp, unsigned long value)\n"
		. "{\n\tfprintf_symbolic (fp, $mode, value,\n"
		. $sbody . "\t\t0);\n}\n\n",
	    deps => []
	};

	&add_arg_func ($text, $deps, $type, $ref, $field);
    } else {
	$templ .= "$field=";
	$$text .= &flush_args;
	$fpl = -1;
	$$text .= "\tfprintf_symbolic (fp, $mode, $ref$field,\n"
	    . $sbody . "\t\t0);\n";
	$templ .= " ";
    }
}

sub flush_args {
    my $text;

    $templ =~ s/\s(\"\n\t\t\")?$//m;
    $args =~ s/,\s*$//g;
    $args =~ s/^\s+$//;

    if ($templ) {
	if ($args) {
    	    $text = "\tfprintf (fp, \"$templ\",\n\t\t$args);\n";
	} else {
    	    $text = "\tfputs (\"$templ\", fp);\n";
	}
    }

    $templ = "";
    $args = "";
    $fpl = 0;

    return ($text);
}

sub aggregate_body {
    my ($text, $deps, $kind, $name, $ref, $skip) = @_;

    $fpl = 0;

    if ($print_func{$name}) {
	$$text .= $print_func{$name} . "\n";
	$skip = 1;
    }

    while (@contents) {
        $_ = shift(@contents);

	# End of aggregate
	if (/^\s*}\s*;/) {
	    $$text .= &flush_args;
	    return ("");
	# End of substruct or union
	} if (/^\s*}\s*($ident)\s*;/) {
	    $$text .= &flush_args;
	    return ($1);
	# Enum.
	} elsif (/^\s*enum\s+($ident)\s+($ident);/) {
	    if (!$skip) {
		&add_arg_func ($text, $deps, "enum $1", $ref, $2);
	    }
	# Substruct or union.
	} elsif (/^\s*(struct|union)\s+($ident)\s+($ident);/) {
	    if (!$skip) {
		&add_arg_func ($text, $deps, "$1 $2", "&$ref", $3);
	    }
	# Substruct or union inline definition w/o declaration
	# Why don't you just shoot me...
	} elsif (/^\s*(struct|union)\s+{/) {
	    my $field, $subtext;

	    $$text .= &flush_args;
	    $subtext = "";
	    $field = &aggregate_body (\$subtext, $deps, $1, $2, "§$ref§", $skip);

	    if ($skip) {
		next;
	    }

	    if ($field ne "") {
		$templ .= "$field={";
		$subtext =~ s/§$ref§/$ref$field./g;
		$$text .= &flush_args() . $subtext;
		$templ .= "} ";
	    } else {
	        $templ .= "? ";
	    }
	# Other stuff, simplified
	} elsif (/^\s*($ident(\s+$ident)*)(\*|\s)+($ident)\s*(\[([a-zA-Z0-9_]+)\]*\s*)?;/) {
	    my $type = $1, $ptr = $3, $field = $4, $size = $6, $hint = "";

	    if ($typedefs{$type}) {
		$hint = $symbolic{$type};
		$type = $typedefs{$type};
	    }

	    if ($symbolic{"$name.$field"}) {
		$hint = $symbolic{"$name.$field"};
	    }

	    if ($skip) {
		next;
	    }

# 	    print "$type $ptr $name . $field [$size] $hint\n";

	    if (0) {
	    # Wisdom: a reserved field contains nothing useful.
	    } elsif ($field eq "reserved") {
		if ($size ne "") {
		    $templ .= "$field\[\] ";
		} else {
		    $templ .= "$field ";
		}
	    # Pointer
	    } elsif ($ptr eq "*") {
		# Array of pointers?
		if ($size ne "") {
		    # Not smart enough, ignore
		    $templ .= "$field\[\]=? ";
	        # Wisdom: char pointer is probably a string.
		} elsif ($type eq "char" || $field eq "name" || $hint eq "string") {
		    &add_arg ("char *", $ref, $field, "\\\"%s\\\"");
		# Other pointer
		} else {
		    &add_arg ("void *", $ref, $field, "%p");
		}
	    # Array of something
	    } elsif ($size ne "") {
	        # Wisdom: a char array contains a string.
		# Names are also commonly strings.
		if ($type eq "char" || $field eq "name" || $hint eq "string") {
		    $args .= "$size, ";
		    &add_arg ("char *", $ref, $field, "\\\"%.*s\\\"");
		# So this is some other kind of array, what now?
		} else {
		    # ignore
		    $templ .= "$field\[\]=? ";
		}
	    # Wisdom: a field named flags typically contains flags.
	    } elsif ($field eq "flags") {
	        if ($hint ne "") {
		    &add_symbolic ($text, $deps, 2, $ref, $field, $hint);
		} else {
		    # flags in hex
		    &add_arg ("unsigned long", $ref, $field, "0x%lx");
		}
	    # Hint: something funny
	    } elsif ($hint eq "hex") {
		&add_arg ("unsigned long", $ref, $field, "0x%lx");
	    } elsif ($hint eq "fourcc") {
		&add_arg ("char *", $ref, $field, "\\\"%.4s\\\"=0x%lx");
		$args .= "(unsigned long) $ref$field, ";
	    # Field contains symbols, could be flags or enum or both
	    } elsif ($hint ne "") {
	        &add_symbolic ($text, $deps, 0, $ref, $field, $hint);
	    # Miscellaneous integers. Suffice to distinguish signed and
	    # unsigned, compiler will convert to long automatically
	    } elsif ($type =~ m/$unsigned/) {
	        &add_arg ("unsigned long", $ref, $field, "%lu");
	    } elsif ($type =~ m/$signed/) {
	        &add_arg ("long", $ref, $field, "%ld");
	    # The Spanish Inquisition.
    	    } else {
	        # Ha! We expected you.
	        $templ .= "$field=? ";
	    }

	    $fpl += 1;
	    if ($fpl >= 2) {
	        $templ .= "\"\n\t\t\"";
	        $args .= "\n\t\t";
	        $fpl = 0;
	    }
	}
    }
}

sub aggregate {
    my ($kind, $name) = @_;
    my $type = "$kind $name";
    my $text, @deps;

    $funcs{$type} = {
	text => "static void\nfprintf_$kind\_$name (FILE *fp, $type *t)\n{\n",
	deps => []
    };

    aggregate_body (\$funcs{$type}->{text},
		    $funcs{$type}->{deps},
		    $kind, $name, "t->", 0);

    $funcs{$type}->{text} .= "}\n\n";
}

sub common_prefix {
    my $prefix = @_[0];
    my $symbol;

    foreach $symbol (@_) {
	while (length ($prefix) > 0) {
	    if (index ($symbol, $prefix) == 0) {
	        last;
	    } else {
	        $prefix = substr ($prefix, 0, -1);
	    }
	}
    }

    return ($prefix);
}

sub enumeration {
    my $name = @_[0];
    my $type = "enum $name";
    my @symbols;

    $funcs{$type} = {
	text => "static void\nfprintf_enum_$name (FILE *fp, int value)\n"
	    . "{\n\tfprintf_symbolic (fp, 1, value,\n",
	deps => []
    };

    while (@contents) {
	$_ = shift(@contents);
	if (/^\s*\}\s*;/) {
	    last;
	} elsif (/^\s*($ident)\s*(=\s*.*)\,/) {
	    push @symbols, $1;
	}
    }

    $prefix = &common_prefix (@symbols);

    foreach $symbol (@symbols) {
	$funcs{$type}->{text} .=
	    "\t\t\"" . substr ($symbol, length ($prefix)) . "\", $symbol,\n";
    }

    $funcs{$type}->{text} .= "\t\t0);\n}\n\n";
}

# Let's parse

while (@contents) {
    $_ = shift(@contents);
#   print ">>$_<<\n";

    if (/^\s*(struct|union)\s*($ident)\s*\{/) {
	&aggregate ($1, $2);
    } elsif (/^\s*enum\s*($ident)\s*\{/) {
	&enumeration ($1);
    } elsif (/^\s*typedef\s*([^;]+)\s+($ident)\s*;/) {
	$typedefs{$2} = $1;
    }
}

#
# Step III - create the file
#

print "/* Generated file, do not edit! */

#include <stdio.h>
#include \"device.h\"

";

sub print_type {
    my ($type) = @_;

    if (!$printed{$type}) {
	foreach $dependency (@{$funcs{$type}->{deps}}) {
	    &print_type ($dependency);
	}

	print $funcs{$type}->{text};

	$printed{$type} = TRUE;
    }
}

$text = "static void\nfprintf_ioctl_arg (FILE *fp, unsigned int cmd, void *arg)\n"
    . "{\n\tswitch (cmd) {\n";

while (($type, $case) = each %ioctl_cases) {
    if ($funcs{$type}) {
	&print_type ($type);
	$type =~ s/ /_/;
	$text .= "$case\t\tfprintf_$type (fp, arg);\n\t\tbreak;\n";
    } elsif ($type =~ m/$unsigned/) {
	$text .= "$case\t\tfprintf (fp, \"%lu\", "
	    . "(unsigned long) * ($type *) arg);\n\t\tbreak;\n";
    } elsif ($type =~ m/$signed/) {
	$text .= "$case\t\tfprintf (fp, \"%ld\", "
	    . "(long) * ($type *) arg);\n\t\tbreak;\n";
    } else {
	$text .= "$case\t\tbreak; /* $type */\n";
    }
}

$text .= "\tdefault:\n"
    . "\t\tif (!arg) { fprintf_unknown_cmd (fp, cmd, arg); return; }\n"
    . "\t\tbreak;\n";
$text .= "\t}\n\}\n\n";

print $text;

print $ioctl_check;
print "\n";
