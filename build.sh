#!/bin/sh

options="-Wall -Wswitch-enum -pedantic -std=c11 -lm"

gcc $1.c -o $1.out $options &&
./$1.out