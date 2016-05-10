#!/usr/bin/env bash

cat $1 | ./compiler > tmp.c && gcc tmp.c -o tmp.out -std=c99 && ./tmp.out
