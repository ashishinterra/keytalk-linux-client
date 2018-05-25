die "Usage: $0 wxi-template version >  ReseptClientInstaller.wxi" unless @ARGV == 2;

$template_path = shift @ARGV; 
open (FTEMPLATE, "<$template_path") || die "Can't open input $template_path: $!\n";
$version_path = shift @ARGV; 
open (FVERSION, "<$version_path") || die "Can't open input $version_path: $!\n";

&process;
close (FVERSION);
close (FTEMPLATE);

sub process 
{
	local $/=undef;  # to grab the entire file 
  
	my $version_contents = <FVERSION>;
	if ($version_contents !~ m/version\=(\d+)\.(\d+)\.(\d+)\s*/)
	    { die "Invalid version format in $version_path" };
	    
	my $major = $1;
	my $minor = $2;
	my $subminor = $3;

	my $template_contents = <FTEMPLATE>;
	$template_contents =~ s/\$\(major\)/$major/;
	$template_contents =~ s/\$\(minor\)/$minor/;
	$template_contents =~ s/\$\(subminor\)/$subminor/;

	printf("<!--  This file is generated from $template_path. Do not edit. -->\n\n");
	print($template_contents);
}


