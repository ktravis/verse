#!/usr/bin/env bash

./bin/compiler $1 > tmp.c && gcc tmp.c -g -o tmp.out -std=c99 && ./tmp.out
