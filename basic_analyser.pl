#!/usr/bin/perl

use warnings;

my $CPM2mRph = 1/(60*11);   # Conversion factor depending on Geiger-Mueller tube.
                            #Here 11 CPS -> 1 mR/h.

my $avg_on = 10;            # Number of values on which to calculate the averages



my $delay = 10;


my $mRph2muSvph = 10;
my $irradtot = 0;
my $count = 0;

sub CPMaverage {
	my $on = shift;
	my @values = @_;
	my $earliest = 1000000;
	my $latest = 0;
	for (my $i = 1; $i < $on+1; $i++) {
		my $t = $values[-$i][0];
		if ($t > $latest) {
			$latest = $t;
		}
		if ($t < $earliest) {
			$earliest = $t;
		}
	}
	return $on * 60 / ($latest - $earliest);

}

sub displaytotalfr {
        $irradtot = $count * $CPM2mRph * $mRph2muSvph;
	printf "Total: %u impulsions detectees; Dose recue: %10.3e microSv.", $count, $irradtot;
}

sub displayaveragefr {
        my $on = shift;
	my @values = @_;
	my $CPMaverage10 = CPMaverage($on, @values);
	my $CPSaverage10 = $CPMaverage10 / 60;
	my $mRpH = $CPMaverage10 * $CPM2mRph;
	printf " | Moyenne glissante sur %u valeurs: %10.4f CPM", $on, $CPMaverage10;
	printf " = %9.5f CPS", $CPSaverage10;
	printf "--> exposition actuelle: %4.2f mR/h", $mRpH;
}

sub displayfr {
	my $on = shift;
	my @values = @_;
	print "t = $values[-1][0]. | ";
	displaytotalfr();
	if (scalar(@values) >= $on) {
	        displayaveragefr($on, @values);
	}
	print "\n";
}

print "Using a conversion factor of $CPM2mRph mR/h <-> 1 CPM\n";

my @vals = ();
my $dt = 0;
while (<>) {
	my ($t, $a) = /(\d+\.\d*)\s+(\d+)/;
	push @vals, [$t, $a];
	$count++;

	if ($t - $dt > $delay) {
	        displayfr($avg_on, @vals);
		$dt = $t;
	}
}

print "Total: En $vals[-1][0] secondes: ";
displaytotalfr($avg_on, @vals);
print "\n";
