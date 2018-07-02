#!/bin/sh
cd bin/
ls -l | grep v_ | wc -l | read NUM_VECS
echo "Number of vectors = $NUM_VECS"
