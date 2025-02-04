#!/bin/bash
# many connections try to connect to the server near-simultaneously.
# file 6 is 1gb - significantly more time required to read/write to stdout

for i in {1..50}
do
  ruby client.rb $((($i % 6) + 1)) &
done
wait
