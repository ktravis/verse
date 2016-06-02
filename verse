#!/usr/bin/env bash

cat $1 | ./bin/compiler > tmp.c && gcc tmp.c -g -o tmp.out -std=c99 && ./tmp.out
