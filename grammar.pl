#!/usr/bin/perl
#
#   Engine1999 - A 2D games engine written in C
#   Copyright (C) 2026  Ekkehard Morgenstern
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Makes the quick reference of the language from bascomp.ebnf:
#
#   perl grammar.pl            writes docs/grammar.md and docs/grammar.html
#   perl grammar.pl --check    writes nothing: fails if a production is used but not defined, or if the two files are not what
#                              the EBNF file would produce (they are out of date: run "perl grammar.pl")
#
# The EBNF file (bascomp.ebnf) is the source:
#   "--# Title"      starts a section (a heading in the output)
#   "-- text"        a comment: prose before the rules it describes (inside a rule or behind its final "." it stays a comment of the rule)
#   "name := ... ."  a production; the tokens are shown the way the programmer writes them (TOK_PRINT -> PRINT, TOK_COMMA -> ",")
# Everything before the line "Language syntax" is the license text and is skipped.

use strict;
use warnings;

my $ebnf = 'bascomp.ebnf';
my $mdfile = 'docs/grammar.md';
my $htmlfile = 'docs/grammar.html';
my $check = ( @ARGV && $ARGV[0] eq '--check' );

# -- the tokens that are punctuation or a kind of literal (everything else is a keyword: TOK_PRINT -> PRINT) --------------------

my %punct = (
    COMMA => ',', SEMIC => ';', COLON => ':', LPAREN => '(', RPAREN => ')', EQ => '=', NE => '<>', LE => '<=', GE => '>=',
    LT => '<', GT => '>', PLUS => '+', MINUS => '-', MULT => '*', DIV => '/', POW => '^', BACKSL => '\\', LATTICE => '#',
    ADDROF => '@', STRING => '$',
);
my %literal = (
    NUMIDENT => 'name', INTIDENT => 'name%', STRIDENT => 'name$', IDENT => 'ident', DECLIT => 'number', HEXLIT => '&Hhex',
    OCTLIT => '&Ooctal', QUALIT => '&Qquaternary', BINLIT => '&Bbinary', STRLIT => 'string', SHLLIT => 'shell', BRKLIT => 'brk-string',
    BRCLIT => 'brc-string', QUOLIT => "'comment", EOL => 'end-of-line', DEC0 => '0', DEC9 => '9',
);

# -- reading the EBNF file --------------------------------------------------------------------------------------------------------

open( my $in, '<', $ebnf ) or die "$0: cannot read $ebnf: $!\n";
my @src = <$in>;
close $in;
chomp @src;

# items: { type => 'section', text }, { type => 'prose', lines => [] }, { type => 'rules', rules => [ { name, lines => [], note } ] }
my @items;
my @prose;
my $started = 0;
my $curr;                   # the rule being read
my %defined;
my @bodies;                 # all rule texts, for the check
my $lastrules;

sub flush_prose {
    if ( @prose ) {
        push @items, { type => 'prose', lines => [ @prose ] };
        @prose = ();
        $lastrules = undef;
    }
}

for my $line ( @src ) {
    if ( !$started ) {
        $started = 1 if $line =~ /^Language syntax/;
        next;
    }
    next if $line =~ /^-+$/ && !$curr;
    if ( $curr ) {
        # inside a rule: comment lines stay in the rule, the rule ends with a "." (a comment may follow it)
        if ( $line =~ /^\s*--\s?(.*)$/ ) {
            push @{ $curr->{lines} }, { comment => $1 };
            next;
        }
        my $text = $line;
        my $note;
        if ( $text =~ /^(.*?\.)\s+--\s*(.*)$/ ) {       # "... .  -- note"
            ( $text, $note ) = ( $1, $2 );
        } elsif ( $text =~ /^(.*?)\s+--\s*(.*)$/ ) {    # a note in the middle of a rule line
            ( $text, $note ) = ( $1, $2 );
        }
        push @{ $curr->{lines} }, { code => $text, note => $note };
        push @bodies, $text;
        if ( $text =~ /\.\s*$/ ) {
            $curr = undef;
        }
        next;
    }
    if ( $line =~ /^--#\s*(.*)$/ ) {
        flush_prose();
        push @items, { type => 'section', text => $1 };
        $lastrules = undef;
        next;
    }
    if ( $line =~ /^--\s?(.*)$/ ) {
        # a comment between rules: it describes what follows (starts a new block)
        push @prose, $1;
        $lastrules = undef;
        next;
    }
    if ( $line =~ /^\s*$/ ) {
        flush_prose();      # (rules separated by blank lines only stay in one block)
        next;
    }
    if ( $line =~ /^([a-z][a-z0-9-]*)\s*:=(.*)$/ ) {
        my ( $name, $rest ) = ( $1, $2 );
        flush_prose();
        if ( !$lastrules ) {
            $lastrules = { type => 'rules', rules => [] };
            push @items, $lastrules;
        }
        die "$0: $ebnf: $name is defined twice\n" if $defined{$name}++;
        $curr = { name => $name, lines => [] };
        push @{ $lastrules->{rules} }, $curr;
        my $text = $line;
        my $note;
        if ( $text =~ /^(.*?\.)\s+--\s*(.*)$/ ) {
            ( $text, $note ) = ( $1, $2 );
        }
        push @{ $curr->{lines} }, { code => $text, note => $note };
        push @bodies, $rest;
        $curr = undef if $text =~ /\.\s*$/;
        next;
    }
    die "$0: $ebnf: cannot read the line: $line\n";
}
flush_prose();

# -- the check: every name used is defined ----------------------------------------------------------------------------------------

my %used;
for my $b ( @bodies ) {
    ( my $t = $b ) =~ s/--.*$//;
    $used{$1} = 1 while $t =~ /(?<![A-Za-z0-9_-])([a-z][a-z0-9-]*)(?![A-Za-z0-9_-])/g;
}
my @undefined = grep { !$defined{$_} && !/^tok/ } sort keys %used;
if ( @undefined ) {
    warn "$0: $ebnf: used but not defined: @undefined\n";
    exit 1 if $check;
}

# -- converting the tokens ----------------------------------------------------------------------------------------------------------

# one rule line (code part) -> list of [ class, text ] pieces: rule, kw, punct, lit, meta, space
sub pieces {
    my ( $code ) = @_;
    my @p;
    my $s = $code;
    $s =~ s/TOK_DEC0\s*\.\.\s*TOK_DEC9/DIGITRANGE/;
    while ( length $s ) {
        if ( $s =~ s/^(\s+)// ) {
            push @p, [ 'space', $1 ];
        } elsif ( $s =~ s/^DIGITRANGE// ) {
            push @p, [ 'punct', '"0"' ], [ 'space', ' ' ], [ 'meta', '..' ], [ 'space', ' ' ], [ 'punct', '"9"' ];
        } elsif ( $s =~ s/^TOK_([A-Z0-9]+)// ) {
            my $t = $1;
            if ( exists $punct{$t} ) {
                my $q = $punct{$t};
                $q = '\\\\' if $q eq '\\';
                push @p, [ 'punct', "\"$q\"" ];
            } elsif ( exists $literal{$t} ) {
                push @p, [ 'lit', "<$literal{$t}>" ];
            } else {
                push @p, [ 'kw', $t ];
            }
        } elsif ( $s =~ s/^([A-Z][A-Z0-9\$]*)// ) {
            push @p, [ 'kw', $1 ];
        } elsif ( $s =~ s/^([a-z][a-z0-9-]*)// ) {
            push @p, [ 'rule', $1 ];
        } elsif ( $s =~ s/^(:=)// ) {
            push @p, [ 'meta', $1 ];
        } elsif ( $s =~ s/^(\.\.)// ) {
            push @p, [ 'meta', $1 ];
        } elsif ( $s =~ s/^([\[\]\{\}\(\)\|\.])// ) {
            push @p, [ 'meta', $1 ];
        } elsif ( $s =~ s/^(.)//s ) {
            push @p, [ 'punct', $1 ];
        }
    }
    return @p;
}

sub plain { join '', map { $_->[1] } @_ }

# -- markdown -------------------------------------------------------------------------------------------------------------------------

sub md_escape {
    my ( $t ) = @_;
    $t =~ s/([\\*_<>|`])/\\$1/g;
    return $t;
}

my @sections = grep { $_->{type} eq 'section' } @items;

my $md = <<'END';
# The language at a glance

A quick reference of the syntax of the built-in BASIC, **generated from `bascomp.ebnf`** by `perl grammar.pl` (`make grammar`): do not edit this file,
edit the EBNF file and run the script. The same grammar with colors and links from every rule to its definition is in [grammar.html](grammar.html).

How to read it:

- `name := ... .` is a rule; a rule name is lower case with hyphens (`print-stmt`). A rule ends with a period.
- Keywords are written in capitals (`PRINT`, `FOR`). **Capitals that are not reserved words** (`OUTPUT`, `AS`, `EACH`, `DOWNTO`, `LAYER`, `PSET` ...) are words that are recognized in that place only:
  they stay free for variable names elsewhere.
- Punctuation is quoted (`","`, `"("`). `<name>` is an identifier, `<name$>` a string identifier, `<name%>` an integer identifier, `<number>`, `<string>`, `<shell>` (a `` `...` `` literal) and so on are literals.
- `[ x ]` is optional, `{ x }` repeated (zero or more times), `( a | b )` a choice. `(* ... *)` is a note.
- Every statement is a `...-stmt` rule; `stmt` at the end lists the groups. Several statements go on one line separated by `":"`; a numbered line is part of the stored program, a line without a number runs at once.
- The grammar is a description, not the specification: the parser (`bascomp.c`) and the code generator decide the details. The notes at the end say what a grammar cannot express.

## Contents

END
for my $i ( 0 .. $#sections ) {
    my $title = $sections[$i]{text};
    ( my $anchor = lc $title ) =~ s/[^a-z0-9 -]//g;
    $anchor =~ s/ /-/g;
    $md .= '- [' . $title . '](#' . $anchor . ")\n";
}
$md .= "- [Notes: what a grammar cannot say](#notes-what-a-grammar-cannot-say)\n\n";

my $lastprose;
for my $it ( @items ) {
    if ( $it->{type} eq 'section' ) {
        $md .= "## $it->{text}\n\n";
    } elsif ( $it->{type} eq 'prose' ) {
        $lastprose = $it;
        next if $it->{lines}[0] =~ /^What this grammar does not say/;      # the closing notes: see below
        my @l = @{ $it->{lines} };
        $md .= join( "\n", map { '> ' . md_escape( $_ ) } @l ) . "\n\n";
    } else {
        my $code = '';
        for my $r ( @{ $it->{rules} } ) {
            for my $l ( @{ $r->{lines} } ) {
                if ( exists $l->{comment} ) {
                    $code .= "    (* $l->{comment} *)\n";
                    next;
                }
                my $text = plain( pieces( $l->{code} ) );
                $text .= "  (* $l->{note} *)" if defined $l->{note};
                $code .= "$text\n";
            }
        }
        $md .= "```ebnf\n$code```\n\n";
    }
}
# the closing notes
for my $it ( @items ) {
    next unless $it->{type} eq 'prose' && $it->{lines}[0] =~ /^What this grammar does not say/;
    $md .= "## Notes: what a grammar cannot say\n\n";
    my @l = @{ $it->{lines} };
    shift @l;
    my @para;
    for my $l ( @l ) {
        if ( $l =~ /^ \*\s*(.*)$/ ) {
            $md .= join( ' ', @para ) . "\n" if @para;
            @para = ();
            $md .= '- ' . md_escape( $1 );
            push @para, '';
            @para = ();
            $md .= "\n";
        } elsif ( $l =~ /^\s+(.*)$/ ) {
            chomp $md;
            $md .= ' ' . md_escape( $1 ) . "\n";
        }
    }
    $md .= "\n";
}
$md =~ s/\n{3,}/\n\n/g;

# -- html -----------------------------------------------------------------------------------------------------------------------------

sub h {
    my ( $t ) = @_;
    $t =~ s/&/&amp;/g;
    $t =~ s/</&lt;/g;
    $t =~ s/>/&gt;/g;
    return $t;
}

sub html_pieces {
    my ( $code ) = @_;
    my $out = '';
    for my $p ( pieces( $code ) ) {
        my ( $c, $t ) = @$p;
        if ( $c eq 'space' ) {
            $out .= $t;
        } elsif ( $c eq 'rule' ) {
            $out .= $defined{$t} ? "<a class=\"rule\" href=\"#r-$t\">" . h( $t ) . '</a>' : '<span class="rule">' . h( $t ) . '</span>';
        } else {
            $out .= "<span class=\"$c\">" . h( $t ) . '</span>';
        }
    }
    return $out;
}

my $html = <<'END';
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Engine1999 BASIC: the language at a glance</title>
<style>
:root { --bg:#fbfbf8; --fg:#222; --kw:#1a1a8c; --rule:#00706b; --punct:#2a7a1e; --lit:#8c1c13; --meta:#888; --note:#777; --code:#f1f1ea; --head:#14335c; --line:#d8d8cc; }
@media (prefers-color-scheme: dark) { :root { --bg:#16181d; --fg:#d8d8d0; --kw:#8aa4ff; --rule:#5fd3c8; --punct:#8fd36f; --lit:#ff9c8c; --meta:#80848c; --note:#8a8f98; --code:#1f2229; --head:#9fc2f0; --line:#33363d; } }
body { background:var(--bg); color:var(--fg); font:16px/1.5 system-ui, sans-serif; margin:0 auto; padding:16px; max-width:70em; }
h1, h2 { color:var(--head); }
h2 { border-bottom:1px solid var(--line); margin-top:1.8em; }
pre { background:var(--code); padding:.7em 1em; overflow-x:auto; border-radius:4px; font:14px/1.45 ui-monospace, monospace; }
p.prose { margin:.4em 0; color:var(--note); }
.kw { color:var(--kw); font-weight:bold; }
.rule, a.rule { color:var(--rule); text-decoration:none; }
a.rule:hover { text-decoration:underline; }
.def { color:var(--rule); font-weight:bold; }
.punct { color:var(--punct); }
.lit { color:var(--lit); }
.meta { color:var(--meta); }
.note { color:var(--note); font-style:italic; }
nav ul { columns:2 18em; list-style:none; padding-left:0; }
li { margin:.15em 0; }
</style>
</head>
<body>
<h1>The language at a glance</h1>
<p>A quick reference of the syntax of the built-in BASIC, generated from <code>bascomp.ebnf</code> by <code>perl grammar.pl</code> (<code>make grammar</code>).
Click a rule name to jump to its definition.</p>
<ul>
<li><span class="kw">KEYWORD</span> in capitals: a reserved word. Capitals that are not reserved words (<span class="kw">OUTPUT</span>, <span class="kw">AS</span>, <span class="kw">EACH</span>,
<span class="kw">LAYER</span> ...) are recognized in that place only.</li>
<li><span class="rule">rule-name</span>: another rule. <span class="punct">","</span>: punctuation, quoted. <span class="lit">&lt;name$&gt;</span>: a kind of token (identifier, number, string ...).</li>
<li><span class="meta">[ x ]</span> optional, <span class="meta">{ x }</span> repeated, <span class="meta">( a | b )</span> a choice.</li>
</ul>
<nav><h2>Contents</h2><ul>
END
my $n = 0;
for my $s ( @sections ) {
    $html .= '<li><a href="#s' . ( ++$n ) . '">' . h( $s->{text} ) . "</a></li>\n";
}
$html .= "<li><a href=\"#notes\">Notes: what a grammar cannot say</a></li>\n</ul></nav>\n";
$n = 0;
for my $it ( @items ) {
    if ( $it->{type} eq 'section' ) {
        $html .= '<h2 id="s' . ( ++$n ) . '">' . h( $it->{text} ) . "</h2>\n";
    } elsif ( $it->{type} eq 'prose' ) {
        next if $it->{lines}[0] =~ /^What this grammar does not say/;
        $html .= '<p class="prose">' . join( '<br>', map { h( $_ ) } @{ $it->{lines} } ) . "</p>\n";
    } else {
        my $code = '';
        for my $r ( @{ $it->{rules} } ) {
            my $first = 1;
            for my $l ( @{ $r->{lines} } ) {
                if ( exists $l->{comment} ) {
                    $code .= '    <span class="note">(* ' . h( $l->{comment} ) . " *)</span>\n";
                    next;
                }
                my $text = $l->{code};
                if ( $first && $text =~ /^([a-z][a-z0-9-]*)(\s*:=.*)$/ ) {
                    my ( $name, $rest ) = ( $1, $2 );
                    $code .= "<span id=\"r-$name\" class=\"def\">" . h( $name ) . '</span>' . html_pieces( $rest );
                } else {
                    $code .= html_pieces( $text );
                }
                $first = 0;
                $code .= '  <span class="note">(* ' . h( $l->{note} ) . ' *)</span>' if defined $l->{note};
                $code .= "\n";
            }
        }
        $html .= "<pre>$code</pre>\n";
    }
}
for my $it ( @items ) {
    next unless $it->{type} eq 'prose' && $it->{lines}[0] =~ /^What this grammar does not say/;
    $html .= "<h2 id=\"notes\">Notes: what a grammar cannot say</h2>\n<ul>\n";
    my @l = @{ $it->{lines} };
    shift @l;
    my $open = 0;
    for my $l ( @l ) {
        if ( $l =~ /^ \*\s*(.*)$/ ) {
            $html .= "</li>\n" if $open;
            $html .= '<li>' . h( $1 );
            $open = 1;
        } elsif ( $l =~ /^\s+(.*)$/ ) {
            $html .= ' ' . h( $1 );
        }
    }
    $html .= "</li>\n" if $open;
    $html .= "</ul>\n";
}
$html .= "</body>\n</html>\n";

# -- output ---------------------------------------------------------------------------------------------------------------------------

sub slurp {
    my ( $f ) = @_;
    open( my $fh, '<', $f ) or return undef;
    local $/;
    my $t = <$fh>;
    close $fh;
    return $t;
}

if ( $check ) {
    my $bad = 0;
    for my $pair ( [ $mdfile, $md ], [ $htmlfile, $html ] ) {
        my $old = slurp( $pair->[0] );
        if ( !defined $old || $old ne $pair->[1] ) {
            warn "$0: $pair->[0] is out of date: run \"perl grammar.pl\"\n";
            $bad = 1;
        }
    }
    exit $bad;
}
for my $pair ( [ $mdfile, $md ], [ $htmlfile, $html ] ) {
    open( my $out, '>', $pair->[0] ) or die "$0: cannot write $pair->[0]: $!\n";
    print $out $pair->[1];
    close $out;
}
printf "%s and %s written (%d rules)\n", $mdfile, $htmlfile, scalar keys %defined;
