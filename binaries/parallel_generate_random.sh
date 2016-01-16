#!/usr/bin/env bash

CORES=50
NUM=$1
WORKDIR="workdir"
time seq 1 $CORES | parallel --results "$WORKDIR" \
                             --jobs "$CORES" \
                             --arg-file - \
                             "./GenerateRandomStrings -number $NUM" &> /dev/null
cat "$WORKDIR"/1/*/stdout
