#!/bin/bash -e

if [ ! -e .west ]; then
	cd manifest-repo
	west init -l
	cd ..
fi
west update

git submodule update --init --recursive

source ./env.sh
make
make degu.bin
