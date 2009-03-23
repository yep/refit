#!/usr/bin/perl

open(OUT, ">gradient1.volicon");
print OUT chr(1).chr(0).chr(128).chr(0).chr(12);
foreach $y (0..11) {
  foreach $x (0..127) {
    if (($x % 4) == int($y / 3)) {
      print OUT chr(0xd6);
    } else {
      print OUT chr($x);
    }
  }
}
close(OUT);


open(OUT, ">gradient2.volicon");
print OUT chr(1).chr(0).chr(128).chr(0).chr(12);
foreach $y (0..11) {
  foreach $x (128..255) {
    if (($x % 4) == int($y / 3)) {
      print OUT chr(0xd6);
    } else {
      print OUT chr($x);
    }
  }
}
close(OUT);


open(OUT, ">gradient3.volicon");
print OUT chr(1).chr(0).chr(32).chr(0).chr(12);
@tab = ( 0x00, 0xf6, 0xf7, 0x2a, 0xf8, 0xf9, 0x55, 0xfa, 0xfb, 0x80, 0xfc, 0xfd, 0xab, 0xfe, 0xff, 0xd6 );
foreach $y (0..11) {
  foreach $x (0..15) {
    foreach $wide (0..1) {
      if (($x % 4) == int($y / 3)) {
        print OUT chr(0xd6);
      } else {
        print OUT chr($tab[$x]);
      }
    }
  }
}

close(OUT);



exit 0;
