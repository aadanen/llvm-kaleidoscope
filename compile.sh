#!/usr/bin/sh

./build/kalos kl/${1}.kl -o kl/${1}.o
clang++ kl/${1}.o kl/main.cpp

