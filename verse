#!/usr/bin/env bash

f=$1
shift

C_FLAGS="-g -std=c99"
CMD="gcc src/*/*.S tmp.c -o tmp.out $C_FLAGS"

for i in "$@"; do
    shift
    if [ "$i" == "-" ]; then
        break
    fi
    CMD="$CMD $i"
done
ARGS=$@

./bin/compiler $f > tmp.c && $CMD && ./tmp.out $ARGS
