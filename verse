#!/usr/bin/env bash

f=$1
shift

CC=gcc
C_FLAGS="-g -std=c99"
TMPFILE_BASE=$(mktemp /tmp/verse-out-XXXXXX)
C_TMPFILE=$TMPFILE_BASE.c
EXE_TMPFILE=$TMPFILE_BASE.out

for i in "$@"; do
    shift
    if [ "$i" == "-" ]; then
        break
    fi
    CMD="$CMD $i"
done
ARGS=$@

./bin/compiler -o $C_TMPFILE $f
if [ $? != 0 ]; then
    exit $?
fi

INPUT_FILES="src/*/*.S $C_TMPFILE"
LINK_FLAGS=$(./bin/compiler -libs $f)
$CC $INPUT_FILES -o $EXE_TMPFILE $C_FLAGS $LINK_FLAGS
if [ $? != 0 ]; then
    exit $?
fi

echo "(running $EXE_TMPFILE ...)"

$EXE_TMPFILE $ARGS
