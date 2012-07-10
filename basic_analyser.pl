#!/usr/bin/perl

use warnings;

my $avg_on = 10; # Number of value on which to calculate the average

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

my @vals = ();
while (<>) {
	my ($t, $a) = /(\d+\.\d*)\s+(\d+)/;
	push @vals, [$t, $a];
	$avg_on = 10;
	if (scalar(@vals) >= $avg_on) {
		my $average10 = CPMaverage($avg_on, @vals);
		print "t = $vals[-1][0]: ";
		print "Moyenne glissante sur $avg_on valeurs: $average10 CPM\n";
	}
}
