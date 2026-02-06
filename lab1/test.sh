#!/usr/bin/env bash

# Command to compare the output of the C and MIPS implementations of heapsort.

# usage: ./test.sh inputfile

# (make sure to run "chmod +x test.sh" first)

# attempt to build the executable if it does not exist
if [ ! -e "heapsort"  ]
then
	make
fi

if diff <(./heapsort < "$1" | tail -n 1) <(spim -file heapsort.s < "$1" | tail -n 1); then
    echo "SUCCESS! Outputs match."
fi
