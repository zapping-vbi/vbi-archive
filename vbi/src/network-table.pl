#!/usr/bin/perl
# $Id: network-table.pl,v 1.1.2.3 2004-09-14 04:52:00 mschimek Exp $

use strict;
use XML::Simple;   # http://search.cpan.org/search?query=XML::Simple
use Data::Dumper;

my $xml = XMLin ("-",
		 ForceContent => 1,
		 ForceArray => ["network"]);

# print Dumper ($xml);

print "/* \$Id\$ */

/* Generated file, do not edit! */

struct country {
	uint16_t		cni_8301;	/* Packet 8/30 format 1 */
	uint16_t		cni_8302;	/* Packet 8/30 format 2 */
	uint16_t		cni_pdc_b;	/* Packet X/26 (PDC B) */
	uint16_t		cni_vps;	/* VPS */
	const char		country_code[4];
	const char *		name;		/* UTF-8 */
};

struct network {
	uint16_t		cni_8301;	/* Packet 8/30 format 1 */
	uint16_t		cni_8302;	/* Packet 8/30 format 2 */
	uint16_t		cni_pdc_b;	/* Packet X/26 (PDC B) */
	uint16_t		cni_vps;	/* VPS */
	unsigned int		country;
	const char *		name;		/* UTF-8 */
};

struct ttx_header {
	const char *		name;		/* UTF-8 */
	const uint8_t *		header;		/* raw Teletext data */
};
";

# Generate table of countries sorted by country code

my @countries = sort { $a->{"country-code"}->{content} cmp
		       $b->{"country-code"}->{content} } @{$xml->{country}};

my @networks = ();
my $country_index = 0;

print "\nstatic const struct country\ncountry_table [] = {\n";

for (@countries) {
    my $record = $_;
    my $count = 0;

    print "\t{ ";

    for (qw/cni-8301 cni-8302 cni-pdc-b cni-vps/) {
	if (defined ($record->{$_}->{content})) {
	    printf "0x%04X, ", hex ($record->{$_}->{content});
	} else {
	    print "0x0000, ";
	}
    }

    print "\"", $record->{"country-code"}->{content}, "\", ";
    print "\"", $record->{"name"}->{content} . "\" },\n";

    for (@{$record->{"network"}}) {
	$_->{my_country_index} = $country_index;
	push (@networks, $_);
    }

    for (@{$record->{"affiliates"}}) {
	my $affiliates = $_;

	for (@{$affiliates->{"network"}}) {
	    $_->{my_country_index} = $country_index;
	    push (@networks, $_);
	}
    }

    $country_index++;
}

print "};\n";

# Generate table of networks sorted by cni-8301

@networks = sort { hex($a->{"cni-8301"}->{content}) <=>
		   hex($b->{"cni-8301"}->{content}) ||
		   hex($a->{"cni-8302"}->{content}) <=>
		   hex($b->{"cni-8302"}->{content}) ||
		   hex($a->{"cni-pdc-b"}->{content}) <=>
		   hex($b->{"cni-pdc-b"}->{content}) ||
		   hex($a->{"cni-vps"}->{content}) <=>
		   hex($b->{"cni-vps"}->{content}) } @networks;

my %cni_lookup;
my $network_index = 0;

print "\nstatic const struct network\nnetwork_table [] = {\n";

for (@networks) {
    my $record = $_;

    if (!(defined ($record->{"cni-8301"}->{content}) ||
	  defined ($record->{"cni-8302"}->{content}) ||
	  defined ($record->{"cni-pdc-b"}->{content}) ||
	  defined ($record->{"cni-vps"}->{content}))) {
	next;
    }

    print "\t{ ";

    for (qw/cni-8301 cni-8302 cni-pdc-b cni-vps/) {
	if (defined ($record->{$_}->{content})) {
	    my $value = hex ($record->{$_}->{content});

	    printf "0x%04X, ", $value;
	    %cni_lookup->{$_}->{sprintf ("%04X", $value)} = $network_index;
	} else {
	    print "0x0000, ";
	}
    }

    printf "%2u, ", $record->{my_country_index};
    print "\"", $record->{"name"}->{content}, "\" },\n";

    $network_index++;
}

printf "};\n\n/* %u */\n", $network_index;

# Reverse lookup tables

for (qw/cni-8302 cni-pdc-b cni-vps/) {
    my %hash = %{%cni_lookup->{$_}};
    my $name = $_;

    $name =~ s/-/_/g;

    print "\nstatic const uint16_t\nlookup_" , $name, " [] = {\n";

    for (sort keys %hash) {
	printf "\t%3d, /* %s */\n", %hash->{$_}, $_;
    }

    print "};\n";
}

# Teletext header table

print "\nstatic const struct ttx_header\nttx_header_table [] = {\n";

for (@networks) {
    my $record = $_;

    if (defined ($record->{"ttx-header"}->{content})) {
	print "\t{ \"", $record->{"name"}->{content} . "\", ";
	print "\"", $record->{"ttx-header"}->{content} . "\" },\n";
    }
}

printf "};\n\n";
