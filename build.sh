#!/bin/bash -e

git submodule update --init --recursive
west update

source ./env.sh
make
make degu.bin
