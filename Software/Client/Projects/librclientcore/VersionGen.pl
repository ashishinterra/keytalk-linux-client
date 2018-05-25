die "Usage: $0 version >  Version.h" unless @ARGV == 1;

$path = shift @ARGV;
open (IN, "<$path") || die "Can't open input $path: $!\n";
&process;
close (IN);

sub process
{
	local $/=undef;  # to grab the entire file i.o. the first line only

	my $contents = <IN>;
	if ($contents !~ m/version\=(\d+)\.(\d+)\.(\d+)\s*/)
	    { die "Invalid version format in $path" };

	my $major = $1;
	my $minor = $2;
	my $subminor = $3;

	printf("#pragma once\n\n");
	printf("// This file is generated from $path. Do not edit.\n\n");
	printf("#include \"ta/version.h\"\n\n");
	printf("namespace rclient \n");
	printf("{\n");
	printf("    static const ta::version::Version ClientVersion(%d,%d,%d);\n", $major, $minor, $subminor);
	printf("}\n\n");
}


