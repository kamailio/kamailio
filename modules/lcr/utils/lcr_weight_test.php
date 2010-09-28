#!/usr/bin/php
<?php

   // This script can be used to find out actual probabilities
   // that correspond to a list of LCR gateway weights.

if ($argc < 2) {
  echo "Usage: lcr_weight_test.php <list of weights (integers 1-254)>\n";
  exit;
 }

$iters = 10000;

$rands = array();
for ($i = 1; $i <= $iters; $i++) {
  $elem = array();
  for ($j = 1; $j < $argc; $j++) {
    $elem["$j"] = $argv[$j] * (rand() >> 8);
  }
  $rands[] = $elem;
 }

$sorted = array();
foreach ($rands as $rand) {
  asort($rand);
  $sorted[] = $rand;
 }

$counts = array();
for ($j = 1; $j < $argc; $j++) {
  $counts["$j"] = 0;
 }

foreach ($sorted as $rand) {
  end($rand);
  $counts[key($rand)]++;
 }

for ($j = 1; $j < $argc; $j++) {
  echo "weight " . $argv[$j] . " probability " . $counts["$j"]/$iters . "\n";
 }

?>
