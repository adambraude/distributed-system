#!/bin/bash

# Count the number of vectors on this slave.

# Get directory of script file
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

# Count the vectors
NUM_VECS="$(ls -l $DIR/bin/ | grep v_ | wc -l)"
echo "Number of vectors = $NUM_VECS"
